/* ai_display.h - AI-Native Bash Shell Display System Header */

#ifndef ANBS_AI_DISPLAY_H
#define ANBS_AI_DISPLAY_H

#include <ncurses.h>
#include <time.h>
#include <stdbool.h>

/* Display system configuration */
#define ANBS_MIN_TERMINAL_WIDTH   120
#define ANBS_MIN_TERMINAL_HEIGHT  40
#define ANBS_DEFAULT_TERMINAL_RATIO 60  /* Terminal panel width percentage */
#define ANBS_DEFAULT_AI_CHAT_RATIO  50  /* AI chat panel height percentage */
#define ANBS_MAX_TEXT_BUFFER_LINES  1000
#define ANBS_REFRESH_INTERVAL_MS    16   /* 60 FPS target */

/* Panel identifiers */
typedef enum {
    ANBS_PANEL_TERMINAL = 0,
    ANBS_PANEL_AI_CHAT,
    ANBS_PANEL_HEALTH,
    ANBS_PANEL_STATUS,
    ANBS_PANEL_COUNT
} anbs_panel_id_t;

/* Output routing destinations */
typedef enum {
    OUTPUT_TERMINAL = 0,     /* Normal bash output */
    OUTPUT_AI_CHAT,          /* @vertex responses */
    OUTPUT_AI_HEALTH,        /* Health updates */
    OUTPUT_STATUS            /* Status messages */
} output_destination_t;

/* Color scheme definitions */
#define ANBS_COLOR_TERMINAL    1  /* White on black */
#define ANBS_COLOR_AI_CHAT     2  /* Cyan on dark blue */
#define ANBS_COLOR_AI_HEALTH   3  /* Green on dark green */
#define ANBS_COLOR_STATUS      4  /* Yellow on black */
#define ANBS_COLOR_BORDER      5  /* Gray borders */
#define ANBS_COLOR_CURSOR      6  /* Bright white cursor */
#define ANBS_COLOR_AI_RESPONSE 7  /* Bright cyan for AI text */
#define ANBS_COLOR_ERROR       8  /* Red for errors */

/* Border characters (UTF-8 box drawing) */
#define ANBS_BORDER_VERTICAL   "│"
#define ANBS_BORDER_HORIZONTAL "─"
#define ANBS_BORDER_CORNER_TL  "┌"
#define ANBS_BORDER_CORNER_TR  "┐"
#define ANBS_BORDER_CORNER_BL  "└"
#define ANBS_BORDER_CORNER_BR  "┘"
#define ANBS_BORDER_JUNCTION_T "┬"
#define ANBS_BORDER_JUNCTION_L "├"
#define ANBS_BORDER_JUNCTION_R "┤"
#define ANBS_BORDER_JUNCTION_B "┴"
#define ANBS_BORDER_CROSS      "┼"

/* Text buffer for efficient scrolling */
typedef struct {
    char **lines;              /* Circular buffer of text lines */
    int max_lines;             /* Buffer size (configurable) */
    int current_line;          /* Current write position */
    int display_start;         /* Top visible line */
    int line_count;            /* Total lines in buffer */
    bool dirty;                /* Needs refresh */
} text_buffer_t;

/* Health monitoring data */
typedef struct {
    char agent_id[64];         /* Agent identifier */
    bool online;               /* Connection status */
    int latency_ms;            /* Response latency */
    float cpu_load;            /* CPU usage percentage */
    float memory_usage;        /* Memory usage percentage */
    int commands_processed;    /* Total commands */
    float success_rate;        /* Success percentage */
    time_t last_update;        /* Last update timestamp */
} health_data_t;

/* Panel buffer for double buffering */
typedef struct {
    WINDOW *window;            /* NCurses window */
    text_buffer_t *buffer;     /* Text content buffer */
    int width, height;         /* Panel dimensions */
    int start_x, start_y;      /* Panel position */
    bool visible;              /* Panel visibility */
    bool has_border;           /* Draw border around panel */
    int color_pair;            /* Color scheme */
    time_t last_refresh;       /* Last refresh timestamp */
} panel_t;

/* Main ANBS display structure */
typedef struct anbs_display {
    /* NCurses management */
    WINDOW *main_screen;       /* Full terminal window */
    bool ncurses_initialized;  /* NCurses init status */
    bool color_supported;      /* Terminal color support */

    /* Panel management */
    panel_t panels[ANBS_PANEL_COUNT];
    int active_panel;          /* Currently focused panel */

    /* Layout configuration */
    int term_width, term_height;   /* Current terminal dimensions */
    int terminal_ratio;            /* Terminal panel width ratio (%) */
    int ai_chat_ratio;             /* AI chat panel height ratio (%) */

    /* Display state */
    bool split_mode_active;    /* Toggle split-screen on/off */
    bool panels_visible;       /* Show/hide AI panels */
    bool borders_enabled;      /* Panel borders on/off */
    int color_scheme;          /* Theme selection */

    /* Performance tracking */
    time_t last_resize;        /* Last resize event */
    time_t last_refresh;       /* Last full refresh */
    int refresh_count;         /* Refresh counter */

    /* Health monitoring */
    health_data_t health_data[10];  /* Up to 10 AI agents */
    int health_agent_count;         /* Number of active agents */

    /* Command detection */
    bool ai_command_active;    /* Currently processing AI command */
    char current_ai_command[256];   /* Current AI command text */
} anbs_display_t;

/* Function declarations */

/* Core display management */
int anbs_display_init(anbs_display_t **display);
int anbs_display_setup_panels(anbs_display_t *display);
int anbs_display_configure_colors(anbs_display_t *display);
void anbs_display_cleanup(anbs_display_t *display);

/* Window management */
int anbs_display_resize(anbs_display_t *display);
int anbs_display_refresh_all(anbs_display_t *display);
int anbs_display_refresh_panel(anbs_display_t *display, anbs_panel_id_t panel_id);
int anbs_display_toggle_split_mode(anbs_display_t *display);
int anbs_display_toggle_borders(anbs_display_t *display);

/* Panel operations */
int anbs_terminal_write(anbs_display_t *display, const char *text);
int anbs_ai_chat_write(anbs_display_t *display, const char *response);
int anbs_health_update(anbs_display_t *display, const health_data_t *data);
int anbs_status_write(anbs_display_t *display, const char *status);

/* Text buffer management */
int anbs_text_buffer_init(text_buffer_t **buffer, int max_lines);
int anbs_text_buffer_append(text_buffer_t *buffer, const char *line);
int anbs_text_buffer_get_lines(text_buffer_t *buffer, char ***lines, int start, int count);
void anbs_text_buffer_cleanup(text_buffer_t *buffer);

/* Panel management */
int anbs_panel_init(panel_t *panel, int width, int height, int start_x, int start_y);
int anbs_panel_resize(panel_t *panel, int width, int height, int start_x, int start_y);
int anbs_panel_write_text(panel_t *panel, const char *text);
int anbs_panel_draw_border(panel_t *panel, const char *title);
int anbs_panel_scroll_up(panel_t *panel, int lines);
int anbs_panel_scroll_down(panel_t *panel, int lines);
void anbs_panel_cleanup(panel_t *panel);

/* Output routing */
int anbs_route_output(anbs_display_t *display, output_destination_t dest, const char *text);
bool anbs_detect_ai_command(const char *command);
int anbs_set_ai_command_active(anbs_display_t *display, const char *command);
int anbs_clear_ai_command_active(anbs_display_t *display);

/* Utility functions */
int anbs_get_terminal_size(int *width, int *height);
bool anbs_terminal_supports_color(void);
bool anbs_terminal_supports_unicode(void);
int anbs_calculate_panel_dimensions(anbs_display_t *display);
const char *anbs_format_timestamp(time_t timestamp);
const char *anbs_format_health_status(const health_data_t *data);

/* Signal handling */
void anbs_signal_resize_handler(int sig);
int anbs_install_signal_handlers(void);

/* Global display instance */
extern anbs_display_t *g_anbs_display;

/* Convenience macros */
#define ANBS_DISPLAY_ENABLED() (g_anbs_display != NULL && g_anbs_display->ncurses_initialized)
#define ANBS_SPLIT_MODE_ACTIVE() (ANBS_DISPLAY_ENABLED() && g_anbs_display->split_mode_active)
#define ANBS_TERMINAL_PANEL() (&g_anbs_display->panels[ANBS_PANEL_TERMINAL])
#define ANBS_AI_CHAT_PANEL() (&g_anbs_display->panels[ANBS_PANEL_AI_CHAT])
#define ANBS_HEALTH_PANEL() (&g_anbs_display->panels[ANBS_PANEL_HEALTH])
#define ANBS_STATUS_PANEL() (&g_anbs_display->panels[ANBS_PANEL_STATUS])

/* Debug macros */
#ifdef ANBS_DEBUG
#define ANBS_DEBUG_LOG(fmt, ...) \
    do { \
        FILE *debug_file = fopen("/tmp/anbs_debug.log", "a"); \
        if (debug_file) { \
            fprintf(debug_file, "[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
            fclose(debug_file); \
        } \
    } while (0)
#else
#define ANBS_DEBUG_LOG(fmt, ...)
#endif

#endif /* ANBS_AI_DISPLAY_H */