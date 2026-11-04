#include "modules/she_plugin.h"
#include "core/she_server.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void she_plugins_init(she_server_t *srv) {
    if (!srv) return;
    srv->plugins.count = 0;
    memset(srv->plugins.plugins, 0, sizeof(srv->plugins.plugins));
}

void she_plugins_cleanup(she_server_t *srv) {
    if (!srv) return;
    for (size_t i = 0; i < srv->plugins.count; i++) {
        she_plugin_t *p = srv->plugins.plugins[i];
        if (p && p->cleanup) {
            p->cleanup(srv, p);
        }
    }
    srv->plugins.count = 0;
}

int she_plugins_register(she_server_t *srv, she_plugin_t *p) {
    if (!srv || !p || srv->plugins.count >= SHE_MAX_PLUGINS) return -1;
    
    if (p->init) {
        if (p->init(srv, p) != SHE_HANDLER_GO_ON) {
            fprintf(stderr, "[WARN] Plugin %s init failed\n", p->name ? p->name : "unknown");
            return -1;
        }
    }
    
    srv->plugins.plugins[srv->plugins.count++] = p;
    return 0;
}

she_handler_t she_plugins_call_handle_uri_clean(she_server_t *srv, she_connection_t *con) {
    if (!srv || !con) return SHE_HANDLER_GO_ON;
    
    for (size_t i = 0; i < srv->plugins.count; i++) {
        she_plugin_t *p = srv->plugins.plugins[i];
        if (p && p->handle_uri_clean) {
            she_handler_t res = p->handle_uri_clean(srv, con, p->data);
            if (res != SHE_HANDLER_GO_ON) return res;
        }
    }
    return SHE_HANDLER_GO_ON;
}

she_handler_t she_plugins_call_handle_subrequest_start(she_server_t *srv, she_connection_t *con) {
    if (!srv || !con) return SHE_HANDLER_GO_ON;
    
    for (size_t i = 0; i < srv->plugins.count; i++) {
        she_plugin_t *p = srv->plugins.plugins[i];
        if (p && p->handle_subrequest_start) {
            she_handler_t res = p->handle_subrequest_start(srv, con, p->data);
            if (res != SHE_HANDLER_GO_ON) return res;
        }
    }
    return SHE_HANDLER_GO_ON;
}

she_handler_t she_plugins_call_handle_response_done(she_server_t *srv, she_connection_t *con) {
    if (!srv || !con) return SHE_HANDLER_GO_ON;
    
    for (size_t i = 0; i < srv->plugins.count; i++) {
        she_plugin_t *p = srv->plugins.plugins[i];
        if (p && p->handle_response_done) {
            she_handler_t res = p->handle_response_done(srv, con, p->data);
            if (res != SHE_HANDLER_GO_ON) return res;
        }
    }
    return SHE_HANDLER_GO_ON;
}
