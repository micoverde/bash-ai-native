/* utility.c - Utility functions for ANBS display system */

#include "ai_display.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/**
 * Check if terminal supports color
 */
bool anbs_terminal_supports_color(void)
{
    /* Check TERM environment variable */
    const char *term = getenv("TERM");
    if (!term) {
        return false;
    }

    /* Known color-supporting terminals */
    const char *color_terms[] = {
        "xterm", "xterm-color", "xterm-256color",
        "screen", "screen-256color",
        "tmux", "tmux-256color",
        "linux", "rxvt", "konsole",
        "gnome-terminal", "iterm",
        NULL
    };

    for (int i = 0; color_terms[i]; i++) {
        if (strstr(term, color_terms[i])) {
            return true;
        }
    }

    return false;
}

/**
 * Check if terminal supports Unicode
 */
bool anbs_terminal_supports_unicode(void)
{
    const char *lang = getenv("LANG");
    const char *lc_all = getenv("LC_ALL");
    const char *lc_ctype = getenv("LC_CTYPE");

    /* Check for UTF-8 in locale variables */
    const char *locale = lc_all ? lc_all : (lc_ctype ? lc_ctype : lang);

    if (locale && (strstr(locale, "UTF-8") || strstr(locale, "utf8"))) {
        return true;
    }

    return false;
}

/**
 * Calculate panel dimensions based on current terminal size
 */
int anbs_calculate_panel_dimensions(anbs_display_t *display)
{
    if (!display) {
        return -1;
    }

    /* This function is implemented in ai_display.c as anbs_calculate_panel_positions */
    return anbs_calculate_panel_positions(display);
}

/**
 * Format timestamp for display
 */
const char *anbs_format_timestamp(time_t timestamp)
{
    static char buffer[64];
    struct tm *tm_info;

    tm_info = localtime(&timestamp);
    if (!tm_info) {
        strcpy(buffer, "Unknown");
        return buffer;
    }

    strftime(buffer, sizeof(buffer), "%H:%M:%S", tm_info);
    return buffer;
}

/**
 * Format health status for display
 */
const char *anbs_format_health_status(const health_data_t *data)
{
    static char buffer[256];

    if (!data || strlen(data->agent_id) == 0) {
        strcpy(buffer, "No data");
        return buffer;
    }

    time_t now = time(NULL);
    const char *status_icon = anbs_health_get_status_icon(data, now);
    const char *status_text = anbs_health_get_status_text(data, now);

    snprintf(buffer, sizeof(buffer),
            "%s %s: %s (%dms, %.1f%% CPU)",
            status_icon, data->agent_id, status_text,
            data->latency_ms, data->cpu_load);

    return buffer;
}

/**
 * Safe string copy with length limit
 */
char *anbs_safe_strncpy(char *dest, const char *src, size_t dest_size)
{
    if (!dest || !src || dest_size == 0) {
        return dest;
    }

    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
    return dest;
}

/**
 * Duplicate string with error checking
 */
char *anbs_safe_strdup(const char *src)
{
    if (!src) {
        return NULL;
    }

    size_t len = strlen(src) + 1;
    char *dst = malloc(len);
    if (!dst) {
        return NULL;
    }

    memcpy(dst, src, len);
    return dst;
}

/**
 * Format memory size for display
 */
const char *anbs_format_memory_size(size_t bytes)
{
    static char buffer[64];

    if (bytes < 1024) {
        snprintf(buffer, sizeof(buffer), "%zu B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buffer, sizeof(buffer), "%.1f KB", (double)bytes / 1024);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(buffer, sizeof(buffer), "%.1f MB", (double)bytes / (1024 * 1024));
    } else {
        snprintf(buffer, sizeof(buffer), "%.1f GB", (double)bytes / (1024 * 1024 * 1024));
    }

    return buffer;
}

/**
 * Format duration for display
 */
const char *anbs_format_duration(time_t seconds)
{
    static char buffer[64];

    if (seconds < 60) {
        snprintf(buffer, sizeof(buffer), "%lds", seconds);
    } else if (seconds < 3600) {
        snprintf(buffer, sizeof(buffer), "%ldm %lds", seconds / 60, seconds % 60);
    } else if (seconds < 86400) {
        long hours = seconds / 3600;
        long minutes = (seconds % 3600) / 60;
        snprintf(buffer, sizeof(buffer), "%ldh %ldm", hours, minutes);
    } else {
        long days = seconds / 86400;
        long hours = (seconds % 86400) / 3600;
        snprintf(buffer, sizeof(buffer), "%ldd %ldh", days, hours);
    }

    return buffer;
}

/**
 * Get available colors count
 */
int anbs_get_available_colors(void)
{
    if (!g_anbs_display || !g_anbs_display->color_supported) {
        return 0;
    }

    return COLORS;
}

/**
 * Get available color pairs count
 */
int anbs_get_available_color_pairs(void)
{
    if (!g_anbs_display || !g_anbs_display->color_supported) {
        return 0;
    }

    return COLOR_PAIRS;
}

/**
 * Toggle split-screen mode
 */
int anbs_display_toggle_split_mode(anbs_display_t *display)
{
    if (!display) {
        return -1;
    }

    display->split_mode_active = !display->split_mode_active;

    if (display->split_mode_active) {
        /* Show AI panels */
        display->panels[ANBS_PANEL_AI_CHAT].visible = true;
        display->panels[ANBS_PANEL_HEALTH].visible = true;
        anbs_status_write(display, "Split-screen mode enabled");
    } else {
        /* Hide AI panels */
        display->panels[ANBS_PANEL_AI_CHAT].visible = false;
        display->panels[ANBS_PANEL_HEALTH].visible = false;
        anbs_status_write(display, "Split-screen mode disabled");

        /* Resize terminal panel to full width */
        display->panels[ANBS_PANEL_TERMINAL].width = display->term_width;
    }

    /* Trigger layout recalculation */
    anbs_display_resize(display);

    return 0;
}

/**
 * Toggle panel borders
 */
int anbs_display_toggle_borders(anbs_display_t *display)
{
    if (!display) {
        return -1;
    }

    display->borders_enabled = !display->borders_enabled;

    /* Update all panels */
    for (int i = 0; i < ANBS_PANEL_COUNT; i++) {
        display->panels[i].has_border = display->borders_enabled;
    }

    /* Refresh display */
    anbs_display_refresh_all(display);

    anbs_status_write(display, display->borders_enabled ?
                     "Panel borders enabled" : "Panel borders disabled");

    return 0;
}

/**
 * Set AI command active state
 */
int anbs_set_ai_command_active(anbs_display_t *display, const char *command)
{
    if (!display || !command) {
        return -1;
    }

    display->ai_command_active = true;
    anbs_safe_strncpy(display->current_ai_command, command,
                     sizeof(display->current_ai_command));

    /* Update status */
    char status[512];
    snprintf(status, sizeof(status), "Processing AI command: %s", command);
    anbs_status_write(display, status);

    return 0;
}

/**
 * Clear AI command active state
 */
int anbs_clear_ai_command_active(anbs_display_t *display)
{
    if (!display) {
        return -1;
    }

    display->ai_command_active = false;
    display->current_ai_command[0] = '\0';

    anbs_status_write(display, "Ready");

    return 0;
}

/**
 * Get system information for display
 */
int anbs_get_system_info(char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0) {
        return -1;
    }

    /* Get basic system information */
    char hostname[256] = "unknown";
    char *user = getenv("USER");
    if (!user) user = "unknown";

    gethostname(hostname, sizeof(hostname) - 1);

    snprintf(buffer, buffer_size, "%s@%s", user, hostname);

    return 0;
}

/**
 * Validate panel configuration
 */
bool anbs_validate_panel_config(anbs_display_t *display)
{
    if (!display) {
        return false;
    }

    /* Check terminal size requirements */
    if (display->term_width < ANBS_MIN_TERMINAL_WIDTH ||
        display->term_height < ANBS_MIN_TERMINAL_HEIGHT) {
        return false;
    }

    /* Check panel ratios */
    if (display->terminal_ratio < 30 || display->terminal_ratio > 80) {
        return false;
    }

    if (display->ai_chat_ratio < 20 || display->ai_chat_ratio > 80) {
        return false;
    }

    return true;
}

/**
 * Log debug message with timestamp
 */
void anbs_debug_log(const char *format, ...)
{
#ifdef ANBS_DEBUG
    FILE *debug_file;
    va_list args;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    debug_file = fopen("/tmp/anbs_debug.log", "a");
    if (!debug_file) {
        return;
    }

    /* Write timestamp */
    fprintf(debug_file, "[%02d:%02d:%02d] ",
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

    /* Write formatted message */
    va_start(args, format);
    vfprintf(debug_file, format, args);
    va_end(args);

    fprintf(debug_file, "\n");
    fclose(debug_file);
#endif
}