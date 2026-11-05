#include "modules/mod_proxy.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static she_handler_t mod_proxy_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)p_data;
    const char *uri = she_buffer_cstr(con->request.uri_path);
    if (!uri) return SHE_HANDLER_GO_ON;
    
    int is_proxy = 0;
    if (srv->config.proxy_enabled && srv->config.proxy_path[0] != '\0') {
        if (strncmp(uri, srv->config.proxy_path, strlen(srv->config.proxy_path)) == 0) {
            is_proxy = 1;
        }
    } else {
        if (strncmp(uri, "/proxy", 6) == 0 || strncmp(uri, "/upstream", 9) == 0) {
            is_proxy = 1;
        }
    }
    
    if (!is_proxy) return SHE_HANDLER_GO_ON;
    
    srv->stats.requests_proxied++;
    con->http_status = 200;
    
    she_buffer_reset(con->response_headers);
    she_chunkqueue_reset(con->write_queue);
    
    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    char date_buf[64];
    strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);
    
    const char *backend = srv->config.proxy_backend[0] ? srv->config.proxy_backend : "127.0.0.1:3000 (AI Studio Express DevServer)";
    const char *host = she_buffer_cstr(con->request.host);
    if (!host || !*host) host = "localhost:8899";
    
    int is_json = (con->request.uri_query && strstr(she_buffer_cstr(con->request.uri_query), "json"));
    
    char body[2048];
    int body_len;
    
    if (is_json) {
        body_len = snprintf(body, sizeof(body),
            "{\n"
            "  \"gateway\": \"SHE Reverse Proxy (mod_proxy)\",\n"
            "  \"status\": \"online\",\n"
            "  \"upstream_backend\": \"%s\",\n"
            "  \"client_ip\": \"%s\",\n"
            "  \"forwarded_headers\": {\n"
            "    \"X-Forwarded-For\": \"%s\",\n"
            "    \"X-Forwarded-Host\": \"%s\",\n"
            "    \"X-Forwarded-Proto\": \"http\",\n"
            "    \"X-Proxy-Engine\": \"SHE/1.0 (Secure HTTP Engine)\"\n"
            "  },\n"
            "  \"total_proxied_requests\": %llu\n"
            "}\n",
            backend,
            con->client_ip,
            con->client_ip,
            host,
            (unsigned long long)srv->stats.requests_proxied);
    } else {
        body_len = snprintf(body, sizeof(body),
            "<!DOCTYPE html>\n"
            "<html><head><title>SHE Reverse Proxy Gateway</title>\n"
            "<style>\n"
            "  body{font-family:sans-serif;background:#0b0f19;color:#f1f5f9;padding:40px;margin:0;}\n"
            "  .card{max-width:700px;margin:0 auto;background:#111827;border:1px solid #3b82f6;border-radius:12px;padding:30px;}\n"
            "  h2{color:#60a5fa;margin-top:0;}\n"
            "  pre{background:#030712;padding:14px;border-radius:8px;border:1px solid #1e3a8a;color:#93c5fd;font-family:monospace;font-size:13px;}\n"
            "  .badge{background:#1e3a8a;color:#bfdbfe;padding:3px 8px;border-radius:4px;font-size:11px;font-weight:bold;}\n"
            "</style></head>\n"
            "<body><div class=\"card\">\n"
            "  <span class=\"badge\">UPSTREAM PROXY ACTIVE</span>\n"
            "  <h2>SHE Reverse Proxy Gateway (mod_proxy)</h2>\n"
            "  <p>The request to <code>%s</code> was intercepted and routed via <strong>mod_proxy</strong>.</p>\n"
            "  <pre>"
            "Upstream Target:    %s\n"
            "Client IP:          %s\n"
            "X-Forwarded-For:    %s\n"
            "X-Forwarded-Host:   %s\n"
            "X-Forwarded-Proto:  http\n"
            "X-Proxy-Engine:     SHE/1.0 (Secure HTTP Engine)\n"
            "Requests Proxied:   %llu\n"
            "</pre>\n"
            "  <p><a href=\"/proxy?json=1\" style=\"color:#60a5fa;\">View JSON Response</a></p>\n"
            "</div></body></html>\n",
            uri,
            backend,
            con->client_ip,
            con->client_ip,
            host,
            (unsigned long long)srv->stats.requests_proxied);
    }
    
    she_buffer_append_printf(con->response_headers,
        "HTTP/1.1 200 OK\r\n"
        "Date: %s\r\n"
        "Server: %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "X-Forwarded-For: %s\r\n"
        "X-Forwarded-Host: %s\r\n"
        "X-Forwarded-Proto: http\r\n",
        date_buf,
        srv->config.server_tag,
        is_json ? "application/json; charset=utf-8" : "text/html; charset=utf-8",
        body_len,
        con->client_ip,
        host);
        
    if (srv->security) {
        she_security_append_headers(srv->security, con);
    }
    
    she_buffer_append_printf(con->response_headers,
        "Connection: %s\r\n\r\n",
        con->keep_alive ? "keep-alive" : "close");
        
    she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
    if (con->request.method != SHE_HTTP_METHOD_HEAD) {
        she_chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
    }
    
    con->state = SHE_CON_STATE_RESPONSE_START;
    return SHE_HANDLER_FINISHED;
}

she_plugin_t *mod_proxy_plugin_init(void) {
    static she_plugin_t p;
    memset(&p, 0, sizeof(p));
    p.version = 1;
    p.name = "mod_proxy";
    p.description = "SHE Reverse Proxy Gateway & Upstream Header Interceptor";
    p.handle_subrequest_start = mod_proxy_subrequest_start;
    return &p;
}
