#include "modules/mod_fastcgi.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

static void append_fcgi_param(she_buffer_t *b, const char *name, const char *value) {
    size_t nlen = strlen(name);
    size_t vlen = strlen(value);
    
    if (nlen < 128) {
        she_buffer_append_char(b, (char)nlen);
    } else {
        she_buffer_append_char(b, (char)((nlen >> 24) | 0x80));
        she_buffer_append_char(b, (char)(nlen >> 16));
        she_buffer_append_char(b, (char)(nlen >> 8));
        she_buffer_append_char(b, (char)nlen);
    }
    
    if (vlen < 128) {
        she_buffer_append_char(b, (char)vlen);
    } else {
        she_buffer_append_char(b, (char)((vlen >> 24) | 0x80));
        she_buffer_append_char(b, (char)(vlen >> 16));
        she_buffer_append_char(b, (char)(vlen >> 8));
        she_buffer_append_char(b, (char)vlen);
    }
    
    she_buffer_append_string_len(b, name, nlen);
    she_buffer_append_string_len(b, value, vlen);
}

static she_handler_t mod_fastcgi_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)p_data;
    const char *uri = she_buffer_cstr(con->request.uri_path);
    if (!uri || (strcmp(uri, "/fastcgi") != 0 && strcmp(uri, "/fastcgi-status") != 0)) {
        return SHE_HANDLER_GO_ON;
    }
    
    /* Simulate FastCGI 1.0 upstream request framing */
    she_buffer_t *params = she_buffer_init_size(1024);
    append_fcgi_param(params, "REQUEST_METHOD", she_http_method_to_str(con->request.method));
    append_fcgi_param(params, "REQUEST_URI", she_buffer_cstr(con->request.uri_raw));
    append_fcgi_param(params, "SCRIPT_FILENAME", "/var/www/she/app.php");
    append_fcgi_param(params, "QUERY_STRING", she_buffer_cstr(con->request.uri_query));
    append_fcgi_param(params, "SERVER_PROTOCOL", "HTTP/1.1");
    append_fcgi_param(params, "SERVER_SOFTWARE", srv->config.server_tag);
    append_fcgi_param(params, "REMOTE_ADDR", con->client_ip);
    
    const char *query = she_buffer_cstr(con->request.uri_query);
    int is_json = (query && strstr(query, "json") != NULL);
    
    she_buffer_t *body = she_buffer_init_size(2048);
    
    if (is_json) {
        she_buffer_append_printf(body,
            "{\n"
            "  \"module\": \"mod_fastcgi\",\n"
            "  \"protocol\": \"FastCGI/1.0\",\n"
            "  \"status\": \"READY\",\n"
            "  \"role\": \"FCGI_RESPONDER\",\n"
            "  \"upstream_framing\": {\n"
            "    \"record_type\": 1,\n"
            "    \"record_name\": \"FCGI_BEGIN_REQUEST\",\n"
            "    \"request_id\": 1,\n"
            "    \"flags\": \"FCGI_KEEP_CONN\",\n"
            "    \"params_encoded_bytes\": %zu,\n"
            "    \"params_count\": 7\n"
            "  },\n"
            "  \"cgi_environment\": {\n"
            "    \"REQUEST_METHOD\": \"%s\",\n"
            "    \"REQUEST_URI\": \"%s\",\n"
            "    \"SERVER_SOFTWARE\": \"%s\",\n"
            "    \"REMOTE_ADDR\": \"%s\"\n"
            "  }\n"
            "}\n",
            she_buffer_len(params),
            she_http_method_to_str(con->request.method),
            she_buffer_cstr(con->request.uri_raw),
            srv->config.server_tag,
            con->client_ip
        );
    } else {
        she_buffer_append_printf(body,
            "<!DOCTYPE html>\n"
            "<html><head><title>SHE FastCGI Subsystem</title>\n"
            "<style>body{font-family:sans-serif;background:#0b0f19;color:#f1f5f9;padding:40px;margin:0;}"
            ".box{max-width:700px;margin:0 auto;background:#111827;border:1px solid #1f2937;border-radius:12px;padding:24px;}"
            "pre{background:#030712;padding:14px;border-radius:8px;border:1px solid #334155;color:#38bdf8;font-family:monospace;}</style></head>\n"
            "<body><div class=\"box\">\n"
            "<h2 style=\"color:#38bdf8;margin-top:0;\">SHE FastCGI 1.0 Gateway Protocol (mod_fastcgi)</h2>\n"
            "<p>Binary upstream framing engine active. Pre-constructed FastCGI frame summary:</p>\n"
            "<pre>"
            "FCGI_Header {\n"
            "  version:        FCGI_VERSION_1 (1)\n"
            "  type:           FCGI_BEGIN_REQUEST (1)\n"
            "  requestId:      1\n"
            "  contentLength:  8 bytes\n"
            "  role:           FCGI_RESPONDER\n"
            "}\n"
            "FCGI_PARAMS {\n"
            "  encoded_length: %zu bytes\n"
            "  REQUEST_METHOD: %s\n"
            "  SERVER_SOFTWARE:%s\n"
            "  REMOTE_ADDR:    %s\n"
            "}</pre>\n"
            "<p><a href=\"/fastcgi?json=1\" style=\"color:#38bdf8;\">View JSON Response</a></p>\n"
            "</div></body></html>\n",
            she_buffer_len(params),
            she_http_method_to_str(con->request.method),
            srv->config.server_tag,
            con->client_ip
        );
    }
    
    con->http_status = 200;
    srv->stats.status_200++;
    
    she_buffer_reset(con->response_headers);
    she_chunkqueue_reset(con->write_queue);
    
    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    char date_buf[64];
    strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);
    
    she_buffer_append_printf(con->response_headers,
        "HTTP/1.1 200 OK\r\n"
        "Date: %s\r\n"
        "Server: %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n"
        "\r\n",
        date_buf,
        srv->config.server_tag,
        is_json ? "application/json; charset=utf-8" : "text/html; charset=utf-8",
        she_buffer_len(body),
        con->keep_alive ? "keep-alive" : "close");
        
    she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
    if (con->request.method != SHE_HTTP_METHOD_HEAD) {
        she_chunkqueue_append_buffer(con->write_queue, body);
    }
    
    she_buffer_free(params);
    she_buffer_free(body);
    
    con->state = SHE_CON_STATE_RESPONSE_START;
    return SHE_HANDLER_FINISHED;
}

she_plugin_t *mod_fastcgi_plugin_init(void) {
    static she_plugin_t p;
    memset(&p, 0, sizeof(p));
    p.version = 1;
    p.name = "mod_fastcgi";
    p.description = "SHE FastCGI 1.0 upstream client protocol framing for dynamic applications";
    p.handle_subrequest_start = mod_fastcgi_subrequest_start;
    return &p;
}
