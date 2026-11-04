#include "modules/mod_vhost.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char hostname[128];
    char docroot[256];
} she_vhost_entry_t;

#define MAX_VHOSTS 16

static she_vhost_entry_t g_vhosts[MAX_VHOSTS];
static size_t g_vhost_count = 0;

static void parse_vhost_map(const char *map_str) {
    g_vhost_count = 0;
    if (!map_str || !*map_str) {
        /* Default virtual hosts */
        strncpy(g_vhosts[0].hostname, "docs.local", sizeof(g_vhosts[0].hostname) - 1);
        strncpy(g_vhosts[0].docroot, "./www/docs", sizeof(g_vhosts[0].docroot) - 1);
        
        strncpy(g_vhosts[1].hostname, "api.local", sizeof(g_vhosts[1].hostname) - 1);
        strncpy(g_vhosts[1].docroot, "./www/api", sizeof(g_vhosts[1].docroot) - 1);
        
        strncpy(g_vhosts[2].hostname, "secure.local", sizeof(g_vhosts[2].hostname) - 1);
        strncpy(g_vhosts[2].docroot, "./www/secure", sizeof(g_vhosts[2].docroot) - 1);
        
        g_vhost_count = 3;
        return;
    }
    
    char buf[512];
    strncpy(buf, map_str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    
    char *token = strtok(buf, ",");
    while (token && g_vhost_count < MAX_VHOSTS) {
        char *colon = strchr(token, ':');
        if (colon) {
            *colon = '\0';
            strncpy(g_vhosts[g_vhost_count].hostname, token, sizeof(g_vhosts[0].hostname) - 1);
            strncpy(g_vhosts[g_vhost_count].docroot, colon + 1, sizeof(g_vhosts[0].docroot) - 1);
            g_vhost_count++;
        }
        token = strtok(NULL, ",");
    }
}

static she_handler_t mod_vhost_uri_clean(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)p_data;
    if (!srv || !con) return SHE_HANDLER_GO_ON;
    
    const char *host = she_buffer_cstr(con->request.host);
    if (!host || !*host) return SHE_HANDLER_GO_ON;
    
    /* Strip port if present e.g. "docs.local:8899" -> "docs.local" */
    char host_only[128];
    strncpy(host_only, host, sizeof(host_only) - 1);
    host_only[sizeof(host_only) - 1] = '\0';
    char *colon = strchr(host_only, ':');
    if (colon) *colon = '\0';
    
    for (size_t i = 0; i < g_vhost_count; i++) {
        if (strcasecmp(host_only, g_vhosts[i].hostname) == 0) {
            she_buffer_copy_string(con->request.vhost_docroot, g_vhosts[i].docroot);
            break;
        }
    }
    
    return SHE_HANDLER_GO_ON;
}

static she_handler_t mod_vhost_init(she_server_t *srv, she_plugin_t *p) {
    (void)p;
    parse_vhost_map(srv->config.vhost_map);
    return SHE_HANDLER_GO_ON;
}

she_plugin_t *mod_vhost_plugin_init(void) {
    static she_plugin_t p;
    memset(&p, 0, sizeof(p));
    p.version = 1;
    p.name = "mod_vhost";
    p.description = "SHE Virtual Host router mapping Host headers to isolated document roots";
    p.init = mod_vhost_init;
    p.handle_uri_clean = mod_vhost_uri_clean;
    return &p;
}
