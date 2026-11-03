#include "core/server.h"
#include "net/network.h"
#include "modules/plugin.h"
#include "modules/mod_status.h"
#include "modules/mod_indexfile.h"
#include "modules/mod_dirlisting.h"
#include "modules/mod_accesslog.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

server_t *g_server = NULL;

static void server_accept_handler(void *context, int revents) {
    server_t *srv = (server_t *)context;
    if (!srv || !(revents & FDEVENT_IN)) return;
    
    while (1) {
        struct sockaddr_in client_addr;
        int client_fd = network_accept_client(srv->srv_socket, &client_addr);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; /* All pending connections accepted */
            }
            break;
        }
        
        if (client_fd >= srv->max_con_fds) {
            close(client_fd);
            continue;
        }
        
        if (srv->stats.active_connections >= (uint32_t)srv->config.max_connections) {
            /* Server full, reject */
            close(client_fd);
            continue;
        }
        
        connection_t *con = connection_init(client_fd, &client_addr);
        if (!con) {
            close(client_fd);
            continue;
        }
        
        con->keep_alive_max_requests = srv->config.max_keepalive_requests;
        
        srv->fd_to_con[client_fd] = con;
        srv->stats.active_connections++;
        srv->stats.total_connections++;
        
        if (fdevent_register(srv->ev, client_fd, connection_handle_fdevent, con) != 0) {
            server_close_connection(srv, con);
            continue;
        }
        
        /* Start state machine for connection */
        connection_state_machine(srv, con);
    }
}

server_t *server_init(const server_config_t *cfg) {
    server_t *srv = (server_t *)calloc(1, sizeof(server_t));
    if (!srv) return NULL;
    
    if (cfg) {
        srv->config = *cfg;
    } else {
        strcpy(srv->config.bind_ip, "0.0.0.0");
        srv->config.port = 8080;
        strcpy(srv->config.document_root, "./www");
        srv->config.max_connections = 1024;
        srv->config.keepalive_timeout_sec = 15;
        srv->config.max_keepalive_requests = 100;
        strcpy(srv->config.server_tag, "c-lighttpd/1.0");
        srv->config.verbose = 1;
    }
    
    srv->srv_socket = -1;
    srv->max_con_fds = srv->config.max_connections + 64;
    
    srv->ev = fdevent_init(srv->max_con_fds);
    if (!srv->ev) {
        free(srv);
        return NULL;
    }
    
    srv->fd_to_con = (connection_t **)calloc(srv->max_con_fds, sizeof(connection_t *));
    if (!srv->fd_to_con) {
        fdevent_free(srv->ev);
        free(srv);
        return NULL;
    }
    
    srv->stats.start_time = time(NULL);
    srv->is_running = 0;
    
    /* Initialize and register Lighttpd plugin pipeline */
    plugins_init(srv);
    plugins_register(srv, mod_status_plugin_init());
    plugins_register(srv, mod_indexfile_plugin_init());
    plugins_register(srv, mod_dirlisting_plugin_init());
    plugins_register(srv, mod_accesslog_plugin_init());
    
    g_server = srv;
    return srv;
}

void server_free(server_t *srv) {
    if (!srv) return;
    
    server_stop(srv);
    plugins_cleanup(srv);
    
    if (srv->fd_to_con) {
        for (int i = 0; i < srv->max_con_fds; i++) {
            if (srv->fd_to_con[i]) {
                connection_free(srv->fd_to_con[i]);
                srv->fd_to_con[i] = NULL;
            }
        }
        free(srv->fd_to_con);
    }
    
    if (srv->ev) {
        fdevent_free(srv->ev);
        srv->ev = NULL;
    }
    
    if (g_server == srv) g_server = NULL;
    free(srv);
}

int server_start(server_t *srv) {
    if (!srv) return -1;
    
    srv->srv_socket = network_create_server_socket(
        srv->config.bind_ip, 
        srv->config.port, 
        srv->config.max_connections
    );
    
    if (srv->srv_socket < 0) {
        fprintf(stderr, "[ERROR] Failed to bind to %s:%d (errno: %s)\n",
                srv->config.bind_ip, srv->config.port, strerror(errno));
        return -1;
    }
    
    if (fdevent_register(srv->ev, srv->srv_socket, server_accept_handler, srv) != 0) {
        fprintf(stderr, "[ERROR] Failed to register server socket with fdevent\n");
        close(srv->srv_socket);
        srv->srv_socket = -1;
        return -1;
    }
    
    fdevent_event_set(srv->ev, srv->srv_socket, FDEVENT_IN);
    srv->is_running = 1;
    
    if (srv->config.verbose) {
        printf("[INFO] c-lighttpd listening on %s:%d (docroot: %s)\n",
               srv->config.bind_ip, srv->config.port, srv->config.document_root);
    }
    
    return 0;
}

void server_stop(server_t *srv) {
    if (!srv) return;
    srv->is_running = 0;
    
    if (srv->srv_socket >= 0) {
        fdevent_unregister(srv->ev, srv->srv_socket);
        close(srv->srv_socket);
        srv->srv_socket = -1;
    }
}

void server_close_connection(server_t *srv, connection_t *con) {
    if (!srv || !con) return;
    
    int fd = con->fd;
    if (fd >= 0 && fd < srv->max_con_fds) {
        fdevent_unregister(srv->ev, fd);
        close(fd);
        srv->fd_to_con[fd] = NULL;
    }
    
    if (srv->stats.active_connections > 0) {
        srv->stats.active_connections--;
    }
    
    connection_free(con);
}

static void server_check_timeouts(server_t *srv) {
    time_t now = time(NULL);
    for (int i = 0; i < srv->max_con_fds; i++) {
        connection_t *con = srv->fd_to_con[i];
        if (con && (now - con->state_timestamp) > srv->config.keepalive_timeout_sec) {
            /* Inactivity timeout */
            server_close_connection(srv, con);
        }
    }
}

int server_step(server_t *srv, int timeout_ms) {
    if (!srv || !srv->is_running) return -1;
    
    int nfds = fdevent_poll(srv->ev, timeout_ms);
    server_check_timeouts(srv);
    return nfds;
}
