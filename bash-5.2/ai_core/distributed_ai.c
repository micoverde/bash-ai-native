/* distributed_ai.c - Distributed AI consciousness system for ANBS */

#include "ai_display.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <json-c/json.h>
#include <uuid/uuid.h>

#define MAX_AI_AGENTS 10
#define MAX_MESSAGE_SIZE 8192
#define DISCOVERY_PORT 9876
#define COMM_PORT_BASE 9877

typedef enum {
    AGENT_STATUS_OFFLINE = 0,
    AGENT_STATUS_DISCOVERING,
    AGENT_STATUS_CONNECTING,
    AGENT_STATUS_ONLINE,
    AGENT_STATUS_BUSY,
    AGENT_STATUS_ERROR
} agent_status_t;

typedef enum {
    MSG_TYPE_DISCOVERY = 1,
    MSG_TYPE_HANDSHAKE,
    MSG_TYPE_TASK_REQUEST,
    MSG_TYPE_TASK_RESPONSE,
    MSG_TYPE_STATUS_UPDATE,
    MSG_TYPE_HEARTBEAT,
    MSG_TYPE_CAPABILITY_QUERY,
    MSG_TYPE_CAPABILITY_RESPONSE,
    MSG_TYPE_COORDINATION,
    MSG_TYPE_SHUTDOWN
} message_type_t;

typedef struct {
    char agent_id[64];
    char hostname[256];
    char ip_address[64];
    int port;
    agent_status_t status;
    time_t last_seen;
    float cpu_load;
    float memory_usage;
    int task_queue_size;
    char capabilities[512];
    char current_task[256];
    pthread_t comm_thread;
    int socket_fd;
} ai_agent_t;

typedef struct {
    message_type_t type;
    char sender_id[64];
    char recipient_id[64];
    char session_id[64];
    time_t timestamp;
    size_t payload_size;
    char payload[MAX_MESSAGE_SIZE];
} ai_message_t;

typedef struct {
    char session_id[64];
    char task_description[512];
    char assigned_agent[64];
    time_t created;
    time_t started;
    time_t completed;
    int priority;
    char result[2048];
    char status[64];
} task_session_t;

typedef struct {
    ai_agent_t agents[MAX_AI_AGENTS];
    int agent_count;
    char local_agent_id[64];
    int discovery_socket;
    pthread_t discovery_thread;
    pthread_t coordination_thread;
    pthread_mutex_t agents_mutex;
    task_session_t active_tasks[100];
    int task_count;
    pthread_mutex_t tasks_mutex;
    anbs_display_t *display;
    int running;
} distributed_ai_system_t;

static distributed_ai_system_t *g_ai_system = NULL;

/* Generate unique agent ID */
static void generate_agent_id(char *agent_id, size_t size) {
    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);

    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    snprintf(agent_id, size, "anbs-%s-%s", hostname, uuid_str);
}

/* Create message */
static int create_message(message_type_t type, const char *recipient,
                         const char *payload, ai_message_t *msg) {
    if (!g_ai_system || !msg) {
        return -1;
    }

    memset(msg, 0, sizeof(ai_message_t));

    msg->type = type;
    strncpy(msg->sender_id, g_ai_system->local_agent_id, sizeof(msg->sender_id) - 1);
    if (recipient) {
        strncpy(msg->recipient_id, recipient, sizeof(msg->recipient_id) - 1);
    }
    msg->timestamp = time(NULL);

    if (payload) {
        size_t payload_len = strlen(payload);
        if (payload_len >= MAX_MESSAGE_SIZE) {
            payload_len = MAX_MESSAGE_SIZE - 1;
        }
        memcpy(msg->payload, payload, payload_len);
        msg->payload_size = payload_len;
    }

    /* Generate session ID for task-related messages */
    if (type == MSG_TYPE_TASK_REQUEST || type == MSG_TYPE_TASK_RESPONSE) {
        uuid_t uuid;
        uuid_generate(uuid);
        uuid_unparse(uuid, msg->session_id);
    }

    return 0;
}

/* Send message to agent */
static int send_message_to_agent(const ai_agent_t *agent, const ai_message_t *msg) {
    if (!agent || !msg) {
        return -1;
    }

    /* Create JSON representation */
    json_object *root = json_object_new_object();
    json_object *type_obj = json_object_new_int(msg->type);
    json_object *sender_obj = json_object_new_string(msg->sender_id);
    json_object *recipient_obj = json_object_new_string(msg->recipient_id);
    json_object *session_obj = json_object_new_string(msg->session_id);
    json_object *timestamp_obj = json_object_new_int64(msg->timestamp);
    json_object *payload_obj = json_object_new_string(msg->payload);

    json_object_object_add(root, "type", type_obj);
    json_object_object_add(root, "sender", sender_obj);
    json_object_object_add(root, "recipient", recipient_obj);
    json_object_object_add(root, "session", session_obj);
    json_object_object_add(root, "timestamp", timestamp_obj);
    json_object_object_add(root, "payload", payload_obj);

    const char *json_str = json_object_to_json_string(root);

    /* Send via UDP for discovery, TCP for regular communication */
    int result = -1;
    if (msg->type == MSG_TYPE_DISCOVERY) {
        /* UDP broadcast */
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock >= 0) {
            int broadcast = 1;
            setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(DISCOVERY_PORT);
            addr.sin_addr.s_addr = INADDR_BROADCAST;

            result = sendto(sock, json_str, strlen(json_str), 0,
                          (struct sockaddr*)&addr, sizeof(addr));
            close(sock);
        }
    } else {
        /* TCP connection */
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock >= 0) {
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(agent->port);
            inet_pton(AF_INET, agent->ip_address, &addr.sin_addr);

            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                result = send(sock, json_str, strlen(json_str), 0);
            }
            close(sock);
        }
    }

    json_object_put(root);

    ANBS_DEBUG_LOG("Sent message type %d to %s: %s", msg->type, agent->agent_id,
                   result > 0 ? "success" : "failed");

    return result > 0 ? 0 : -1;
}

/* Parse received message */
static int parse_message(const char *json_str, ai_message_t *msg) {
    json_object *root = json_tokener_parse(json_str);
    if (!root) {
        return -1;
    }

    memset(msg, 0, sizeof(ai_message_t));

    json_object *type_obj, *sender_obj, *recipient_obj, *session_obj, *timestamp_obj, *payload_obj;

    if (json_object_object_get_ex(root, "type", &type_obj)) {
        msg->type = json_object_get_int(type_obj);
    }

    if (json_object_object_get_ex(root, "sender", &sender_obj)) {
        strncpy(msg->sender_id, json_object_get_string(sender_obj), sizeof(msg->sender_id) - 1);
    }

    if (json_object_object_get_ex(root, "recipient", &recipient_obj)) {
        strncpy(msg->recipient_id, json_object_get_string(recipient_obj), sizeof(msg->recipient_id) - 1);
    }

    if (json_object_object_get_ex(root, "session", &session_obj)) {
        strncpy(msg->session_id, json_object_get_string(session_obj), sizeof(msg->session_id) - 1);
    }

    if (json_object_object_get_ex(root, "timestamp", &timestamp_obj)) {
        msg->timestamp = json_object_get_int64(timestamp_obj);
    }

    if (json_object_object_get_ex(root, "payload", &payload_obj)) {
        const char *payload = json_object_get_string(payload_obj);
        size_t payload_len = strlen(payload);
        if (payload_len >= MAX_MESSAGE_SIZE) {
            payload_len = MAX_MESSAGE_SIZE - 1;
        }
        memcpy(msg->payload, payload, payload_len);
        msg->payload_size = payload_len;
    }

    json_object_put(root);
    return 0;
}

/* Handle received message */
static void handle_message(const ai_message_t *msg) {
    if (!msg || !g_ai_system) {
        return;
    }

    pthread_mutex_lock(&g_ai_system->agents_mutex);

    switch (msg->type) {
        case MSG_TYPE_DISCOVERY: {
            /* Another agent is discovering - respond with our info */
            ai_message_t response;
            char payload[512];

            snprintf(payload, sizeof(payload),
                    "capabilities=terminal,ai_commands,memory_search,file_analysis;"
                    "status=online;load=%.1f;memory=%.1f",
                    0.0, 0.0); /* TODO: Get actual system stats */

            create_message(MSG_TYPE_HANDSHAKE, msg->sender_id, payload, &response);

            /* Find agent or create new entry */
            ai_agent_t *agent = NULL;
            for (int i = 0; i < g_ai_system->agent_count; i++) {
                if (strcmp(g_ai_system->agents[i].agent_id, msg->sender_id) == 0) {
                    agent = &g_ai_system->agents[i];
                    break;
                }
            }

            if (!agent && g_ai_system->agent_count < MAX_AI_AGENTS) {
                agent = &g_ai_system->agents[g_ai_system->agent_count++];
                strncpy(agent->agent_id, msg->sender_id, sizeof(agent->agent_id) - 1);
                agent->port = COMM_PORT_BASE + g_ai_system->agent_count;
                agent->status = AGENT_STATUS_DISCOVERING;
            }

            if (agent) {
                agent->last_seen = time(NULL);
                /* TODO: Send handshake response */
            }
            break;
        }

        case MSG_TYPE_HANDSHAKE: {
            /* Agent responding to our discovery */
            for (int i = 0; i < g_ai_system->agent_count; i++) {
                if (strcmp(g_ai_system->agents[i].agent_id, msg->sender_id) == 0) {
                    g_ai_system->agents[i].status = AGENT_STATUS_ONLINE;
                    g_ai_system->agents[i].last_seen = time(NULL);
                    strncpy(g_ai_system->agents[i].capabilities, msg->payload,
                           sizeof(g_ai_system->agents[i].capabilities) - 1);

                    if (g_ai_system->display) {
                        char status_msg[256];
                        snprintf(status_msg, sizeof(status_msg),
                                "Connected to AI agent: %s", msg->sender_id);
                        anbs_status_write(g_ai_system->display, status_msg);
                    }
                    break;
                }
            }
            break;
        }

        case MSG_TYPE_TASK_REQUEST: {
            /* Another agent requesting task execution */
            pthread_mutex_lock(&g_ai_system->tasks_mutex);

            if (g_ai_system->task_count < 100) {
                task_session_t *task = &g_ai_system->active_tasks[g_ai_system->task_count++];
                strncpy(task->session_id, msg->session_id, sizeof(task->session_id) - 1);
                strncpy(task->task_description, msg->payload, sizeof(task->task_description) - 1);
                strncpy(task->assigned_agent, g_ai_system->local_agent_id, sizeof(task->assigned_agent) - 1);
                task->created = msg->timestamp;
                task->started = time(NULL);
                task->priority = 5; /* Default priority */
                strcpy(task->status, "processing");

                /* TODO: Actually execute the task */
                snprintf(task->result, sizeof(task->result),
                        "Task processed by %s: %s", g_ai_system->local_agent_id, msg->payload);
                task->completed = time(NULL);
                strcpy(task->status, "completed");

                /* Send response */
                ai_message_t response;
                create_message(MSG_TYPE_TASK_RESPONSE, msg->sender_id, task->result, &response);
                strncpy(response.session_id, msg->session_id, sizeof(response.session_id) - 1);

                /* TODO: Actually send the response */
            }

            pthread_mutex_unlock(&g_ai_system->tasks_mutex);
            break;
        }

        case MSG_TYPE_TASK_RESPONSE: {
            /* Response to our task request */
            pthread_mutex_lock(&g_ai_system->tasks_mutex);

            for (int i = 0; i < g_ai_system->task_count; i++) {
                task_session_t *task = &g_ai_system->active_tasks[i];
                if (strcmp(task->session_id, msg->session_id) == 0) {
                    strncpy(task->result, msg->payload, sizeof(task->result) - 1);
                    task->completed = time(NULL);
                    strcpy(task->status, "completed");

                    if (g_ai_system->display) {
                        char result_msg[1024];
                        snprintf(result_msg, sizeof(result_msg),
                                "🤖 Distributed AI: %s\n", msg->payload);
                        anbs_ai_chat_write(g_ai_system->display, result_msg);
                        anbs_display_refresh_panel(g_ai_system->display, ANBS_PANEL_AI_CHAT);
                    }
                    break;
                }
            }

            pthread_mutex_unlock(&g_ai_system->tasks_mutex);
            break;
        }

        case MSG_TYPE_HEARTBEAT: {
            /* Agent health update */
            for (int i = 0; i < g_ai_system->agent_count; i++) {
                if (strcmp(g_ai_system->agents[i].agent_id, msg->sender_id) == 0) {
                    g_ai_system->agents[i].last_seen = time(NULL);
                    g_ai_system->agents[i].status = AGENT_STATUS_ONLINE;

                    /* Parse heartbeat data */
                    /* TODO: Parse CPU, memory, task queue info from payload */
                    break;
                }
            }
            break;
        }

        default:
            ANBS_DEBUG_LOG("Unknown message type %d from %s", msg->type, msg->sender_id);
            break;
    }

    pthread_mutex_unlock(&g_ai_system->agents_mutex);
}

/* Discovery thread */
static void *discovery_thread(void *arg) {
    int sock;
    struct sockaddr_in addr, sender_addr;
    socklen_t sender_len;
    char buffer[MAX_MESSAGE_SIZE];

    /* Create UDP socket for discovery */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        ANBS_DEBUG_LOG("Failed to create discovery socket");
        return NULL;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(DISCOVERY_PORT);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ANBS_DEBUG_LOG("Failed to bind discovery socket");
        close(sock);
        return NULL;
    }

    ANBS_DEBUG_LOG("Discovery thread started on port %d", DISCOVERY_PORT);

    while (g_ai_system && g_ai_system->running) {
        sender_len = sizeof(sender_addr);
        int bytes = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                           (struct sockaddr*)&sender_addr, &sender_len);

        if (bytes > 0) {
            buffer[bytes] = '\0';

            ai_message_t msg;
            if (parse_message(buffer, &msg) == 0) {
                /* Don't handle our own discovery messages */
                if (strcmp(msg.sender_id, g_ai_system->local_agent_id) != 0) {
                    handle_message(&msg);
                }
            }
        }

        usleep(100000); /* 100ms delay */
    }

    close(sock);
    return NULL;
}

/* Coordination thread */
static void *coordination_thread(void *arg) {
    while (g_ai_system && g_ai_system->running) {
        /* Send periodic discovery broadcasts */
        ai_message_t discovery_msg;
        char payload[256];

        snprintf(payload, sizeof(payload),
                "capabilities=terminal,ai_commands,memory_search,file_analysis;"
                "status=online");

        create_message(MSG_TYPE_DISCOVERY, NULL, payload, &discovery_msg);

        /* Broadcast discovery */
        ai_agent_t broadcast_agent = {0};
        strcpy(broadcast_agent.ip_address, "255.255.255.255");
        broadcast_agent.port = DISCOVERY_PORT;

        send_message_to_agent(&broadcast_agent, &discovery_msg);

        /* Send heartbeats to known agents */
        pthread_mutex_lock(&g_ai_system->agents_mutex);

        for (int i = 0; i < g_ai_system->agent_count; i++) {
            ai_agent_t *agent = &g_ai_system->agents[i];

            if (agent->status == AGENT_STATUS_ONLINE) {
                time_t now = time(NULL);

                /* Check if agent is still alive */
                if (now - agent->last_seen > 30) {
                    agent->status = AGENT_STATUS_OFFLINE;
                    continue;
                }

                /* Send heartbeat */
                ai_message_t heartbeat;
                snprintf(payload, sizeof(payload),
                        "load=%.1f;memory=%.1f;tasks=%d",
                        agent->cpu_load, agent->memory_usage, agent->task_queue_size);

                create_message(MSG_TYPE_HEARTBEAT, agent->agent_id, payload, &heartbeat);
                send_message_to_agent(agent, &heartbeat);
            }
        }

        pthread_mutex_unlock(&g_ai_system->agents_mutex);

        /* Update health display */
        if (g_ai_system->display) {
            for (int i = 0; i < g_ai_system->agent_count; i++) {
                ai_agent_t *agent = &g_ai_system->agents[i];

                health_data_t health;
                memset(&health, 0, sizeof(health));

                strncpy(health.agent_id, agent->agent_id, sizeof(health.agent_id) - 1);
                health.online = (agent->status == AGENT_STATUS_ONLINE);
                health.latency_ms = 50; /* TODO: Measure actual latency */
                health.cpu_load = agent->cpu_load;
                health.memory_usage = agent->memory_usage;
                health.commands_processed = 0; /* TODO: Track commands */
                health.success_rate = 99.0; /* TODO: Calculate actual rate */
                health.last_update = agent->last_seen;

                anbs_health_update(g_ai_system->display, &health);
            }
        }

        sleep(10); /* 10 second coordination cycle */
    }

    return NULL;
}

/* Initialize distributed AI system */
int anbs_distributed_ai_init(anbs_display_t *display) {
    if (g_ai_system) {
        return 0; /* Already initialized */
    }

    g_ai_system = calloc(1, sizeof(distributed_ai_system_t));
    if (!g_ai_system) {
        return -1;
    }

    generate_agent_id(g_ai_system->local_agent_id, sizeof(g_ai_system->local_agent_id));
    g_ai_system->display = display;
    g_ai_system->running = 1;

    pthread_mutex_init(&g_ai_system->agents_mutex, NULL);
    pthread_mutex_init(&g_ai_system->tasks_mutex, NULL);

    /* Start discovery thread */
    if (pthread_create(&g_ai_system->discovery_thread, NULL, discovery_thread, NULL) != 0) {
        anbs_distributed_ai_cleanup();
        return -1;
    }

    /* Start coordination thread */
    if (pthread_create(&g_ai_system->coordination_thread, NULL, coordination_thread, NULL) != 0) {
        anbs_distributed_ai_cleanup();
        return -1;
    }

    ANBS_DEBUG_LOG("Distributed AI system initialized with agent ID: %s", g_ai_system->local_agent_id);

    if (display) {
        anbs_status_write(display, "Distributed AI system online - discovering agents...");
    }

    return 0;
}

/* Submit task to distributed AI network */
int anbs_distributed_ai_submit_task(const char *task_description, char **result) {
    if (!g_ai_system || !task_description) {
        return -1;
    }

    /* Find best available agent */
    pthread_mutex_lock(&g_ai_system->agents_mutex);

    ai_agent_t *best_agent = NULL;
    for (int i = 0; i < g_ai_system->agent_count; i++) {
        ai_agent_t *agent = &g_ai_system->agents[i];
        if (agent->status == AGENT_STATUS_ONLINE && agent->task_queue_size < 5) {
            if (!best_agent || agent->task_queue_size < best_agent->task_queue_size) {
                best_agent = agent;
            }
        }
    }

    if (!best_agent) {
        pthread_mutex_unlock(&g_ai_system->agents_mutex);
        *result = strdup("No available AI agents in distributed network");
        return -1;
    }

    /* Create task session */
    pthread_mutex_lock(&g_ai_system->tasks_mutex);

    if (g_ai_system->task_count >= 100) {
        pthread_mutex_unlock(&g_ai_system->tasks_mutex);
        pthread_mutex_unlock(&g_ai_system->agents_mutex);
        *result = strdup("Task queue full");
        return -1;
    }

    task_session_t *task = &g_ai_system->active_tasks[g_ai_system->task_count++];
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse(uuid, task->session_id);

    strncpy(task->task_description, task_description, sizeof(task->task_description) - 1);
    strncpy(task->assigned_agent, best_agent->agent_id, sizeof(task->assigned_agent) - 1);
    task->created = time(NULL);
    task->priority = 5;
    strcpy(task->status, "submitted");

    /* Send task request */
    ai_message_t task_msg;
    create_message(MSG_TYPE_TASK_REQUEST, best_agent->agent_id, task_description, &task_msg);
    strncpy(task_msg.session_id, task->session_id, sizeof(task_msg.session_id) - 1);

    send_message_to_agent(best_agent, &task_msg);

    best_agent->task_queue_size++;

    pthread_mutex_unlock(&g_ai_system->tasks_mutex);
    pthread_mutex_unlock(&g_ai_system->agents_mutex);

    /* Wait for result (simplified - in practice would use callbacks) */
    int timeout = 30; /* 30 seconds */
    while (timeout > 0) {
        pthread_mutex_lock(&g_ai_system->tasks_mutex);

        if (strcmp(task->status, "completed") == 0) {
            *result = strdup(task->result);
            pthread_mutex_unlock(&g_ai_system->tasks_mutex);
            return 0;
        }

        pthread_mutex_unlock(&g_ai_system->tasks_mutex);

        sleep(1);
        timeout--;
    }

    *result = strdup("Task timeout - no response from distributed AI network");
    return -1;
}

/* Get distributed AI network status */
int anbs_distributed_ai_get_status(char **status_report) {
    if (!g_ai_system) {
        return -1;
    }

    char report[4096];
    int offset = 0;

    offset += snprintf(report + offset, sizeof(report) - offset,
                      "Distributed AI Network Status\n");
    offset += snprintf(report + offset, sizeof(report) - offset,
                      "Local Agent ID: %s\n", g_ai_system->local_agent_id);
    offset += snprintf(report + offset, sizeof(report) - offset,
                      "Connected Agents: %d\n\n", g_ai_system->agent_count);

    pthread_mutex_lock(&g_ai_system->agents_mutex);

    for (int i = 0; i < g_ai_system->agent_count; i++) {
        ai_agent_t *agent = &g_ai_system->agents[i];
        const char *status_str = "";

        switch (agent->status) {
            case AGENT_STATUS_OFFLINE: status_str = "OFFLINE"; break;
            case AGENT_STATUS_DISCOVERING: status_str = "DISCOVERING"; break;
            case AGENT_STATUS_CONNECTING: status_str = "CONNECTING"; break;
            case AGENT_STATUS_ONLINE: status_str = "ONLINE"; break;
            case AGENT_STATUS_BUSY: status_str = "BUSY"; break;
            case AGENT_STATUS_ERROR: status_str = "ERROR"; break;
        }

        offset += snprintf(report + offset, sizeof(report) - offset,
                          "Agent: %s\n  Status: %s\n  Last Seen: %ld seconds ago\n  Load: %.1f%%\n\n",
                          agent->agent_id, status_str,
                          time(NULL) - agent->last_seen, agent->cpu_load);
    }

    pthread_mutex_unlock(&g_ai_system->agents_mutex);

    pthread_mutex_lock(&g_ai_system->tasks_mutex);

    offset += snprintf(report + offset, sizeof(report) - offset,
                      "Active Tasks: %d\n", g_ai_system->task_count);

    for (int i = 0; i < g_ai_system->task_count && i < 10; i++) {
        task_session_t *task = &g_ai_system->active_tasks[i];
        offset += snprintf(report + offset, sizeof(report) - offset,
                          "  Task %d: %s (%s)\n", i + 1, task->task_description, task->status);
    }

    pthread_mutex_unlock(&g_ai_system->tasks_mutex);

    *status_report = strdup(report);
    return 0;
}

/* Cleanup distributed AI system */
void anbs_distributed_ai_cleanup(void) {
    if (!g_ai_system) {
        return;
    }

    g_ai_system->running = 0;

    /* Send shutdown messages to agents */
    pthread_mutex_lock(&g_ai_system->agents_mutex);

    for (int i = 0; i < g_ai_system->agent_count; i++) {
        ai_agent_t *agent = &g_ai_system->agents[i];
        if (agent->status == AGENT_STATUS_ONLINE) {
            ai_message_t shutdown_msg;
            create_message(MSG_TYPE_SHUTDOWN, agent->agent_id, "System shutting down", &shutdown_msg);
            send_message_to_agent(agent, &shutdown_msg);
        }
    }

    pthread_mutex_unlock(&g_ai_system->agents_mutex);

    /* Wait for threads to finish */
    if (g_ai_system->discovery_thread) {
        pthread_join(g_ai_system->discovery_thread, NULL);
    }

    if (g_ai_system->coordination_thread) {
        pthread_join(g_ai_system->coordination_thread, NULL);
    }

    pthread_mutex_destroy(&g_ai_system->agents_mutex);
    pthread_mutex_destroy(&g_ai_system->tasks_mutex);

    free(g_ai_system);
    g_ai_system = NULL;

    ANBS_DEBUG_LOG("Distributed AI system cleaned up");
}