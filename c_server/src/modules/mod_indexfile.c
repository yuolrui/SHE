#include "modules/mod_indexfile.h"
#include "core/she_server.h"
#include "core/she_connection.h"
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

static she_handler_t mod_indexfile_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)p_data;
    const char *docroot = srv->config.document_root;
    if (!she_buffer_is_empty(con->request.vhost_docroot)) {
        docroot = she_buffer_cstr(con->request.vhost_docroot);
    }
    
    const char *uri_path = she_buffer_cstr(con->request.uri_path);
    char full_path[2048];
    snprintf(full_path, sizeof(full_path), "%s%s", docroot, uri_path);
    
    struct stat st;
    if (stat(full_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return SHE_HANDLER_GO_ON;
    }
    
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
            if (has_slash) {
                she_buffer_append_string(con->request.uri_path, default_index_files[i]);
            } else {
                she_buffer_append_char(con->request.uri_path, '/');
                she_buffer_append_string(con->request.uri_path, default_index_files[i]);
            }
            return SHE_HANDLER_GO_ON;
        }
    }
    
    return SHE_HANDLER_GO_ON;
}

she_plugin_t *mod_indexfile_plugin_init(void) {
    static she_plugin_t p;
    memset(&p, 0, sizeof(p));
    p.version = 1;
    p.name = "mod_indexfile";
    p.description = "Configurable directory index file resolution cascade";
    p.handle_subrequest_start = mod_indexfile_subrequest_start;
    return &p;
}
