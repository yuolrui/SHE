#ifndef CONNECTION_H
#define CONNECTION_H

#include <time.h>
#include <netinet/in.h>
#include "buffer.h"
#include "chunkqueue.h"
#include "http_parser.h"

/* Forward declaration */
typedef struct server server_t;

/**
 * Lighttpd Connection State Machine:
 *
 * [CONNECT] -> [REQUEST_START] -> [READ] (read HTTP request)
 *                                  |
 *                                  v
 *                             [REQUEST_END]
 *                                  |
 *                                  v
 *                            [HANDLE_REQUEST] (lookup docroot, MIME, range, headers)
 *                                  |
 *                                  v
 *                            [RESPONSE_START] (construct HTTP response headers)
 *                                  |
 *                                  v
 *                               [WRITE] (non-blocking write headers + body chunks)
 *                                  |
 *                                  v
 *                            [RESPONSE_END]
 *                            /             \
 *                 (keep-alive)             (close)
 *                      v                      v
 *             [REQUEST_START]              [CLOSE]
 */
typedef enum {
    CON_STATE_CONNECT = 0,
    CON_STATE_REQUEST_START,
    CON_STATE_READ,
    CON_STATE_REQUEST_END,
    CON_STATE_HANDLE_REQUEST,
    CON_STATE_RESPONSE_START,
    CON_STATE_WRITE,
    CON_STATE_RESPONSE_END,
    CON_STATE_ERROR,
    CON_STATE_CLOSE
} connection_state_t;

typedef struct connection {
    int fd;
    struct sockaddr_in client_addr;
    char client_ip[INET_ADDRSTRLEN];
    int client_port;
    
    connection_state_t state;
    time_t state_timestamp;
    
    /* Request buffers */
    buffer_t *read_buf;
    http_request_t request;
    
    /* Response state */
    int http_status;
    buffer_t *response_headers;
    chunkqueue_t *write_queue;
    
    /* Connection attributes */
    int keep_alive;
    int keep_alive_max_requests;
    int request_count;
    
    /* Response metrics */
    off_t bytes_sent;
    
    struct connection *next;
    struct connection *prev;
} connection_t;

connection_t *connection_init(int client_fd, struct sockaddr_in *client_addr);
void connection_free(connection_t *con);
void connection_reset(connection_t *con);

const char *connection_state_to_str(connection_state_t state);

/* State machine driver */
int connection_state_machine(server_t *srv, connection_t *con);

/* fdevent handler callback */
void connection_handle_fdevent(void *context, int revents);

#endif /* CONNECTION_H */
