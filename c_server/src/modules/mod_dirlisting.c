#include "modules/mod_dirlisting.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

static void get_http_date(time_t t, char *buf, size_t max) {
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(buf, max, "%a, %d %b %Y %H:%M:%S GMT", &tm);
}

static void format_size(off_t size, char *buf, size_t max) {
    if (size < 1024) {
        snprintf(buf, max, "%lld B", (long long)size);
    } else if (size < 1024 * 1024) {
        snprintf(buf, max, "%.1f KB", (double)size / 1024.0);
    } else {
        snprintf(buf, max, "%.1f MB", (double)size / (1024.0 * 1024.0));
    }
}

static she_handler_t mod_dirlisting_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
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
    
    DIR *dir = opendir(full_path);
    if (!dir) {
        return SHE_HANDLER_GO_ON;
    }
    
    she_buffer_t *body = she_buffer_init_size(4096);
    she_buffer_append_printf(body,
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "  <title>Index of %s</title>\n"
        "  <style>\n"
        "    body { font-family: system-ui, -apple-system, sans-serif; background: #0b0f19; color: #f1f5f9; padding: 40px; margin: 0; }\n"
        "    .card { max-width: 860px; margin: 0 auto; background: #111827; border: 1px solid #1f2937; border-radius: 12px; padding: 28px; }\n"
        "    h1 { font-size: 20px; color: #38bdf8; margin-top: 0; border-bottom: 1px solid #1f2937; padding-bottom: 12px; }\n"
        "    table { width: 100%%; border-collapse: collapse; font-family: monospace; font-size: 13px; margin-top: 15px; }\n"
        "    th, td { padding: 9px 12px; text-align: left; border-bottom: 1px solid #1f2937; }\n"
        "    th { color: #94a3b8; font-size: 11px; text-transform: uppercase; }\n"
        "    tr:hover { background: #1e293b; }\n"
        "    a { color: #38bdf8; text-decoration: none; }\n"
        "    a:hover { text-decoration: underline; }\n"
        "    .dir { color: #fbbf24; font-weight: bold; }\n"
        "    .footer { margin-top: 20px; font-size: 11px; color: #64748b; }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <div class=\"card\">\n"
        "    <h1>Index of %s</h1>\n"
        "    <table>\n"
        "      <thead>\n"
        "        <tr><th>Name</th><th>Size</th><th>Last Modified</th></tr>\n"
        "      </thead>\n"
        "      <tbody>\n",
        uri_path, uri_path
    );
    
    if (strcmp(uri_path, "/") != 0) {
        she_buffer_append_string(body,
            "        <tr><td><a href=\"../\" class=\"dir\">[Parent Directory] ../</a></td><td>-</td><td>-</td></tr>\n"
        );
    }
    
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        
        char item_path[4096];
        snprintf(item_path, sizeof(item_path), "%s/%s", full_path, ent->d_name);
        
        struct stat item_st;
        if (stat(item_path, &item_st) != 0) {
            continue;
        }
        
        int is_sub_dir = S_ISDIR(item_st.st_mode);
        char size_buf[32];
        if (is_sub_dir) {
            strcpy(size_buf, "-");
        } else {
            format_size(item_st.st_size, size_buf, sizeof(size_buf));
        }
        
        char date_buf[64];
        struct tm tm;
        localtime_r(&item_st.st_mtime, &tm);
        strftime(date_buf, sizeof(date_buf), "%Y-%m-%d %H:%M", &tm);
        
        she_buffer_append_printf(body,
            "        <tr>"
            "<td><a href=\"%s%s%s\" class=\"%s\">%s%s</a></td>"
            "<td>%s</td>"
            "<td>%s</td>"
            "</tr>\n",
            uri_path,
            (uri_path[strlen(uri_path) - 1] == '/') ? "" : "/",
            ent->d_name,
            is_sub_dir ? "dir" : "file",
            ent->d_name,
            is_sub_dir ? "/" : "",
            size_buf,
            date_buf
        );
    }
    closedir(dir);
    
    she_buffer_append_printf(body,
        "      </tbody>\n"
        "    </table>\n"
        "    <div class=\"footer\">%s mod_dirlisting · Directory Auto-Index</div>\n"
        "  </div>\n"
        "</body>\n"
        "</html>\n",
        srv->config.server_tag
    );
    
    char cur_date[64];
    get_http_date(time(NULL), cur_date, sizeof(cur_date));
    
    con->http_status = 200;
    srv->stats.status_200++;
    
    she_buffer_reset(con->response_headers);
    she_chunkqueue_reset(con->write_queue);
    
    she_buffer_append_printf(con->response_headers,
        "HTTP/1.1 200 OK\r\n"
        "Date: %s\r\n"
        "Server: %s\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n"
        "\r\n",
        cur_date,
        srv->config.server_tag,
        she_buffer_len(body),
        con->keep_alive ? "keep-alive" : "close"
    );
    
    she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
    if (con->request.method != SHE_HTTP_METHOD_HEAD) {
        she_chunkqueue_append_buffer(con->write_queue, body);
    }
    
    she_buffer_free(body);
    con->state = SHE_CON_STATE_RESPONSE_START;
    return SHE_HANDLER_FINISHED;
}

she_plugin_t *mod_dirlisting_plugin_init(void) {
    static she_plugin_t p;
    memset(&p, 0, sizeof(p));
    p.version = 1;
    p.name = "mod_dirlisting";
    p.description = "Automatic HTML directory indexing when no index file exists";
    p.handle_subrequest_start = mod_dirlisting_subrequest_start;
    return &p;
}
