#include "core/she_server.h"
#include "net/she_net.h"
#include "modules/she_plugin.h"
#include "modules/mod_status.h"
#include "modules/mod_indexfile.h"
#include "modules/mod_dirlisting.h"
#include "modules/mod_accesslog.h"
#include "modules/mod_auth.h"
#include "modules/mod_vhost.h"
#include "modules/mod_fastcgi.h"
#include "modules/mod_rewrite.h"
#include "modules/mod_deflate.h"
#include "modules/mod_proxy.h"
#include "modules/mod_websocket.h"
#include "modules/mod_ssl.h"
#include "modules/mod_http2.h"
#include "security/she_security.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

she_server_t *g_she_server = NULL;

static void she_server_accept_handler(void *context, int revents) {
    she_server_t *srv = (she_server_t *)context;
    if (!srv || !(revents & SHE_FDEVENT_IN)) return;
    
    while (1) {
        struct sockaddr_in client_addr;
        int client_fd = she_net_accept_client(srv->srv_socket, &client_addr);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            break;
        }
        
        if (client_fd >= srv->max_con_fds) {
            close(client_fd);
            continue;
        }
        
        if (srv->stats.active_connections >= (uint32_t)srv->config.max_connections) {
            close(client_fd);
            continue;
        }
        
        she_connection_t *con = she_connection_init(client_fd, &client_addr);
        if (!con) {
            close(client_fd);
            continue;
        }
        
        con->keep_alive_max_requests = srv->config.max_keepalive_requests;
        
        srv->fd_to_con[client_fd] = con;
        srv->stats.active_connections++;
        srv->stats.total_connections++;
        
        if (she_fdevent_register(srv->ev, client_fd, she_connection_handle_fdevent, con) != 0) {
            she_server_close_connection(srv, con);
            continue;
        }
        
        she_connection_state_machine(srv, con);
    }
}

she_server_t *she_server_init(const she_server_config_t *cfg) {
    she_server_t *srv = (she_server_t *)calloc(1, sizeof(she_server_t));
    if (!srv) return NULL;
    
    if (cfg) {
        srv->config = *cfg;
    } else {
        strcpy(srv->config.bind_ip, "0.0.0.0");
        srv->config.port = 8899;
        strcpy(srv->config.document_root, "./www");
        srv->config.max_connections = 1024;
        srv->config.keepalive_timeout_sec = 15;
        srv->config.max_keepalive_requests = 100;
        strcpy(srv->config.server_tag, "SHE/1.0 (Secure HTTP Engine)");
        srv->config.verbose = 1;
        
        srv->config.security_cfg.enable_rate_limit = 1;
        srv->config.security_cfg.rate_limit_rps = 40.0;
        srv->config.security_cfg.rate_limit_burst = 30.0;
        srv->config.security_cfg.enable_security_headers = 1;
        srv->config.security_cfg.strict_host_check = 1;
        srv->config.security_cfg.block_smuggling = 1;
    }
    
    srv->srv_socket = -1;
    srv->max_con_fds = srv->config.max_connections + 64;
    
    srv->ev = she_fdevent_init(srv->max_con_fds);
    if (!srv->ev) {
        free(srv);
        return NULL;
    }
    
    srv->fd_to_con = (she_connection_t **)calloc(srv->max_con_fds, sizeof(she_connection_t *));
    if (!srv->fd_to_con) {
        she_fdevent_free(srv->ev);
        free(srv);
        return NULL;
    }
    
    srv->stats.start_time = time(NULL);
    srv->is_running = 0;
    
    /* Initialize security subsystem */
    srv->security = she_security_init(&srv->config.security_cfg);
    
    /* Initialize and register SHE modular plugin pipeline */
    she_plugins_init(srv);
    she_plugins_register(srv, mod_rewrite_plugin_init());
    she_plugins_register(srv, mod_vhost_plugin_init());
    she_plugins_register(srv, mod_auth_plugin_init());
    she_plugins_register(srv, mod_ssl_plugin_init());
    she_plugins_register(srv, mod_http2_plugin_init());
    she_plugins_register(srv, mod_websocket_plugin_init());
    she_plugins_register(srv, mod_proxy_plugin_init());
    she_plugins_register(srv, mod_deflate_plugin_init());
    she_plugins_register(srv, mod_status_plugin_init());
    she_plugins_register(srv, mod_fastcgi_plugin_init());
    she_plugins_register(srv, mod_indexfile_plugin_init());
    she_plugins_register(srv, mod_dirlisting_plugin_init());
    she_plugins_register(srv, mod_accesslog_plugin_init());
    
    g_she_server = srv;
    return srv;
}

void she_server_free(she_server_t *srv) {
    if (!srv) return;
    
    she_server_stop(srv);
    she_plugins_cleanup(srv);
    
    if (srv->security) {
        she_security_free(srv->security);
        srv->security = NULL;
    }
    
    if (srv->fd_to_con) {
        for (int i = 0; i < srv->max_con_fds; i++) {
            if (srv->fd_to_con[i]) {
                she_connection_free(srv->fd_to_con[i]);
                srv->fd_to_con[i] = NULL;
            }
        }
        free(srv->fd_to_con);
    }
    
    if (srv->ev) {
        she_fdevent_free(srv->ev);
        srv->ev = NULL;
    }
    
    if (g_she_server == srv) g_she_server = NULL;
    free(srv);
}

int she_server_start(she_server_t *srv) {
    if (!srv) return -1;
    
    srv->srv_socket = she_net_create_server_socket(
        srv->config.bind_ip, 
        srv->config.port, 
        srv->config.max_connections
    );
    
    if (srv->srv_socket < 0) {
        fprintf(stderr, "[ERROR] Failed to bind to %s:%d (errno: %s)\n",
                srv->config.bind_ip, srv->config.port, strerror(errno));
        return -1;
    }
    
    if (she_fdevent_register(srv->ev, srv->srv_socket, she_server_accept_handler, srv) != 0) {
        fprintf(stderr, "[ERROR] Failed to register server socket with fdevent\n");
        close(srv->srv_socket);
        srv->srv_socket = -1;
        return -1;
    }
    
    she_fdevent_event_set(srv->ev, srv->srv_socket, SHE_FDEVENT_IN);
    srv->is_running = 1;
    
    if (srv->config.verbose) {
        printf("[INFO] SHE (Secure HTTP Engine) listening on %s:%d (docroot: %s)\n",
               srv->config.bind_ip, srv->config.port, srv->config.document_root);
    }
    
    return 0;
}

void she_server_stop(she_server_t *srv) {
    if (!srv) return;
    srv->is_running = 0;
    
    if (srv->srv_socket >= 0) {
        she_fdevent_unregister(srv->ev, srv->srv_socket);
        close(srv->srv_socket);
        srv->srv_socket = -1;
    }
}

void she_server_close_connection(she_server_t *srv, she_connection_t *con) {
    if (!srv || !con) return;
    
    int fd = con->fd;
    if (fd >= 0 && fd < srv->max_con_fds) {
        she_fdevent_unregister(srv->ev, fd);
        close(fd);
        srv->fd_to_con[fd] = NULL;
    }
    
    if (srv->stats.active_connections > 0) {
        srv->stats.active_connections--;
    }
    
    she_connection_free(con);
}

static void she_server_check_timeouts(she_server_t *srv) {
    time_t now = time(NULL);
    for (int i = 0; i < srv->max_con_fds; i++) {
        she_connection_t *con = srv->fd_to_con[i];
        if (con && (now - con->state_timestamp) > srv->config.keepalive_timeout_sec) {
            she_server_close_connection(srv, con);
        }
    }
}

int she_server_step(she_server_t *srv, int timeout_ms) {
    if (!srv || !srv->is_running) return -1;
    
    int nfds = she_fdevent_poll(srv->ev, timeout_ms);
    she_server_check_timeouts(srv);
    return nfds;
}
