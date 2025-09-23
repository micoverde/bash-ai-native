# ANBS API Reference

## Table of Contents
1. [Core Display API](#core-display-api)
2. [Memory System API](#memory-system-api)
3. [AI Commands API](#ai-commands-api)
4. [WebSocket Client API](#websocket-client-api)
5. [Distributed AI API](#distributed-ai-api)
6. [Security API](#security-api)
7. [Performance API](#performance-api)
8. [Utility Functions](#utility-functions)
9. [Data Structures](#data-structures)
10. [Error Codes](#error-codes)

## Core Display API

### Display Management

#### `anbs_display_init`
```c
int anbs_display_init(anbs_display_t **display);
```
**Description**: Initialize the ANBS display system with NCurses.

**Parameters**:
- `display`: Pointer to display structure pointer (allocated by function)

**Returns**:
- `0`: Success
- `-1`: Initialization failed
- `-2`: NCurses initialization failed
- `-3`: Memory allocation failed

**Example**:
```c
anbs_display_t *display = NULL;
if (anbs_display_init(&display) != 0) {
    fprintf(stderr, "Failed to initialize display\n");
    exit(1);
}
```

#### `anbs_display_cleanup`
```c
void anbs_display_cleanup(anbs_display_t *display);
```
**Description**: Clean up display resources and terminate NCurses.

**Parameters**:
- `display`: Display structure to clean up

**Example**:
```c
anbs_display_cleanup(display);
display = NULL;
```

#### `anbs_display_refresh`
```c
int anbs_display_refresh(anbs_display_t *display);
```
**Description**: Refresh all visible panels and update screen.

**Parameters**:
- `display`: Display structure

**Returns**:
- `0`: Success
- `-1`: Invalid display
- `-2`: NCurses error

### Panel Management

#### `anbs_display_create_panel`
```c
int anbs_display_create_panel(anbs_display_t *display, panel_type_t type);
```
**Description**: Create a new panel of specified type.

**Parameters**:
- `display`: Display structure
- `type`: Panel type (`ANBS_PANEL_TERMINAL`, `ANBS_PANEL_AI_CHAT`, `ANBS_PANEL_HEALTH`)

**Returns**:
- `>= 0`: Panel ID
- `-1`: Invalid parameters
- `-2`: Maximum panels reached
- `-3`: Panel creation failed

**Example**:
```c
int panel_id = anbs_display_create_panel(display, ANBS_PANEL_AI_CHAT);
if (panel_id < 0) {
    printf("Failed to create panel\n");
}
```

#### `anbs_display_switch_panel`
```c
int anbs_display_switch_panel(anbs_display_t *display, int panel_id);
```
**Description**: Switch focus to specified panel.

**Parameters**:
- `display`: Display structure
- `panel_id`: Target panel ID

**Returns**:
- `0`: Success
- `-1`: Invalid panel ID
- `-2`: Panel not visible

#### `anbs_display_resize_panels`
```c
int anbs_display_resize_panels(anbs_display_t *display);
```
**Description**: Resize all panels based on terminal dimensions.

**Parameters**:
- `display`: Display structure

**Returns**:
- `0`: Success
- `-1`: Resize failed

### Text Operations

#### `anbs_display_add_text`
```c
int anbs_display_add_text(anbs_display_t *display, int panel_id,
                          const char *text, text_color_t color);
```
**Description**: Add text to specified panel with color.

**Parameters**:
- `display`: Display structure
- `panel_id`: Target panel ID
- `text`: Text to add
- `color`: Text color (`ANBS_COLOR_NORMAL`, `ANBS_COLOR_AI`, `ANBS_COLOR_ERROR`, `ANBS_COLOR_SUCCESS`)

**Returns**:
- `0`: Success
- `-1`: Invalid panel
- `-2`: Text too long
- `-3`: Buffer full

**Example**:
```c
anbs_display_add_text(display, ai_panel_id, "AI Response", ANBS_COLOR_AI);
```

#### `anbs_display_clear_panel`
```c
int anbs_display_clear_panel(anbs_display_t *display, int panel_id);
```
**Description**: Clear all text from specified panel.

**Parameters**:
- `display`: Display structure
- `panel_id`: Panel to clear

**Returns**:
- `0`: Success
- `-1`: Invalid panel

### Input Handling

#### `anbs_display_handle_input`
```c
int anbs_display_handle_input(anbs_display_t *display, int ch);
```
**Description**: Process keyboard input and handle shortcuts.

**Parameters**:
- `display`: Display structure
- `ch`: Character from getch()

**Returns**:
- `0`: Input handled
- `-1`: Input not handled
- `1`: Exit requested

**Keyboard Shortcuts**:
- `Ctrl+A`: Switch to AI chat panel
- `Ctrl+T`: Switch to terminal panel
- `Ctrl+S`: Toggle split-screen mode
- `Ctrl+M`: Maximize/minimize panel
- `Ctrl+L`: Clear current panel

## Memory System API

### Memory Storage

#### `anbs_memory_store`
```c
int anbs_memory_store(const char *command, const char *response,
                      const char *context, memory_type_t type);
```
**Description**: Store a command/response pair in memory with vector embedding.

**Parameters**:
- `command`: Original command or query
- `response`: AI response or output
- `context`: Additional context (optional, can be NULL)
- `type`: Memory type (`MEMORY_TYPE_COMMAND`, `MEMORY_TYPE_CONVERSATION`, `MEMORY_TYPE_FILE`)

**Returns**:
- `0`: Success
- `-1`: Database error
- `-2`: Embedding generation failed
- `-3`: Invalid parameters

**Example**:
```c
anbs_memory_store("ls -la", "total 42\ndrwxr-xr-x...",
                  "/home/user", MEMORY_TYPE_COMMAND);
```

#### `anbs_memory_search`
```c
int anbs_memory_search(const char *query, memory_entry_t **results,
                       int max_results);
```
**Description**: Search memory using vector similarity.

**Parameters**:
- `query`: Search query
- `results`: Pointer to results array (allocated by function)
- `max_results`: Maximum number of results

**Returns**:
- `>= 0`: Number of results found
- `-1`: Search failed
- `-2`: No results found

**Example**:
```c
memory_entry_t *results = NULL;
int count = anbs_memory_search("python debugging", &results, 10);
if (count > 0) {
    for (int i = 0; i < count; i++) {
        printf("Result %d: %s\n", i, results[i].command);
    }
    free(results);
}
```

### Memory Management

#### `anbs_memory_cleanup`
```c
int anbs_memory_cleanup(int max_age_days);
```
**Description**: Remove memory entries older than specified days.

**Parameters**:
- `max_age_days`: Maximum age in days (0 = no limit)

**Returns**:
- `>= 0`: Number of entries removed
- `-1`: Cleanup failed

#### `anbs_memory_get_stats`
```c
int anbs_memory_get_stats(memory_stats_t *stats);
```
**Description**: Get memory system statistics.

**Parameters**:
- `stats`: Statistics structure to fill

**Returns**:
- `0`: Success
- `-1`: Statistics unavailable

**Statistics Structure**:
```c
typedef struct memory_stats {
    int total_entries;
    int commands_count;
    int conversations_count;
    int files_count;
    size_t database_size_kb;
    double avg_relevance_score;
} memory_stats_t;
```

## AI Commands API

### Command Registration

#### `vertex_builtin`
```c
int vertex_builtin(WORD_LIST *list);
```
**Description**: Main @vertex command implementation.

**Parameters**:
- `list`: Bash word list containing command arguments

**Returns**:
- `EXECUTION_SUCCESS` (0): Command successful
- `EXECUTION_FAILURE` (1): Command failed

**Command Options**:
- `--model MODEL`: Specify AI model
- `--timeout SECONDS`: Set timeout
- `--stream`: Enable streaming response
- `--health`: Health check
- `--format FORMAT`: Output format (text, json, markdown)

#### `memory_builtin`
```c
int memory_builtin(WORD_LIST *list);
```
**Description**: @memory command implementation.

**Parameters**:
- `list`: Bash word list containing command arguments

**Returns**:
- `EXECUTION_SUCCESS`: Command successful
- `EXECUTION_FAILURE`: Command failed

**Subcommands**:
- `search QUERY`: Search memory
- `recent [COUNT]`: Show recent entries
- `clear`: Clear all memory
- `stats`: Show statistics
- `export FILE`: Export to file

#### `analyze_builtin`
```c
int analyze_builtin(WORD_LIST *list);
```
**Description**: @analyze command implementation.

**Parameters**:
- `list`: Bash word list containing command arguments

**Returns**:
- `EXECUTION_SUCCESS`: Command successful
- `EXECUTION_FAILURE`: Command failed

**Analysis Options**:
- `--type TYPE`: Analysis type (code, security, performance, docs)
- `--format FORMAT`: Output format
- `--recursive`: Recursive directory analysis
- `--output FILE`: Save to file

### AI Service Integration

#### `send_ai_query`
```c
int send_ai_query(const char *query, const char *model,
                  int timeout, char **response);
```
**Description**: Send query to AI service and get response.

**Parameters**:
- `query`: Query text
- `model`: Model name (e.g., "gpt-4", "claude-3")
- `timeout`: Timeout in seconds
- `response`: Pointer to response string (allocated by function)

**Returns**:
- `0`: Success
- `AI_ERROR_NETWORK`: Network error
- `AI_ERROR_AUTH`: Authentication failed
- `AI_ERROR_TIMEOUT`: Request timeout
- `AI_ERROR_QUOTA`: API quota exceeded

**Example**:
```c
char *response = NULL;
int result = send_ai_query("Explain bash variables", "gpt-4", 30, &response);
if (result == 0) {
    printf("AI Response: %s\n", response);
    free(response);
}
```

#### `parse_ai_options`
```c
int parse_ai_options(WORD_LIST *list, struct ai_options *opts);
```
**Description**: Parse command line options for AI commands.

**Parameters**:
- `list`: Bash word list
- `opts`: Options structure to fill

**Returns**:
- `0`: Success
- `-1`: Invalid options

**Options Structure**:
```c
struct ai_options {
    char *query;
    char *model;
    int timeout;
    bool stream;
    bool health_check;
    output_format_t format;
    char *output_file;
};
```

## WebSocket Client API

### Connection Management

#### `anbs_websocket_connect`
```c
int anbs_websocket_connect(websocket_client_t **client, const char *url);
```
**Description**: Connect to WebSocket server.

**Parameters**:
- `client`: Pointer to client structure pointer (allocated by function)
- `url`: WebSocket URL (ws:// or wss://)

**Returns**:
- `0`: Success
- `-1`: Invalid URL
- `-2`: Connection failed
- `-3`: SSL initialization failed

**Example**:
```c
websocket_client_t *client = NULL;
if (anbs_websocket_connect(&client, "wss://api.example.com/ws") == 0) {
    printf("Connected to WebSocket\n");
}
```

#### `anbs_websocket_disconnect`
```c
void anbs_websocket_disconnect(websocket_client_t *client);
```
**Description**: Disconnect from WebSocket server and clean up.

**Parameters**:
- `client`: Client structure

### Message Operations

#### `anbs_websocket_send_message`
```c
int anbs_websocket_send_message(websocket_client_t *client, const char *message);
```
**Description**: Send message to WebSocket server.

**Parameters**:
- `client`: Client structure
- `message`: Message to send

**Returns**:
- `0`: Success
- `-1`: Not connected
- `-2`: Send failed

#### `anbs_websocket_receive_message`
```c
int anbs_websocket_receive_message(websocket_client_t *client, char **message);
```
**Description**: Receive message from WebSocket server.

**Parameters**:
- `client`: Client structure
- `message`: Pointer to message string (allocated by function)

**Returns**:
- `0`: Message received
- `-1`: No message available
- `-2`: Connection closed
- `-3`: Receive error

### Frame Processing

#### `parse_websocket_frame`
```c
int parse_websocket_frame(const uint8_t *data, size_t len, ws_frame_t *frame);
```
**Description**: Parse WebSocket frame according to RFC 6455.

**Parameters**:
- `data`: Frame data
- `len`: Data length
- `frame`: Frame structure to fill

**Returns**:
- `0`: Success
- `-1`: Invalid frame
- `-2`: Incomplete frame

#### `create_websocket_frame`
```c
int create_websocket_frame(const char *payload, uint8_t opcode,
                          uint8_t **frame_data, size_t *frame_len);
```
**Description**: Create WebSocket frame for sending.

**Parameters**:
- `payload`: Message payload
- `opcode`: Frame opcode (1=text, 2=binary, 8=close, 9=ping, 10=pong)
- `frame_data`: Pointer to frame data (allocated by function)
- `frame_len`: Pointer to frame length

**Returns**:
- `0`: Success
- `-1`: Frame creation failed

## Distributed AI API

### Agent Management

#### `anbs_distributed_ai_init`
```c
int anbs_distributed_ai_init(void);
```
**Description**: Initialize distributed AI system.

**Returns**:
- `0`: Success
- `-1`: Initialization failed

#### `anbs_distributed_ai_discover_agents`
```c
int anbs_distributed_ai_discover_agents(ai_agent_t **agents, int *count);
```
**Description**: Discover available AI agents on network.

**Parameters**:
- `agents`: Pointer to agents array (allocated by function)
- `count`: Pointer to agent count

**Returns**:
- `0`: Success
- `-1`: Discovery failed

**Agent Structure**:
```c
typedef struct ai_agent {
    char id[64];
    char hostname[256];
    char ip_address[16];
    int port;
    agent_capability_t capabilities;
    agent_status_t status;
    double load_average;
    time_t last_seen;
} ai_agent_t;
```

#### `anbs_distributed_ai_submit_task`
```c
int anbs_distributed_ai_submit_task(const char *task_description, char **result);
```
**Description**: Submit task to best available AI agent.

**Parameters**:
- `task_description`: Task to execute
- `result`: Pointer to result string (allocated by function)

**Returns**:
- `0`: Success
- `-1`: No agents available
- `-2`: Task submission failed
- `-3`: Task execution failed

### Load Balancing

#### `anbs_distributed_ai_get_best_agent`
```c
ai_agent_t *anbs_distributed_ai_get_best_agent(agent_capability_t required_caps);
```
**Description**: Find best agent for given capabilities.

**Parameters**:
- `required_caps`: Required agent capabilities

**Returns**:
- Pointer to best agent or NULL if none available

**Capabilities**:
```c
typedef enum {
    AGENT_CAP_TEXT_GENERATION = 1 << 0,
    AGENT_CAP_CODE_ANALYSIS = 1 << 1,
    AGENT_CAP_FILE_PROCESSING = 1 << 2,
    AGENT_CAP_MEMORY_SEARCH = 1 << 3,
    AGENT_CAP_HEALTH_MONITORING = 1 << 4
} agent_capability_t;
```

## Security API

### Sandbox Management

#### `anbs_sandbox_create`
```c
int anbs_sandbox_create(const char *sandbox_root, int sandbox_id);
```
**Description**: Create isolated execution sandbox.

**Parameters**:
- `sandbox_root`: Root directory for sandbox
- `sandbox_id`: Unique sandbox identifier

**Returns**:
- `0`: Success
- `-1`: Sandbox creation failed
- `-2`: Permission denied

#### `anbs_sandbox_enter`
```c
int anbs_sandbox_enter(int sandbox_id, pid_t *child_pid);
```
**Description**: Enter sandbox for secure execution.

**Parameters**:
- `sandbox_id`: Sandbox to enter
- `child_pid`: Pointer to store child process PID

**Returns**:
- `0`: Success (in parent process)
- `-1`: Sandbox entry failed
- Does not return in child process

#### `anbs_sandbox_destroy`
```c
int anbs_sandbox_destroy(int sandbox_id);
```
**Description**: Destroy sandbox and clean up.

**Parameters**:
- `sandbox_id`: Sandbox to destroy

**Returns**:
- `0`: Success
- `-1`: Destruction failed

### Permission System

#### `anbs_permissions_init`
```c
int anbs_permissions_init(void);
```
**Description**: Initialize role-based access control system.

**Returns**:
- `0`: Success
- `-1`: Initialization failed

#### `anbs_permissions_check`
```c
int anbs_permissions_check(const char *username, const char *command,
                          const char *resource);
```
**Description**: Check if user has permission for command on resource.

**Parameters**:
- `username`: User to check
- `command`: Command being executed
- `resource`: Resource being accessed

**Returns**:
- `1`: Permission granted
- `0`: Permission denied
- `-1`: Check failed

#### `anbs_permissions_add_role`
```c
int anbs_permissions_add_role(const char *username, user_role_t role);
```
**Description**: Add role to user.

**Parameters**:
- `username`: Target user
- `role`: Role to add

**Returns**:
- `0`: Success
- `-1`: Failed to add role

**User Roles**:
```c
typedef enum {
    ANBS_ROLE_GUEST = 1 << 0,
    ANBS_ROLE_USER = 1 << 1,
    ANBS_ROLE_DEVELOPER = 1 << 2,
    ANBS_ROLE_ADMIN = 1 << 3
} user_role_t;
```

## Performance API

### Caching System

#### `anbs_cache_init`
```c
int anbs_cache_init(int max_entries);
```
**Description**: Initialize response cache system.

**Parameters**:
- `max_entries`: Maximum cache entries (0 for default)

**Returns**:
- `0`: Success
- `-1`: Initialization failed

#### `anbs_cache_put`
```c
int anbs_cache_put(const char *command, const char *response, int ttl_seconds);
```
**Description**: Store response in cache.

**Parameters**:
- `command`: Command/query used as key
- `response`: Response to cache
- `ttl_seconds`: Time to live (0 for default)

**Returns**:
- `0`: Success
- `-1`: Cache failed

#### `anbs_cache_get`
```c
char *anbs_cache_get(const char *command, double *cache_age_ms);
```
**Description**: Retrieve response from cache.

**Parameters**:
- `command`: Command/query key
- `cache_age_ms`: Pointer to store cache age (optional)

**Returns**:
- Cached response string (caller must free) or NULL if not found

#### `anbs_cache_get_stats`
```c
int anbs_cache_get_stats(char **stats_json);
```
**Description**: Get cache statistics as JSON.

**Parameters**:
- `stats_json`: Pointer to JSON string (allocated by function)

**Returns**:
- `0`: Success
- `-1`: Failed to generate stats

### Performance Monitoring

#### `anbs_metrics_start_timer`
```c
void anbs_metrics_start_timer(const char *metric_name);
```
**Description**: Start performance timer for metric.

**Parameters**:
- `metric_name`: Name of metric to time

#### `anbs_metrics_end_timer`
```c
double anbs_metrics_end_timer(const char *metric_name);
```
**Description**: End performance timer and return elapsed time.

**Parameters**:
- `metric_name`: Name of metric

**Returns**:
- Elapsed time in milliseconds

#### `anbs_metrics_get_report`
```c
int anbs_metrics_get_report(char **report_json);
```
**Description**: Get performance metrics report.

**Parameters**:
- `report_json`: Pointer to JSON report (allocated by function)

**Returns**:
- `0`: Success
- `-1`: Report generation failed

### Optimization

#### `anbs_optimize_runtime`
```c
int anbs_optimize_runtime(void);
```
**Description**: Perform runtime optimizations.

**Returns**:
- `0`: Success
- `-1`: Optimization failed

#### `anbs_optimize_thread_pool_resize`
```c
int anbs_optimize_thread_pool_resize(int new_size);
```
**Description**: Resize worker thread pool.

**Parameters**:
- `new_size`: New thread pool size

**Returns**:
- `0`: Success
- `-1`: Resize failed

## Utility Functions

### String Operations

#### `anbs_string_hash`
```c
unsigned int anbs_string_hash(const char *str);
```
**Description**: Generate hash for string.

**Parameters**:
- `str`: String to hash

**Returns**:
- Hash value

#### `anbs_string_similarity`
```c
double anbs_string_similarity(const char *str1, const char *str2);
```
**Description**: Calculate string similarity (0.0 to 1.0).

**Parameters**:
- `str1`, `str2`: Strings to compare

**Returns**:
- Similarity score

### File Operations

#### `anbs_file_read_contents`
```c
int anbs_file_read_contents(const char *filename, char **contents, size_t *size);
```
**Description**: Read entire file contents.

**Parameters**:
- `filename`: File to read
- `contents`: Pointer to contents string (allocated by function)
- `size`: Pointer to store file size

**Returns**:
- `0`: Success
- `-1`: File not found
- `-2`: Read error

#### `anbs_file_get_type`
```c
file_type_t anbs_file_get_type(const char *filename);
```
**Description**: Determine file type from extension and contents.

**Parameters**:
- `filename`: File to analyze

**Returns**:
- File type enum value

**File Types**:
```c
typedef enum {
    ANBS_FILE_TYPE_UNKNOWN,
    ANBS_FILE_TYPE_TEXT,
    ANBS_FILE_TYPE_CODE_C,
    ANBS_FILE_TYPE_CODE_PYTHON,
    ANBS_FILE_TYPE_CODE_JAVASCRIPT,
    ANBS_FILE_TYPE_CONFIG_JSON,
    ANBS_FILE_TYPE_CONFIG_YAML,
    ANBS_FILE_TYPE_MARKDOWN,
    ANBS_FILE_TYPE_BINARY
} file_type_t;
```

### Time and Date

#### `anbs_time_format`
```c
char *anbs_time_format(time_t timestamp, const char *format);
```
**Description**: Format timestamp as string.

**Parameters**:
- `timestamp`: Unix timestamp
- `format`: Format string (strftime format)

**Returns**:
- Formatted time string (caller must free)

#### `anbs_time_elapsed_ms`
```c
double anbs_time_elapsed_ms(struct timeval *start, struct timeval *end);
```
**Description**: Calculate elapsed time in milliseconds.

**Parameters**:
- `start`: Start time
- `end`: End time

**Returns**:
- Elapsed time in milliseconds

## Data Structures

### Core Structures

#### `anbs_display_t`
```c
typedef struct anbs_display {
    WINDOW *main_screen;
    panel_t panels[ANBS_PANEL_COUNT];
    int active_panel;
    int term_width, term_height;
    bool split_mode_active;
    health_data_t health_data[10];
    double terminal_ratio;
    double ai_chat_ratio;
    pthread_mutex_t display_mutex;
} anbs_display_t;
```

#### `panel_t`
```c
typedef struct panel {
    WINDOW *window;
    int x, y, width, height;
    panel_type_t type;
    bool visible;
    bool has_focus;
    text_buffer_t *buffer;
    char title[64];
    int color_pair;
    int scroll_offset;
} panel_t;
```

#### `text_buffer_t`
```c
typedef struct text_buffer {
    char **lines;
    int *line_colors;
    int capacity;
    int count;
    int head;
    int tail;
    bool is_full;
    pthread_mutex_t buffer_mutex;
} text_buffer_t;
```

#### `memory_entry_t`
```c
typedef struct memory_entry {
    char *command;
    char *response;
    char *context;
    float embedding[EMBEDDING_DIMENSION];
    time_t timestamp;
    memory_type_t type;
    double relevance_score;
} memory_entry_t;
```

#### `websocket_client_t`
```c
typedef struct websocket_client {
    int socket_fd;
    SSL *ssl;
    SSL_CTX *ssl_ctx;
    char *host;
    int port;
    bool use_ssl;
    ws_state_t state;
    pthread_mutex_t send_mutex;
    uint8_t *frame_buffer;
    size_t buffer_size;
} websocket_client_t;
```

### Enumerations

#### `panel_type_t`
```c
typedef enum {
    ANBS_PANEL_TERMINAL = 0,
    ANBS_PANEL_AI_CHAT = 1,
    ANBS_PANEL_HEALTH = 2,
    ANBS_PANEL_COUNT = 3
} panel_type_t;
```

#### `text_color_t`
```c
typedef enum {
    ANBS_COLOR_NORMAL = 0,
    ANBS_COLOR_AI = 1,
    ANBS_COLOR_ERROR = 2,
    ANBS_COLOR_SUCCESS = 3,
    ANBS_COLOR_WARNING = 4,
    ANBS_COLOR_INFO = 5
} text_color_t;
```

#### `memory_type_t`
```c
typedef enum {
    MEMORY_TYPE_COMMAND = 0,
    MEMORY_TYPE_CONVERSATION = 1,
    MEMORY_TYPE_FILE = 2,
    MEMORY_TYPE_ERROR = 3
} memory_type_t;
```

#### `ws_state_t`
```c
typedef enum {
    WS_STATE_CONNECTING = 0,
    WS_STATE_CONNECTED = 1,
    WS_STATE_CLOSING = 2,
    WS_STATE_CLOSED = 3,
    WS_STATE_ERROR = 4
} ws_state_t;
```

## Error Codes

### General Error Codes
```c
#define ANBS_SUCCESS                 0
#define ANBS_ERROR_INVALID_PARAM    -1
#define ANBS_ERROR_MEMORY_ALLOC     -2
#define ANBS_ERROR_SYSTEM_CALL      -3
#define ANBS_ERROR_NOT_INITIALIZED  -4
#define ANBS_ERROR_ALREADY_EXISTS   -5
#define ANBS_ERROR_NOT_FOUND        -6
#define ANBS_ERROR_PERMISSION       -7
#define ANBS_ERROR_TIMEOUT          -8
#define ANBS_ERROR_NETWORK          -9
#define ANBS_ERROR_PROTOCOL        -10
```

### Display Error Codes
```c
#define ANBS_DISPLAY_ERROR_NCURSES_INIT    -100
#define ANBS_DISPLAY_ERROR_WINDOW_CREATE   -101
#define ANBS_DISPLAY_ERROR_PANEL_FULL      -102
#define ANBS_DISPLAY_ERROR_INVALID_PANEL   -103
#define ANBS_DISPLAY_ERROR_RESIZE_FAILED   -104
```

### Memory Error Codes
```c
#define ANBS_MEMORY_ERROR_DATABASE      -200
#define ANBS_MEMORY_ERROR_EMBEDDING     -201
#define ANBS_MEMORY_ERROR_NO_RESULTS    -202
#define ANBS_MEMORY_ERROR_QUERY_FAILED  -203
```

### AI Error Codes
```c
#define ANBS_AI_ERROR_AUTH         -300
#define ANBS_AI_ERROR_QUOTA        -301
#define ANBS_AI_ERROR_MODEL        -302
#define ANBS_AI_ERROR_PARSE        -303
#define ANBS_AI_ERROR_RESPONSE     -304
```

### WebSocket Error Codes
```c
#define ANBS_WS_ERROR_INVALID_URL     -400
#define ANBS_WS_ERROR_CONNECT_FAILED  -401
#define ANBS_WS_ERROR_SSL_INIT        -402
#define ANBS_WS_ERROR_HANDSHAKE      -403
#define ANBS_WS_ERROR_FRAME_INVALID   -404
#define ANBS_WS_ERROR_SEND_FAILED     -405
```

### Security Error Codes
```c
#define ANBS_SECURITY_ERROR_SANDBOX_CREATE  -500
#define ANBS_SECURITY_ERROR_CHROOT_FAILED   -501
#define ANBS_SECURITY_ERROR_SECCOMP_FAILED  -502
#define ANBS_SECURITY_ERROR_PERMISSION      -503
#define ANBS_SECURITY_ERROR_USER_NOT_FOUND  -504
```

### Performance Error Codes
```c
#define ANBS_PERF_ERROR_CACHE_FULL      -600
#define ANBS_PERF_ERROR_CACHE_INIT      -601
#define ANBS_PERF_ERROR_METRICS_FAILED  -602
#define ANBS_PERF_ERROR_OPTIMIZATION    -603
```

### Usage Examples

#### Complete Display Setup
```c
#include "ai_display.h"

int main() {
    anbs_display_t *display = NULL;

    // Initialize display
    if (anbs_display_init(&display) != 0) {
        fprintf(stderr, "Failed to initialize display\n");
        return 1;
    }

    // Create panels
    int terminal_panel = anbs_display_create_panel(display, ANBS_PANEL_TERMINAL);
    int ai_panel = anbs_display_create_panel(display, ANBS_PANEL_AI_CHAT);
    int health_panel = anbs_display_create_panel(display, ANBS_PANEL_HEALTH);

    // Add some content
    anbs_display_add_text(display, terminal_panel, "$ pwd", ANBS_COLOR_NORMAL);
    anbs_display_add_text(display, terminal_panel, "/home/user", ANBS_COLOR_NORMAL);
    anbs_display_add_text(display, ai_panel, "🤖 AI Ready", ANBS_COLOR_AI);

    // Main loop
    int ch;
    while ((ch = getch()) != 'q') {
        if (anbs_display_handle_input(display, ch) == 1) {
            break; // Exit requested
        }
        anbs_display_refresh(display);
    }

    // Cleanup
    anbs_display_cleanup(display);
    return 0;
}
```

#### Memory System Usage
```c
#include "memory_system.h"

void example_memory_usage() {
    // Initialize memory system
    if (anbs_memory_init("/tmp/anbs_memory.db") != 0) {
        return;
    }

    // Store a command
    anbs_memory_store("git status", "On branch main\nnothing to commit",
                      "/project", MEMORY_TYPE_COMMAND);

    // Search memory
    memory_entry_t *results = NULL;
    int count = anbs_memory_search("git", &results, 5);

    printf("Found %d results:\n", count);
    for (int i = 0; i < count; i++) {
        printf("  %s -> %s (score: %.2f)\n",
               results[i].command, results[i].response,
               results[i].relevance_score);
    }

    free(results);
    anbs_memory_cleanup_system();
}
```

This API reference provides comprehensive documentation for all ANBS functions, structures, and usage patterns.