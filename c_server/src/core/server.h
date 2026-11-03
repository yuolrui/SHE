#ifndef SERVER_H
#define SERVER_H

#include <time.h>
#include <sys/types.h>
#include "fdevent.h"
#include "connection.h"

typedef struct {
    char bind_ip[64];
    int port;
    char document_root[512];
    int max_connections;
    int keepalive_timeout_sec;
    int max_keepalive_requests;
    char server_tag[128];
    int verbose;
} server_config_t;

typedef struct {
    uint64_t total_connections;
    uint64_t total_requests;
    uint32_t active_connections;
    uint64_t bytes_read;
    uint64_t bytes_written;
    time_t start_time;
    
    /* HTTP response code counters */
    uint64_t status_200;
    uint64_t status_206;
    uint64_t status_304;
    uint64_t status_400;
    uint64_t status_403;
    uint64_t status_404;
    uint64_t status_500;
} server_stats_t;

struct server {
    int srv_socket;
    fdevent_system_t *ev;
    server_config_t config;
    server_stats_t stats;
    
    connection_t **fd_to_con;
    int max_con_fds;
    
    volatile int is_running;
};

extern server_t *g_server;

server_t *server_init(const server_config_t *cfg);
void server_free(server_t *srv);
int server_start(server_t *srv);
void server_stop(server_t *srv);
int server_step(server_t *srv, int timeout_ms);

void server_close_connection(server_t *srv, connection_t *con);

#endif /* SERVER_H */
