/* memory_system.c - Vector search and conversation memory for ANBS */

#include "ai_display.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <sqlite3.h>
#include <time.h>

#define MAX_MEMORY_ENTRIES 10000
#define EMBEDDING_DIMENSION 1536  /* OpenAI ada-002 dimension */
#define MEMORY_DB_PATH "/tmp/anbs_memory.db"

typedef struct {
    char *content;
    float *embedding;
    time_t timestamp;
    char *context;
    char *source;
    float relevance_score;
} memory_entry_t;

typedef struct {
    memory_entry_t *entries;
    int count;
    int capacity;
    sqlite3 *db;
    pthread_mutex_t mutex;
} memory_system_t;

static memory_system_t *g_memory = NULL;

/* Simple text embedding using character frequency analysis */
static void generate_simple_embedding(const char *text, float *embedding) {
    /* Clear embedding */
    memset(embedding, 0, EMBEDDING_DIMENSION * sizeof(float));

    int len = strlen(text);
    if (len == 0) return;

    /* Character frequency analysis */
    int char_freq[256] = {0};
    for (int i = 0; i < len; i++) {
        char_freq[(unsigned char)text[i]]++;
    }

    /* Normalize frequencies and map to embedding space */
    for (int i = 0; i < 256 && i < EMBEDDING_DIMENSION; i++) {
        embedding[i] = (float)char_freq[i] / len;
    }

    /* Add word length features */
    int word_count = 1;
    int total_word_len = 0;
    int current_word_len = 0;

    for (int i = 0; i < len; i++) {
        if (isspace(text[i])) {
            if (current_word_len > 0) {
                total_word_len += current_word_len;
                word_count++;
                current_word_len = 0;
            }
        } else {
            current_word_len++;
        }
    }
    if (current_word_len > 0) {
        total_word_len += current_word_len;
    }

    /* Add statistical features to embedding */
    if (EMBEDDING_DIMENSION > 256) {
        embedding[256] = (float)word_count / len;  /* Word density */
        embedding[257] = (float)total_word_len / word_count;  /* Average word length */
        embedding[258] = len > 100 ? 1.0 : (float)len / 100;  /* Text length feature */
    }

    /* Add semantic features based on common programming terms */
    const char *prog_keywords[] = {
        "function", "class", "variable", "loop", "if", "else", "return",
        "import", "export", "const", "let", "var", "async", "await",
        "bash", "shell", "command", "script", "file", "directory",
        "error", "debug", "fix", "issue", "problem", "solution"
    };

    int prog_keyword_count = sizeof(prog_keywords) / sizeof(prog_keywords[0]);
    for (int i = 0; i < prog_keyword_count && (259 + i) < EMBEDDING_DIMENSION; i++) {
        if (strstr(text, prog_keywords[i])) {
            embedding[259 + i] = 1.0;
        }
    }
}

/* Calculate cosine similarity between two embeddings */
static float calculate_similarity(const float *embedding1, const float *embedding2) {
    float dot_product = 0.0;
    float norm1 = 0.0;
    float norm2 = 0.0;

    for (int i = 0; i < EMBEDDING_DIMENSION; i++) {
        dot_product += embedding1[i] * embedding2[i];
        norm1 += embedding1[i] * embedding1[i];
        norm2 += embedding2[i] * embedding2[i];
    }

    if (norm1 == 0.0 || norm2 == 0.0) {
        return 0.0;
    }

    return dot_product / (sqrt(norm1) * sqrt(norm2));
}

/* Initialize memory system */
int anbs_memory_init(void) {
    if (g_memory) {
        return 0; /* Already initialized */
    }

    g_memory = calloc(1, sizeof(memory_system_t));
    if (!g_memory) {
        return -1;
    }

    g_memory->capacity = MAX_MEMORY_ENTRIES;
    g_memory->entries = calloc(g_memory->capacity, sizeof(memory_entry_t));
    if (!g_memory->entries) {
        free(g_memory);
        g_memory = NULL;
        return -1;
    }

    pthread_mutex_init(&g_memory->mutex, NULL);

    /* Initialize SQLite database */
    int rc = sqlite3_open(MEMORY_DB_PATH, &g_memory->db);
    if (rc != SQLITE_OK) {
        ANBS_DEBUG_LOG("Failed to open memory database: %s", sqlite3_errmsg(g_memory->db));
        anbs_memory_cleanup();
        return -1;
    }

    /* Create tables */
    const char *create_table_sql =
        "CREATE TABLE IF NOT EXISTS memories ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "content TEXT NOT NULL,"
        "embedding BLOB,"
        "timestamp INTEGER,"
        "context TEXT,"
        "source TEXT,"
        "relevance_score REAL DEFAULT 0.0"
        ");";

    rc = sqlite3_exec(g_memory->db, create_table_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        ANBS_DEBUG_LOG("Failed to create memory table: %s", sqlite3_errmsg(g_memory->db));
        anbs_memory_cleanup();
        return -1;
    }

    /* Load existing memories from database */
    anbs_memory_load_from_db();

    ANBS_DEBUG_LOG("Memory system initialized with %d entries", g_memory->count);
    return 0;
}

/* Add memory entry */
int anbs_memory_add(const char *content, const char *context, const char *source) {
    if (!g_memory || !content) {
        return -1;
    }

    pthread_mutex_lock(&g_memory->mutex);

    /* Check if we need to remove old entries */
    if (g_memory->count >= g_memory->capacity) {
        /* Remove oldest entry */
        free(g_memory->entries[0].content);
        free(g_memory->entries[0].embedding);
        free(g_memory->entries[0].context);
        free(g_memory->entries[0].source);

        /* Shift entries */
        memmove(&g_memory->entries[0], &g_memory->entries[1],
                (g_memory->count - 1) * sizeof(memory_entry_t));
        g_memory->count--;
    }

    /* Add new entry */
    memory_entry_t *entry = &g_memory->entries[g_memory->count];

    entry->content = strdup(content);
    entry->embedding = malloc(EMBEDDING_DIMENSION * sizeof(float));
    entry->timestamp = time(NULL);
    entry->context = context ? strdup(context) : NULL;
    entry->source = source ? strdup(source) : strdup("terminal");
    entry->relevance_score = 0.0;

    if (!entry->content || !entry->embedding) {
        pthread_mutex_unlock(&g_memory->mutex);
        return -1;
    }

    /* Generate embedding */
    generate_simple_embedding(content, entry->embedding);

    g_memory->count++;

    /* Save to database */
    anbs_memory_save_to_db(entry);

    pthread_mutex_unlock(&g_memory->mutex);

    ANBS_DEBUG_LOG("Added memory entry: %.50s...", content);
    return 0;
}

/* Search memories by similarity */
int anbs_memory_search(const char *query, memory_entry_t **results, int max_results) {
    if (!g_memory || !query || !results) {
        return -1;
    }

    float query_embedding[EMBEDDING_DIMENSION];
    generate_simple_embedding(query, query_embedding);

    pthread_mutex_lock(&g_memory->mutex);

    /* Calculate similarities */
    for (int i = 0; i < g_memory->count; i++) {
        g_memory->entries[i].relevance_score =
            calculate_similarity(query_embedding, g_memory->entries[i].embedding);
    }

    /* Sort by relevance (bubble sort for simplicity) */
    for (int i = 0; i < g_memory->count - 1; i++) {
        for (int j = 0; j < g_memory->count - 1 - i; j++) {
            if (g_memory->entries[j].relevance_score < g_memory->entries[j + 1].relevance_score) {
                memory_entry_t temp = g_memory->entries[j];
                g_memory->entries[j] = g_memory->entries[j + 1];
                g_memory->entries[j + 1] = temp;
            }
        }
    }

    /* Return top results */
    int result_count = (max_results < g_memory->count) ? max_results : g_memory->count;
    *results = malloc(result_count * sizeof(memory_entry_t));

    if (!*results) {
        pthread_mutex_unlock(&g_memory->mutex);
        return -1;
    }

    for (int i = 0; i < result_count; i++) {
        memory_entry_t *src = &g_memory->entries[i];
        memory_entry_t *dst = &(*results)[i];

        dst->content = strdup(src->content);
        dst->embedding = NULL; /* Don't copy embedding for results */
        dst->timestamp = src->timestamp;
        dst->context = src->context ? strdup(src->context) : NULL;
        dst->source = src->source ? strdup(src->source) : NULL;
        dst->relevance_score = src->relevance_score;
    }

    pthread_mutex_unlock(&g_memory->mutex);

    ANBS_DEBUG_LOG("Memory search for '%s' returned %d results", query, result_count);
    return result_count;
}

/* Get recent memories */
int anbs_memory_get_recent(memory_entry_t **results, int max_results) {
    if (!g_memory || !results) {
        return -1;
    }

    pthread_mutex_lock(&g_memory->mutex);

    int result_count = (max_results < g_memory->count) ? max_results : g_memory->count;
    *results = malloc(result_count * sizeof(memory_entry_t));

    if (!*results) {
        pthread_mutex_unlock(&g_memory->mutex);
        return -1;
    }

    /* Copy most recent entries */
    for (int i = 0; i < result_count; i++) {
        memory_entry_t *src = &g_memory->entries[g_memory->count - 1 - i];
        memory_entry_t *dst = &(*results)[i];

        dst->content = strdup(src->content);
        dst->embedding = NULL;
        dst->timestamp = src->timestamp;
        dst->context = src->context ? strdup(src->context) : NULL;
        dst->source = src->source ? strdup(src->source) : NULL;
        dst->relevance_score = src->relevance_score;
    }

    pthread_mutex_unlock(&g_memory->mutex);

    return result_count;
}

/* Save memory entry to database */
int anbs_memory_save_to_db(const memory_entry_t *entry) {
    if (!g_memory || !entry) {
        return -1;
    }

    const char *sql =
        "INSERT INTO memories (content, embedding, timestamp, context, source, relevance_score) "
        "VALUES (?, ?, ?, ?, ?, ?)";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_memory->db, sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, entry->content, -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, entry->embedding, EMBEDDING_DIMENSION * sizeof(float), SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, entry->timestamp);
    sqlite3_bind_text(stmt, 4, entry->context, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, entry->source, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 6, entry->relevance_score);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Load memories from database */
int anbs_memory_load_from_db(void) {
    if (!g_memory) {
        return -1;
    }

    const char *sql =
        "SELECT content, embedding, timestamp, context, source FROM memories "
        "ORDER BY timestamp DESC LIMIT ?";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_memory->db, sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int(stmt, 1, g_memory->capacity);

    pthread_mutex_lock(&g_memory->mutex);
    g_memory->count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && g_memory->count < g_memory->capacity) {
        memory_entry_t *entry = &g_memory->entries[g_memory->count];

        const char *content = (const char*)sqlite3_column_text(stmt, 0);
        const void *embedding_blob = sqlite3_column_blob(stmt, 1);
        int embedding_size = sqlite3_column_bytes(stmt, 1);

        entry->content = strdup(content);
        entry->embedding = malloc(EMBEDDING_DIMENSION * sizeof(float));

        if (embedding_size == EMBEDDING_DIMENSION * sizeof(float)) {
            memcpy(entry->embedding, embedding_blob, embedding_size);
        } else {
            /* Regenerate embedding if size mismatch */
            generate_simple_embedding(content, entry->embedding);
        }

        entry->timestamp = sqlite3_column_int64(stmt, 2);

        const char *context = (const char*)sqlite3_column_text(stmt, 3);
        entry->context = context ? strdup(context) : NULL;

        const char *source = (const char*)sqlite3_column_text(stmt, 4);
        entry->source = source ? strdup(source) : strdup("unknown");

        entry->relevance_score = 0.0;

        g_memory->count++;
    }

    pthread_mutex_unlock(&g_memory->mutex);
    sqlite3_finalize(stmt);

    return g_memory->count;
}

/* Free memory results */
void anbs_memory_free_results(memory_entry_t *results, int count) {
    if (!results) return;

    for (int i = 0; i < count; i++) {
        free(results[i].content);
        free(results[i].context);
        free(results[i].source);
    }

    free(results);
}

/* Get memory statistics */
int anbs_memory_get_stats(int *total_entries, int *db_entries, size_t *memory_usage) {
    if (!g_memory) {
        return -1;
    }

    pthread_mutex_lock(&g_memory->mutex);

    if (total_entries) {
        *total_entries = g_memory->count;
    }

    if (memory_usage) {
        *memory_usage = g_memory->count * sizeof(memory_entry_t);
        for (int i = 0; i < g_memory->count; i++) {
            *memory_usage += strlen(g_memory->entries[i].content);
            if (g_memory->entries[i].context) {
                *memory_usage += strlen(g_memory->entries[i].context);
            }
            if (g_memory->entries[i].source) {
                *memory_usage += strlen(g_memory->entries[i].source);
            }
            *memory_usage += EMBEDDING_DIMENSION * sizeof(float);
        }
    }

    pthread_mutex_unlock(&g_memory->mutex);

    if (db_entries) {
        const char *sql = "SELECT COUNT(*) FROM memories";
        sqlite3_stmt *stmt;

        if (sqlite3_prepare_v2(g_memory->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                *db_entries = sqlite3_column_int(stmt, 0);
            } else {
                *db_entries = -1;
            }
            sqlite3_finalize(stmt);
        } else {
            *db_entries = -1;
        }
    }

    return 0;
}

/* Cleanup memory system */
void anbs_memory_cleanup(void) {
    if (!g_memory) {
        return;
    }

    pthread_mutex_lock(&g_memory->mutex);

    /* Free all entries */
    for (int i = 0; i < g_memory->count; i++) {
        free(g_memory->entries[i].content);
        free(g_memory->entries[i].embedding);
        free(g_memory->entries[i].context);
        free(g_memory->entries[i].source);
    }

    free(g_memory->entries);

    /* Close database */
    if (g_memory->db) {
        sqlite3_close(g_memory->db);
    }

    pthread_mutex_unlock(&g_memory->mutex);
    pthread_mutex_destroy(&g_memory->mutex);

    free(g_memory);
    g_memory = NULL;

    ANBS_DEBUG_LOG("Memory system cleaned up");
}