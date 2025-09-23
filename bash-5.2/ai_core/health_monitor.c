/* health_monitor.c - Health monitoring for ANBS AI agents */

#include "ai_display.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Update health data for an AI agent
 */
int anbs_health_update(anbs_display_t *display, const health_data_t *data)
{
    int i, slot = -1;

    if (!display || !data) {
        return -1;
    }

    /* Find existing agent or empty slot */
    for (i = 0; i < 10; i++) {
        if (strcmp(display->health_data[i].agent_id, data->agent_id) == 0) {
            /* Found existing agent */
            slot = i;
            break;
        } else if (strlen(display->health_data[i].agent_id) == 0 && slot == -1) {
            /* Found empty slot */
            slot = i;
        }
    }

    if (slot == -1) {
        /* No slot available */
        ANBS_DEBUG_LOG("No slot available for agent %s", data->agent_id);
        return -1;
    }

    /* Update health data */
    memcpy(&display->health_data[slot], data, sizeof(health_data_t));
    display->health_data[slot].last_update = time(NULL);

    /* Update agent count */
    if (slot >= display->health_agent_count) {
        display->health_agent_count = slot + 1;
    }

    /* Refresh health panel */
    anbs_health_refresh_display(display);

    return 0;
}

/**
 * Refresh the health monitoring display
 */
int anbs_health_refresh_display(anbs_display_t *display)
{
    panel_t *panel;
    int i, line = 0;
    char status_line[256];
    time_t now;

    if (!display) {
        return -1;
    }

    panel = &display->panels[ANBS_PANEL_HEALTH];
    if (!panel->window || !panel->visible) {
        return 0;
    }

    now = time(NULL);

    /* Clear panel */
    anbs_panel_clear(panel);

    /* Draw border with title */
    if (panel->has_border) {
        anbs_panel_draw_border(panel, "Vertex Health");
        anbs_panel_set_cursor(panel, 0, 0);
    }

    /* Display health data for each agent */
    for (i = 0; i < display->health_agent_count; i++) {
        health_data_t *health = &display->health_data[i];

        if (strlen(health->agent_id) == 0) {
            continue;
        }

        /* Format status line */
        const char *status_icon = anbs_health_get_status_icon(health, now);
        const char *status_text = anbs_health_get_status_text(health, now);

        snprintf(status_line, sizeof(status_line),
                "%s %-12s %s %3dms Load:%2.0f%%",
                status_icon,
                health->agent_id,
                status_text,
                health->latency_ms,
                health->cpu_load);

        /* Write status line with appropriate color */
        int color = anbs_health_get_status_color(health, now);
        anbs_panel_set_cursor(panel, 0, line);
        anbs_panel_write_colored(panel, status_line, color);

        line++;

        /* Add detailed info if space allows */
        if (line < panel->height - (panel->has_border ? 3 : 1)) {
            snprintf(status_line, sizeof(status_line),
                    "  Mem:%3.0f%% Cmds:%d Success:%3.1f%%",
                    health->memory_usage,
                    health->commands_processed,
                    health->success_rate);

            anbs_panel_set_cursor(panel, 0, line);
            anbs_panel_write_text(panel, status_line);
            line++;
        }

        /* Add spacing between agents */
        line++;
    }

    /* Add summary information */
    if (line < panel->height - (panel->has_border ? 4 : 2)) {
        line++; /* Add space */

        /* Overall statistics */
        int online_count = 0;
        int total_commands = 0;
        float avg_success_rate = 0.0;

        for (i = 0; i < display->health_agent_count; i++) {
            health_data_t *health = &display->health_data[i];
            if (strlen(health->agent_id) > 0 && health->online) {
                online_count++;
                total_commands += health->commands_processed;
                avg_success_rate += health->success_rate;
            }
        }

        if (online_count > 0) {
            avg_success_rate /= online_count;
        }

        snprintf(status_line, sizeof(status_line),
                "📊 Summary: %d/%d online",
                online_count, display->health_agent_count);
        anbs_panel_set_cursor(panel, 0, line++);
        anbs_panel_write_colored(panel, status_line, ANBS_COLOR_STATUS);

        snprintf(status_line, sizeof(status_line),
                "Commands: %d Success: %.1f%%",
                total_commands, avg_success_rate);
        anbs_panel_set_cursor(panel, 0, line++);
        anbs_panel_write_text(panel, status_line);

        /* Last update timestamp */
        snprintf(status_line, sizeof(status_line),
                "🔄 Last update: %s", anbs_format_timestamp(now));
        anbs_panel_set_cursor(panel, 0, line++);
        anbs_panel_write_text(panel, status_line);
    }

    /* Refresh panel */
    wrefresh(panel->window);

    return 0;
}

/**
 * Get status icon based on agent health
 */
const char *anbs_health_get_status_icon(const health_data_t *health, time_t now)
{
    if (!health) {
        return "❓";
    }

    /* Check if agent is offline (no update for 30 seconds) */
    if (!health->online || (now - health->last_update) > 30) {
        return "🔴";
    }

    /* Check latency and load */
    if (health->latency_ms > 500 || health->cpu_load > 90.0) {
        return "🟡"; /* Warning */
    }

    if (health->success_rate < 95.0) {
        return "🟠"; /* Degraded */
    }

    return "🟢"; /* Good */
}

/**
 * Get status text based on agent health
 */
const char *anbs_health_get_status_text(const health_data_t *health, time_t now)
{
    if (!health) {
        return "Unknown";
    }

    if (!health->online || (now - health->last_update) > 30) {
        return "Offline";
    }

    if (health->latency_ms > 500) {
        return "Slow";
    }

    if (health->cpu_load > 90.0) {
        return "Overloaded";
    }

    if (health->success_rate < 95.0) {
        return "Degraded";
    }

    return "Online";
}

/**
 * Get status color based on agent health
 */
int anbs_health_get_status_color(const health_data_t *health, time_t now)
{
    if (!health) {
        return ANBS_COLOR_ERROR;
    }

    if (!health->online || (now - health->last_update) > 30) {
        return ANBS_COLOR_ERROR;
    }

    if (health->latency_ms > 500 || health->cpu_load > 90.0 || health->success_rate < 95.0) {
        return ANBS_COLOR_STATUS; /* Yellow for warnings */
    }

    return ANBS_COLOR_AI_HEALTH; /* Green for good */
}

/**
 * Remove an agent from health monitoring
 */
int anbs_health_remove_agent(anbs_display_t *display, const char *agent_id)
{
    int i;

    if (!display || !agent_id) {
        return -1;
    }

    /* Find and clear the agent */
    for (i = 0; i < display->health_agent_count; i++) {
        if (strcmp(display->health_data[i].agent_id, agent_id) == 0) {
            memset(&display->health_data[i], 0, sizeof(health_data_t));

            /* Compact the array if this was the last agent */
            if (i == display->health_agent_count - 1) {
                display->health_agent_count--;
            }

            /* Refresh display */
            anbs_health_refresh_display(display);
            return 0;
        }
    }

    return -1; /* Agent not found */
}

/**
 * Clear all health data
 */
void anbs_health_clear_all(anbs_display_t *display)
{
    int i;

    if (!display) {
        return;
    }

    for (i = 0; i < 10; i++) {
        memset(&display->health_data[i], 0, sizeof(health_data_t));
    }

    display->health_agent_count = 0;

    /* Refresh display */
    anbs_health_refresh_display(display);
}

/**
 * Get health summary
 */
int anbs_health_get_summary(anbs_display_t *display, int *online_count,
                           int *total_count, float *avg_latency, float *avg_success_rate)
{
    int i, online = 0, total = 0;
    float total_latency = 0.0, total_success = 0.0;
    time_t now = time(NULL);

    if (!display) {
        return -1;
    }

    for (i = 0; i < display->health_agent_count; i++) {
        health_data_t *health = &display->health_data[i];

        if (strlen(health->agent_id) == 0) {
            continue;
        }

        total++;

        if (health->online && (now - health->last_update) <= 30) {
            online++;
            total_latency += health->latency_ms;
            total_success += health->success_rate;
        }
    }

    if (online_count) *online_count = online;
    if (total_count) *total_count = total;
    if (avg_latency) *avg_latency = online > 0 ? total_latency / online : 0.0;
    if (avg_success_rate) *avg_success_rate = online > 0 ? total_success / online : 0.0;

    return 0;
}

/**
 * Create sample health data for testing
 */
health_data_t anbs_health_create_sample(const char *agent_id, bool online,
                                       int latency, float cpu_load, float memory_usage,
                                       int commands, float success_rate)
{
    health_data_t health;

    memset(&health, 0, sizeof(health_data_t));

    strncpy(health.agent_id, agent_id, sizeof(health.agent_id) - 1);
    health.online = online;
    health.latency_ms = latency;
    health.cpu_load = cpu_load;
    health.memory_usage = memory_usage;
    health.commands_processed = commands;
    health.success_rate = success_rate;
    health.last_update = time(NULL);

    return health;
}