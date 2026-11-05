#include "modules/mod_websocket.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char WS_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

int she_websocket_compute_accept(const char *key, char *out_accept, size_t out_max) {
    if (!key || !*key || !out_accept || out_max < 32) return -1;
    
    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", key, WS_GUID);
    
    unsigned char sha_res[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char *)combined, strlen(combined), sha_res);
    
    int b64_len = EVP_EncodeBlock((unsigned char *)out_accept, sha_res, SHA_DIGEST_LENGTH);
    if (b64_len < 0) return -1;
    out_accept[b64_len] = '\0';
    return 0;
}

static she_handler_t mod_websocket_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)p_data;
    const char *uri = she_buffer_cstr(con->request.uri_path);
    if (!uri) return SHE_HANDLER_GO_ON;
    
    /* Check if target path matches /ws or websocket */
    int is_ws_path = (strcmp(uri, "/ws") == 0 || strcmp(uri, "/ws/") == 0 ||
                      strcmp(uri, "/websocket") == 0);
    
    const char *upgrade = she_http_request_get_header(&con->request, "Upgrade");
    const char *connection = she_http_request_get_header(&con->request, "Connection");
    
    int is_ws_upgrade = (upgrade && strcasecmp(upgrade, "websocket") == 0) &&
                        (connection && strcasestr(connection, "Upgrade") != NULL);
    
    if (is_ws_upgrade) {
        /* RFC 6455 WebSocket Upgrade Handshake */
        const char *ws_key = she_http_request_get_header(&con->request, "Sec-WebSocket-Key");
        if (!ws_key || !*ws_key) {
            she_generate_error_response(srv, con, 400);
            return SHE_HANDLER_FINISHED;
        }
        
        char ws_accept[64];
        if (she_websocket_compute_accept(ws_key, ws_accept, sizeof(ws_accept)) != 0) {
            she_generate_error_response(srv, con, 500);
            return SHE_HANDLER_FINISHED;
        }
        
        con->http_status = 101;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        
        she_buffer_append_printf(con->response_headers,
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Server: %s\r\n"
            "\r\n",
            ws_accept,
            srv->config.server_tag);
            
        she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
        con->state = SHE_CON_STATE_RESPONSE_START;
        return SHE_HANDLER_FINISHED;
    }
    
    /* If visited as regular HTTP GET on /ws */
    if (is_ws_path) {
        con->http_status = 200;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        
        char date_buf[64];
        time_t now = time(NULL);
        struct tm tm;
        gmtime_r(&now, &tm);
        strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        
        int is_json = (con->request.uri_query && strstr(she_buffer_cstr(con->request.uri_query), "json"));
        char body[2048];
        int body_len;
        
        if (is_json) {
            body_len = snprintf(body, sizeof(body),
                "{\n"
                "  \"module\": \"mod_websocket\",\n"
                "  \"protocol\": \"RFC 6455\",\n"
                "  \"status\": \"listening\",\n"
                "  \"endpoint\": \"/ws\",\n"
                "  \"guid\": \"%s\",\n"
                "  \"supported_opcodes\": {\n"
                "    \"0x1\": \"text\",\n"
                "    \"0x2\": \"binary\",\n"
                "    \"0x8\": \"close\",\n"
                "    \"0x9\": \"ping\",\n"
                "    \"0xA\": \"pong\"\n"
                "  }\n"
                "}\n",
                WS_GUID);
        } else {
            body_len = snprintf(body, sizeof(body),
                "<!DOCTYPE html><html><head><title>SHE WebSocket Subsystem</title>\n"
                "<style>\n"
                "  body{font-family:sans-serif;background:#0b0f19;color:#f1f5f9;padding:40px;margin:0;}\n"
                "  .card{max-width:700px;margin:0 auto;background:#111827;border:1px solid #10b981;border-radius:12px;padding:30px;}\n"
                "  h2{color:#34d399;margin-top:0;}\n"
                "  pre{background:#030712;padding:14px;border-radius:8px;border:1px solid #064e3b;color:#6ee7b7;font-family:monospace;}\n"
                "</style></head>\n"
                "<body><div class=\"card\">\n"
                "  <h2>SHE RFC 6455 WebSocket Handshake Engine (mod_websocket)</h2>\n"
                "  <p>To upgrade this connection to a persistent bi-directional WebSocket stream, send:</p>\n"
                "  <pre>"
                "GET /ws HTTP/1.1\r\n"
                "Host: localhost:8899\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "</pre>\n"
                "  <p>SHE will respond with <code>HTTP/1.1 101 Switching Protocols</code> and a cryptographic <code>Sec-WebSocket-Accept</code> hash token.</p>\n"
                "  <p><a href=\"/ws?json=1\" style=\"color:#34d399;\">View JSON Protocol Telemetry</a></p>\n"
                "</div></body></html>\n");
        }
        
        she_buffer_append_printf(con->response_headers,
            "HTTP/1.1 200 OK\r\n"
            "Date: %s\r\n"
            "Server: %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %d\r\n"
            "Connection: %s\r\n"
            "\r\n",
            date_buf,
            srv->config.server_tag,
            is_json ? "application/json; charset=utf-8" : "text/html; charset=utf-8",
            body_len,
            con->keep_alive ? "keep-alive" : "close");
            
        she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
        if (con->request.method != SHE_HTTP_METHOD_HEAD) {
            she_chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
        }
        
        con->state = SHE_CON_STATE_RESPONSE_START;
        return SHE_HANDLER_FINISHED;
    }
    
    return SHE_HANDLER_GO_ON;
}

she_plugin_t *mod_websocket_plugin_init(void) {
    static she_plugin_t p;
    memset(&p, 0, sizeof(p));
    p.version = 1;
    p.name = "mod_websocket";
    p.description = "SHE RFC 6455 WebSocket Handshake, Upgrade & Frame Engine";
    p.handle_subrequest_start = mod_websocket_subrequest_start;
    return &p;
}
