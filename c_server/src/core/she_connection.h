#ifndef SHE_CONNECTION_H
#define SHE_CONNECTION_H

#include <time.h>
#include <netinet/in.h>
#include "core/she_buffer.h"
#include "core/she_chunkqueue.h"
#include "http/she_http_parser.h"

/* Forward declaration */
typedef struct she_server she_server_t;

/**
 * SHE (Secure HTTP Engine) Connection 10-State Machine:
 *
 * [CONNECT] -> [REQUEST_START] -> [READ] (read HTTP request)
 *                                  |
 *                                  v
 *                             [REQUEST_END]
 *                                  |
 *                                  v
 *                            [HANDLE_REQUEST] (security checks, plugins, docroot, Range)
 *                                  |
 *                                  v
 *                            [RESPONSE_START] (construct headers + security headers)
 *                                  |
 *                                  v
 *                               [WRITE] (non-blocking chunkqueue + sendfile)
 *                                  |
 *                                  v
 *                            [RESPONSE_END]
 *                            /             \
 *                 (keep-alive)             (close)
 *                      v                      v
 *             [REQUEST_START]              [CLOSE]
 */
typedef enum {
    SHE_CON_STATE_CONNECT = 0,
    SHE_CON_STATE_REQUEST_START,
    SHE_CON_STATE_READ,
    SHE_CON_STATE_REQUEST_END,
    SHE_CON_STATE_HANDLE_REQUEST,
    SHE_CON_STATE_RESPONSE_START,
    SHE_CON_STATE_WRITE,
    SHE_CON_STATE_RESPONSE_END,
    SHE_CON_STATE_ERROR,
    SHE_CON_STATE_CLOSE
} she_con_state_t;

typedef struct she_connection {
    int fd;
    struct sockaddr_in client_addr;
    char client_ip[INET_ADDRSTRLEN];
    int client_port;
    
    she_con_state_t state;
    time_t state_timestamp;
    
    /* Request buffers */
    she_buffer_t *read_buf;
    she_http_request_t request;
    
    /* Response state */
    int http_status;
    she_buffer_t *response_headers;
    she_chunkqueue_t *write_queue;
    
    /* Connection attributes */
    int keep_alive;
    int keep_alive_max_requests;
    int request_count;
    
    /* Response metrics */
    off_t bytes_sent;
    
    struct she_connection *next;
    struct she_connection *prev;
} she_connection_t;

/* Backward compatibility */
typedef she_con_state_t connection_state_t;
typedef she_connection_t connection_t;

#define CON_STATE_CONNECT         SHE_CON_STATE_CONNECT
#define CON_STATE_REQUEST_START   SHE_CON_STATE_REQUEST_START
#define CON_STATE_READ            SHE_CON_STATE_READ
#define CON_STATE_REQUEST_END     SHE_CON_STATE_REQUEST_END
#define CON_STATE_HANDLE_REQUEST  SHE_CON_STATE_HANDLE_REQUEST
#define CON_STATE_RESPONSE_START  SHE_CON_STATE_RESPONSE_START
#define CON_STATE_WRITE           SHE_CON_STATE_WRITE
#define CON_STATE_RESPONSE_END    SHE_CON_STATE_RESPONSE_END
#define CON_STATE_ERROR           SHE_CON_STATE_ERROR
#define CON_STATE_CLOSE           SHE_CON_STATE_CLOSE

she_connection_t *she_connection_init(int client_fd, struct sockaddr_in *client_addr);
void she_connection_free(she_connection_t *con);
void she_connection_reset(she_connection_t *con);

const char *she_connection_state_to_str(she_con_state_t state);
int she_connection_state_machine(she_server_t *srv, she_connection_t *con);
void she_connection_handle_fdevent(void *context, int revents);
void she_generate_error_response(she_server_t *srv, she_connection_t *con, int status);

/* Compatibility macros */
#define connection_init she_connection_init
#define connection_free she_connection_free
#define connection_reset she_connection_reset
#define connection_state_to_str she_connection_state_to_str
#define connection_state_machine she_connection_state_machine
#define connection_handle_fdevent she_connection_handle_fdevent

#endif /* SHE_CONNECTION_H */
