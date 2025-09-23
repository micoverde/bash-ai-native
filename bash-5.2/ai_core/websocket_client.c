/* websocket_client.c - WebSocket client for real-time AI communication */

#include "ai_display.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <base64.h>

#define WS_MAGIC_STRING "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_BUFFER_SIZE 8192

typedef struct {
    int socket_fd;
    SSL *ssl;
    SSL_CTX *ssl_ctx;
    char *host;
    int port;
    char *path;
    int connected;
    pthread_t thread;
    pthread_mutex_t write_mutex;
    anbs_display_t *display;
} websocket_client_t;

static websocket_client_t *g_ws_client = NULL;

/* Base64 encoding for WebSocket handshake */
static char *base64_encode(const unsigned char *input, int length) {
    BIO *bmem, *b64;
    BUF_MEM *bptr;
    char *output;

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    output = malloc(bptr->length + 1);
    memcpy(output, bptr->data, bptr->length);
    output[bptr->length] = 0;

    BIO_free_all(b64);
    return output;
}

/* Generate WebSocket key for handshake */
static char *generate_websocket_key(void) {
    unsigned char key[16];
    FILE *urandom = fopen("/dev/urandom", "r");

    if (urandom) {
        fread(key, 1, 16, urandom);
        fclose(urandom);
    } else {
        /* Fallback to time-based random */
        srand(time(NULL));
        for (int i = 0; i < 16; i++) {
            key[i] = rand() % 256;
        }
    }

    return base64_encode(key, 16);
}

/* Calculate WebSocket accept key */
static char *calculate_accept_key(const char *client_key) {
    char combined[256];
    unsigned char hash[SHA_DIGEST_LENGTH];

    snprintf(combined, sizeof(combined), "%s%s", client_key, WS_MAGIC_STRING);
    SHA1((unsigned char*)combined, strlen(combined), hash);

    return base64_encode(hash, SHA_DIGEST_LENGTH);
}

/* Create WebSocket frame */
static int create_websocket_frame(const char *payload, unsigned char **frame, size_t *frame_len) {
    size_t payload_len = strlen(payload);
    size_t header_len;
    unsigned char *result;

    /* Determine header length */
    if (payload_len < 126) {
        header_len = 2 + 4; /* Basic header + mask */
    } else if (payload_len < 65536) {
        header_len = 4 + 4; /* Extended 16-bit length + mask */
    } else {
        header_len = 10 + 4; /* Extended 64-bit length + mask */
    }

    *frame_len = header_len + payload_len;
    result = malloc(*frame_len);

    /* FIN + opcode (text frame) */
    result[0] = 0x81;

    /* Payload length and mask bit */
    if (payload_len < 126) {
        result[1] = 0x80 | payload_len; /* Mask bit + length */
    } else if (payload_len < 65536) {
        result[1] = 0x80 | 126;
        result[2] = (payload_len >> 8) & 0xFF;
        result[3] = payload_len & 0xFF;
    } else {
        result[1] = 0x80 | 127;
        /* 64-bit length (big endian) */
        for (int i = 0; i < 8; i++) {
            result[2 + i] = (payload_len >> (8 * (7 - i))) & 0xFF;
        }
    }

    /* Masking key (simple XOR pattern) */
    unsigned char mask[4] = {0x12, 0x34, 0x56, 0x78};
    memcpy(result + header_len - 4, mask, 4);

    /* Masked payload */
    for (size_t i = 0; i < payload_len; i++) {
        result[header_len + i] = payload[i] ^ mask[i % 4];
    }

    *frame = result;
    return 0;
}

/* Parse WebSocket frame */
static int parse_websocket_frame(const unsigned char *data, size_t len, char **payload) {
    if (len < 2) return -1;

    int fin = (data[0] & 0x80) != 0;
    int opcode = data[0] & 0x0F;
    int masked = (data[1] & 0x80) != 0;
    uint64_t payload_len = data[1] & 0x7F;

    size_t offset = 2;

    /* Extended payload length */
    if (payload_len == 126) {
        if (len < offset + 2) return -1;
        payload_len = (data[offset] << 8) | data[offset + 1];
        offset += 2;
    } else if (payload_len == 127) {
        if (len < offset + 8) return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | data[offset + i];
        }
        offset += 8;
    }

    /* Masking key */
    unsigned char mask[4] = {0};
    if (masked) {
        if (len < offset + 4) return -1;
        memcpy(mask, data + offset, 4);
        offset += 4;
    }

    /* Payload */
    if (len < offset + payload_len) return -1;

    *payload = malloc(payload_len + 1);
    for (uint64_t i = 0; i < payload_len; i++) {
        (*payload)[i] = masked ? data[offset + i] ^ mask[i % 4] : data[offset + i];
    }
    (*payload)[payload_len] = '\0';

    return fin ? 1 : 0; /* Return 1 if this is the final frame */
}

/* WebSocket handshake */
static int websocket_handshake(websocket_client_t *client) {
    char *key = generate_websocket_key();
    char request[1024];
    char response[2048];
    int bytes_read;

    /* Send handshake request */
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "User-Agent: ANBS-WebSocket/1.0\r\n"
        "\r\n",
        client->path, client->host, client->port, key);

    if (client->ssl) {
        SSL_write(client->ssl, request, strlen(request));
        bytes_read = SSL_read(client->ssl, response, sizeof(response) - 1);
    } else {
        send(client->socket_fd, request, strlen(request), 0);
        bytes_read = recv(client->socket_fd, response, sizeof(response) - 1, 0);
    }

    if (bytes_read <= 0) {
        free(key);
        return -1;
    }

    response[bytes_read] = '\0';

    /* Verify handshake response */
    if (strstr(response, "HTTP/1.1 101") == NULL) {
        ANBS_DEBUG_LOG("WebSocket handshake failed: %s", response);
        free(key);
        return -1;
    }

    /* Calculate expected accept key */
    char *expected_accept = calculate_accept_key(key);
    char accept_line[256];
    snprintf(accept_line, sizeof(accept_line), "Sec-WebSocket-Accept: %s", expected_accept);

    int handshake_ok = strstr(response, accept_line) != NULL;

    free(key);
    free(expected_accept);

    return handshake_ok ? 0 : -1;
}

/* WebSocket message handler thread */
static void *websocket_thread(void *arg) {
    websocket_client_t *client = (websocket_client_t*)arg;
    unsigned char buffer[WS_BUFFER_SIZE];
    int bytes_received;
    char *payload;

    while (client->connected) {
        /* Read WebSocket frame */
        if (client->ssl) {
            bytes_received = SSL_read(client->ssl, buffer, sizeof(buffer));
        } else {
            bytes_received = recv(client->socket_fd, buffer, sizeof(buffer), 0);
        }

        if (bytes_received <= 0) {
            ANBS_DEBUG_LOG("WebSocket connection lost");
            client->connected = 0;
            break;
        }

        /* Parse frame */
        if (parse_websocket_frame(buffer, bytes_received, &payload) > 0) {
            ANBS_DEBUG_LOG("Received WebSocket message: %s", payload);

            /* Handle AI response */
            if (client->display) {
                char formatted_msg[4096];
                snprintf(formatted_msg, sizeof(formatted_msg), "🌐 AI: %s\n", payload);
                anbs_ai_chat_write(client->display, formatted_msg);
                anbs_display_refresh_panel(client->display, ANBS_PANEL_AI_CHAT);
            }

            free(payload);
        }

        usleep(10000); /* 10ms delay */
    }

    return NULL;
}

/* Initialize WebSocket client */
int anbs_websocket_init(anbs_display_t *display, const char *host, int port, const char *path, int use_ssl) {
    if (g_ws_client) {
        return -1; /* Already initialized */
    }

    g_ws_client = calloc(1, sizeof(websocket_client_t));
    if (!g_ws_client) {
        return -1;
    }

    g_ws_client->host = strdup(host);
    g_ws_client->port = port;
    g_ws_client->path = strdup(path);
    g_ws_client->display = display;

    pthread_mutex_init(&g_ws_client->write_mutex, NULL);

    /* Initialize SSL if needed */
    if (use_ssl) {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();

        g_ws_client->ssl_ctx = SSL_CTX_new(TLS_client_method());
        if (!g_ws_client->ssl_ctx) {
            ANBS_DEBUG_LOG("Failed to create SSL context");
            anbs_websocket_cleanup();
            return -1;
        }
    }

    return 0;
}

/* Connect to WebSocket server */
int anbs_websocket_connect(void) {
    if (!g_ws_client) {
        return -1;
    }

    struct sockaddr_in server_addr;
    struct hostent *server;

    /* Create socket */
    g_ws_client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_ws_client->socket_fd < 0) {
        return -1;
    }

    /* Resolve hostname */
    server = gethostbyname(g_ws_client->host);
    if (!server) {
        close(g_ws_client->socket_fd);
        return -1;
    }

    /* Connect to server */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_ws_client->port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(g_ws_client->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(g_ws_client->socket_fd);
        return -1;
    }

    /* Set up SSL if needed */
    if (g_ws_client->ssl_ctx) {
        g_ws_client->ssl = SSL_new(g_ws_client->ssl_ctx);
        SSL_set_fd(g_ws_client->ssl, g_ws_client->socket_fd);

        if (SSL_connect(g_ws_client->ssl) <= 0) {
            ERR_print_errors_fp(stderr);
            close(g_ws_client->socket_fd);
            return -1;
        }
    }

    /* Perform WebSocket handshake */
    if (websocket_handshake(g_ws_client) != 0) {
        anbs_websocket_disconnect();
        return -1;
    }

    g_ws_client->connected = 1;

    /* Start message handler thread */
    if (pthread_create(&g_ws_client->thread, NULL, websocket_thread, g_ws_client) != 0) {
        anbs_websocket_disconnect();
        return -1;
    }

    ANBS_DEBUG_LOG("WebSocket connected to %s:%d%s", g_ws_client->host, g_ws_client->port, g_ws_client->path);
    return 0;
}

/* Send message via WebSocket */
int anbs_websocket_send(const char *message) {
    if (!g_ws_client || !g_ws_client->connected) {
        return -1;
    }

    pthread_mutex_lock(&g_ws_client->write_mutex);

    unsigned char *frame;
    size_t frame_len;

    if (create_websocket_frame(message, &frame, &frame_len) != 0) {
        pthread_mutex_unlock(&g_ws_client->write_mutex);
        return -1;
    }

    int result;
    if (g_ws_client->ssl) {
        result = SSL_write(g_ws_client->ssl, frame, frame_len);
    } else {
        result = send(g_ws_client->socket_fd, frame, frame_len, 0);
    }

    free(frame);
    pthread_mutex_unlock(&g_ws_client->write_mutex);

    return result > 0 ? 0 : -1;
}

/* Disconnect WebSocket */
void anbs_websocket_disconnect(void) {
    if (!g_ws_client) {
        return;
    }

    g_ws_client->connected = 0;

    /* Wait for thread to finish */
    if (g_ws_client->thread) {
        pthread_join(g_ws_client->thread, NULL);
    }

    /* Close SSL */
    if (g_ws_client->ssl) {
        SSL_shutdown(g_ws_client->ssl);
        SSL_free(g_ws_client->ssl);
    }

    /* Close socket */
    if (g_ws_client->socket_fd >= 0) {
        close(g_ws_client->socket_fd);
    }
}

/* Cleanup WebSocket client */
void anbs_websocket_cleanup(void) {
    if (!g_ws_client) {
        return;
    }

    anbs_websocket_disconnect();

    if (g_ws_client->ssl_ctx) {
        SSL_CTX_free(g_ws_client->ssl_ctx);
    }

    if (g_ws_client->host) {
        free(g_ws_client->host);
    }

    if (g_ws_client->path) {
        free(g_ws_client->path);
    }

    pthread_mutex_destroy(&g_ws_client->write_mutex);

    free(g_ws_client);
    g_ws_client = NULL;
}

/* Check WebSocket connection status */
int anbs_websocket_is_connected(void) {
    return g_ws_client && g_ws_client->connected;
}

/* Send ping frame */
int anbs_websocket_ping(void) {
    if (!g_ws_client || !g_ws_client->connected) {
        return -1;
    }

    /* Create ping frame (opcode 0x9) */
    unsigned char ping_frame[] = {0x89, 0x80, 0x12, 0x34, 0x56, 0x78}; /* Masked ping */

    pthread_mutex_lock(&g_ws_client->write_mutex);

    int result;
    if (g_ws_client->ssl) {
        result = SSL_write(g_ws_client->ssl, ping_frame, sizeof(ping_frame));
    } else {
        result = send(g_ws_client->socket_fd, ping_frame, sizeof(ping_frame), 0);
    }

    pthread_mutex_unlock(&g_ws_client->write_mutex);

    return result > 0 ? 0 : -1;
}