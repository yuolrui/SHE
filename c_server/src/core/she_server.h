#ifndef SHE_SERVER_H
#define SHE_SERVER_H

#include <time.h>
#include <stdint.h>
#include <sys/types.h>
#include "event/she_fdevent.h"
#include "core/she_connection.h"
#include "modules/she_plugin.h"
#include "security/she_security.h"

typedef struct {
    char bind_ip[64];
    int port;
    char document_root[512];
    int max_connections;
    int keepalive_timeout_sec;
    int max_keepalive_requests;
    char server_tag[128];
    int verbose;
    
    char config_file[256];
    she_security_config_t security_cfg;
    
    /* Basic Auth settings */
    int auth_enabled;
    char auth_path[128];     /* e.g. "/secure/" or "/admin/" */
    char auth_user[64];     /* default: "admin" */
    char auth_pass[64];     /* default: "she_secret123" */
    
    /* Vhost settings */
    int vhost_enabled;
    char vhost_map[512];    /* e.g. "api.local:./www/api,docs.local:./www/docs" */
} she_server_config_t;

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
    uint64_t status_401;
    uint64_t status_403;
    uint64_t status_404;
    uint64_t status_405;
    uint64_t status_429;
    uint64_t status_500;
} she_server_stats_t;

struct she_server {
    int srv_socket;
    she_fdevent_system_t *ev;
    she_server_config_t config;
    she_server_stats_t stats;
    
    she_connection_t **fd_to_con;
    int max_con_fds;
    
    she_plugin_array_t plugins;
    she_security_t *security;
    
    volatile int is_running;
};

/* Compatibility typedefs */
typedef she_server_config_t server_config_t;
typedef she_server_stats_t server_stats_t;
typedef she_server_t server_t;

extern she_server_t *g_she_server;
#define g_server g_she_server

she_server_t *she_server_init(const she_server_config_t *cfg);
void she_server_free(she_server_t *srv);
int she_server_start(she_server_t *srv);
void she_server_stop(she_server_t *srv);
int she_server_step(she_server_t *srv, int timeout_ms);

void she_server_close_connection(she_server_t *srv, she_connection_t *con);

/* Compatibility macros */
#define server_init she_server_init
#define server_free she_server_free
#define server_start she_server_start
#define server_stop she_server_stop
#define server_step she_server_step
#define server_close_connection she_server_close_connection

#endif /* SHE_SERVER_H */
