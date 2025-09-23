/* metrics.c - Real-time performance monitoring for ANBS */

#include "../ai_display.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

#define MAX_METRICS 1000
#define MAX_COMMAND_TYPES 50
#define METRIC_HISTORY_SIZE 100

typedef enum {
    METRIC_RESPONSE_TIME = 1,
    METRIC_CACHE_HIT_RATE,
    METRIC_MEMORY_USAGE,
    METRIC_CPU_USAGE,
    METRIC_NETWORK_LATENCY,
    METRIC_ERROR_RATE,
    METRIC_THROUGHPUT,
    METRIC_QUEUE_DEPTH
} metric_type_t;

typedef struct {
    double value;
    time_t timestamp;
    char context[128];
} metric_sample_t;

typedef struct {
    char command_type[64];
    metric_sample_t samples[METRIC_HISTORY_SIZE];
    int sample_count;
    int sample_index;
    double min_value;
    double max_value;
    double avg_value;
    double p95_value;
    double p99_value;
    uint64_t total_samples;
} command_metrics_t;

typedef struct {
    metric_type_t type;
    char name[64];
    char description[256];
    command_metrics_t command_metrics[MAX_COMMAND_TYPES];
    int command_count;
    metric_sample_t global_samples[METRIC_HISTORY_SIZE];
    int global_sample_count;
    int global_sample_index;
    double target_value;
    double alert_threshold;
    bool alert_active;
} performance_metric_t;

typedef struct {
    performance_metric_t metrics[MAX_METRICS];
    int metric_count;
    pthread_mutex_t mutex;
    time_t start_time;
    uint64_t total_commands;
    uint64_t failed_commands;
    double total_response_time;
    bool monitoring_enabled;
} metrics_system_t;

static metrics_system_t *g_metrics = NULL;

/* Initialize metrics system */
int anbs_metrics_init(void) {
    if (g_metrics) {
        return 0; /* Already initialized */
    }

    g_metrics = calloc(1, sizeof(metrics_system_t));
    if (!g_metrics) {
        return -1;
    }

    pthread_mutex_init(&g_metrics->mutex, NULL);
    g_metrics->start_time = time(NULL);
    g_metrics->monitoring_enabled = true;

    /* Initialize default metrics */
    anbs_metrics_create_default_metrics();

    ANBS_DEBUG_LOG("Performance metrics system initialized");
    return 0;
}

/* Create default performance metrics */
int anbs_metrics_create_default_metrics(void) {
    if (!g_metrics) {
        return -1;
    }

    pthread_mutex_lock(&g_metrics->mutex);

    /* Response time metric */
    performance_metric_t *response_time = &g_metrics->metrics[g_metrics->metric_count++];
    response_time->type = METRIC_RESPONSE_TIME;
    strcpy(response_time->name, "response_time_ms");
    strcpy(response_time->description, "AI command response time in milliseconds");
    response_time->target_value = 50.0;  /* Target: <50ms */
    response_time->alert_threshold = 100.0;  /* Alert: >100ms */

    /* Cache hit rate metric */
    performance_metric_t *cache_hit = &g_metrics->metrics[g_metrics->metric_count++];
    cache_hit->type = METRIC_CACHE_HIT_RATE;
    strcpy(cache_hit->name, "cache_hit_rate");
    strcpy(cache_hit->description, "Response cache hit rate percentage");
    cache_hit->target_value = 80.0;  /* Target: >80% */
    cache_hit->alert_threshold = 50.0;  /* Alert: <50% */

    /* Memory usage metric */
    performance_metric_t *memory = &g_metrics->metrics[g_metrics->metric_count++];
    memory->type = METRIC_MEMORY_USAGE;
    strcpy(memory->name, "memory_usage_mb");
    strcpy(memory->description, "Memory usage in megabytes");
    memory->target_value = 512.0;  /* Target: <512MB */
    memory->alert_threshold = 1024.0;  /* Alert: >1GB */

    /* CPU usage metric */
    performance_metric_t *cpu = &g_metrics->metrics[g_metrics->metric_count++];
    cpu->type = METRIC_CPU_USAGE;
    strcpy(cpu->name, "cpu_usage_percent");
    strcpy(cpu->description, "CPU usage percentage");
    cpu->target_value = 50.0;  /* Target: <50% */
    cpu->alert_threshold = 80.0;  /* Alert: >80% */

    /* Error rate metric */
    performance_metric_t *error_rate = &g_metrics->metrics[g_metrics->metric_count++];
    error_rate->type = METRIC_ERROR_RATE;
    strcpy(error_rate->name, "error_rate_percent");
    strcpy(error_rate->description, "Command error rate percentage");
    error_rate->target_value = 1.0;  /* Target: <1% */
    error_rate->alert_threshold = 5.0;  /* Alert: >5% */

    /* Throughput metric */
    performance_metric_t *throughput = &g_metrics->metrics[g_metrics->metric_count++];
    throughput->type = METRIC_THROUGHPUT;
    strcpy(throughput->name, "throughput_cmd_per_sec");
    strcpy(throughput->description, "Commands processed per second");
    throughput->target_value = 10.0;  /* Target: >10 cmd/s */
    throughput->alert_threshold = 2.0;  /* Alert: <2 cmd/s */

    pthread_mutex_unlock(&g_metrics->mutex);

    ANBS_DEBUG_LOG("Created %d default metrics", g_metrics->metric_count);
    return 0;
}

/* Record a metric sample */
int anbs_metrics_record(metric_type_t type, const char *command_type, double value, const char *context) {
    if (!g_metrics || !g_metrics->monitoring_enabled) {
        return -1;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);

    pthread_mutex_lock(&g_metrics->mutex);

    /* Find metric */
    performance_metric_t *metric = NULL;
    for (int i = 0; i < g_metrics->metric_count; i++) {
        if (g_metrics->metrics[i].type == type) {
            metric = &g_metrics->metrics[i];
            break;
        }
    }

    if (!metric) {
        pthread_mutex_unlock(&g_metrics->mutex);
        return -1;
    }

    /* Record global sample */
    metric_sample_t *global_sample = &metric->global_samples[metric->global_sample_index];
    global_sample->value = value;
    global_sample->timestamp = tv.tv_sec;
    if (context) {
        strncpy(global_sample->context, context, sizeof(global_sample->context) - 1);
    } else {
        global_sample->context[0] = '\0';
    }

    metric->global_sample_index = (metric->global_sample_index + 1) % METRIC_HISTORY_SIZE;
    if (metric->global_sample_count < METRIC_HISTORY_SIZE) {
        metric->global_sample_count++;
    }

    /* Record command-specific sample if command_type provided */
    if (command_type) {
        command_metrics_t *cmd_metric = NULL;

        /* Find existing command metric */
        for (int i = 0; i < metric->command_count; i++) {
            if (strcmp(metric->command_metrics[i].command_type, command_type) == 0) {
                cmd_metric = &metric->command_metrics[i];
                break;
            }
        }

        /* Create new command metric if not found */
        if (!cmd_metric && metric->command_count < MAX_COMMAND_TYPES) {
            cmd_metric = &metric->command_metrics[metric->command_count++];
            strncpy(cmd_metric->command_type, command_type, sizeof(cmd_metric->command_type) - 1);
            cmd_metric->min_value = value;
            cmd_metric->max_value = value;
            cmd_metric->avg_value = value;
        }

        if (cmd_metric) {
            metric_sample_t *cmd_sample = &cmd_metric->samples[cmd_metric->sample_index];
            cmd_sample->value = value;
            cmd_sample->timestamp = tv.tv_sec;
            if (context) {
                strncpy(cmd_sample->context, context, sizeof(cmd_sample->context) - 1);
            } else {
                cmd_sample->context[0] = '\0';
            }

            cmd_metric->sample_index = (cmd_metric->sample_index + 1) % METRIC_HISTORY_SIZE;
            if (cmd_metric->sample_count < METRIC_HISTORY_SIZE) {
                cmd_metric->sample_count++;
            }

            /* Update statistics */
            cmd_metric->total_samples++;
            if (value < cmd_metric->min_value) {
                cmd_metric->min_value = value;
            }
            if (value > cmd_metric->max_value) {
                cmd_metric->max_value = value;
            }

            /* Recalculate average */
            double sum = 0.0;
            int count = cmd_metric->sample_count;
            for (int i = 0; i < count; i++) {
                sum += cmd_metric->samples[i].value;
            }
            cmd_metric->avg_value = sum / count;

            /* Calculate percentiles (simple sorting approach) */
            if (count >= 10) {
                double sorted_values[METRIC_HISTORY_SIZE];
                for (int i = 0; i < count; i++) {
                    sorted_values[i] = cmd_metric->samples[i].value;
                }

                /* Simple bubble sort */
                for (int i = 0; i < count - 1; i++) {
                    for (int j = 0; j < count - 1 - i; j++) {
                        if (sorted_values[j] > sorted_values[j + 1]) {
                            double temp = sorted_values[j];
                            sorted_values[j] = sorted_values[j + 1];
                            sorted_values[j + 1] = temp;
                        }
                    }
                }

                cmd_metric->p95_value = sorted_values[(int)(count * 0.95)];
                cmd_metric->p99_value = sorted_values[(int)(count * 0.99)];
            }
        }
    }

    /* Check alert threshold */
    bool should_alert = false;
    if ((type == METRIC_RESPONSE_TIME || type == METRIC_MEMORY_USAGE ||
         type == METRIC_CPU_USAGE || type == METRIC_ERROR_RATE) &&
        value > metric->alert_threshold) {
        should_alert = true;
    } else if ((type == METRIC_CACHE_HIT_RATE || type == METRIC_THROUGHPUT) &&
               value < metric->alert_threshold) {
        should_alert = true;
    }

    if (should_alert && !metric->alert_active) {
        metric->alert_active = true;
        ANBS_DEBUG_LOG("PERFORMANCE ALERT: %s = %.2f (threshold: %.2f)",
                       metric->name, value, metric->alert_threshold);
    } else if (!should_alert && metric->alert_active) {
        metric->alert_active = false;
        ANBS_DEBUG_LOG("Performance alert cleared: %s = %.2f", metric->name, value);
    }

    pthread_mutex_unlock(&g_metrics->mutex);
    return 0;
}

/* Get current system resource usage */
int anbs_metrics_collect_system_stats(void) {
    if (!g_metrics) {
        return -1;
    }

    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        /* Memory usage */
        double memory_mb = usage.ru_maxrss / 1024.0;  /* Convert KB to MB */
        anbs_metrics_record(METRIC_MEMORY_USAGE, "system", memory_mb, "rusage");

        /* Calculate CPU usage (simplified) */
        static time_t last_time = 0;
        static long last_cpu_time = 0;

        time_t current_time = time(NULL);
        long current_cpu_time = usage.ru_utime.tv_sec + usage.ru_stime.tv_sec;

        if (last_time > 0) {
            double time_diff = current_time - last_time;
            double cpu_diff = current_cpu_time - last_cpu_time;
            double cpu_percent = (cpu_diff / time_diff) * 100.0;

            anbs_metrics_record(METRIC_CPU_USAGE, "system", cpu_percent, "rusage");
        }

        last_time = current_time;
        last_cpu_time = current_cpu_time;
    }

    /* Calculate error rate */
    if (g_metrics->total_commands > 0) {
        double error_rate = (double)g_metrics->failed_commands / g_metrics->total_commands * 100.0;
        anbs_metrics_record(METRIC_ERROR_RATE, "system", error_rate, "calculated");
    }

    /* Calculate throughput */
    time_t uptime = time(NULL) - g_metrics->start_time;
    if (uptime > 0) {
        double throughput = (double)g_metrics->total_commands / uptime;
        anbs_metrics_record(METRIC_THROUGHPUT, "system", throughput, "calculated");
    }

    return 0;
}

/* Start performance measurement */
struct timeval *anbs_metrics_start_timer(void) {
    struct timeval *start_time = malloc(sizeof(struct timeval));
    if (start_time) {
        gettimeofday(start_time, NULL);
    }
    return start_time;
}

/* End performance measurement and record */
double anbs_metrics_end_timer(struct timeval *start_time, const char *command_type, const char *context) {
    if (!start_time) {
        return -1.0;
    }

    struct timeval end_time;
    gettimeofday(&end_time, NULL);

    double elapsed_ms = (end_time.tv_sec - start_time->tv_sec) * 1000.0 +
                       (end_time.tv_usec - start_time->tv_usec) / 1000.0;

    free(start_time);

    if (g_metrics) {
        pthread_mutex_lock(&g_metrics->mutex);
        g_metrics->total_commands++;
        g_metrics->total_response_time += elapsed_ms;
        pthread_mutex_unlock(&g_metrics->mutex);

        anbs_metrics_record(METRIC_RESPONSE_TIME, command_type, elapsed_ms, context);
    }

    return elapsed_ms;
}

/* Record command failure */
void anbs_metrics_record_failure(const char *command_type, const char *error_context) {
    if (!g_metrics) {
        return;
    }

    pthread_mutex_lock(&g_metrics->mutex);
    g_metrics->failed_commands++;
    pthread_mutex_unlock(&g_metrics->mutex);

    ANBS_DEBUG_LOG("Command failure recorded: %s (%s)", command_type, error_context);
}

/* Get performance dashboard data */
int anbs_metrics_get_dashboard(char **dashboard_json) {
    if (!g_metrics || !dashboard_json) {
        return -1;
    }

    pthread_mutex_lock(&g_metrics->mutex);

    /* Collect current system stats */
    anbs_metrics_collect_system_stats();

    char *dashboard = malloc(8192);
    if (!dashboard) {
        pthread_mutex_unlock(&g_metrics->mutex);
        return -1;
    }

    time_t uptime = time(NULL) - g_metrics->start_time;
    double avg_response_time = g_metrics->total_commands > 0 ?
                              g_metrics->total_response_time / g_metrics->total_commands : 0.0;

    int offset = snprintf(dashboard, 8192,
                         "{"
                         "\"uptime_seconds\": %ld,"
                         "\"total_commands\": %lu,"
                         "\"failed_commands\": %lu,"
                         "\"average_response_time_ms\": %.2f,"
                         "\"metrics\": [",
                         uptime, g_metrics->total_commands, g_metrics->failed_commands, avg_response_time);

    /* Add metrics */
    for (int i = 0; i < g_metrics->metric_count; i++) {
        performance_metric_t *metric = &g_metrics->metrics[i];

        double current_value = 0.0;
        if (metric->global_sample_count > 0) {
            int latest_index = (metric->global_sample_index - 1 + METRIC_HISTORY_SIZE) % METRIC_HISTORY_SIZE;
            current_value = metric->global_samples[latest_index].value;
        }

        offset += snprintf(dashboard + offset, 8192 - offset,
                          "%s{"
                          "\"name\": \"%s\","
                          "\"current_value\": %.2f,"
                          "\"target_value\": %.2f,"
                          "\"alert_threshold\": %.2f,"
                          "\"alert_active\": %s,"
                          "\"samples_count\": %d"
                          "}",
                          i > 0 ? "," : "",
                          metric->name,
                          current_value,
                          metric->target_value,
                          metric->alert_threshold,
                          metric->alert_active ? "true" : "false",
                          metric->global_sample_count);
    }

    offset += snprintf(dashboard + offset, 8192 - offset, "]}");

    *dashboard_json = dashboard;

    pthread_mutex_unlock(&g_metrics->mutex);
    return 0;
}

/* Get detailed metrics for specific command type */
int anbs_metrics_get_command_stats(const char *command_type, char **stats_json) {
    if (!g_metrics || !command_type || !stats_json) {
        return -1;
    }

    pthread_mutex_lock(&g_metrics->mutex);

    /* Find response time metric */
    performance_metric_t *response_metric = NULL;
    for (int i = 0; i < g_metrics->metric_count; i++) {
        if (g_metrics->metrics[i].type == METRIC_RESPONSE_TIME) {
            response_metric = &g_metrics->metrics[i];
            break;
        }
    }

    if (!response_metric) {
        pthread_mutex_unlock(&g_metrics->mutex);
        return -1;
    }

    /* Find command metrics */
    command_metrics_t *cmd_metrics = NULL;
    for (int i = 0; i < response_metric->command_count; i++) {
        if (strcmp(response_metric->command_metrics[i].command_type, command_type) == 0) {
            cmd_metrics = &response_metric->command_metrics[i];
            break;
        }
    }

    char *stats = malloc(1024);
    if (!stats) {
        pthread_mutex_unlock(&g_metrics->mutex);
        return -1;
    }

    if (cmd_metrics) {
        snprintf(stats, 1024,
                "{"
                "\"command_type\": \"%s\","
                "\"total_samples\": %lu,"
                "\"min_response_time_ms\": %.2f,"
                "\"max_response_time_ms\": %.2f,"
                "\"avg_response_time_ms\": %.2f,"
                "\"p95_response_time_ms\": %.2f,"
                "\"p99_response_time_ms\": %.2f"
                "}",
                cmd_metrics->command_type,
                cmd_metrics->total_samples,
                cmd_metrics->min_value,
                cmd_metrics->max_value,
                cmd_metrics->avg_value,
                cmd_metrics->p95_value,
                cmd_metrics->p99_value);
    } else {
        snprintf(stats, 1024,
                "{"
                "\"command_type\": \"%s\","
                "\"total_samples\": 0,"
                "\"error\": \"No data available\""
                "}",
                command_type);
    }

    *stats_json = stats;

    pthread_mutex_unlock(&g_metrics->mutex);
    return 0;
}

/* Enable/disable monitoring */
void anbs_metrics_set_enabled(bool enabled) {
    if (g_metrics) {
        pthread_mutex_lock(&g_metrics->mutex);
        g_metrics->monitoring_enabled = enabled;
        pthread_mutex_unlock(&g_metrics->mutex);

        ANBS_DEBUG_LOG("Performance monitoring %s", enabled ? "enabled" : "disabled");
    }
}

/* Reset all metrics */
void anbs_metrics_reset(void) {
    if (!g_metrics) {
        return;
    }

    pthread_mutex_lock(&g_metrics->mutex);

    /* Reset all metrics */
    for (int i = 0; i < g_metrics->metric_count; i++) {
        performance_metric_t *metric = &g_metrics->metrics[i];
        metric->global_sample_count = 0;
        metric->global_sample_index = 0;
        metric->alert_active = false;

        for (int j = 0; j < metric->command_count; j++) {
            command_metrics_t *cmd_metric = &metric->command_metrics[j];
            cmd_metric->sample_count = 0;
            cmd_metric->sample_index = 0;
            cmd_metric->total_samples = 0;
        }
    }

    g_metrics->start_time = time(NULL);
    g_metrics->total_commands = 0;
    g_metrics->failed_commands = 0;
    g_metrics->total_response_time = 0.0;

    pthread_mutex_unlock(&g_metrics->mutex);

    ANBS_DEBUG_LOG("Performance metrics reset");
}

/* Cleanup metrics system */
void anbs_metrics_cleanup(void) {
    if (!g_metrics) {
        return;
    }

    pthread_mutex_destroy(&g_metrics->mutex);
    free(g_metrics);
    g_metrics = NULL;

    ANBS_DEBUG_LOG("Performance metrics system cleaned up");
}