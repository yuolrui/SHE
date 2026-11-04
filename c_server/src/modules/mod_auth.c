#include "modules/mod_auth.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_decode(const char *src, size_t src_len, char *dst, size_t dst_max) {
    int dtable[256];
    memset(dtable, 0x80, sizeof(dtable));
    for (size_t i = 0; i < sizeof(base64_table) - 1; i++) {
        dtable[(unsigned char)base64_table[i]] = (int)i;
    }
    dtable['='] = 0;
    
    size_t di = 0;
    unsigned int buf = 0;
    int bits = 0;
    
    for (size_t i = 0; i < src_len; i++) {
        unsigned char c = (unsigned char)src[i];
        if (isspace(c)) continue;
        if (c == '=') break;
        int val = dtable[c];
        if (val & 0x80) continue;
        
        buf = (buf << 6) | val;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (di + 1 >= dst_max) return -1;
            dst[di++] = (char)((buf >> bits) & 0xff);
        }
    }
    dst[di] = '\0';
    return (int)di;
}

static void send_401_unauthorized(she_server_t *srv, she_connection_t *con) {
    con->http_status = 401;
    srv->stats.status_401++;
    
    she_buffer_reset(con->response_headers);
    she_chunkqueue_reset(con->write_queue);
    
    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    char date_buf[64];
    strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);
    
    const char *body = 
        "<!DOCTYPE html>\n"
        "<html><head><title>401 Authorization Required</title></head>\n"
        "<body style=\"font-family:sans-serif;text-align:center;padding:50px;background:#0b0f19;color:#f1f5f9;\">\n"
        "<h1 style=\"color:#f43f5e;\">401 Authorization Required</h1>\n"
        "<p>This area is protected by SHE (Secure HTTP Engine) Basic Authentication.</p>\n"
        "<p>Credentials required: realm 'SHE Secure Area'</p>\n"
        "<hr style=\"border-color:#334155;max-width:500px;\">\n"
        "<p style=\"color:#94a3b8;font-size:12px;\">SHE/1.0 Engine</p>\n"
        "</body></html>\n";
    int body_len = (int)strlen(body);
    
    she_buffer_append_printf(con->response_headers,
        "HTTP/1.1 401 Unauthorized\r\n"
        "Date: %s\r\n"
        "Server: %s\r\n"
        "WWW-Authenticate: Basic realm=\"SHE Secure Area\"\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: %s\r\n"
        "\r\n",
        date_buf,
        srv->config.server_tag,
        body_len,
        con->keep_alive ? "keep-alive" : "close");
        
    she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
    if (con->request.method != SHE_HTTP_METHOD_HEAD) {
        she_chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
    }
    
    con->state = SHE_CON_STATE_RESPONSE_START;
}

static she_handler_t mod_auth_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)p_data;
    const char *uri = she_buffer_cstr(con->request.uri_path);
    if (!uri) return SHE_HANDLER_GO_ON;
    
    /* Protected paths: check srv->config.auth_path or default to /secure or /admin */
    int is_protected = 0;
    if (srv->config.auth_enabled && srv->config.auth_path[0] != '\0') {
        char prefix[128];
        strncpy(prefix, srv->config.auth_path, sizeof(prefix) - 1);
        prefix[sizeof(prefix) - 1] = '\0';
        size_t plen = strlen(prefix);
        while (plen > 1 && prefix[plen - 1] == '/') {
            prefix[--plen] = '\0';
        }
        if (strcmp(uri, prefix) == 0 || 
            (strncmp(uri, prefix, plen) == 0 && (uri[plen] == '/' || uri[plen] == '\0'))) {
            is_protected = 1;
        }
    } else {
        if (strcmp(uri, "/secure") == 0 || strncmp(uri, "/secure/", 8) == 0 ||
            strcmp(uri, "/admin") == 0 || strncmp(uri, "/admin/", 7) == 0) {
            is_protected = 1;
        }
    }
    
    if (!is_protected) {
        return SHE_HANDLER_GO_ON;
    }
    
    /* Check Authorization header */
    const char *auth_hdr = she_buffer_cstr(con->request.authorization);
    if (!auth_hdr || !*auth_hdr) {
        auth_hdr = she_http_request_get_header(&con->request, "Authorization");
    }
    
    if (!auth_hdr || strncasecmp(auth_hdr, "Basic ", 6) != 0) {
        send_401_unauthorized(srv, con);
        return SHE_HANDLER_FINISHED;
    }
    
    const char *b64 = auth_hdr + 6;
    while (*b64 == ' ') b64++;
    
    char creds[128];
    if (base64_decode(b64, strlen(b64), creds, sizeof(creds)) <= 0) {
        send_401_unauthorized(srv, con);
        return SHE_HANDLER_FINISHED;
    }
    
    char expected[128];
    const char *user = srv->config.auth_user[0] ? srv->config.auth_user : "admin";
    const char *pass = srv->config.auth_pass[0] ? srv->config.auth_pass : "she_secret123";
    snprintf(expected, sizeof(expected), "%s:%s", user, pass);
    
    /* Also allow demo account demo:she_demo */
    if (strcmp(creds, expected) != 0 && strcmp(creds, "demo:she_demo") != 0) {
        send_401_unauthorized(srv, con);
        return SHE_HANDLER_FINISHED;
    }
    
    /* Credentials valid, proceed */
    return SHE_HANDLER_GO_ON;
}

she_plugin_t *mod_auth_plugin_init(void) {
    static she_plugin_t p;
    memset(&p, 0, sizeof(p));
    p.version = 1;
    p.name = "mod_auth";
    p.description = "SHE HTTP Basic Authentication subsystem (RFC 7617) for secure directories";
    p.handle_subrequest_start = mod_auth_subrequest_start;
    return &p;
}
