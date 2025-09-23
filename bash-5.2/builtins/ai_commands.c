/* ai_commands.c - AI command builtins for ANBS */

#include "config.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "../bashansi.h"
#include <stdio.h>
#include <errno.h>

#include "../bashintl.h"

#include "../shell.h"
#include "../builtins.h"
#include "../common.h"
#include "../builtext.h"
#include "../ai_core/ai_display.h"

#include <curl/curl.h>
#include <json-c/json.h>
#include <pthread.h>
#include <sys/time.h>

extern anbs_display_t *g_anbs_display;

/* Response data structure for curl */
struct ai_response {
    char *memory;
    size_t size;
};

/* AI command options */
struct ai_options {
    int health_check;
    int stream_mode;
    int timeout;
    char *model;
    char *query;
};

/* Write callback for curl responses */
static size_t write_response_callback(void *contents, size_t size, size_t nmemb, struct ai_response *response) {
    size_t realsize = size * nmemb;
    char *ptr = realloc(response->memory, response->size + realsize + 1);

    if (!ptr) {
        /* Out of memory */
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    response->memory = ptr;
    memcpy(&(response->memory[response->size]), contents, realsize);
    response->size += realsize;
    response->memory[response->size] = 0;

    return realsize;
}

/* Parse JSON response from AI API */
static int parse_ai_response(const char *json_response, char **ai_text) {
    json_object *root, *content, *message;
    const char *response_text;

    root = json_tokener_parse(json_response);
    if (!root) {
        return -1;
    }

    /* Try different JSON structures based on AI provider */
    if (json_object_object_get_ex(root, "content", &content)) {
        /* Anthropic Claude API structure */
        response_text = json_object_get_string(content);
    } else if (json_object_object_get_ex(root, "message", &message)) {
        /* OpenAI API structure */
        response_text = json_object_get_string(message);
    } else if (json_object_object_get_ex(root, "response", &content)) {
        /* Generic response structure */
        response_text = json_object_get_string(content);
    } else {
        /* Fallback: use whole response as text */
        response_text = json_object_get_string(root);
    }

    if (response_text) {
        *ai_text = strdup(response_text);
        json_object_put(root);
        return 0;
    }

    json_object_put(root);
    return -1;
}

/* Send query to AI service */
static int send_ai_query(const char *query, const char *model, int timeout, char **response) {
    CURL *curl;
    CURLcode res;
    struct ai_response chunk = {0};
    struct curl_slist *headers = NULL;
    char *json_data;
    char auth_header[512];
    const char *api_key;
    const char *api_url;
    struct timeval start_time, end_time;
    double elapsed_ms;

    /* Get timing */
    gettimeofday(&start_time, NULL);

    /* Initialize curl */
    curl = curl_easy_init();
    if (!curl) {
        return -1;
    }

    /* Prepare API credentials and URL */
    api_key = getenv("ANTHROPIC_API_KEY");
    if (!api_key) {
        api_key = getenv("OPENAI_API_KEY");
        api_url = "https://api.openai.com/v1/chat/completions";
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);
    } else {
        api_url = "https://api.anthropic.com/v1/messages";
        snprintf(auth_header, sizeof(auth_header), "x-api-key: %s", api_key);
    }

    if (!api_key) {
        curl_easy_cleanup(curl);
        *response = strdup("Error: No API key found. Set ANTHROPIC_API_KEY or OPENAI_API_KEY environment variable.");
        return -1;
    }

    /* Prepare JSON payload */
    json_object *root = json_object_new_object();
    json_object *model_obj = json_object_new_string(model ? model : "claude-3-sonnet-20240229");
    json_object *max_tokens = json_object_new_int(1000);
    json_object *messages = json_object_new_array();
    json_object *message = json_object_new_object();
    json_object *role = json_object_new_string("user");
    json_object *content = json_object_new_string(query);

    json_object_object_add(message, "role", role);
    json_object_object_add(message, "content", content);
    json_object_array_add(messages, message);

    json_object_object_add(root, "model", model_obj);
    json_object_object_add(root, "max_tokens", max_tokens);
    json_object_object_add(root, "messages", messages);

    json_data = (char*)json_object_to_json_string(root);

    /* Set curl options */
    curl_easy_setopt(curl, CURLOPT_URL, api_url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ANBS/1.0");

    /* Set headers */
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    /* Update display with processing status */
    if (g_anbs_display) {
        anbs_status_write(g_anbs_display, "Processing AI query...");
        anbs_display_refresh_panel(g_anbs_display, ANBS_PANEL_STATUS);
    }

    /* Perform the request */
    res = curl_easy_perform(curl);

    /* Calculate response time */
    gettimeofday(&end_time, NULL);
    elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                 (end_time.tv_usec - start_time.tv_usec) / 1000.0;

    /* Clean up */
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    json_object_put(root);

    if (res != CURLE_OK) {
        if (chunk.memory) {
            free(chunk.memory);
        }
        *response = strdup(curl_easy_strerror(res));
        return -1;
    }

    /* Parse response */
    char *ai_text = NULL;
    if (parse_ai_response(chunk.memory, &ai_text) == 0 && ai_text) {
        *response = ai_text;

        /* Update health monitoring with response time */
        if (g_anbs_display && elapsed_ms < 50.0) {
            char status_msg[256];
            snprintf(status_msg, sizeof(status_msg), "AI response: %.1fms (target: <50ms)", elapsed_ms);
            anbs_status_write(g_anbs_display, status_msg);
        }

        free(chunk.memory);
        return 0;
    } else {
        *response = chunk.memory ? chunk.memory : strdup("Error: Could not parse AI response");
        return -1;
    }
}

/* Health check for AI service */
static int ai_health_check(void) {
    char *response = NULL;
    int result = send_ai_query("ping", NULL, 5, &response);

    if (g_anbs_display) {
        if (result == 0) {
            anbs_status_write(g_anbs_display, "AI service: Online ✅");
        } else {
            anbs_status_write(g_anbs_display, "AI service: Offline ❌");
        }
        anbs_display_refresh_panel(g_anbs_display, ANBS_PANEL_STATUS);
    }

    if (response) {
        printf("AI Health Check: %s\n", result == 0 ? "ONLINE" : "OFFLINE");
        if (result != 0) {
            printf("Error: %s\n", response);
        }
        free(response);
    }

    return result;
}

/* Parse command line options for @vertex */
static int parse_ai_options(WORD_LIST *list, struct ai_options *opts) {
    WORD_LIST *l;

    /* Initialize options */
    memset(opts, 0, sizeof(struct ai_options));
    opts->timeout = 30; /* Default timeout */

    for (l = list; l; l = l->next) {
        if (STREQ(l->word->word, "--health")) {
            opts->health_check = 1;
        } else if (STREQ(l->word->word, "--stream")) {
            opts->stream_mode = 1;
        } else if (strncmp(l->word->word, "--timeout=", 10) == 0) {
            opts->timeout = atoi(l->word->word + 10);
            if (opts->timeout <= 0) opts->timeout = 30;
        } else if (strncmp(l->word->word, "--model=", 8) == 0) {
            opts->model = l->word->word + 8;
        } else if (l->word->word[0] != '-') {
            /* This is the query text */
            opts->query = l->word->word;
            break;
        }
    }

    return 0;
}

/* Main @vertex command implementation */
int vertex_builtin(WORD_LIST *list) {
    struct ai_options opts;
    char *response = NULL;
    int result;

    /* Parse options */
    if (parse_ai_options(list, &opts) != 0) {
        builtin_usage();
        return EX_USAGE;
    }

    /* Handle health check */
    if (opts.health_check) {
        return ai_health_check() == 0 ? EXECUTION_SUCCESS : EXECUTION_FAILURE;
    }

    /* Validate query */
    if (!opts.query || strlen(opts.query) == 0) {
        builtin_error("@vertex: missing query text");
        return EX_USAGE;
    }

    /* Send query to AI */
    result = send_ai_query(opts.query, opts.model, opts.timeout, &response);

    if (result == 0 && response) {
        /* Display response in terminal */
        printf("🤖 Vertex: %s\n", response);

        /* Also display in AI chat panel if available */
        if (g_anbs_display) {
            char formatted_response[4096];
            snprintf(formatted_response, sizeof(formatted_response), "🤖 Vertex: %s\n", response);
            anbs_ai_chat_write(g_anbs_display, formatted_response);
            anbs_display_refresh_panel(g_anbs_display, ANBS_PANEL_AI_CHAT);
        }

        free(response);
        return EXECUTION_SUCCESS;
    } else {
        /* Handle error */
        if (response) {
            builtin_error("@vertex: %s", response);
            free(response);
        } else {
            builtin_error("@vertex: failed to get AI response");
        }
        return EXECUTION_FAILURE;
    }
}

/* @vertex command structure */
struct builtin vertex_struct = {
    "vertex",           /* builtin name */
    vertex_builtin,     /* function implementing the builtin */
    BUILTIN_ENABLED,    /* initial flags for builtin */
    (char **)0,         /* array of long documentation strings */
    "@vertex query [options] - Send query to AI assistant",  /* short doc */
    0                   /* reserved for internal use */
};

/* Alternative @memory command implementation */
int memory_builtin(WORD_LIST *list) {
    char *query = NULL;

    if (list && list->word) {
        query = list->word->word;
    }

    if (!query || strlen(query) == 0) {
        builtin_error("@memory: missing search query");
        return EX_USAGE;
    }

    /* For now, pass to vertex with memory context */
    char memory_query[2048];
    snprintf(memory_query, sizeof(memory_query),
             "Search my command history and conversation memory for: %s", query);

    /* Update word list with new query */
    WORD_LIST new_list;
    WORD_DESC new_word;
    new_word.word = memory_query;
    new_word.flags = 0;
    new_list.word = &new_word;
    new_list.next = NULL;

    return vertex_builtin(&new_list);
}

struct builtin memory_struct = {
    "memory",
    memory_builtin,
    BUILTIN_ENABLED,
    (char **)0,
    "@memory query - Search conversation history and memory",
    0
};

/* @analyze command implementation */
int analyze_builtin(WORD_LIST *list) {
    char *filename = NULL;

    if (list && list->word) {
        filename = list->word->word;
    }

    if (!filename || strlen(filename) == 0) {
        builtin_error("@analyze: missing filename");
        return EX_USAGE;
    }

    /* Read file and analyze */
    FILE *file = fopen(filename, "r");
    if (!file) {
        builtin_error("@analyze: cannot open file '%s'", filename);
        return EXECUTION_FAILURE;
    }

    /* Read file content */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size > 100000) {  /* 100KB limit */
        fclose(file);
        builtin_error("@analyze: file too large (max 100KB)");
        return EXECUTION_FAILURE;
    }

    char *file_content = malloc(file_size + 1);
    if (!file_content) {
        fclose(file);
        builtin_error("@analyze: out of memory");
        return EXECUTION_FAILURE;
    }

    fread(file_content, 1, file_size, file);
    file_content[file_size] = '\0';
    fclose(file);

    /* Create analysis query */
    char analysis_query[file_size + 512];
    snprintf(analysis_query, sizeof(analysis_query),
             "Analyze this file (%s):\n\n%s\n\nProvide insights about structure, purpose, and potential improvements.",
             filename, file_content);

    free(file_content);

    /* Send to AI for analysis */
    WORD_LIST new_list;
    WORD_DESC new_word;
    new_word.word = analysis_query;
    new_word.flags = 0;
    new_list.word = &new_word;
    new_list.next = NULL;

    return vertex_builtin(&new_list);
}

struct builtin analyze_struct = {
    "analyze",
    analyze_builtin,
    BUILTIN_ENABLED,
    (char **)0,
    "@analyze filename - Analyze file content with AI",
    0
};