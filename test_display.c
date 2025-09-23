/* test_display.c - Simple test program for ANBS display system */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

/* Include our AI display system */
#include "bash-5.2/ai_core/ai_display.h"

static anbs_display_t *g_display = NULL;

void cleanup_and_exit(int sig) {
    if (g_display) {
        anbs_display_cleanup(g_display);
    }
    printf("\nANBS Display Test Terminated\n");
    exit(0);
}

int main() {
    printf("ANBS Split-Screen Interface Test\n");
    printf("=================================\n\n");

    /* Set up signal handler for clean exit */
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);

    /* Initialize display system */
    printf("Initializing ANBS display system...\n");
    if (anbs_display_init(&g_display) != 0) {
        fprintf(stderr, "Failed to initialize ANBS display system\n");
        return 1;
    }

    printf("Display system initialized successfully!\n");
    printf("Terminal size: %dx%d\n", g_display->term_width, g_display->term_height);
    printf("Color support: %s\n", g_display->color_supported ? "Yes" : "No");
    printf("Unicode support: %s\n", g_display->unicode_supported ? "Yes" : "No");
    printf("\nPress 's' to toggle split-screen mode, 'b' to toggle borders, 'q' to quit\n\n");

    /* Add some sample content */
    anbs_terminal_write(g_display, "Welcome to AI-Native Bash Shell (ANBS)\n");
    anbs_terminal_write(g_display, "This is the terminal panel where bash commands appear.\n");
    anbs_terminal_write(g_display, "Type commands here as you normally would.\n\n");

    anbs_ai_chat_write(g_display, "AI: Hello! I'm your AI assistant.\n");
    anbs_ai_chat_write(g_display, "AI: I can help with commands, explain outputs, and provide suggestions.\n");
    anbs_ai_chat_write(g_display, "AI: Try typing '@vertex help' for AI commands.\n\n");

    /* Add sample health data */
    health_data_t health_vertex = anbs_health_create_sample("vertex", true, 45, 12.5, 18.7, 156, 98.2);
    health_data_t health_claude = anbs_health_create_sample("claude", true, 38, 8.3, 15.2, 203, 99.1);

    anbs_health_update(g_display, &health_vertex);
    anbs_health_update(g_display, &health_claude);

    /* Status message */
    anbs_status_write(g_display, "ANBS Test Mode - Ready for commands");

    /* Refresh all panels */
    anbs_display_refresh_all(g_display);

    /* Main event loop */
    int ch;
    while ((ch = getch()) != 'q' && ch != 'Q') {
        switch (ch) {
            case 's':
            case 'S':
                anbs_display_toggle_split_mode(g_display);
                anbs_display_refresh_all(g_display);
                break;

            case 'b':
            case 'B':
                anbs_display_toggle_borders(g_display);
                anbs_display_refresh_all(g_display);
                break;

            case 'r':
            case 'R':
                anbs_display_refresh_all(g_display);
                break;

            case KEY_RESIZE:
                anbs_display_resize(g_display);
                break;

            default:
                /* Add some interactive content */
                char msg[128];
                snprintf(msg, sizeof(msg), "Key pressed: %c (code: %d)\n", ch, ch);
                anbs_terminal_write(g_display, msg);
                anbs_display_refresh_panel(g_display, ANBS_PANEL_TERMINAL);
                break;
        }

        /* Small delay to prevent busy waiting */
        usleep(10000);
    }

    /* Cleanup */
    cleanup_and_exit(0);
    return 0;
}