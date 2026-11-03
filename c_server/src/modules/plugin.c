#include "modules/plugin.h"
#include "core/server.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void plugins_init(server_t *srv) {
    if (!srv) return;
    srv->plugins.count = 0;
    memset(srv->plugins.plugins, 0, sizeof(srv->plugins.plugins));
}

void plugins_cleanup(server_t *srv) {
    if (!srv) return;
    for (size_t i = 0; i < srv->plugins.count; i++) {
        plugin_t *p = srv->plugins.plugins[i];
        if (p && p->cleanup) {
            p->cleanup(srv, p);
        }
    }
    srv->plugins.count = 0;
}

int plugins_register(server_t *srv, plugin_t *p) {
    if (!srv || !p || srv->plugins.count >= MAX_PLUGINS) return -1;
    
    if (p->init) {
        if (p->init(srv, p) != HANDLER_GO_ON) {
            fprintf(stderr, "[WARN] Plugin %s init failed\n", p->name ? p->name : "unknown");
            return -1;
        }
    }
    
    srv->plugins.plugins[srv->plugins.count++] = p;
    return 0;
}

handler_t plugins_call_handle_uri_clean(server_t *srv, connection_t *con) {
    if (!srv || !con) return HANDLER_GO_ON;
    
    for (size_t i = 0; i < srv->plugins.count; i++) {
        plugin_t *p = srv->plugins.plugins[i];
        if (p && p->handle_uri_clean) {
            handler_t res = p->handle_uri_clean(srv, con, p->data);
            if (res != HANDLER_GO_ON) return res;
        }
    }
    return HANDLER_GO_ON;
}

handler_t plugins_call_handle_subrequest_start(server_t *srv, connection_t *con) {
    if (!srv || !con) return HANDLER_GO_ON;
    
    for (size_t i = 0; i < srv->plugins.count; i++) {
        plugin_t *p = srv->plugins.plugins[i];
        if (p && p->handle_subrequest_start) {
            handler_t res = p->handle_subrequest_start(srv, con, p->data);
            if (res != HANDLER_GO_ON) return res;
        }
    }
    return HANDLER_GO_ON;
}

handler_t plugins_call_handle_response_done(server_t *srv, connection_t *con) {
    if (!srv || !con) return HANDLER_GO_ON;
    
    for (size_t i = 0; i < srv->plugins.count; i++) {
        plugin_t *p = srv->plugins.plugins[i];
        if (p && p->handle_response_done) {
            handler_t res = p->handle_response_done(srv, con, p->data);
            if (res != HANDLER_GO_ON) return res;
        }
    }
    return HANDLER_GO_ON;
}
