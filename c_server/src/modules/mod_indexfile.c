#include "modules/mod_indexfile.h"
#include "core/server.h"
#include "core/connection.h"
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>

static const char *default_index_files[] = {
    "index.html",
    "index.htm",
    "default.html",
    "main.html",
    NULL
};

static handler_t mod_indexfile_subrequest_start(server_t *srv, connection_t *con, void *p_data) {
    (void)p_data;
    const char *uri_path = buffer_cstr(con->request.uri_path);
    char full_path[2048];
    snprintf(full_path, sizeof(full_path), "%s%s", srv->config.document_root, uri_path);
    
    struct stat st;
    if (stat(full_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return HANDLER_GO_ON;
    }
    
    /* Ensure trailing slash for directory */
    size_t dlen = strlen(full_path);
    int has_slash = (dlen > 0 && full_path[dlen - 1] == '/');
    
    for (int i = 0; default_index_files[i] != NULL; i++) {
        char try_path[2048];
        if (has_slash) {
            snprintf(try_path, sizeof(try_path), "%s%s", full_path, default_index_files[i]);
        } else {
            snprintf(try_path, sizeof(try_path), "%s/%s", full_path, default_index_files[i]);
        }
        
        struct stat idx_st;
        if (stat(try_path, &idx_st) == 0 && S_ISREG(idx_st.st_mode)) {
            /* Found index file! Rewrite con->request.uri_path */
            if (has_slash) {
                buffer_append_string(con->request.uri_path, default_index_files[i]);
            } else {
                buffer_append_char(con->request.uri_path, '/');
                buffer_append_string(con->request.uri_path, default_index_files[i]);
            }
            return HANDLER_GO_ON;
        }
    }
    
    return HANDLER_GO_ON;
}

static plugin_t g_mod_indexfile = {
    .version = 1,
    .name = "mod_indexfile",
    .description = "Configurable directory index file resolution cascade",
    .init = NULL,
    .cleanup = NULL,
    .handle_uri_clean = NULL,
    .handle_subrequest_start = mod_indexfile_subrequest_start,
    .handle_response_done = NULL,
    .data = NULL,
};

plugin_t *mod_indexfile_plugin_init(void) {
    return &g_mod_indexfile;
}
