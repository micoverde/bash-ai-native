/* text_buffer.c - Text buffer management for ANBS display system */

#include "ai_display.h"
#include <stdlib.h>
#include <string.h>

/**
 * Initialize a text buffer
 */
int anbs_text_buffer_init(text_buffer_t **buffer, int max_lines)
{
    text_buffer_t *buf;
    int i;

    if (!buffer || max_lines <= 0) {
        return -1;
    }

    /* Allocate buffer structure */
    buf = (text_buffer_t *)calloc(1, sizeof(text_buffer_t));
    if (!buf) {
        return -1;
    }

    /* Allocate line array */
    buf->lines = (char **)calloc(max_lines, sizeof(char *));
    if (!buf->lines) {
        free(buf);
        return -1;
    }

    /* Initialize line pointers to NULL */
    for (i = 0; i < max_lines; i++) {
        buf->lines[i] = NULL;
    }

    /* Set buffer parameters */
    buf->max_lines = max_lines;
    buf->current_line = 0;
    buf->display_start = 0;
    buf->line_count = 0;
    buf->dirty = false;

    *buffer = buf;
    return 0;
}

/**
 * Append a line to the text buffer
 */
int anbs_text_buffer_append(text_buffer_t *buffer, const char *line)
{
    char *line_copy;

    if (!buffer || !line) {
        return -1;
    }

    /* Create copy of the line */
    line_copy = strdup(line);
    if (!line_copy) {
        return -1;
    }

    /* If we're at capacity, free the oldest line */
    if (buffer->lines[buffer->current_line] != NULL) {
        free(buffer->lines[buffer->current_line]);
    } else {
        /* First time filling this slot */
        buffer->line_count++;
    }

    /* Store the new line */
    buffer->lines[buffer->current_line] = line_copy;

    /* Advance to next position (circular buffer) */
    buffer->current_line = (buffer->current_line + 1) % buffer->max_lines;

    /* Update display start if buffer is full */
    if (buffer->line_count >= buffer->max_lines) {
        buffer->display_start = buffer->current_line;
    }

    /* Mark as dirty for refresh */
    buffer->dirty = true;

    return 0;
}

/**
 * Get lines from buffer for display
 */
int anbs_text_buffer_get_lines(text_buffer_t *buffer, char ***lines, int start, int count)
{
    char **result;
    int i, line_index;
    int available_lines;

    if (!buffer || !lines || count <= 0) {
        return -1;
    }

    /* Calculate available lines */
    available_lines = (buffer->line_count < buffer->max_lines) ?
                     buffer->line_count : buffer->max_lines;

    /* Adjust count if requesting more than available */
    if (count > available_lines) {
        count = available_lines;
    }

    /* Adjust start if out of bounds */
    if (start >= available_lines) {
        start = available_lines - 1;
    }
    if (start < 0) {
        start = 0;
    }

    /* Allocate result array */
    result = (char **)calloc(count, sizeof(char *));
    if (!result) {
        return -1;
    }

    /* Copy line pointers */
    for (i = 0; i < count; i++) {
        if (buffer->line_count < buffer->max_lines) {
            /* Buffer not full yet, simple indexing */
            line_index = start + i;
        } else {
            /* Buffer is full, use circular indexing */
            line_index = (buffer->display_start + start + i) % buffer->max_lines;
        }

        if (line_index < available_lines && buffer->lines[line_index] != NULL) {
            result[i] = buffer->lines[line_index];
        } else {
            result[i] = "";
        }
    }

    *lines = result;
    return count;
}

/**
 * Get the most recent lines from buffer
 */
int anbs_text_buffer_get_recent_lines(text_buffer_t *buffer, char ***lines, int count)
{
    int available_lines;
    int start_index;

    if (!buffer || !lines || count <= 0) {
        return -1;
    }

    available_lines = (buffer->line_count < buffer->max_lines) ?
                     buffer->line_count : buffer->max_lines;

    if (count > available_lines) {
        count = available_lines;
    }

    /* Start from the most recent lines */
    start_index = available_lines - count;
    if (start_index < 0) {
        start_index = 0;
    }

    return anbs_text_buffer_get_lines(buffer, lines, start_index, count);
}

/**
 * Clear the text buffer
 */
void anbs_text_buffer_clear(text_buffer_t *buffer)
{
    int i;

    if (!buffer) {
        return;
    }

    /* Free all line strings */
    for (i = 0; i < buffer->max_lines; i++) {
        if (buffer->lines[i]) {
            free(buffer->lines[i]);
            buffer->lines[i] = NULL;
        }
    }

    /* Reset counters */
    buffer->current_line = 0;
    buffer->display_start = 0;
    buffer->line_count = 0;
    buffer->dirty = true;
}

/**
 * Cleanup text buffer
 */
void anbs_text_buffer_cleanup(text_buffer_t *buffer)
{
    if (!buffer) {
        return;
    }

    /* Clear all lines */
    anbs_text_buffer_clear(buffer);

    /* Free line array */
    if (buffer->lines) {
        free(buffer->lines);
    }

    /* Free buffer structure */
    free(buffer);
}

/**
 * Get buffer statistics
 */
int anbs_text_buffer_get_stats(text_buffer_t *buffer, int *total_lines,
                               int *used_lines, bool *is_dirty)
{
    if (!buffer) {
        return -1;
    }

    if (total_lines) {
        *total_lines = buffer->max_lines;
    }

    if (used_lines) {
        *used_lines = (buffer->line_count < buffer->max_lines) ?
                     buffer->line_count : buffer->max_lines;
    }

    if (is_dirty) {
        *is_dirty = buffer->dirty;
    }

    return 0;
}

/**
 * Mark buffer as clean (after refresh)
 */
void anbs_text_buffer_mark_clean(text_buffer_t *buffer)
{
    if (buffer) {
        buffer->dirty = false;
    }
}

/**
 * Search for text in buffer
 */
int anbs_text_buffer_search(text_buffer_t *buffer, const char *search_term,
                           int **matching_lines, int max_matches)
{
    int i, matches = 0;
    int *results;
    int available_lines;

    if (!buffer || !search_term || !matching_lines || max_matches <= 0) {
        return -1;
    }

    available_lines = (buffer->line_count < buffer->max_lines) ?
                     buffer->line_count : buffer->max_lines;

    /* Allocate results array */
    results = (int *)calloc(max_matches, sizeof(int));
    if (!results) {
        return -1;
    }

    /* Search through all lines */
    for (i = 0; i < available_lines && matches < max_matches; i++) {
        int line_index;

        if (buffer->line_count < buffer->max_lines) {
            line_index = i;
        } else {
            line_index = (buffer->display_start + i) % buffer->max_lines;
        }

        if (buffer->lines[line_index] &&
            strstr(buffer->lines[line_index], search_term) != NULL) {
            results[matches] = i;  /* Store display index, not buffer index */
            matches++;
        }
    }

    if (matches == 0) {
        free(results);
        *matching_lines = NULL;
        return 0;
    }

    *matching_lines = results;
    return matches;
}

/**
 * Get a specific line by display index
 */
const char *anbs_text_buffer_get_line(text_buffer_t *buffer, int display_index)
{
    int line_index;
    int available_lines;

    if (!buffer || display_index < 0) {
        return NULL;
    }

    available_lines = (buffer->line_count < buffer->max_lines) ?
                     buffer->line_count : buffer->max_lines;

    if (display_index >= available_lines) {
        return NULL;
    }

    if (buffer->line_count < buffer->max_lines) {
        line_index = display_index;
    } else {
        line_index = (buffer->display_start + display_index) % buffer->max_lines;
    }

    return buffer->lines[line_index];
}