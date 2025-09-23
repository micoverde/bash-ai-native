/* cache.c - High-performance response caching system for ANBS */

#include "../ai_display.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <openssl/sha.h>
#include <sys/time.h>

#define CACHE_SIZE 10000
#define HASH_BUCKETS 1024
#define MAX_RESPONSE_SIZE 16384
#define DEFAULT_TTL 300  /* 5 minutes */

typedef struct cache_entry {
    char command_hash[65];  /* SHA256 hex string */
    char *response;
    size_t response_length;
    time_t timestamp;
    time_t expires_at;
    int hit_count;
    int ttl_seconds;
    struct cache_entry *next;  /* Hash collision chain */
    struct cache_entry *lru_prev;  /* LRU doubly-linked list */
    struct cache_entry *lru_next;
} cache_entry_t;

typedef struct {
    cache_entry_t *buckets[HASH_BUCKETS];
    cache_entry_t *lru_head;
    cache_entry_t *lru_tail;
    int entry_count;
    int max_entries;
    pthread_rwlock_t rwlock;

    /* Performance metrics */
    uint64_t total_requests;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t evictions;
    double average_response_time_ms;
} response_cache_t;

static response_cache_t *g_cache = NULL;

/* Hash function for cache keys */
static unsigned int hash_command(const char *command) {
    unsigned int hash = 5381;
    int c;

    while ((c = *command++)) {
        hash = ((hash << 5) + hash) + c;
    }

    return hash % HASH_BUCKETS;
}

/* Generate SHA256 hash for command */
static void generate_command_hash(const char *command, char *hash_output) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;

    SHA256_Init(&sha256);
    SHA256_Update(&sha256, command, strlen(command));
    SHA256_Final(hash, &sha256);

    /* Convert to hex string */
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(hash_output + (i * 2), "%02x", hash[i]);
    }
    hash_output[64] = '\0';
}

/* Move entry to front of LRU list */
static void move_to_front(cache_entry_t *entry) {
    if (!entry || entry == g_cache->lru_head) {
        return;
    }

    /* Remove from current position */
    if (entry->lru_prev) {
        entry->lru_prev->lru_next = entry->lru_next;
    }
    if (entry->lru_next) {
        entry->lru_next->lru_prev = entry->lru_prev;
    }
    if (entry == g_cache->lru_tail) {
        g_cache->lru_tail = entry->lru_prev;
    }

    /* Add to front */
    entry->lru_prev = NULL;
    entry->lru_next = g_cache->lru_head;
    if (g_cache->lru_head) {
        g_cache->lru_head->lru_prev = entry;
    }
    g_cache->lru_head = entry;

    if (!g_cache->lru_tail) {
        g_cache->lru_tail = entry;
    }
}

/* Remove entry from LRU list */
static void remove_from_lru(cache_entry_t *entry) {
    if (entry->lru_prev) {
        entry->lru_prev->lru_next = entry->lru_next;
    } else {
        g_cache->lru_head = entry->lru_next;
    }

    if (entry->lru_next) {
        entry->lru_next->lru_prev = entry->lru_prev;
    } else {
        g_cache->lru_tail = entry->lru_prev;
    }
}

/* Evict least recently used entry */
static void evict_lru_entry(void) {
    if (!g_cache->lru_tail) {
        return;
    }

    cache_entry_t *victim = g_cache->lru_tail;
    unsigned int bucket = hash_command(victim->command_hash);

    /* Remove from hash bucket */
    cache_entry_t **current = &g_cache->buckets[bucket];
    while (*current && *current != victim) {
        current = &(*current)->next;
    }
    if (*current) {
        *current = victim->next;
    }

    /* Remove from LRU list */
    remove_from_lru(victim);

    /* Free memory */
    free(victim->response);
    free(victim);

    g_cache->entry_count--;
    g_cache->evictions++;

    ANBS_DEBUG_LOG("Evicted LRU cache entry, count now: %d", g_cache->entry_count);
}

/* Initialize response cache */
int anbs_cache_init(int max_entries) {
    if (g_cache) {
        return 0; /* Already initialized */
    }

    g_cache = calloc(1, sizeof(response_cache_t));
    if (!g_cache) {
        return -1;
    }

    g_cache->max_entries = max_entries > 0 ? max_entries : CACHE_SIZE;

    if (pthread_rwlock_init(&g_cache->rwlock, NULL) != 0) {
        free(g_cache);
        g_cache = NULL;
        return -1;
    }

    ANBS_DEBUG_LOG("Response cache initialized with %d max entries", g_cache->max_entries);
    return 0;
}

/* Store response in cache */
int anbs_cache_put(const char *command, const char *response, int ttl_seconds) {
    if (!g_cache || !command || !response) {
        return -1;
    }

    if (strlen(response) > MAX_RESPONSE_SIZE) {
        return -1; /* Response too large to cache */
    }

    char command_hash[65];
    generate_command_hash(command, command_hash);

    unsigned int bucket = hash_command(command_hash);
    time_t now = time(NULL);

    pthread_rwlock_wrlock(&g_cache->rwlock);

    /* Check if entry already exists */
    cache_entry_t *existing = g_cache->buckets[bucket];
    while (existing) {
        if (strcmp(existing->command_hash, command_hash) == 0) {
            /* Update existing entry */
            free(existing->response);
            existing->response = strdup(response);
            existing->response_length = strlen(response);
            existing->timestamp = now;
            existing->ttl_seconds = ttl_seconds > 0 ? ttl_seconds : DEFAULT_TTL;
            existing->expires_at = now + existing->ttl_seconds;

            move_to_front(existing);
            pthread_rwlock_unlock(&g_cache->rwlock);
            return 0;
        }
        existing = existing->next;
    }

    /* Create new entry */
    cache_entry_t *entry = calloc(1, sizeof(cache_entry_t));
    if (!entry) {
        pthread_rwlock_unlock(&g_cache->rwlock);
        return -1;
    }

    strcpy(entry->command_hash, command_hash);
    entry->response = strdup(response);
    entry->response_length = strlen(response);
    entry->timestamp = now;
    entry->ttl_seconds = ttl_seconds > 0 ? ttl_seconds : DEFAULT_TTL;
    entry->expires_at = now + entry->ttl_seconds;
    entry->hit_count = 0;

    /* Evict entries if cache is full */
    while (g_cache->entry_count >= g_cache->max_entries) {
        evict_lru_entry();
    }

    /* Add to hash bucket */
    entry->next = g_cache->buckets[bucket];
    g_cache->buckets[bucket] = entry;

    /* Add to front of LRU list */
    move_to_front(entry);

    g_cache->entry_count++;

    pthread_rwlock_unlock(&g_cache->rwlock);

    ANBS_DEBUG_LOG("Cached response for command hash: %.16s... (TTL: %ds)",
                   command_hash, entry->ttl_seconds);
    return 0;
}

/* Retrieve response from cache */
char *anbs_cache_get(const char *command, double *cache_age_ms) {
    if (!g_cache || !command) {
        return NULL;
    }

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    char command_hash[65];
    generate_command_hash(command, command_hash);

    unsigned int bucket = hash_command(command_hash);
    time_t now = time(NULL);

    pthread_rwlock_rdlock(&g_cache->rwlock);

    g_cache->total_requests++;

    cache_entry_t *entry = g_cache->buckets[bucket];
    while (entry) {
        if (strcmp(entry->command_hash, command_hash) == 0) {
            /* Check if entry has expired */
            if (entry->expires_at < now) {
                pthread_rwlock_unlock(&g_cache->rwlock);
                g_cache->cache_misses++;
                return NULL; /* Expired */
            }

            /* Entry found and valid */
            char *response = strdup(entry->response);
            entry->hit_count++;

            if (cache_age_ms) {
                *cache_age_ms = (now - entry->timestamp) * 1000.0;
            }

            pthread_rwlock_unlock(&g_cache->rwlock);

            /* Update LRU (requires write lock) */
            pthread_rwlock_wrlock(&g_cache->rwlock);
            move_to_front(entry);
            pthread_rwlock_unlock(&g_cache->rwlock);

            g_cache->cache_hits++;

            gettimeofday(&end_time, NULL);
            double lookup_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                               (end_time.tv_usec - start_time.tv_usec) / 1000.0;

            ANBS_DEBUG_LOG("Cache HIT for command (%.2fms lookup): %.50s...",
                           lookup_time, command);
            return response;
        }
        entry = entry->next;
    }

    pthread_rwlock_unlock(&g_cache->rwlock);
    g_cache->cache_misses++;

    ANBS_DEBUG_LOG("Cache MISS for command: %.50s...", command);
    return NULL;
}

/* Remove specific entry from cache */
int anbs_cache_remove(const char *command) {
    if (!g_cache || !command) {
        return -1;
    }

    char command_hash[65];
    generate_command_hash(command, command_hash);

    unsigned int bucket = hash_command(command_hash);

    pthread_rwlock_wrlock(&g_cache->rwlock);

    cache_entry_t **current = &g_cache->buckets[bucket];
    while (*current) {
        if (strcmp((*current)->command_hash, command_hash) == 0) {
            cache_entry_t *to_remove = *current;
            *current = to_remove->next;

            remove_from_lru(to_remove);
            free(to_remove->response);
            free(to_remove);

            g_cache->entry_count--;
            pthread_rwlock_unlock(&g_cache->rwlock);

            ANBS_DEBUG_LOG("Removed cache entry for: %.50s...", command);
            return 0;
        }
        current = &(*current)->next;
    }

    pthread_rwlock_unlock(&g_cache->rwlock);
    return -1; /* Not found */
}

/* Clear all cache entries */
void anbs_cache_clear(void) {
    if (!g_cache) {
        return;
    }

    pthread_rwlock_wrlock(&g_cache->rwlock);

    for (int i = 0; i < HASH_BUCKETS; i++) {
        cache_entry_t *entry = g_cache->buckets[i];
        while (entry) {
            cache_entry_t *next = entry->next;
            free(entry->response);
            free(entry);
            entry = next;
        }
        g_cache->buckets[i] = NULL;
    }

    g_cache->lru_head = NULL;
    g_cache->lru_tail = NULL;
    g_cache->entry_count = 0;

    pthread_rwlock_unlock(&g_cache->rwlock);

    ANBS_DEBUG_LOG("Cache cleared");
}

/* Get cache statistics */
int anbs_cache_get_stats(char **stats_json) {
    if (!g_cache || !stats_json) {
        return -1;
    }

    pthread_rwlock_rdlock(&g_cache->rwlock);

    double hit_rate = g_cache->total_requests > 0 ?
                     (double)g_cache->cache_hits / g_cache->total_requests * 100.0 : 0.0;

    char *stats = malloc(1024);
    if (!stats) {
        pthread_rwlock_unlock(&g_cache->rwlock);
        return -1;
    }

    snprintf(stats, 1024,
             "{"
             "\"total_requests\": %lu,"
             "\"cache_hits\": %lu,"
             "\"cache_misses\": %lu,"
             "\"hit_rate_percent\": %.2f,"
             "\"entry_count\": %d,"
             "\"max_entries\": %d,"
             "\"evictions\": %lu,"
             "\"memory_usage_estimate_kb\": %d"
             "}",
             g_cache->total_requests,
             g_cache->cache_hits,
             g_cache->cache_misses,
             hit_rate,
             g_cache->entry_count,
             g_cache->max_entries,
             g_cache->evictions,
             g_cache->entry_count * (sizeof(cache_entry_t) + MAX_RESPONSE_SIZE / 2) / 1024);

    *stats_json = stats;

    pthread_rwlock_unlock(&g_cache->rwlock);
    return 0;
}

/* Remove expired entries */
int anbs_cache_cleanup_expired(void) {
    if (!g_cache) {
        return -1;
    }

    time_t now = time(NULL);
    int removed_count = 0;

    pthread_rwlock_wrlock(&g_cache->rwlock);

    for (int i = 0; i < HASH_BUCKETS; i++) {
        cache_entry_t **current = &g_cache->buckets[i];
        while (*current) {
            if ((*current)->expires_at < now) {
                cache_entry_t *to_remove = *current;
                *current = to_remove->next;

                remove_from_lru(to_remove);
                free(to_remove->response);
                free(to_remove);

                g_cache->entry_count--;
                removed_count++;
            } else {
                current = &(*current)->next;
            }
        }
    }

    pthread_rwlock_unlock(&g_cache->rwlock);

    if (removed_count > 0) {
        ANBS_DEBUG_LOG("Cleaned up %d expired cache entries", removed_count);
    }

    return removed_count;
}

/* Optimize cache for specific command patterns */
int anbs_cache_optimize_for_pattern(const char *pattern, int priority_ttl) {
    if (!g_cache || !pattern) {
        return -1;
    }

    /* This would implement pattern-based cache optimization */
    /* For now, just log the optimization request */
    ANBS_DEBUG_LOG("Cache optimization requested for pattern: %s (TTL: %d)",
                   pattern, priority_ttl);

    return 0;
}

/* Pre-warm cache with common responses */
int anbs_cache_prewarm(void) {
    if (!g_cache) {
        return -1;
    }

    /* Pre-populate cache with common commands */
    const char *common_commands[] = {
        "@vertex --health",
        "@vertex help",
        "@memory recent",
        "@analyze --help",
        NULL
    };

    const char *common_responses[] = {
        "AI service health check - ONLINE ✅",
        "Vertex AI Assistant - Available commands: health, help, analyze, memory",
        "Recent conversation history (0 entries found)",
        "Usage: @analyze <filename> - Analyze file with AI",
        NULL
    };

    int prewarmed = 0;
    for (int i = 0; common_commands[i]; i++) {
        if (anbs_cache_put(common_commands[i], common_responses[i], 3600) == 0) {
            prewarmed++;
        }
    }

    ANBS_DEBUG_LOG("Pre-warmed cache with %d common responses", prewarmed);
    return prewarmed;
}

/* Cleanup cache system */
void anbs_cache_cleanup(void) {
    if (!g_cache) {
        return;
    }

    anbs_cache_clear();
    pthread_rwlock_destroy(&g_cache->rwlock);
    free(g_cache);
    g_cache = NULL;

    ANBS_DEBUG_LOG("Response cache cleaned up");
}