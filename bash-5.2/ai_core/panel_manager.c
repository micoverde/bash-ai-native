/* panel_manager.c - Panel management utilities for ANBS display system */

#include "ai_display.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/**
 * Initialize a panel
 */
int anbs_panel_init(panel_t *panel, int width, int height, int start_x, int start_y)
{
    if (!panel || width <= 0 || height <= 0) {
        return -1;
    }

    /* Set panel dimensions and position */
    panel->width = width;
    panel->height = height;
    panel->start_x = start_x;
    panel->start_y = start_y;
    panel->visible = true;
    panel->has_border = false;
    panel->color_pair = ANBS_COLOR_TERMINAL;
    panel->last_refresh = 0;

    /* Window will be created by setup_panels */
    panel->window = NULL;

    return 0;
}

/**
 * Resize a panel
 */
int anbs_panel_resize(panel_t *panel, int width, int height, int start_x, int start_y)
{
    if (!panel || width <= 0 || height <= 0) {
        return -1;
    }

    /* Update panel dimensions */
    panel->width = width;
    panel->height = height;
    panel->start_x = start_x;
    panel->start_y = start_y;

    /* If window exists, it will be recreated by the resize handler */
    return 0;
}

/**
 * Write text to a panel with word wrapping
 */
int anbs_panel_write_text(panel_t *panel, const char *text)
{
    if (!panel || !text || !panel->window) {
        return -1;
    }

    /* Get current cursor position */
    int cur_y, cur_x;
    getyx(panel->window, cur_y, cur_x);

    /* Get panel dimensions (account for borders) */
    int max_width = panel->width - (panel->has_border ? 2 : 0);
    int max_height = panel->height - (panel->has_border ? 2 : 0);

    /* Process text character by character for word wrapping */
    const char *p = text;
    char line_buffer[512];
    int line_pos = 0;

    while (*p) {
        /* Handle newlines */
        if (*p == '\n') {
            line_buffer[line_pos] = '\0';
            if (line_pos > 0) {
                wprintw(panel->window, "%s", line_buffer);
                line_pos = 0;
            }
            wprintw(panel->window, "\n");
            p++;
            continue;
        }

        /* Add character to line buffer */
        if (line_pos < sizeof(line_buffer) - 1) {
            line_buffer[line_pos++] = *p;
        }

        /* Check for word boundary and line length */
        if (line_pos >= max_width || isspace(*p)) {
            line_buffer[line_pos] = '\0';

            /* Find last space for word wrapping */
            if (line_pos >= max_width && !isspace(*p)) {
                int last_space = line_pos - 1;
                while (last_space > 0 && !isspace(line_buffer[last_space])) {
                    last_space--;
                }

                if (last_space > 0) {
                    /* Break at last space */
                    line_buffer[last_space] = '\0';
                    wprintw(panel->window, "%s\n", line_buffer);

                    /* Continue with remaining characters */
                    memmove(line_buffer, &line_buffer[last_space + 1],
                           line_pos - last_space);
                    line_pos = line_pos - last_space - 1;
                } else {
                    /* No space found, force break */
                    wprintw(panel->window, "%s\n", line_buffer);
                    line_pos = 0;
                }
            } else {
                /* Normal line end */
                wprintw(panel->window, "%s", line_buffer);
                if (isspace(*p) && *p != ' ') {
                    wprintw(panel->window, "\n");
                }
                line_pos = 0;
            }
        }

        p++;
    }

    /* Write any remaining text */
    if (line_pos > 0) {
        line_buffer[line_pos] = '\0';
        wprintw(panel->window, "%s", line_buffer);
    }

    return 0;
}

/**
 * Draw border around panel with optional title
 */
int anbs_panel_draw_border(panel_t *panel, const char *title)
{
    if (!panel || !panel->window) {
        return -1;
    }

    if (!panel->has_border) {
        return 0;
    }

    /* Draw border box */
    box(panel->window, 0, 0);

    /* Add title if provided */
    if (title && strlen(title) > 0) {
        int title_len = strlen(title);
        int title_x = (panel->width - title_len - 2) / 2;
        if (title_x < 1) title_x = 1;

        /* Clear title area and write title */
        mvwprintw(panel->window, 0, title_x, " %s ", title);
    }

    return 0;
}

/**
 * Scroll panel content up
 */
int anbs_panel_scroll_up(panel_t *panel, int lines)
{
    if (!panel || !panel->window || lines <= 0) {
        return -1;
    }

    /* Use NCurses scrolling */
    wscrl(panel->window, lines);

    return 0;
}

/**
 * Scroll panel content down
 */
int anbs_panel_scroll_down(panel_t *panel, int lines)
{
    if (!panel || !panel->window || lines <= 0) {
        return -1;
    }

    /* Use NCurses scrolling (negative value) */
    wscrl(panel->window, -lines);

    return 0;
}

/**
 * Clear panel content
 */
int anbs_panel_clear(panel_t *panel)
{
    if (!panel || !panel->window) {
        return -1;
    }

    /* Clear the window */
    werase(panel->window);

    /* Redraw border if enabled */
    if (panel->has_border) {
        anbs_panel_draw_border(panel, NULL);
    }

    return 0;
}

/**
 * Get panel content area dimensions (excluding borders)
 */
int anbs_panel_get_content_area(panel_t *panel, int *width, int *height)
{
    if (!panel) {
        return -1;
    }

    if (width) {
        *width = panel->width - (panel->has_border ? 2 : 0);
    }

    if (height) {
        *height = panel->height - (panel->has_border ? 2 : 0);
    }

    return 0;
}

/**
 * Set panel cursor position (relative to content area)
 */
int anbs_panel_set_cursor(panel_t *panel, int x, int y)
{
    if (!panel || !panel->window) {
        return -1;
    }

    int offset_x = panel->has_border ? 1 : 0;
    int offset_y = panel->has_border ? 1 : 0;

    wmove(panel->window, y + offset_y, x + offset_x);

    return 0;
}

/**
 * Get panel cursor position (relative to content area)
 */
int anbs_panel_get_cursor(panel_t *panel, int *x, int *y)
{
    if (!panel || !panel->window) {
        return -1;
    }

    int cur_y, cur_x;
    getyx(panel->window, cur_y, cur_x);

    int offset_x = panel->has_border ? 1 : 0;
    int offset_y = panel->has_border ? 1 : 0;

    if (x) {
        *x = cur_x - offset_x;
    }

    if (y) {
        *y = cur_y - offset_y;
    }

    return 0;
}

/**
 * Cleanup panel resources
 */
void anbs_panel_cleanup(panel_t *panel)
{
    if (!panel) {
        return;
    }

    /* Delete NCurses window */
    if (panel->window) {
        delwin(panel->window);
        panel->window = NULL;
    }

    /* Clear panel data */
    memset(panel, 0, sizeof(panel_t));
}

/**
 * Check if point is within panel bounds
 */
bool anbs_panel_contains_point(panel_t *panel, int x, int y)
{
    if (!panel) {
        return false;
    }

    return (x >= panel->start_x && x < panel->start_x + panel->width &&
            y >= panel->start_y && y < panel->start_y + panel->height);
}

/**
 * Highlight panel (for focus indication)
 */
int anbs_panel_highlight(panel_t *panel, bool highlight)
{
    if (!panel || !panel->window) {
        return -1;
    }

    if (highlight) {
        /* Add highlight attributes */
        wattron(panel->window, A_BOLD);
        if (panel->has_border) {
            anbs_panel_draw_border(panel, "ACTIVE");
        }
    } else {
        /* Remove highlight attributes */
        wattroff(panel->window, A_BOLD);
        if (panel->has_border) {
            anbs_panel_draw_border(panel, NULL);
        }
    }

    wrefresh(panel->window);
    return 0;
}

/**
 * Print formatted text to panel
 */
int anbs_panel_printf(panel_t *panel, const char *format, ...)
{
    char buffer[2048];
    va_list args;

    if (!panel || !format) {
        return -1;
    }

    /* Format the string */
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    /* Write to panel */
    return anbs_panel_write_text(panel, buffer);
}

/**
 * Add colored text to panel
 */
int anbs_panel_write_colored(panel_t *panel, const char *text, int color_pair)
{
    if (!panel || !text || !panel->window) {
        return -1;
    }

    /* Apply color */
    if (g_anbs_display && g_anbs_display->color_supported) {
        wattron(panel->window, COLOR_PAIR(color_pair));
    }

    /* Write text */
    int result = anbs_panel_write_text(panel, text);

    /* Remove color */
    if (g_anbs_display && g_anbs_display->color_supported) {
        wattroff(panel->window, COLOR_PAIR(color_pair));
    }

    return result;
}

/**
 * Refresh panel if dirty
 */
int anbs_panel_refresh_if_dirty(panel_t *panel)
{
    if (!panel || !panel->window || !panel->visible) {
        return 0;
    }

    /* Check if buffer is dirty */
    bool is_dirty = false;
    if (panel->buffer) {
        anbs_text_buffer_get_stats(panel->buffer, NULL, NULL, &is_dirty);
    }

    if (is_dirty) {
        wrefresh(panel->window);
        if (panel->buffer) {
            anbs_text_buffer_mark_clean(panel->buffer);
        }
        panel->last_refresh = time(NULL);
    }

    return 0;
}