/* ai_display.c - AI-Native Bash Shell Display System Implementation */

#include "ai_display.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

/* Global display instance */
anbs_display_t *g_anbs_display = NULL;

/* Forward declarations for internal functions */
static int anbs_init_ncurses(anbs_display_t *display);
static int anbs_setup_color_pairs(anbs_display_t *display);
static int anbs_calculate_panel_positions(anbs_display_t *display);
static void anbs_draw_panel_borders(anbs_display_t *display);
static int anbs_refresh_panel_content(anbs_display_t *display, anbs_panel_id_t panel_id);

/**
 * Initialize the ANBS display system
 */
int anbs_display_init(anbs_display_t **display)
{
    anbs_display_t *disp;
    int i;

    ANBS_DEBUG_LOG("Initializing ANBS display system");

    /* Allocate display structure */
    disp = (anbs_display_t *)calloc(1, sizeof(anbs_display_t));
    if (!disp) {
        return -1;
    }

    /* Set default configuration */
    disp->terminal_ratio = ANBS_DEFAULT_TERMINAL_RATIO;
    disp->ai_chat_ratio = ANBS_DEFAULT_AI_CHAT_RATIO;
    disp->split_mode_active = true;
    disp->panels_visible = true;
    disp->borders_enabled = true;
    disp->color_scheme = 0;
    disp->active_panel = ANBS_PANEL_TERMINAL;
    disp->ai_command_active = false;
    disp->health_agent_count = 0;

    /* Get initial terminal size */
    if (anbs_get_terminal_size(&disp->term_width, &disp->term_height) != 0) {
        free(disp);
        return -1;
    }

    /* Check minimum terminal size */
    if (disp->term_width < ANBS_MIN_TERMINAL_WIDTH ||
        disp->term_height < ANBS_MIN_TERMINAL_HEIGHT) {
        ANBS_DEBUG_LOG("Terminal too small: %dx%d (minimum: %dx%d)",
                       disp->term_width, disp->term_height,
                       ANBS_MIN_TERMINAL_WIDTH, ANBS_MIN_TERMINAL_HEIGHT);
        free(disp);
        return -1;
    }

    /* Initialize NCurses */
    if (anbs_init_ncurses(disp) != 0) {
        free(disp);
        return -1;
    }

    /* Initialize text buffers for all panels */
    for (i = 0; i < ANBS_PANEL_COUNT; i++) {
        if (anbs_text_buffer_init(&disp->panels[i].buffer, ANBS_MAX_TEXT_BUFFER_LINES) != 0) {
            /* Cleanup already initialized buffers */
            for (int j = 0; j < i; j++) {
                anbs_text_buffer_cleanup(disp->panels[j].buffer);
            }
            anbs_display_cleanup(disp);
            return -1;
        }
    }

    /* Setup panel layout */
    if (anbs_display_setup_panels(disp) != 0) {
        anbs_display_cleanup(disp);
        return -1;
    }

    /* Install signal handlers */
    if (anbs_install_signal_handlers() != 0) {
        anbs_display_cleanup(disp);
        return -1;
    }

    /* Initialize health monitoring */
    for (i = 0; i < 10; i++) {
        disp->health_data[i].online = false;
        strcpy(disp->health_data[i].agent_id, "");
    }

    /* Set global instance */
    g_anbs_display = disp;
    *display = disp;

    ANBS_DEBUG_LOG("ANBS display system initialized successfully");
    return 0;
}

/**
 * Initialize NCurses subsystem
 */
static int anbs_init_ncurses(anbs_display_t *display)
{
    ANBS_DEBUG_LOG("Initializing NCurses");

    /* Initialize NCurses */
    display->main_screen = initscr();
    if (!display->main_screen) {
        return -1;
    }

    display->ncurses_initialized = true;

    /* Configure NCurses settings */
    cbreak();           /* Disable line buffering */
    noecho();           /* Don't echo keys to screen */
    keypad(stdscr, TRUE); /* Enable function keys */
    nodelay(stdscr, TRUE); /* Non-blocking input */
    curs_set(1);        /* Show cursor */

    /* Check for color support */
    if (has_colors()) {
        start_color();
        display->color_supported = true;
        if (anbs_setup_color_pairs(display) != 0) {
            ANBS_DEBUG_LOG("Failed to setup color pairs");
            return -1;
        }
    } else {
        display->color_supported = false;
        ANBS_DEBUG_LOG("Terminal does not support colors");
    }

    /* Clear screen */
    clear();
    refresh();

    ANBS_DEBUG_LOG("NCurses initialized successfully");
    return 0;
}

/**
 * Setup color pairs for different UI elements
 */
static int anbs_setup_color_pairs(anbs_display_t *display)
{
    /* Terminal panel: white on black */
    init_pair(ANBS_COLOR_TERMINAL, COLOR_WHITE, COLOR_BLACK);

    /* AI chat panel: cyan on blue */
    init_pair(ANBS_COLOR_AI_CHAT, COLOR_CYAN, COLOR_BLUE);

    /* Health panel: green on dark green */
    init_pair(ANBS_COLOR_AI_HEALTH, COLOR_GREEN, COLOR_BLACK);

    /* Status line: yellow on black */
    init_pair(ANBS_COLOR_STATUS, COLOR_YELLOW, COLOR_BLACK);

    /* Borders: gray */
    init_pair(ANBS_COLOR_BORDER, COLOR_WHITE, COLOR_BLACK);

    /* Cursor: bright white */
    init_pair(ANBS_COLOR_CURSOR, COLOR_WHITE, COLOR_BLACK);

    /* AI responses: bright cyan */
    init_pair(ANBS_COLOR_AI_RESPONSE, COLOR_CYAN, COLOR_BLACK);

    /* Errors: red */
    init_pair(ANBS_COLOR_ERROR, COLOR_RED, COLOR_BLACK);

    return 0;
}

/**
 * Setup panel layout and windows
 */
int anbs_display_setup_panels(anbs_display_t *display)
{
    ANBS_DEBUG_LOG("Setting up panel layout");

    /* Calculate panel positions and dimensions */
    if (anbs_calculate_panel_positions(display) != 0) {
        return -1;
    }

    /* Initialize individual panels */
    int i;
    for (i = 0; i < ANBS_PANEL_COUNT; i++) {
        panel_t *panel = &display->panels[i];

        /* Create NCurses window */
        panel->window = newwin(panel->height, panel->width,
                              panel->start_y, panel->start_x);
        if (!panel->window) {
            ANBS_DEBUG_LOG("Failed to create window for panel %d", i);
            return -1;
        }

        /* Configure panel settings */
        panel->visible = true;
        panel->has_border = display->borders_enabled;

        /* Set color scheme based on panel type */
        switch (i) {
            case ANBS_PANEL_TERMINAL:
                panel->color_pair = ANBS_COLOR_TERMINAL;
                break;
            case ANBS_PANEL_AI_CHAT:
                panel->color_pair = ANBS_COLOR_AI_CHAT;
                break;
            case ANBS_PANEL_HEALTH:
                panel->color_pair = ANBS_COLOR_AI_HEALTH;
                break;
            case ANBS_PANEL_STATUS:
                panel->color_pair = ANBS_COLOR_STATUS;
                break;
        }

        /* Apply color scheme */
        if (display->color_supported) {
            wbkgd(panel->window, COLOR_PAIR(panel->color_pair));
        }

        /* Enable scrolling for content panels */
        if (i != ANBS_PANEL_STATUS) {
            scrollok(panel->window, TRUE);
        }
    }

    /* Draw initial layout */
    anbs_draw_panel_borders(display);

    /* Initial refresh */
    anbs_display_refresh_all(display);

    ANBS_DEBUG_LOG("Panel layout setup complete");
    return 0;
}

/**
 * Calculate panel positions and dimensions based on terminal size
 */
static int anbs_calculate_panel_positions(anbs_display_t *display)
{
    int term_width = display->term_width;
    int term_height = display->term_height;
    int terminal_width, ai_panel_width;
    int ai_chat_height, health_height;

    ANBS_DEBUG_LOG("Calculating panel positions for %dx%d terminal",
                   term_width, term_height);

    /* Calculate panel widths */
    terminal_width = (term_width * display->terminal_ratio) / 100;
    ai_panel_width = term_width - terminal_width - 1; /* -1 for border */

    /* Calculate AI panel heights */
    ai_chat_height = ((term_height - 2) * display->ai_chat_ratio) / 100; /* -2 for status line and border */
    health_height = term_height - ai_chat_height - 2;

    /* Terminal panel (left side) */
    display->panels[ANBS_PANEL_TERMINAL].width = terminal_width;
    display->panels[ANBS_PANEL_TERMINAL].height = term_height - 1; /* -1 for status line */
    display->panels[ANBS_PANEL_TERMINAL].start_x = 0;
    display->panels[ANBS_PANEL_TERMINAL].start_y = 0;

    /* AI chat panel (top right) */
    display->panels[ANBS_PANEL_AI_CHAT].width = ai_panel_width;
    display->panels[ANBS_PANEL_AI_CHAT].height = ai_chat_height;
    display->panels[ANBS_PANEL_AI_CHAT].start_x = terminal_width + 1;
    display->panels[ANBS_PANEL_AI_CHAT].start_y = 0;

    /* Health panel (bottom right) */
    display->panels[ANBS_PANEL_HEALTH].width = ai_panel_width;
    display->panels[ANBS_PANEL_HEALTH].height = health_height;
    display->panels[ANBS_PANEL_HEALTH].start_x = terminal_width + 1;
    display->panels[ANBS_PANEL_HEALTH].start_y = ai_chat_height;

    /* Status panel (bottom full width) */
    display->panels[ANBS_PANEL_STATUS].width = term_width;
    display->panels[ANBS_PANEL_STATUS].height = 1;
    display->panels[ANBS_PANEL_STATUS].start_x = 0;
    display->panels[ANBS_PANEL_STATUS].start_y = term_height - 1;

    ANBS_DEBUG_LOG("Panel layout: Terminal=%dx%d AI_Chat=%dx%d Health=%dx%d Status=%dx%d",
                   display->panels[ANBS_PANEL_TERMINAL].width, display->panels[ANBS_PANEL_TERMINAL].height,
                   display->panels[ANBS_PANEL_AI_CHAT].width, display->panels[ANBS_PANEL_AI_CHAT].height,
                   display->panels[ANBS_PANEL_HEALTH].width, display->panels[ANBS_PANEL_HEALTH].height,
                   display->panels[ANBS_PANEL_STATUS].width, display->panels[ANBS_PANEL_STATUS].height);

    return 0;
}

/**
 * Draw borders around panels
 */
static void anbs_draw_panel_borders(anbs_display_t *display)
{
    if (!display->borders_enabled) {
        return;
    }

    /* Draw vertical separator between terminal and AI panels */
    int separator_x = display->panels[ANBS_PANEL_TERMINAL].width;
    for (int y = 0; y < display->term_height - 1; y++) {
        mvaddch(y, separator_x, '|');
    }

    /* Draw horizontal separator between AI chat and health panels */
    int separator_y = display->panels[ANBS_PANEL_AI_CHAT].height;
    int ai_start_x = display->panels[ANBS_PANEL_AI_CHAT].start_x;
    int ai_width = display->panels[ANBS_PANEL_AI_CHAT].width;

    for (int x = ai_start_x; x < ai_start_x + ai_width; x++) {
        mvaddch(separator_y, x, '-');
    }

    /* Add junction character */
    mvaddch(separator_y, separator_x, '+');
}

/**
 * Get current terminal size
 */
int anbs_get_terminal_size(int *width, int *height)
{
    struct winsize w;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
        return -1;
    }

    *width = w.ws_col;
    *height = w.ws_row;

    return 0;
}

/**
 * Handle terminal resize
 */
int anbs_display_resize(anbs_display_t *display)
{
    int new_width, new_height;
    int i;

    ANBS_DEBUG_LOG("Handling terminal resize");

    /* Get new terminal size */
    if (anbs_get_terminal_size(&new_width, &new_height) != 0) {
        return -1;
    }

    /* Check if size actually changed */
    if (new_width == display->term_width && new_height == display->term_height) {
        return 0;
    }

    /* Update stored dimensions */
    display->term_width = new_width;
    display->term_height = new_height;

    /* Check minimum size requirements */
    if (new_width < ANBS_MIN_TERMINAL_WIDTH || new_height < ANBS_MIN_TERMINAL_HEIGHT) {
        ANBS_DEBUG_LOG("Terminal too small after resize: %dx%d", new_width, new_height);
        return -1;
    }

    /* Resize NCurses screen */
    resizeterm(new_height, new_width);

    /* Recalculate panel positions */
    if (anbs_calculate_panel_positions(display) != 0) {
        return -1;
    }

    /* Resize and reposition panels */
    for (i = 0; i < ANBS_PANEL_COUNT; i++) {
        panel_t *panel = &display->panels[i];

        /* Delete old window */
        if (panel->window) {
            delwin(panel->window);
        }

        /* Create new window with updated dimensions */
        panel->window = newwin(panel->height, panel->width,
                              panel->start_y, panel->start_x);
        if (!panel->window) {
            ANBS_DEBUG_LOG("Failed to recreate window for panel %d after resize", i);
            return -1;
        }

        /* Restore panel settings */
        if (display->color_supported) {
            wbkgd(panel->window, COLOR_PAIR(panel->color_pair));
        }

        if (i != ANBS_PANEL_STATUS) {
            scrollok(panel->window, TRUE);
        }
    }

    /* Redraw everything */
    clear();
    anbs_draw_panel_borders(display);
    anbs_display_refresh_all(display);

    display->last_resize = time(NULL);

    ANBS_DEBUG_LOG("Terminal resize handled successfully: %dx%d", new_width, new_height);
    return 0;
}

/**
 * Refresh all panels
 */
int anbs_display_refresh_all(anbs_display_t *display)
{
    int i;

    if (!display || !display->ncurses_initialized) {
        return -1;
    }

    /* Refresh main screen */
    refresh();

    /* Refresh all panels */
    for (i = 0; i < ANBS_PANEL_COUNT; i++) {
        if (display->panels[i].visible && display->panels[i].window) {
            wrefresh(display->panels[i].window);
        }
    }

    display->last_refresh = time(NULL);
    display->refresh_count++;

    return 0;
}

/**
 * Write text to terminal panel
 */
int anbs_terminal_write(anbs_display_t *display, const char *text)
{
    if (!display || !text) {
        return -1;
    }

    panel_t *panel = &display->panels[ANBS_PANEL_TERMINAL];

    /* Add text to buffer */
    anbs_text_buffer_append(panel->buffer, text);

    /* Write to window */
    if (panel->window && panel->visible) {
        wprintw(panel->window, "%s", text);
        wrefresh(panel->window);
    }

    return 0;
}

/**
 * Write AI response to chat panel
 */
int anbs_ai_chat_write(anbs_display_t *display, const char *response)
{
    if (!display || !response) {
        return -1;
    }

    panel_t *panel = &display->panels[ANBS_PANEL_AI_CHAT];

    /* Add response to buffer with AI formatting */
    char formatted_response[2048];
    snprintf(formatted_response, sizeof(formatted_response), "🤖 %s", response);

    anbs_text_buffer_append(panel->buffer, formatted_response);

    /* Write to window with color */
    if (panel->window && panel->visible) {
        if (display->color_supported) {
            wattron(panel->window, COLOR_PAIR(ANBS_COLOR_AI_RESPONSE));
        }
        wprintw(panel->window, "%s\n", formatted_response);
        if (display->color_supported) {
            wattroff(panel->window, COLOR_PAIR(ANBS_COLOR_AI_RESPONSE));
        }
        wrefresh(panel->window);
    }

    return 0;
}

/**
 * Signal handler for terminal resize
 */
void anbs_signal_resize_handler(int sig)
{
    if (g_anbs_display) {
        anbs_display_resize(g_anbs_display);
    }
}

/**
 * Install signal handlers
 */
int anbs_install_signal_handlers(void)
{
    struct sigaction sa;

    /* Setup SIGWINCH handler for terminal resize */
    sa.sa_handler = anbs_signal_resize_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGWINCH, &sa, NULL) == -1) {
        return -1;
    }

    return 0;
}

/**
 * Cleanup display system
 */
void anbs_display_cleanup(anbs_display_t *display)
{
    int i;

    if (!display) {
        return;
    }

    ANBS_DEBUG_LOG("Cleaning up ANBS display system");

    /* Cleanup panels */
    for (i = 0; i < ANBS_PANEL_COUNT; i++) {
        if (display->panels[i].window) {
            delwin(display->panels[i].window);
        }
        if (display->panels[i].buffer) {
            anbs_text_buffer_cleanup(display->panels[i].buffer);
        }
    }

    /* Cleanup NCurses */
    if (display->ncurses_initialized) {
        endwin();
    }

    /* Clear global reference */
    if (g_anbs_display == display) {
        g_anbs_display = NULL;
    }

    /* Free display structure */
    free(display);

    ANBS_DEBUG_LOG("ANBS display system cleanup complete");
}

/**
 * Detect if command is an AI command
 */
bool anbs_detect_ai_command(const char *command)
{
    if (!command) {
        return false;
    }

    /* Check for AI command prefixes */
    return (strncmp(command, "@vertex", 7) == 0 ||
            strncmp(command, "@memory", 7) == 0 ||
            strncmp(command, "@analyze", 8) == 0 ||
            strncmp(command, "@health", 7) == 0);
}

/**
 * Route output to appropriate panel
 */
int anbs_route_output(anbs_display_t *display, output_destination_t dest, const char *text)
{
    if (!display || !text) {
        return -1;
    }

    switch (dest) {
        case OUTPUT_TERMINAL:
            return anbs_terminal_write(display, text);
        case OUTPUT_AI_CHAT:
            return anbs_ai_chat_write(display, text);
        case OUTPUT_AI_HEALTH:
            /* Health updates handled by anbs_health_update */
            return 0;
        case OUTPUT_STATUS:
            return anbs_status_write(display, text);
        default:
            return -1;
    }
}

/**
 * Write status message
 */
int anbs_status_write(anbs_display_t *display, const char *status)
{
    if (!display || !status) {
        return -1;
    }

    panel_t *panel = &display->panels[ANBS_PANEL_STATUS];

    if (panel->window && panel->visible) {
        /* Clear status line */
        werase(panel->window);

        /* Write new status */
        if (display->color_supported) {
            wattron(panel->window, COLOR_PAIR(ANBS_COLOR_STATUS));
        }
        mvwprintw(panel->window, 0, 0, "ANBS: %s", status);
        if (display->color_supported) {
            wattroff(panel->window, COLOR_PAIR(ANBS_COLOR_STATUS));
        }

        wrefresh(panel->window);
    }

    return 0;
}