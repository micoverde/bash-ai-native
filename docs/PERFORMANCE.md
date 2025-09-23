# ANBS Performance Guide

## Table of Contents
1. [Performance Overview](#performance-overview)
2. [Performance Targets](#performance-targets)
3. [Caching System](#caching-system)
4. [Memory Management](#memory-management)
5. [Thread Pool Optimization](#thread-pool-optimization)
6. [Network Optimization](#network-optimization)
7. [Database Performance](#database-performance)
8. [Display Optimization](#display-optimization)
9. [Monitoring and Metrics](#monitoring-and-metrics)
10. [Performance Tuning](#performance-tuning)
11. [Benchmarking](#benchmarking)
12. [Troubleshooting Performance Issues](#troubleshooting-performance-issues)

## Performance Overview

ANBS (AI-Native Bash Shell) is designed for high-performance AI interactions with sub-50ms response times for cached queries and optimized resource utilization. The performance architecture includes multi-level caching, intelligent memory management, optimized network communication, and real-time performance monitoring.

### Key Performance Features
- **Sub-50ms Response Times**: Aggressive caching and optimization for rapid AI interactions
- **Multi-level Caching**: SHA256-based response cache with LRU eviction
- **Thread Pool Management**: Dynamic worker thread allocation for parallel processing
- **Memory Optimization**: Efficient memory allocation with object pooling
- **Network Optimization**: Connection pooling and request batching
- **Real-time Monitoring**: Comprehensive performance metrics and alerting

### Performance Architecture
```
┌─────────────────────────────────────────────────────────────┐
│                    ANBS Performance Stack                   │
├─────────────────────────────────────────────────────────────┤
│  Application Layer: @vertex, @memory, @analyze commands    │
├─────────────────────────────────────────────────────────────┤
│  Caching Layer: SHA256 cache, Memory cache, Query cache    │
├─────────────────────────────────────────────────────────────┤
│  Processing Layer: Thread pools, Task queues, Load balancer│
├─────────────────────────────────────────────────────────────┤
│  Network Layer: Connection pools, Request batching, HTTP/2 │
├─────────────────────────────────────────────────────────────┤
│  Storage Layer: SQLite optimization, Memory mapping, SSD   │
├─────────────────────────────────────────────────────────────┤
│  System Layer: Memory management, CPU scheduling, I/O      │
└─────────────────────────────────────────────────────────────┘
```

## Performance Targets

### Response Time Targets
| Operation | Target | Acceptable | Critical |
|-----------|--------|------------|----------|
| Cached AI Query | < 10ms | < 50ms | < 100ms |
| Uncached AI Query | < 2000ms | < 5000ms | < 10000ms |
| Memory Search | < 20ms | < 100ms | < 500ms |
| File Analysis | < 500ms | < 2000ms | < 5000ms |
| Display Refresh | < 16ms | < 33ms | < 100ms |
| Database Query | < 5ms | < 20ms | < 100ms |

### Throughput Targets
| Metric | Target | Acceptable | Critical |
|--------|--------|------------|----------|
| AI Queries/sec | > 100 | > 50 | > 10 |
| Memory Searches/sec | > 1000 | > 500 | > 100 |
| Display Updates/sec | > 60 | > 30 | > 10 |
| Cache Hit Rate | > 80% | > 60% | > 40% |

### Resource Utilization Targets
| Resource | Target | Acceptable | Critical |
|----------|--------|------------|----------|
| CPU Usage | < 30% | < 60% | < 90% |
| Memory Usage | < 100MB | < 500MB | < 1GB |
| Disk I/O | < 10MB/s | < 50MB/s | < 100MB/s |
| Network I/O | < 1MB/s | < 10MB/s | < 50MB/s |

## Caching System

### Multi-Level Cache Architecture

ANBS implements a sophisticated multi-level caching system for optimal performance:

#### L1 Cache: In-Memory Response Cache
```c
typedef struct response_cache {
    cache_entry_t *buckets[HASH_BUCKETS];
    cache_entry_t *lru_head;
    cache_entry_t *lru_tail;
    int entry_count;
    int max_entries;
    pthread_rwlock_t rwlock;

    // Performance metrics
    uint64_t total_requests;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t evictions;
    double average_response_time_ms;
} response_cache_t;
```

#### Cache Entry Structure
```c
typedef struct cache_entry {
    char command_hash[65];        // SHA256 hex string
    char *response;               // Cached response
    size_t response_length;       // Response size
    time_t timestamp;             // Creation time
    time_t expires_at;            // Expiration time
    int hit_count;                // Access frequency
    int ttl_seconds;              // Time to live
    struct cache_entry *next;     // Hash collision chain
    struct cache_entry *lru_prev; // LRU doubly-linked list
    struct cache_entry *lru_next;
} cache_entry_t;
```

### Cache Performance Optimization

#### SHA256-Based Cache Keys
```c
static void generate_command_hash(const char *command, char *hash_output) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;

    SHA256_Init(&sha256);
    SHA256_Update(&sha256, command, strlen(command));
    SHA256_Final(hash, &sha256);

    // Convert to hex string for fast comparison
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(hash_output + (i * 2), "%02x", hash[i]);
    }
    hash_output[64] = '\0';
}
```

#### LRU Eviction with Hot/Cold Separation
```c
static void move_to_front(cache_entry_t *entry) {
    if (!entry || entry == g_cache->lru_head) {
        return;
    }

    // Remove from current position
    if (entry->lru_prev) {
        entry->lru_prev->lru_next = entry->lru_next;
    }
    if (entry->lru_next) {
        entry->lru_next->lru_prev = entry->lru_prev;
    }
    if (entry == g_cache->lru_tail) {
        g_cache->lru_tail = entry->lru_prev;
    }

    // Add to front (hot section)
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
```

### Cache Performance Metrics
```c
typedef struct cache_stats {
    uint64_t total_requests;
    uint64_t cache_hits;
    uint64_t cache_misses;
    double hit_rate_percent;
    int entry_count;
    int max_entries;
    uint64_t evictions;
    int memory_usage_kb;
    double avg_lookup_time_ms;
    double avg_insertion_time_ms;
} cache_stats_t;

int anbs_cache_get_detailed_stats(cache_stats_t *stats) {
    if (!g_cache || !stats) {
        return -1;
    }

    pthread_rwlock_rdlock(&g_cache->rwlock);

    stats->total_requests = g_cache->total_requests;
    stats->cache_hits = g_cache->cache_hits;
    stats->cache_misses = g_cache->cache_misses;
    stats->hit_rate_percent = g_cache->total_requests > 0 ?
        (double)g_cache->cache_hits / g_cache->total_requests * 100.0 : 0.0;
    stats->entry_count = g_cache->entry_count;
    stats->max_entries = g_cache->max_entries;
    stats->evictions = g_cache->evictions;
    stats->memory_usage_kb = estimate_cache_memory_usage() / 1024;

    pthread_rwlock_unlock(&g_cache->rwlock);
    return 0;
}
```

## Memory Management

### Object Pooling for Frequent Allocations

```c
typedef struct object_pool {
    void **objects;              // Pool of objects
    int count;                   // Current count
    int capacity;                // Maximum capacity
    size_t object_size;          // Size of each object
    pthread_mutex_t mutex;       // Thread safety
    void (*init_object)(void *); // Object initializer
    void (*cleanup_object)(void *); // Object cleanup
} object_pool_t;

// Pool operations
void *pool_alloc(object_pool_t *pool) {
    pthread_mutex_lock(&pool->mutex);

    void *obj = NULL;
    if (pool->count > 0) {
        // Reuse existing object
        obj = pool->objects[--pool->count];
    } else {
        // Allocate new object
        obj = malloc(pool->object_size);
        if (obj && pool->init_object) {
            pool->init_object(obj);
        }
    }

    pthread_mutex_unlock(&pool->mutex);
    return obj;
}

void pool_free(object_pool_t *pool, void *obj) {
    if (!obj) return;

    pthread_mutex_lock(&pool->mutex);

    if (pool->count < pool->capacity) {
        // Return to pool
        if (pool->cleanup_object) {
            pool->cleanup_object(obj);
        }
        pool->objects[pool->count++] = obj;
    } else {
        // Pool full, actually free
        free(obj);
    }

    pthread_mutex_unlock(&pool->mutex);
}
```

### Memory-Mapped Files for Large Data
```c
typedef struct memory_mapped_file {
    void *data;
    size_t size;
    int fd;
    char *filename;
} mmap_file_t;

int anbs_mmap_file(const char *filename, mmap_file_t *mmap_info) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        return -1;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        return -1;
    }

    void *data = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return -1;
    }

    // Advise kernel about access pattern
    madvise(data, sb.st_size, MADV_SEQUENTIAL);

    mmap_info->data = data;
    mmap_info->size = sb.st_size;
    mmap_info->fd = fd;
    mmap_info->filename = strdup(filename);

    return 0;
}
```

### Memory Usage Monitoring
```c
typedef struct memory_stats {
    size_t heap_allocated;
    size_t heap_freed;
    size_t current_usage;
    size_t peak_usage;
    int allocation_count;
    int free_count;
    double fragmentation_ratio;
} memory_stats_t;

static memory_stats_t g_memory_stats = {0};
static pthread_mutex_t g_memory_mutex = PTHREAD_MUTEX_INITIALIZER;

void *anbs_malloc_tracked(size_t size) {
    void *ptr = malloc(size);
    if (ptr) {
        pthread_mutex_lock(&g_memory_mutex);
        g_memory_stats.heap_allocated += size;
        g_memory_stats.current_usage += size;
        g_memory_stats.allocation_count++;

        if (g_memory_stats.current_usage > g_memory_stats.peak_usage) {
            g_memory_stats.peak_usage = g_memory_stats.current_usage;
        }

        pthread_mutex_unlock(&g_memory_mutex);
    }
    return ptr;
}
```

## Thread Pool Optimization

### Dynamic Thread Pool Management

```c
typedef struct thread_pool {
    pthread_t *threads;          // Worker threads
    int thread_count;            // Current thread count
    int max_threads;             // Maximum threads
    int min_threads;             // Minimum threads

    // Task queue
    task_t *task_queue;
    int queue_size;
    int queue_capacity;
    int queue_head;
    int queue_tail;

    // Synchronization
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_not_empty;
    pthread_cond_t queue_not_full;

    // Statistics
    uint64_t tasks_completed;
    uint64_t tasks_queued;
    double avg_task_time_ms;

    bool shutdown;
} thread_pool_t;
```

### Adaptive Thread Scaling
```c
void *thread_pool_monitor(void *arg) {
    thread_pool_t *pool = (thread_pool_t *)arg;

    while (!pool->shutdown) {
        sleep(THREAD_POOL_MONITOR_INTERVAL);

        pthread_mutex_lock(&pool->queue_mutex);

        int queue_length = pool->queue_size;
        int active_threads = pool->thread_count;

        // Scale up if queue is growing
        if (queue_length > active_threads * 2 &&
            active_threads < pool->max_threads) {

            add_worker_thread(pool);
            ANBS_DEBUG_LOG("Scaled up thread pool to %d threads",
                          pool->thread_count);
        }

        // Scale down if threads are idle
        else if (queue_length == 0 &&
                 active_threads > pool->min_threads) {

            remove_worker_thread(pool);
            ANBS_DEBUG_LOG("Scaled down thread pool to %d threads",
                          pool->thread_count);
        }

        pthread_mutex_unlock(&pool->queue_mutex);
    }

    return NULL;
}
```

### Work-Stealing Task Distribution
```c
typedef struct work_stealing_queue {
    task_t *tasks;
    int capacity;
    volatile int top;    // Owner thread access
    volatile int bottom; // Stealer thread access
    pthread_mutex_t mutex;
} ws_queue_t;

task_t *steal_task(ws_queue_t *queue) {
    pthread_mutex_lock(&queue->mutex);

    int bottom = queue->bottom;
    int top = queue->top;

    if (bottom <= top) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL; // Queue empty
    }

    task_t *task = queue->tasks[bottom];
    queue->bottom = bottom + 1;

    pthread_mutex_unlock(&queue->mutex);
    return task;
}
```

## Network Optimization

### Connection Pooling

```c
typedef struct connection_pool {
    connection_t *connections;
    int pool_size;
    int active_connections;
    int max_connections;
    pthread_mutex_t pool_mutex;
    pthread_cond_t connection_available;

    // Performance metrics
    uint64_t requests_served;
    double avg_response_time_ms;
    int connection_reuse_count;
} connection_pool_t;

connection_t *get_pooled_connection(connection_pool_t *pool, const char *host) {
    pthread_mutex_lock(&pool->pool_mutex);

    // Find available connection
    for (int i = 0; i < pool->pool_size; i++) {
        connection_t *conn = &pool->connections[i];

        if (!conn->in_use && strcmp(conn->host, host) == 0) {
            // Check if connection is still alive
            if (is_connection_alive(conn)) {
                conn->in_use = true;
                conn->last_used = time(NULL);
                pthread_mutex_unlock(&pool->pool_mutex);
                return conn;
            } else {
                // Connection dead, mark for reconnection
                close_connection(conn);
            }
        }
    }

    // No available connection, create new one
    connection_t *new_conn = create_new_connection(host);
    if (new_conn) {
        add_to_pool(pool, new_conn);
    }

    pthread_mutex_unlock(&pool->pool_mutex);
    return new_conn;
}
```

### Request Batching and Pipelining
```c
typedef struct request_batch {
    http_request_t *requests[MAX_BATCH_SIZE];
    int request_count;
    time_t batch_start_time;
    pthread_mutex_t batch_mutex;
} request_batch_t;

int add_to_batch(request_batch_t *batch, http_request_t *request) {
    pthread_mutex_lock(&batch->batch_mutex);

    if (batch->request_count >= MAX_BATCH_SIZE) {
        pthread_mutex_unlock(&batch->batch_mutex);
        return -1; // Batch full
    }

    batch->requests[batch->request_count++] = request;

    // Trigger batch processing if full or timeout
    time_t now = time(NULL);
    if (batch->request_count >= MAX_BATCH_SIZE ||
        (now - batch->batch_start_time) >= BATCH_TIMEOUT_SECONDS) {

        process_request_batch(batch);
    }

    pthread_mutex_unlock(&batch->batch_mutex);
    return 0;
}
```

### HTTP/2 and Keep-Alive Optimization
```c
typedef struct http_config {
    bool use_http2;
    bool use_keepalive;
    int keepalive_timeout;
    int max_requests_per_connection;
    int tcp_nodelay;
    int socket_buffer_size;
} http_config_t;

CURL *create_optimized_curl_handle(const http_config_t *config) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    // HTTP/2 support
    if (config->use_http2) {
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    }

    // Keep-alive settings
    if (config->use_keepalive) {
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, config->keepalive_timeout);
    }

    // TCP optimization
    if (config->tcp_nodelay) {
        curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
    }

    // Buffer size optimization
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, config->socket_buffer_size);

    return curl;
}
```

## Database Performance

### SQLite Optimization

```c
int optimize_sqlite_database(sqlite3 *db) {
    const char *optimization_pragmas[] = {
        "PRAGMA journal_mode = WAL",           // Write-Ahead Logging
        "PRAGMA synchronous = NORMAL",         // Balanced durability/performance
        "PRAGMA cache_size = 10000",           // 10MB cache
        "PRAGMA temp_store = MEMORY",          // Temp tables in memory
        "PRAGMA mmap_size = 268435456",        // 256MB memory-mapped I/O
        "PRAGMA optimize",                     // Optimize statistics
        NULL
    };

    for (int i = 0; optimization_pragmas[i]; i++) {
        char *err_msg = NULL;
        int rc = sqlite3_exec(db, optimization_pragmas[i], NULL, NULL, &err_msg);

        if (rc != SQLITE_OK) {
            ANBS_DEBUG_LOG("SQLite optimization failed: %s", err_msg);
            sqlite3_free(err_msg);
            return -1;
        }
    }

    return 0;
}
```

### Prepared Statement Caching
```c
typedef struct statement_cache {
    sqlite3_stmt **statements;
    char **sql_queries;
    int cache_size;
    int current_count;
    pthread_mutex_t cache_mutex;
} statement_cache_t;

sqlite3_stmt *get_cached_statement(statement_cache_t *cache, const char *sql) {
    pthread_mutex_lock(&cache->cache_mutex);

    // Look for cached statement
    for (int i = 0; i < cache->current_count; i++) {
        if (strcmp(cache->sql_queries[i], sql) == 0) {
            sqlite3_stmt *stmt = cache->statements[i];
            sqlite3_reset(stmt); // Reset for reuse
            pthread_mutex_unlock(&cache->cache_mutex);
            return stmt;
        }
    }

    // Not found, prepare new statement
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_memory_db, sql, -1, &stmt, NULL);

    if (rc == SQLITE_OK && cache->current_count < cache->cache_size) {
        // Cache the new statement
        cache->statements[cache->current_count] = stmt;
        cache->sql_queries[cache->current_count] = strdup(sql);
        cache->current_count++;
    }

    pthread_mutex_unlock(&cache->cache_mutex);
    return rc == SQLITE_OK ? stmt : NULL;
}
```

### Async Database Operations
```c
typedef struct async_db_operation {
    sqlite3 *db;
    char *sql;
    void *params;
    void (*callback)(void *result, void *user_data);
    void *user_data;
} async_db_op_t;

void *async_database_worker(void *arg) {
    thread_pool_t *pool = (thread_pool_t *)arg;

    while (!pool->shutdown) {
        async_db_op_t *op = get_next_db_operation(pool);
        if (!op) continue;

        // Execute database operation
        struct timeval start, end;
        gettimeofday(&start, NULL);

        void *result = execute_db_operation(op);

        gettimeofday(&end, NULL);
        double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +
                        (end.tv_usec - start.tv_usec) / 1000.0;

        // Update performance metrics
        update_db_performance_metrics(elapsed);

        // Call completion callback
        if (op->callback) {
            op->callback(result, op->user_data);
        }

        free_db_operation(op);
    }

    return NULL;
}
```

## Display Optimization

### Efficient NCurses Operations

```c
typedef struct display_buffer {
    chtype *buffer;              // Character buffer
    int width, height;           // Dimensions
    bool dirty_lines[MAX_LINES]; // Track changed lines
    int dirty_count;             // Number of dirty lines
    pthread_mutex_t buffer_mutex;
} display_buffer_t;

// Optimize screen updates by tracking dirty regions
int anbs_display_refresh_optimized(anbs_display_t *display) {
    pthread_mutex_lock(&display->buffer_mutex);

    if (display->dirty_count == 0) {
        pthread_mutex_unlock(&display->buffer_mutex);
        return 0; // Nothing to update
    }

    // Update only dirty lines
    for (int line = 0; line < display->height; line++) {
        if (display->dirty_lines[line]) {
            // Copy line to NCurses window
            mvwaddchnstr(display->main_screen, line, 0,
                        &display->buffer[line * display->width],
                        display->width);
            display->dirty_lines[line] = false;
        }
    }

    display->dirty_count = 0;

    // Single refresh call for all changes
    wrefresh(display->main_screen);

    pthread_mutex_unlock(&display->buffer_mutex);
    return 0;
}
```

### Double Buffering for Smooth Updates
```c
typedef struct double_buffer {
    display_buffer_t front_buffer;
    display_buffer_t back_buffer;
    bool swap_pending;
    pthread_mutex_t swap_mutex;
} double_buffer_t;

void swap_display_buffers(double_buffer_t *db) {
    pthread_mutex_lock(&db->swap_mutex);

    // Swap buffer pointers
    display_buffer_t temp = db->front_buffer;
    db->front_buffer = db->back_buffer;
    db->back_buffer = temp;

    db->swap_pending = false;

    pthread_mutex_unlock(&db->swap_mutex);

    // Update display with new front buffer
    update_display_from_buffer(&db->front_buffer);
}
```

### Text Rendering Optimization
```c
// Cache commonly used text attributes
static int color_pairs_cache[256];
static bool color_cache_initialized = false;

int get_cached_color_pair(text_color_t color) {
    if (!color_cache_initialized) {
        initialize_color_cache();
        color_cache_initialized = true;
    }

    if (color < 256) {
        return color_pairs_cache[color];
    }

    return COLOR_PAIR(0); // Default
}

// Batch text operations for efficiency
void batch_text_output(panel_t *panel, text_line_t *lines, int line_count) {
    WINDOW *win = panel->window;

    for (int i = 0; i < line_count; i++) {
        text_line_t *line = &lines[i];

        // Set attributes once per line
        wattrset(win, get_cached_color_pair(line->color) | line->attributes);

        // Output entire line at once
        mvwaddnstr(win, line->y, line->x, line->text, line->length);
    }

    // Single refresh for all lines
    wrefresh(win);
}
```

## Monitoring and Metrics

### Real-time Performance Monitoring

```c
typedef struct performance_metrics {
    // Response times
    double ai_query_avg_ms;
    double ai_query_p95_ms;
    double ai_query_p99_ms;

    // Throughput
    uint64_t queries_per_second;
    uint64_t cache_hits_per_second;
    uint64_t display_updates_per_second;

    // Resource utilization
    double cpu_usage_percent;
    size_t memory_usage_bytes;
    uint64_t disk_io_bytes_per_sec;
    uint64_t network_io_bytes_per_sec;

    // Error rates
    double error_rate_percent;
    uint64_t timeout_count;
    uint64_t network_error_count;

    time_t last_updated;
} performance_metrics_t;

static performance_metrics_t g_metrics = {0};
static pthread_mutex_t g_metrics_mutex = PTHREAD_MUTEX_INITIALIZER;
```

### Metrics Collection System
```c
typedef struct metric_collector {
    char name[64];
    metric_type_t type;
    double value;
    time_t timestamp;
    uint64_t sample_count;
    double min_value;
    double max_value;
    double sum_value;
    double sum_squares; // For standard deviation
} metric_collector_t;

void update_metric(const char *metric_name, double value) {
    metric_collector_t *metric = find_or_create_metric(metric_name);

    pthread_mutex_lock(&g_metrics_mutex);

    metric->value = value;
    metric->timestamp = time(NULL);
    metric->sample_count++;

    // Update statistics
    if (metric->sample_count == 1) {
        metric->min_value = metric->max_value = value;
    } else {
        if (value < metric->min_value) metric->min_value = value;
        if (value > metric->max_value) metric->max_value = value;
    }

    metric->sum_value += value;
    metric->sum_squares += value * value;

    pthread_mutex_unlock(&g_metrics_mutex);
}

double get_metric_average(const char *metric_name) {
    metric_collector_t *metric = find_metric(metric_name);
    if (!metric || metric->sample_count == 0) {
        return 0.0;
    }

    return metric->sum_value / metric->sample_count;
}
```

### Performance Dashboard
```c
void display_performance_dashboard(anbs_display_t *display) {
    performance_metrics_t metrics;
    get_current_metrics(&metrics);

    int health_panel = get_health_panel_id(display);

    // Clear previous content
    anbs_display_clear_panel(display, health_panel);

    // Display key metrics
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "CPU: %s %5.1f%%",
             get_bar_graph(metrics.cpu_usage_percent, 100.0),
             metrics.cpu_usage_percent);
    anbs_display_add_text(display, health_panel, buffer, ANBS_COLOR_INFO);

    snprintf(buffer, sizeof(buffer), "Memory: %s %5ldMB",
             get_bar_graph(metrics.memory_usage_bytes, 1024*1024*1024),
             metrics.memory_usage_bytes / (1024*1024));
    anbs_display_add_text(display, health_panel, buffer, ANBS_COLOR_INFO);

    snprintf(buffer, sizeof(buffer), "AI Avg: %6.1fms | Cache Hit: %5.1f%%",
             metrics.ai_query_avg_ms,
             get_cache_hit_rate());
    anbs_display_add_text(display, health_panel, buffer, ANBS_COLOR_SUCCESS);

    snprintf(buffer, sizeof(buffer), "QPS: %4ld | Errors: %5.1f%%",
             metrics.queries_per_second,
             metrics.error_rate_percent);

    text_color_t qps_color = metrics.queries_per_second > 50 ?
                            ANBS_COLOR_SUCCESS : ANBS_COLOR_WARNING;
    anbs_display_add_text(display, health_panel, buffer, qps_color);
}
```

## Performance Tuning

### Automatic Performance Tuning

```c
typedef struct tuning_parameters {
    int cache_size;
    int thread_pool_size;
    int database_cache_size;
    int network_timeout_ms;
    int batch_size;
    bool enable_compression;
} tuning_parameters_t;

void auto_tune_performance(void) {
    performance_metrics_t metrics;
    get_current_metrics(&metrics);

    tuning_parameters_t params;
    get_current_tuning_parameters(&params);

    // Tune cache size based on hit rate
    if (get_cache_hit_rate() < 60.0 && params.cache_size < MAX_CACHE_SIZE) {
        params.cache_size = (int)(params.cache_size * 1.5);
        anbs_cache_resize(params.cache_size);
        ANBS_DEBUG_LOG("Increased cache size to %d", params.cache_size);
    }

    // Tune thread pool based on queue length
    int queue_length = get_thread_pool_queue_length();
    if (queue_length > params.thread_pool_size * 2) {
        params.thread_pool_size = min(params.thread_pool_size + 2, MAX_THREADS);
        resize_thread_pool(params.thread_pool_size);
        ANBS_DEBUG_LOG("Increased thread pool to %d", params.thread_pool_size);
    }

    // Tune batch size based on latency
    if (metrics.ai_query_avg_ms > 1000 && params.batch_size < MAX_BATCH_SIZE) {
        params.batch_size = min(params.batch_size + 5, MAX_BATCH_SIZE);
        set_request_batch_size(params.batch_size);
        ANBS_DEBUG_LOG("Increased batch size to %d", params.batch_size);
    }

    save_tuning_parameters(&params);
}
```

### Configuration-Based Tuning
```yaml
# /etc/anbs/performance.yaml
performance:
  cache:
    max_entries: 10000
    default_ttl: 300
    cleanup_interval: 3600
    memory_limit_mb: 100

  threading:
    min_threads: 2
    max_threads: 16
    queue_size: 1000
    idle_timeout: 30

  database:
    cache_size_kb: 10240
    journal_mode: "WAL"
    synchronous: "NORMAL"
    temp_store: "memory"

  network:
    connection_pool_size: 10
    request_timeout_ms: 5000
    keepalive_timeout: 30
    enable_http2: true
    enable_compression: true

  monitoring:
    metrics_interval: 5
    dashboard_refresh_rate: 10
    log_slow_queries: true
    slow_query_threshold_ms: 1000
```

### Manual Performance Controls
```c
// Performance control commands
int anbs_perf_set_cache_size(int size) {
    if (size < MIN_CACHE_SIZE || size > MAX_CACHE_SIZE) {
        return -1;
    }

    return anbs_cache_resize(size);
}

int anbs_perf_set_thread_count(int count) {
    if (count < 1 || count > MAX_THREADS) {
        return -1;
    }

    return resize_thread_pool(count);
}

int anbs_perf_enable_compression(bool enable) {
    return set_compression_enabled(enable);
}

int anbs_perf_set_timeout(int timeout_ms) {
    if (timeout_ms < 100 || timeout_ms > 60000) {
        return -1;
    }

    return set_network_timeout(timeout_ms);
}
```

## Benchmarking

### Performance Benchmark Suite

```c
typedef struct benchmark_result {
    char test_name[64];
    double avg_time_ms;
    double min_time_ms;
    double max_time_ms;
    double p95_time_ms;
    double p99_time_ms;
    uint64_t iterations;
    uint64_t errors;
    double throughput_ops_per_sec;
} benchmark_result_t;

// AI Query Performance Benchmark
benchmark_result_t benchmark_ai_queries(int iterations) {
    benchmark_result_t result = {0};
    strcpy(result.test_name, "AI Query Performance");

    double *times = malloc(iterations * sizeof(double));

    for (int i = 0; i < iterations; i++) {
        struct timeval start, end;
        gettimeofday(&start, NULL);

        char *response = NULL;
        int ret = send_ai_query("What is 2+2?", "gpt-4", 5000, &response);

        gettimeofday(&end, NULL);
        double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +
                        (end.tv_usec - start.tv_usec) / 1000.0;

        times[i] = elapsed;

        if (ret != 0) {
            result.errors++;
        }

        free(response);
    }

    // Calculate statistics
    calculate_benchmark_stats(times, iterations, &result);
    free(times);

    return result;
}

// Cache Performance Benchmark
benchmark_result_t benchmark_cache_performance(int iterations) {
    benchmark_result_t result = {0};
    strcpy(result.test_name, "Cache Performance");

    // Pre-populate cache
    for (int i = 0; i < 1000; i++) {
        char command[64];
        snprintf(command, sizeof(command), "test command %d", i);
        anbs_cache_put(command, "test response", 3600);
    }

    double *times = malloc(iterations * sizeof(double));

    for (int i = 0; i < iterations; i++) {
        char command[64];
        snprintf(command, sizeof(command), "test command %d", i % 1000);

        struct timeval start, end;
        gettimeofday(&start, NULL);

        double cache_age;
        char *response = anbs_cache_get(command, &cache_age);

        gettimeofday(&end, NULL);
        double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +
                        (end.tv_usec - start.tv_usec) / 1000.0;

        times[i] = elapsed;

        if (!response) {
            result.errors++;
        } else {
            free(response);
        }
    }

    calculate_benchmark_stats(times, iterations, &result);
    free(times);

    return result;
}
```

### Stress Testing
```c
typedef struct stress_test_config {
    int concurrent_users;
    int duration_seconds;
    int queries_per_user_per_second;
    bool enable_cache;
    bool mixed_workload;
} stress_test_config_t;

void run_stress_test(const stress_test_config_t *config) {
    printf("Starting stress test:\n");
    printf("  Concurrent users: %d\n", config->concurrent_users);
    printf("  Duration: %d seconds\n", config->duration_seconds);
    printf("  QPS per user: %d\n", config->queries_per_user_per_second);

    // Create user threads
    pthread_t *user_threads = malloc(config->concurrent_users * sizeof(pthread_t));
    stress_user_data_t *user_data = malloc(config->concurrent_users * sizeof(stress_user_data_t));

    time_t start_time = time(NULL);

    for (int i = 0; i < config->concurrent_users; i++) {
        user_data[i].user_id = i;
        user_data[i].config = *config;
        user_data[i].start_time = start_time;

        pthread_create(&user_threads[i], NULL, stress_test_user, &user_data[i]);
    }

    // Wait for test completion
    for (int i = 0; i < config->concurrent_users; i++) {
        pthread_join(user_threads[i], NULL);
    }

    // Collect and report results
    report_stress_test_results(user_data, config->concurrent_users);

    free(user_threads);
    free(user_data);
}
```

### Load Testing Framework
```c
void *load_test_worker(void *arg) {
    load_test_params_t *params = (load_test_params_t *)arg;

    while (!params->stop_flag) {
        // Generate random query
        char query[256];
        generate_random_query(query, sizeof(query));

        // Measure response time
        struct timeval start, end;
        gettimeofday(&start, NULL);

        char *response = NULL;
        int result = send_ai_query(query, "gpt-4", 5000, &response);

        gettimeofday(&end, NULL);
        double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +
                        (end.tv_usec - start.tv_usec) / 1000.0;

        // Record metrics
        record_load_test_metric(params->thread_id, elapsed, result == 0);

        free(response);

        // Wait for next iteration
        usleep(params->interval_microseconds);
    }

    return NULL;
}
```

## Troubleshooting Performance Issues

### Common Performance Problems

#### High Latency Diagnosis
```c
typedef struct latency_breakdown {
    double network_time_ms;
    double processing_time_ms;
    double database_time_ms;
    double cache_lookup_time_ms;
    double total_time_ms;
} latency_breakdown_t;

void diagnose_high_latency(const char *query, latency_breakdown_t *breakdown) {
    struct timeval start, network_start, processing_start, db_start, cache_start, end;

    gettimeofday(&start, NULL);

    // Cache lookup timing
    gettimeofday(&cache_start, NULL);
    char *cached_response = anbs_cache_get(query, NULL);
    gettimeofday(&network_start, NULL);
    breakdown->cache_lookup_time_ms = time_diff_ms(&cache_start, &network_start);

    if (cached_response) {
        breakdown->network_time_ms = 0;
        breakdown->processing_time_ms = 0;
        breakdown->database_time_ms = 0;
        free(cached_response);
    } else {
        // Network timing
        char *response = NULL;
        int result = send_ai_query_timed(query, "gpt-4", 5000, &response,
                                        &breakdown->network_time_ms);

        // Processing timing
        gettimeofday(&processing_start, NULL);
        process_ai_response(response);
        gettimeofday(&db_start, NULL);
        breakdown->processing_time_ms = time_diff_ms(&processing_start, &db_start);

        // Database timing
        store_response_in_memory(query, response);
        gettimeofday(&end, NULL);
        breakdown->database_time_ms = time_diff_ms(&db_start, &end);

        free(response);
    }

    gettimeofday(&end, NULL);
    breakdown->total_time_ms = time_diff_ms(&start, &end);
}
```

#### Memory Leak Detection
```c
void detect_memory_leaks(void) {
    memory_stats_t stats;
    get_memory_stats(&stats);

    if (stats.current_usage > stats.peak_usage * 0.9) {
        ANBS_DEBUG_LOG("Warning: Memory usage near peak (%ld bytes)",
                       stats.current_usage);
    }

    // Check for allocation/free imbalance
    if (stats.allocation_count > stats.free_count + 1000) {
        ANBS_DEBUG_LOG("Warning: Potential memory leak detected "
                       "(allocs: %d, frees: %d)",
                       stats.allocation_count, stats.free_count);

        // Trigger memory dump for analysis
        dump_memory_allocations("/tmp/anbs_memory_dump.txt");
    }
}
```

#### Performance Regression Detection
```c
typedef struct performance_baseline {
    double ai_query_baseline_ms;
    double cache_hit_rate_baseline;
    double memory_usage_baseline_mb;
    time_t baseline_timestamp;
} performance_baseline_t;

bool detect_performance_regression(void) {
    performance_baseline_t baseline;
    load_performance_baseline(&baseline);

    performance_metrics_t current;
    get_current_metrics(&current);

    bool regression_detected = false;

    // Check AI query performance
    if (current.ai_query_avg_ms > baseline.ai_query_baseline_ms * 1.5) {
        ANBS_DEBUG_LOG("Performance regression: AI query time increased from "
                       "%.1fms to %.1fms",
                       baseline.ai_query_baseline_ms, current.ai_query_avg_ms);
        regression_detected = true;
    }

    // Check cache hit rate
    double current_hit_rate = get_cache_hit_rate();
    if (current_hit_rate < baseline.cache_hit_rate_baseline * 0.8) {
        ANBS_DEBUG_LOG("Performance regression: Cache hit rate decreased from "
                       "%.1f%% to %.1f%%",
                       baseline.cache_hit_rate_baseline, current_hit_rate);
        regression_detected = true;
    }

    return regression_detected;
}
```

### Performance Optimization Recommendations

#### Automated Performance Advisor
```c
typedef struct performance_recommendation {
    char description[256];
    int priority; // 1-10, 10 being highest
    double estimated_improvement_percent;
    bool requires_restart;
} performance_recommendation_t;

void generate_performance_recommendations(performance_recommendation_t **recommendations,
                                        int *count) {
    performance_metrics_t metrics;
    get_current_metrics(&metrics);

    *recommendations = malloc(10 * sizeof(performance_recommendation_t));
    *count = 0;

    // Cache size recommendation
    if (get_cache_hit_rate() < 70.0) {
        performance_recommendation_t *rec = &(*recommendations)[(*count)++];
        strcpy(rec->description, "Increase cache size to improve hit rate");
        rec->priority = 8;
        rec->estimated_improvement_percent = 25.0;
        rec->requires_restart = false;
    }

    // Thread pool recommendation
    int avg_queue_length = get_average_queue_length();
    if (avg_queue_length > 10) {
        performance_recommendation_t *rec = &(*recommendations)[(*count)++];
        strcpy(rec->description, "Increase thread pool size to reduce queue length");
        rec->priority = 7;
        rec->estimated_improvement_percent = 15.0;
        rec->requires_restart = false;
    }

    // Memory optimization recommendation
    if (metrics.memory_usage_bytes > 500 * 1024 * 1024) {
        performance_recommendation_t *rec = &(*recommendations)[(*count)++];
        strcpy(rec->description, "Enable memory compression to reduce usage");
        rec->priority = 5;
        rec->estimated_improvement_percent = 20.0;
        rec->requires_restart = true;
    }
}
```

This comprehensive performance guide provides the foundation for optimizing ANBS performance across all components, from caching and memory management to network optimization and real-time monitoring.