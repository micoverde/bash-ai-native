/* sandbox.c - Agent sandboxing system for ANBS enterprise security */

#include "../ai_display.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/capability.h>
#include <seccomp.h>
#include <pwd.h>
#include <grp.h>
#include <linux/limits.h>

#define MAX_AGENTS 50
#define MAX_POLICIES 100
#define SANDBOX_UID_BASE 10000
#define SANDBOX_GID_BASE 10000

typedef enum {
    PERM_READ = 1,
    PERM_WRITE = 2,
    PERM_EXECUTE = 4,
    PERM_NETWORK = 8,
    PERM_ADMIN = 16
} permission_flags_t;

typedef struct {
    char path_pattern[PATH_MAX];
    permission_flags_t permissions;
    bool recursive;
} access_rule_t;

typedef struct {
    size_t max_memory_mb;
    double max_cpu_percent;
    size_t max_disk_mb;
    int max_open_files;
    int max_processes;
    int max_network_connections;
} resource_limits_t;

typedef struct {
    char agent_id[64];
    uid_t sandbox_uid;
    gid_t sandbox_gid;
    char sandbox_root[PATH_MAX];
    access_rule_t access_rules[MAX_POLICIES];
    int rule_count;
    resource_limits_t limits;
    bool network_enabled;
    char allowed_networks[256];
    bool active;
    pid_t sandbox_pid;
    time_t created;
    time_t last_activity;
} sandbox_t;

typedef struct {
    sandbox_t sandboxes[MAX_AGENTS];
    int sandbox_count;
    pthread_mutex_t mutex;
    char sandbox_base_dir[PATH_MAX];
} sandbox_manager_t;

static sandbox_manager_t *g_sandbox_manager = NULL;

/* Initialize sandbox manager */
int anbs_sandbox_init(const char *base_dir) {
    if (g_sandbox_manager) {
        return 0; /* Already initialized */
    }

    g_sandbox_manager = calloc(1, sizeof(sandbox_manager_t));
    if (!g_sandbox_manager) {
        return -1;
    }

    strncpy(g_sandbox_manager->sandbox_base_dir, base_dir, sizeof(g_sandbox_manager->sandbox_base_dir) - 1);
    pthread_mutex_init(&g_sandbox_manager->mutex, NULL);

    /* Create base sandbox directory */
    if (mkdir(base_dir, 0755) != 0 && errno != EEXIST) {
        ANBS_DEBUG_LOG("Failed to create sandbox base directory: %s", base_dir);
        anbs_sandbox_cleanup();
        return -1;
    }

    ANBS_DEBUG_LOG("Sandbox manager initialized with base: %s", base_dir);
    return 0;
}

/* Create sandbox for agent */
int anbs_sandbox_create(const char *agent_id, const resource_limits_t *limits) {
    if (!g_sandbox_manager || !agent_id) {
        return -1;
    }

    pthread_mutex_lock(&g_sandbox_manager->mutex);

    /* Check if sandbox already exists */
    for (int i = 0; i < g_sandbox_manager->sandbox_count; i++) {
        if (strcmp(g_sandbox_manager->sandboxes[i].agent_id, agent_id) == 0) {
            pthread_mutex_unlock(&g_sandbox_manager->mutex);
            return i; /* Return existing sandbox ID */
        }
    }

    if (g_sandbox_manager->sandbox_count >= MAX_AGENTS) {
        pthread_mutex_unlock(&g_sandbox_manager->mutex);
        return -1;
    }

    sandbox_t *sandbox = &g_sandbox_manager->sandboxes[g_sandbox_manager->sandbox_count];

    /* Initialize sandbox */
    strncpy(sandbox->agent_id, agent_id, sizeof(sandbox->agent_id) - 1);
    sandbox->sandbox_uid = SANDBOX_UID_BASE + g_sandbox_manager->sandbox_count;
    sandbox->sandbox_gid = SANDBOX_GID_BASE + g_sandbox_manager->sandbox_count;

    snprintf(sandbox->sandbox_root, sizeof(sandbox->sandbox_root),
             "%s/agent_%s", g_sandbox_manager->sandbox_base_dir, agent_id);

    if (limits) {
        sandbox->limits = *limits;
    } else {
        /* Default limits */
        sandbox->limits.max_memory_mb = 512;
        sandbox->limits.max_cpu_percent = 50.0;
        sandbox->limits.max_disk_mb = 1024;
        sandbox->limits.max_open_files = 100;
        sandbox->limits.max_processes = 10;
        sandbox->limits.max_network_connections = 20;
    }

    sandbox->created = time(NULL);
    sandbox->active = false;

    /* Create sandbox directory structure */
    if (mkdir(sandbox->sandbox_root, 0755) != 0 && errno != EEXIST) {
        ANBS_DEBUG_LOG("Failed to create sandbox directory: %s", sandbox->sandbox_root);
        pthread_mutex_unlock(&g_sandbox_manager->mutex);
        return -1;
    }

    /* Create subdirectories */
    char subdir[PATH_MAX];
    const char *subdirs[] = {"tmp", "logs", "work", "data", NULL};

    for (int i = 0; subdirs[i]; i++) {
        snprintf(subdir, sizeof(subdir), "%s/%s", sandbox->sandbox_root, subdirs[i]);
        mkdir(subdir, 0755);
    }

    /* Set default access rules */
    anbs_sandbox_add_access_rule(g_sandbox_manager->sandbox_count,
                                sandbox->sandbox_root, PERM_READ | PERM_WRITE, true);
    anbs_sandbox_add_access_rule(g_sandbox_manager->sandbox_count,
                                "/usr/bin", PERM_READ | PERM_EXECUTE, false);
    anbs_sandbox_add_access_rule(g_sandbox_manager->sandbox_count,
                                "/bin", PERM_READ | PERM_EXECUTE, false);

    int sandbox_id = g_sandbox_manager->sandbox_count++;

    pthread_mutex_unlock(&g_sandbox_manager->mutex);

    ANBS_DEBUG_LOG("Created sandbox %d for agent %s", sandbox_id, agent_id);
    return sandbox_id;
}

/* Add access rule to sandbox */
int anbs_sandbox_add_access_rule(int sandbox_id, const char *path_pattern,
                                 permission_flags_t permissions, bool recursive) {
    if (!g_sandbox_manager || sandbox_id < 0 || sandbox_id >= g_sandbox_manager->sandbox_count) {
        return -1;
    }

    pthread_mutex_lock(&g_sandbox_manager->mutex);

    sandbox_t *sandbox = &g_sandbox_manager->sandboxes[sandbox_id];

    if (sandbox->rule_count >= MAX_POLICIES) {
        pthread_mutex_unlock(&g_sandbox_manager->mutex);
        return -1;
    }

    access_rule_t *rule = &sandbox->access_rules[sandbox->rule_count];
    strncpy(rule->path_pattern, path_pattern, sizeof(rule->path_pattern) - 1);
    rule->permissions = permissions;
    rule->recursive = recursive;

    sandbox->rule_count++;

    pthread_mutex_unlock(&g_sandbox_manager->mutex);

    ANBS_DEBUG_LOG("Added access rule to sandbox %d: %s (permissions: %d)",
                   sandbox_id, path_pattern, permissions);
    return 0;
}

/* Check if path access is allowed */
bool anbs_sandbox_check_access(int sandbox_id, const char *path, permission_flags_t required_perm) {
    if (!g_sandbox_manager || sandbox_id < 0 || sandbox_id >= g_sandbox_manager->sandbox_count) {
        return false;
    }

    pthread_mutex_lock(&g_sandbox_manager->mutex);

    sandbox_t *sandbox = &g_sandbox_manager->sandboxes[sandbox_id];

    for (int i = 0; i < sandbox->rule_count; i++) {
        access_rule_t *rule = &sandbox->access_rules[i];

        /* Check if path matches pattern */
        bool matches = false;
        if (rule->recursive) {
            /* Check if path starts with pattern */
            matches = strncmp(path, rule->path_pattern, strlen(rule->path_pattern)) == 0;
        } else {
            /* Exact match or parent directory */
            matches = strcmp(path, rule->path_pattern) == 0;
            if (!matches) {
                /* Check if it's in the directory */
                char pattern_with_slash[PATH_MAX];
                snprintf(pattern_with_slash, sizeof(pattern_with_slash), "%s/", rule->path_pattern);
                matches = strncmp(path, pattern_with_slash, strlen(pattern_with_slash)) == 0;
            }
        }

        if (matches && (rule->permissions & required_perm)) {
            pthread_mutex_unlock(&g_sandbox_manager->mutex);
            return true;
        }
    }

    pthread_mutex_unlock(&g_sandbox_manager->mutex);
    return false;
}

/* Setup seccomp filter for system call restrictions */
static int setup_seccomp_filter(sandbox_t *sandbox) {
    scmp_filter_ctx ctx;

    /* Create filter context (default deny) */
    ctx = seccomp_init(SCMP_ACT_KILL);
    if (!ctx) {
        return -1;
    }

    /* Allow basic system calls */
    const int allowed_syscalls[] = {
        SCMP_SYS(read), SCMP_SYS(write), SCMP_SYS(open), SCMP_SYS(close),
        SCMP_SYS(stat), SCMP_SYS(fstat), SCMP_SYS(lstat), SCMP_SYS(access),
        SCMP_SYS(mmap), SCMP_SYS(munmap), SCMP_SYS(brk), SCMP_SYS(exit),
        SCMP_SYS(exit_group), SCMP_SYS(getpid), SCMP_SYS(getuid), SCMP_SYS(getgid),
        SCMP_SYS(rt_sigaction), SCMP_SYS(rt_sigprocmask), SCMP_SYS(rt_sigreturn),
        SCMP_SYS(ioctl), SCMP_SYS(poll), SCMP_SYS(select), SCMP_SYS(getcwd),
        SCMP_SYS(dup), SCMP_SYS(dup2), SCMP_SYS(pipe), SCMP_SYS(fork),
        SCMP_SYS(execve), SCMP_SYS(wait4), SCMP_SYS(kill)
    };

    for (size_t i = 0; i < sizeof(allowed_syscalls) / sizeof(allowed_syscalls[0]); i++) {
        if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, allowed_syscalls[i], 0) < 0) {
            seccomp_release(ctx);
            return -1;
        }
    }

    /* Conditionally allow network calls */
    if (sandbox->network_enabled) {
        const int network_syscalls[] = {
            SCMP_SYS(socket), SCMP_SYS(connect), SCMP_SYS(bind),
            SCMP_SYS(listen), SCMP_SYS(accept), SCMP_SYS(sendto),
            SCMP_SYS(recvfrom), SCMP_SYS(shutdown)
        };

        for (size_t i = 0; i < sizeof(network_syscalls) / sizeof(network_syscalls[0]); i++) {
            if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, network_syscalls[i], 0) < 0) {
                seccomp_release(ctx);
                return -1;
            }
        }
    }

    /* Load the filter */
    if (seccomp_load(ctx) < 0) {
        seccomp_release(ctx);
        return -1;
    }

    seccomp_release(ctx);
    return 0;
}

/* Set resource limits for sandbox */
static int set_resource_limits(const resource_limits_t *limits) {
    struct rlimit rlim;

    /* Memory limit */
    rlim.rlim_cur = rlim.rlim_max = limits->max_memory_mb * 1024 * 1024;
    if (setrlimit(RLIMIT_AS, &rlim) != 0) {
        ANBS_DEBUG_LOG("Failed to set memory limit");
        return -1;
    }

    /* CPU time limit (soft limit only for monitoring) */
    rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CPU, &rlim);

    /* Open files limit */
    rlim.rlim_cur = rlim.rlim_max = limits->max_open_files;
    if (setrlimit(RLIMIT_NOFILE, &rlim) != 0) {
        ANBS_DEBUG_LOG("Failed to set file descriptor limit");
        return -1;
    }

    /* Process limit */
    rlim.rlim_cur = rlim.rlim_max = limits->max_processes;
    if (setrlimit(RLIMIT_NPROC, &rlim) != 0) {
        ANBS_DEBUG_LOG("Failed to set process limit");
        return -1;
    }

    return 0;
}

/* Enter sandbox environment */
int anbs_sandbox_enter(int sandbox_id, pid_t *child_pid) {
    if (!g_sandbox_manager || sandbox_id < 0 || sandbox_id >= g_sandbox_manager->sandbox_count) {
        return -1;
    }

    sandbox_t *sandbox = &g_sandbox_manager->sandboxes[sandbox_id];

    pid_t pid = fork();
    if (pid == -1) {
        return -1;
    }

    if (pid == 0) {
        /* Child process - enter sandbox */

        /* Change to sandbox directory */
        if (chdir(sandbox->sandbox_root) != 0) {
            ANBS_DEBUG_LOG("Failed to chdir to sandbox root");
            exit(1);
        }

        /* Change root to sandbox (chroot) */
        if (chroot(sandbox->sandbox_root) != 0) {
            ANBS_DEBUG_LOG("Failed to chroot to sandbox");
            exit(1);
        }

        /* Drop privileges */
        if (setgid(sandbox->sandbox_gid) != 0 || setuid(sandbox->sandbox_uid) != 0) {
            ANBS_DEBUG_LOG("Failed to drop privileges");
            exit(1);
        }

        /* Set resource limits */
        if (set_resource_limits(&sandbox->limits) != 0) {
            ANBS_DEBUG_LOG("Failed to set resource limits");
            exit(1);
        }

        /* Apply seccomp filter */
        if (setup_seccomp_filter(sandbox) != 0) {
            ANBS_DEBUG_LOG("Failed to apply seccomp filter");
            exit(1);
        }

        /* Clear capabilities */
        cap_t caps = cap_init();
        if (cap_set_proc(caps) != 0) {
            ANBS_DEBUG_LOG("Failed to drop capabilities");
            cap_free(caps);
            exit(1);
        }
        cap_free(caps);

        /* Sandbox is now active */
        return 0;
    } else {
        /* Parent process */
        pthread_mutex_lock(&g_sandbox_manager->mutex);
        sandbox->active = true;
        sandbox->sandbox_pid = pid;
        sandbox->last_activity = time(NULL);
        pthread_mutex_unlock(&g_sandbox_manager->mutex);

        if (child_pid) {
            *child_pid = pid;
        }

        ANBS_DEBUG_LOG("Agent %s entered sandbox %d (PID: %d)",
                       sandbox->agent_id, sandbox_id, pid);
        return sandbox_id;
    }
}

/* Exit sandbox environment */
int anbs_sandbox_exit(int sandbox_id) {
    if (!g_sandbox_manager || sandbox_id < 0 || sandbox_id >= g_sandbox_manager->sandbox_count) {
        return -1;
    }

    pthread_mutex_lock(&g_sandbox_manager->mutex);

    sandbox_t *sandbox = &g_sandbox_manager->sandboxes[sandbox_id];

    if (sandbox->active && sandbox->sandbox_pid > 0) {
        /* Kill sandbox process if still running */
        kill(sandbox->sandbox_pid, SIGTERM);

        /* Wait for process to exit */
        int status;
        waitpid(sandbox->sandbox_pid, &status, WNOHANG);

        sandbox->active = false;
        sandbox->sandbox_pid = 0;
    }

    pthread_mutex_unlock(&g_sandbox_manager->mutex);

    ANBS_DEBUG_LOG("Agent %s exited sandbox %d", sandbox->agent_id, sandbox_id);
    return 0;
}

/* Get sandbox status */
int anbs_sandbox_get_status(int sandbox_id, char **status_json) {
    if (!g_sandbox_manager || sandbox_id < 0 || sandbox_id >= g_sandbox_manager->sandbox_count) {
        return -1;
    }

    pthread_mutex_lock(&g_sandbox_manager->mutex);

    sandbox_t *sandbox = &g_sandbox_manager->sandboxes[sandbox_id];

    char *status_str = malloc(2048);
    if (!status_str) {
        pthread_mutex_unlock(&g_sandbox_manager->mutex);
        return -1;
    }

    snprintf(status_str, 2048,
             "{"
             "\"agent_id\": \"%s\","
             "\"sandbox_id\": %d,"
             "\"active\": %s,"
             "\"uid\": %d,"
             "\"gid\": %d,"
             "\"root_path\": \"%s\","
             "\"limits\": {"
             "\"max_memory_mb\": %zu,"
             "\"max_cpu_percent\": %.1f,"
             "\"max_disk_mb\": %zu,"
             "\"max_open_files\": %d,"
             "\"max_processes\": %d"
             "},"
             "\"rules_count\": %d,"
             "\"network_enabled\": %s,"
             "\"created\": %ld,"
             "\"last_activity\": %ld"
             "}",
             sandbox->agent_id, sandbox_id,
             sandbox->active ? "true" : "false",
             sandbox->sandbox_uid, sandbox->sandbox_gid,
             sandbox->sandbox_root,
             sandbox->limits.max_memory_mb,
             sandbox->limits.max_cpu_percent,
             sandbox->limits.max_disk_mb,
             sandbox->limits.max_open_files,
             sandbox->limits.max_processes,
             sandbox->rule_count,
             sandbox->network_enabled ? "true" : "false",
             sandbox->created,
             sandbox->last_activity);

    *status_json = status_str;

    pthread_mutex_unlock(&g_sandbox_manager->mutex);
    return 0;
}

/* Cleanup sandbox manager */
void anbs_sandbox_cleanup(void) {
    if (!g_sandbox_manager) {
        return;
    }

    pthread_mutex_lock(&g_sandbox_manager->mutex);

    /* Exit all active sandboxes */
    for (int i = 0; i < g_sandbox_manager->sandbox_count; i++) {
        if (g_sandbox_manager->sandboxes[i].active) {
            anbs_sandbox_exit(i);
        }
    }

    pthread_mutex_unlock(&g_sandbox_manager->mutex);
    pthread_mutex_destroy(&g_sandbox_manager->mutex);

    free(g_sandbox_manager);
    g_sandbox_manager = NULL;

    ANBS_DEBUG_LOG("Sandbox manager cleaned up");
}