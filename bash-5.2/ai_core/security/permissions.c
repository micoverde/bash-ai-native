/* permissions.c - Fine-grained permission system for ANBS enterprise security */

#include "../ai_display.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fnmatch.h>
#include <json-c/json.h>

#define MAX_PERMISSION_RULES 1000
#define MAX_ROLES 100
#define MAX_AGENT_PERMISSIONS 500

typedef enum {
    PERM_TYPE_FILE_READ = 1,
    PERM_TYPE_FILE_WRITE = 2,
    PERM_TYPE_FILE_EXECUTE = 4,
    PERM_TYPE_NETWORK_CONNECT = 8,
    PERM_TYPE_NETWORK_LISTEN = 16,
    PERM_TYPE_SYSTEM_ADMIN = 32,
    PERM_TYPE_AI_API_ACCESS = 64,
    PERM_TYPE_MEMORY_ACCESS = 128,
    PERM_TYPE_PROCESS_CONTROL = 256
} permission_type_t;

typedef enum {
    EFFECT_ALLOW = 1,
    EFFECT_DENY = 2
} permission_effect_t;

typedef struct {
    char resource_pattern[512];
    permission_type_t permission_type;
    permission_effect_t effect;
    char conditions[256];
    time_t valid_from;
    time_t valid_until;
    int priority;
    bool active;
} permission_rule_t;

typedef struct {
    char role_name[64];
    permission_rule_t rules[MAX_PERMISSION_RULES];
    int rule_count;
    char description[256];
    bool inheritable;
} role_t;

typedef struct {
    char agent_id[64];
    char role_names[MAX_ROLES][64];
    int role_count;
    permission_rule_t custom_rules[MAX_AGENT_PERMISSIONS];
    int custom_rule_count;
    time_t last_access_check;
    int denied_operations_count;
    int allowed_operations_count;
} agent_permissions_t;

typedef struct {
    role_t roles[MAX_ROLES];
    int role_count;
    agent_permissions_t agent_perms[MAX_AGENTS];
    int agent_count;
    pthread_mutex_t mutex;
    char policy_file_path[512];
} permission_manager_t;

static permission_manager_t *g_perm_manager = NULL;

/* Initialize permission manager */
int anbs_permissions_init(const char *policy_file) {
    if (g_perm_manager) {
        return 0; /* Already initialized */
    }

    g_perm_manager = calloc(1, sizeof(permission_manager_t));
    if (!g_perm_manager) {
        return -1;
    }

    pthread_mutex_init(&g_perm_manager->mutex, NULL);

    if (policy_file) {
        strncpy(g_perm_manager->policy_file_path, policy_file,
                sizeof(g_perm_manager->policy_file_path) - 1);
    } else {
        strcpy(g_perm_manager->policy_file_path, "/etc/anbs/permissions.json");
    }

    /* Create default roles */
    anbs_permissions_create_default_roles();

    /* Load existing policy if available */
    anbs_permissions_load_policy();

    ANBS_DEBUG_LOG("Permission manager initialized with policy: %s",
                   g_perm_manager->policy_file_path);
    return 0;
}

/* Create default roles */
int anbs_permissions_create_default_roles(void) {
    if (!g_perm_manager) {
        return -1;
    }

    pthread_mutex_lock(&g_perm_manager->mutex);

    /* Guest role - minimal permissions */
    role_t *guest_role = &g_perm_manager->roles[g_perm_manager->role_count++];
    strcpy(guest_role->role_name, "guest");
    strcpy(guest_role->description, "Minimal read-only access");

    /* Add read permissions for safe directories */
    permission_rule_t *rule = &guest_role->rules[guest_role->rule_count++];
    strcpy(rule->resource_pattern, "/tmp/anbs/guest/*");
    rule->permission_type = PERM_TYPE_FILE_READ;
    rule->effect = EFFECT_ALLOW;
    rule->priority = 100;
    rule->active = true;

    /* User role - standard user permissions */
    role_t *user_role = &g_perm_manager->roles[g_perm_manager->role_count++];
    strcpy(user_role->role_name, "user");
    strcpy(user_role->description, "Standard user access for AI agents");

    rule = &user_role->rules[user_role->rule_count++];
    strcpy(rule->resource_pattern, "/home/*/");
    rule->permission_type = PERM_TYPE_FILE_READ | PERM_TYPE_FILE_WRITE;
    rule->effect = EFFECT_ALLOW;
    rule->priority = 200;
    rule->active = true;

    rule = &user_role->rules[user_role->rule_count++];
    strcpy(rule->resource_pattern, "api.anthropic.com");
    rule->permission_type = PERM_TYPE_AI_API_ACCESS;
    rule->effect = EFFECT_ALLOW;
    rule->priority = 200;
    rule->active = true;

    /* Developer role - enhanced permissions */
    role_t *dev_role = &g_perm_manager->roles[g_perm_manager->role_count++];
    strcpy(dev_role->role_name, "developer");
    strcpy(dev_role->description, "Enhanced access for development tasks");

    rule = &dev_role->rules[dev_role->rule_count++];
    strcpy(rule->resource_pattern, "/usr/src/*");
    rule->permission_type = PERM_TYPE_FILE_READ | PERM_TYPE_FILE_WRITE | PERM_TYPE_FILE_EXECUTE;
    rule->effect = EFFECT_ALLOW;
    rule->priority = 300;
    rule->active = true;

    rule = &dev_role->rules[dev_role->rule_count++];
    strcpy(rule->resource_pattern, "*.anthropic.com");
    rule->permission_type = PERM_TYPE_AI_API_ACCESS;
    rule->effect = EFFECT_ALLOW;
    rule->priority = 300;
    rule->active = true;

    /* Admin role - full permissions */
    role_t *admin_role = &g_perm_manager->roles[g_perm_manager->role_count++];
    strcpy(admin_role->role_name, "admin");
    strcpy(admin_role->description, "Full administrative access");

    rule = &admin_role->rules[admin_role->rule_count++];
    strcpy(rule->resource_pattern, "*");
    rule->permission_type = PERM_TYPE_FILE_READ | PERM_TYPE_FILE_WRITE | PERM_TYPE_FILE_EXECUTE |
                           PERM_TYPE_NETWORK_CONNECT | PERM_TYPE_NETWORK_LISTEN |
                           PERM_TYPE_SYSTEM_ADMIN | PERM_TYPE_AI_API_ACCESS |
                           PERM_TYPE_MEMORY_ACCESS | PERM_TYPE_PROCESS_CONTROL;
    rule->effect = EFFECT_ALLOW;
    rule->priority = 1000;
    rule->active = true;

    pthread_mutex_unlock(&g_perm_manager->mutex);

    ANBS_DEBUG_LOG("Created %d default roles", g_perm_manager->role_count);
    return 0;
}

/* Assign role to agent */
int anbs_permissions_assign_role(const char *agent_id, const char *role_name) {
    if (!g_perm_manager || !agent_id || !role_name) {
        return -1;
    }

    pthread_mutex_lock(&g_perm_manager->mutex);

    /* Find or create agent permissions */
    agent_permissions_t *agent_perms = NULL;
    for (int i = 0; i < g_perm_manager->agent_count; i++) {
        if (strcmp(g_perm_manager->agent_perms[i].agent_id, agent_id) == 0) {
            agent_perms = &g_perm_manager->agent_perms[i];
            break;
        }
    }

    if (!agent_perms && g_perm_manager->agent_count < MAX_AGENTS) {
        agent_perms = &g_perm_manager->agent_perms[g_perm_manager->agent_count++];
        strncpy(agent_perms->agent_id, agent_id, sizeof(agent_perms->agent_id) - 1);
    }

    if (!agent_perms) {
        pthread_mutex_unlock(&g_perm_manager->mutex);
        return -1;
    }

    /* Check if role already assigned */
    for (int i = 0; i < agent_perms->role_count; i++) {
        if (strcmp(agent_perms->role_names[i], role_name) == 0) {
            pthread_mutex_unlock(&g_perm_manager->mutex);
            return 0; /* Already assigned */
        }
    }

    /* Add role to agent */
    if (agent_perms->role_count < MAX_ROLES) {
        strncpy(agent_perms->role_names[agent_perms->role_count],
                role_name, sizeof(agent_perms->role_names[0]) - 1);
        agent_perms->role_count++;
    }

    pthread_mutex_unlock(&g_perm_manager->mutex);

    ANBS_DEBUG_LOG("Assigned role '%s' to agent '%s'", role_name, agent_id);
    return 0;
}

/* Check if agent has permission for operation */
bool anbs_permissions_check(const char *agent_id, const char *resource,
                           permission_type_t permission_type) {
    if (!g_perm_manager || !agent_id || !resource) {
        return false;
    }

    pthread_mutex_lock(&g_perm_manager->mutex);

    /* Find agent permissions */
    agent_permissions_t *agent_perms = NULL;
    for (int i = 0; i < g_perm_manager->agent_count; i++) {
        if (strcmp(g_perm_manager->agent_perms[i].agent_id, agent_id) == 0) {
            agent_perms = &g_perm_manager->agent_perms[i];
            break;
        }
    }

    if (!agent_perms) {
        pthread_mutex_unlock(&g_perm_manager->mutex);
        ANBS_DEBUG_LOG("Permission denied: agent '%s' not found", agent_id);
        return false;
    }

    /* Update access check time */
    agent_perms->last_access_check = time(NULL);

    /* Collect all applicable rules */
    permission_rule_t applicable_rules[MAX_PERMISSION_RULES];
    int applicable_count = 0;

    /* Check custom rules first (highest priority) */
    for (int i = 0; i < agent_perms->custom_rule_count; i++) {
        permission_rule_t *rule = &agent_perms->custom_rules[i];
        if (rule->active && (rule->permission_type & permission_type) &&
            fnmatch(rule->resource_pattern, resource, FNM_PATHNAME) == 0) {
            applicable_rules[applicable_count++] = *rule;
        }
    }

    /* Check role-based rules */
    for (int r = 0; r < agent_perms->role_count; r++) {
        const char *role_name = agent_perms->role_names[r];

        /* Find role */
        for (int i = 0; i < g_perm_manager->role_count; i++) {
            role_t *role = &g_perm_manager->roles[i];
            if (strcmp(role->role_name, role_name) == 0) {
                /* Check rules in this role */
                for (int j = 0; j < role->rule_count; j++) {
                    permission_rule_t *rule = &role->rules[j];
                    if (rule->active && (rule->permission_type & permission_type) &&
                        fnmatch(rule->resource_pattern, resource, FNM_PATHNAME) == 0) {
                        applicable_rules[applicable_count++] = *rule;
                    }
                }
                break;
            }
        }
    }

    /* Sort rules by priority (higher priority first) */
    for (int i = 0; i < applicable_count - 1; i++) {
        for (int j = i + 1; j < applicable_count; j++) {
            if (applicable_rules[i].priority < applicable_rules[j].priority) {
                permission_rule_t temp = applicable_rules[i];
                applicable_rules[i] = applicable_rules[j];
                applicable_rules[j] = temp;
            }
        }
    }

    /* Evaluate rules (first match wins) */
    bool access_granted = false;
    for (int i = 0; i < applicable_count; i++) {
        permission_rule_t *rule = &applicable_rules[i];

        /* Check time constraints */
        time_t now = time(NULL);
        if (rule->valid_from > 0 && now < rule->valid_from) {
            continue;
        }
        if (rule->valid_until > 0 && now > rule->valid_until) {
            continue;
        }

        /* Apply rule effect */
        if (rule->effect == EFFECT_ALLOW) {
            access_granted = true;
            agent_perms->allowed_operations_count++;
            break;
        } else if (rule->effect == EFFECT_DENY) {
            access_granted = false;
            agent_perms->denied_operations_count++;
            break;
        }
    }

    if (!access_granted) {
        agent_perms->denied_operations_count++;
    }

    pthread_mutex_unlock(&g_perm_manager->mutex);

    ANBS_DEBUG_LOG("Permission check for agent '%s', resource '%s', type %d: %s",
                   agent_id, resource, permission_type,
                   access_granted ? "GRANTED" : "DENIED");

    return access_granted;
}

/* Add custom permission rule to agent */
int anbs_permissions_add_custom_rule(const char *agent_id, const char *resource_pattern,
                                    permission_type_t permission_type,
                                    permission_effect_t effect, int priority) {
    if (!g_perm_manager || !agent_id || !resource_pattern) {
        return -1;
    }

    pthread_mutex_lock(&g_perm_manager->mutex);

    /* Find agent permissions */
    agent_permissions_t *agent_perms = NULL;
    for (int i = 0; i < g_perm_manager->agent_count; i++) {
        if (strcmp(g_perm_manager->agent_perms[i].agent_id, agent_id) == 0) {
            agent_perms = &g_perm_manager->agent_perms[i];
            break;
        }
    }

    if (!agent_perms) {
        pthread_mutex_unlock(&g_perm_manager->mutex);
        return -1;
    }

    if (agent_perms->custom_rule_count >= MAX_AGENT_PERMISSIONS) {
        pthread_mutex_unlock(&g_perm_manager->mutex);
        return -1;
    }

    /* Add custom rule */
    permission_rule_t *rule = &agent_perms->custom_rules[agent_perms->custom_rule_count++];
    strncpy(rule->resource_pattern, resource_pattern, sizeof(rule->resource_pattern) - 1);
    rule->permission_type = permission_type;
    rule->effect = effect;
    rule->priority = priority;
    rule->active = true;

    pthread_mutex_unlock(&g_perm_manager->mutex);

    ANBS_DEBUG_LOG("Added custom rule for agent '%s': %s (%s)",
                   agent_id, resource_pattern,
                   effect == EFFECT_ALLOW ? "ALLOW" : "DENY");
    return 0;
}

/* Load permission policy from JSON file */
int anbs_permissions_load_policy(void) {
    if (!g_perm_manager) {
        return -1;
    }

    FILE *file = fopen(g_perm_manager->policy_file_path, "r");
    if (!file) {
        ANBS_DEBUG_LOG("Policy file not found: %s", g_perm_manager->policy_file_path);
        return 0; /* Not an error - use defaults */
    }

    /* Read file content */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return -1;
    }

    fread(content, 1, file_size, file);
    content[file_size] = '\0';
    fclose(file);

    /* Parse JSON */
    json_object *root = json_tokener_parse(content);
    free(content);

    if (!root) {
        ANBS_DEBUG_LOG("Failed to parse policy JSON");
        return -1;
    }

    pthread_mutex_lock(&g_perm_manager->mutex);

    /* Load roles */
    json_object *roles_obj;
    if (json_object_object_get_ex(root, "roles", &roles_obj)) {
        int roles_len = json_object_array_length(roles_obj);
        for (int i = 0; i < roles_len && g_perm_manager->role_count < MAX_ROLES; i++) {
            json_object *role_obj = json_object_array_get_idx(roles_obj, i);
            json_object *name_obj, *desc_obj, *rules_obj;

            if (json_object_object_get_ex(role_obj, "name", &name_obj) &&
                json_object_object_get_ex(role_obj, "description", &desc_obj) &&
                json_object_object_get_ex(role_obj, "rules", &rules_obj)) {

                role_t *role = &g_perm_manager->roles[g_perm_manager->role_count++];
                strncpy(role->role_name, json_object_get_string(name_obj),
                       sizeof(role->role_name) - 1);
                strncpy(role->description, json_object_get_string(desc_obj),
                       sizeof(role->description) - 1);

                /* Load rules for this role */
                int rules_len = json_object_array_length(rules_obj);
                for (int j = 0; j < rules_len && role->rule_count < MAX_PERMISSION_RULES; j++) {
                    json_object *rule_obj = json_object_array_get_idx(rules_obj, j);
                    json_object *pattern_obj, *type_obj, *effect_obj, *priority_obj;

                    if (json_object_object_get_ex(rule_obj, "resource", &pattern_obj) &&
                        json_object_object_get_ex(rule_obj, "permission", &type_obj) &&
                        json_object_object_get_ex(rule_obj, "effect", &effect_obj) &&
                        json_object_object_get_ex(rule_obj, "priority", &priority_obj)) {

                        permission_rule_t *rule = &role->rules[role->rule_count++];
                        strncpy(rule->resource_pattern, json_object_get_string(pattern_obj),
                               sizeof(rule->resource_pattern) - 1);
                        rule->permission_type = json_object_get_int(type_obj);
                        rule->effect = json_object_get_int(effect_obj);
                        rule->priority = json_object_get_int(priority_obj);
                        rule->active = true;
                    }
                }
            }
        }
    }

    /* Load agent assignments */
    json_object *agents_obj;
    if (json_object_object_get_ex(root, "agents", &agents_obj)) {
        int agents_len = json_object_array_length(agents_obj);
        for (int i = 0; i < agents_len && g_perm_manager->agent_count < MAX_AGENTS; i++) {
            json_object *agent_obj = json_object_array_get_idx(agents_obj, i);
            json_object *id_obj, *roles_array_obj;

            if (json_object_object_get_ex(agent_obj, "agent_id", &id_obj) &&
                json_object_object_get_ex(agent_obj, "roles", &roles_array_obj)) {

                const char *agent_id = json_object_get_string(id_obj);
                int roles_array_len = json_object_array_length(roles_array_obj);

                for (int j = 0; j < roles_array_len; j++) {
                    json_object *role_name_obj = json_object_array_get_idx(roles_array_obj, j);
                    const char *role_name = json_object_get_string(role_name_obj);
                    anbs_permissions_assign_role(agent_id, role_name);
                }
            }
        }
    }

    pthread_mutex_unlock(&g_perm_manager->mutex);

    json_object_put(root);

    ANBS_DEBUG_LOG("Loaded permission policy with %d roles and %d agents",
                   g_perm_manager->role_count, g_perm_manager->agent_count);
    return 0;
}

/* Save permission policy to JSON file */
int anbs_permissions_save_policy(void) {
    if (!g_perm_manager) {
        return -1;
    }

    pthread_mutex_lock(&g_perm_manager->mutex);

    json_object *root = json_object_new_object();

    /* Save roles */
    json_object *roles_array = json_object_new_array();
    for (int i = 0; i < g_perm_manager->role_count; i++) {
        role_t *role = &g_perm_manager->roles[i];

        json_object *role_obj = json_object_new_object();
        json_object_object_add(role_obj, "name", json_object_new_string(role->role_name));
        json_object_object_add(role_obj, "description", json_object_new_string(role->description));

        json_object *rules_array = json_object_new_array();
        for (int j = 0; j < role->rule_count; j++) {
            permission_rule_t *rule = &role->rules[j];

            json_object *rule_obj = json_object_new_object();
            json_object_object_add(rule_obj, "resource", json_object_new_string(rule->resource_pattern));
            json_object_object_add(rule_obj, "permission", json_object_new_int(rule->permission_type));
            json_object_object_add(rule_obj, "effect", json_object_new_int(rule->effect));
            json_object_object_add(rule_obj, "priority", json_object_new_int(rule->priority));

            json_object_array_add(rules_array, rule_obj);
        }

        json_object_object_add(role_obj, "rules", rules_array);
        json_object_array_add(roles_array, role_obj);
    }
    json_object_object_add(root, "roles", roles_array);

    /* Save agent assignments */
    json_object *agents_array = json_object_new_array();
    for (int i = 0; i < g_perm_manager->agent_count; i++) {
        agent_permissions_t *agent = &g_perm_manager->agent_perms[i];

        json_object *agent_obj = json_object_new_object();
        json_object_object_add(agent_obj, "agent_id", json_object_new_string(agent->agent_id));

        json_object *roles_array_obj = json_object_new_array();
        for (int j = 0; j < agent->role_count; j++) {
            json_object_array_add(roles_array_obj, json_object_new_string(agent->role_names[j]));
        }
        json_object_object_add(agent_obj, "roles", roles_array_obj);

        json_object_array_add(agents_array, agent_obj);
    }
    json_object_object_add(root, "agents", agents_array);

    /* Write to file */
    const char *json_string = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    FILE *file = fopen(g_perm_manager->policy_file_path, "w");
    if (file) {
        fprintf(file, "%s\n", json_string);
        fclose(file);
    }

    json_object_put(root);

    pthread_mutex_unlock(&g_perm_manager->mutex);

    ANBS_DEBUG_LOG("Saved permission policy to %s", g_perm_manager->policy_file_path);
    return 0;
}

/* Get permission statistics for agent */
int anbs_permissions_get_stats(const char *agent_id, char **stats_json) {
    if (!g_perm_manager || !agent_id || !stats_json) {
        return -1;
    }

    pthread_mutex_lock(&g_perm_manager->mutex);

    /* Find agent permissions */
    agent_permissions_t *agent_perms = NULL;
    for (int i = 0; i < g_perm_manager->agent_count; i++) {
        if (strcmp(g_perm_manager->agent_perms[i].agent_id, agent_id) == 0) {
            agent_perms = &g_perm_manager->agent_perms[i];
            break;
        }
    }

    if (!agent_perms) {
        pthread_mutex_unlock(&g_perm_manager->mutex);
        return -1;
    }

    char *stats = malloc(1024);
    if (!stats) {
        pthread_mutex_unlock(&g_perm_manager->mutex);
        return -1;
    }

    snprintf(stats, 1024,
             "{"
             "\"agent_id\": \"%s\","
             "\"roles_count\": %d,"
             "\"custom_rules_count\": %d,"
             "\"allowed_operations\": %d,"
             "\"denied_operations\": %d,"
             "\"last_access_check\": %ld,"
             "\"success_rate\": %.2f"
             "}",
             agent_perms->agent_id,
             agent_perms->role_count,
             agent_perms->custom_rule_count,
             agent_perms->allowed_operations_count,
             agent_perms->denied_operations_count,
             agent_perms->last_access_check,
             agent_perms->allowed_operations_count > 0 ?
             (float)agent_perms->allowed_operations_count /
             (agent_perms->allowed_operations_count + agent_perms->denied_operations_count) * 100.0 : 0.0);

    *stats_json = stats;

    pthread_mutex_unlock(&g_perm_manager->mutex);
    return 0;
}

/* Cleanup permission manager */
void anbs_permissions_cleanup(void) {
    if (!g_perm_manager) {
        return;
    }

    /* Save current policy */
    anbs_permissions_save_policy();

    pthread_mutex_destroy(&g_perm_manager->mutex);

    free(g_perm_manager);
    g_perm_manager = NULL;

    ANBS_DEBUG_LOG("Permission manager cleaned up");
}