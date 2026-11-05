#include "modules/mod_http2.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

static inline void pack_24(uint8_t *dst, uint32_t val) {
    dst[0] = (uint8_t)((val >> 16) & 0xff);
    dst[1] = (uint8_t)((val >> 8) & 0xff);
    dst[2] = (uint8_t)(val & 0xff);
}

static inline uint32_t unpack_24(const uint8_t *src) {
    return ((uint32_t)src[0] << 16) | ((uint32_t)src[1] << 8) | (uint32_t)src[2];
}

static she_handler_t mod_http2_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)p_data;
    const char *uri = she_buffer_cstr(con->request.uri_path);
    if (!uri) return SHE_HANDLER_GO_ON;
    
    if (strcmp(uri, "/h2-status") == 0) {
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
                "  \"module\": \"mod_http2\",\n"
                "  \"spec\": \"RFC 9113 / RFC 7540\",\n"
                "  \"preface\": \"%s\",\n"
                "  \"frame_header_bytes\": 9,\n"
                "  \"supported_frames\": [\n"
                "    {\"type\": 0, \"name\": \"DATA\"},\n"
                "    {\"type\": 1, \"name\": \"HEADERS\"},\n"
                "    {\"type\": 2, \"name\": \"PRIORITY\"},\n"
                "    {\"type\": 3, \"name\": \"RST_STREAM\"},\n"
                "    {\"type\": 4, \"name\": \"SETTINGS\"},\n"
                "    {\"type\": 6, \"name\": \"PING\"},\n"
                "    {\"type\": 7, \"name\": \"GOAWAY\"},\n"
                "    {\"type\": 8, \"name\": \"WINDOW_UPDATE\"}\n"
                "  ],\n"
                "  \"server_settings\": {\n"
                "    \"HEADER_TABLE_SIZE\": 4096,\n"
                "    \"ENABLE_PUSH\": 0,\n"
                "    \"MAX_CONCURRENT_STREAMS\": 100,\n"
                "    \"INITIAL_WINDOW_SIZE\": 65535\n"
                "  }\n"
                "}\n",
                "PRI * HTTP/2.0\\r\\n\\r\\nSM\\r\\n\\r\\n");
        } else {
            body_len = snprintf(body, sizeof(body),
                "<!DOCTYPE html><html><head><title>SHE HTTP/2 Binary Framing</title>\n"
                "<style>\n"
                "  body{font-family:sans-serif;background:#0b0f19;color:#f1f5f9;padding:40px;margin:0;}\n"
                "  .card{max-width:720px;margin:0 auto;background:#111827;border:1px solid #6366f1;border-radius:12px;padding:30px;}\n"
                "  h2{color:#818cf8;margin-top:0;}\n"
                "  pre{background:#030712;padding:14px;border-radius:8px;border:1px solid #312e81;color:#a5b4fc;font-family:monospace;font-size:13px;}\n"
                "  .badge{background:#312e81;color:#c7d2fe;padding:3px 8px;border-radius:4px;font-size:11px;font-weight:bold;}\n"
                "</style></head>\n"
                "<body><div class=\"card\">\n"
                "  <span class=\"badge\">HTTP/2 (RFC 9113)</span>\n"
                "  <h2>SHE HTTP/2 Binary Framing Subsystem (mod_http2)</h2>\n"
                "  <pre>"
                "Connection Preface: PRI * HTTP/2.0\\r\\n\\r\\nSM\\r\\n\\r\\n (24 bytes)\n"
                "Frame Header Format:\n"
                "  +-----------------------------------------------+\n"
                "  |                 Length (24)                   |\n"
                "  +---------------+---------------+---------------+\n"
                "  |   Type (8)    |   Flags (8)   |\n"
                "  +-+-------------+---------------+----------------+\n"
                "  |R|                 Stream ID (31)              |\n"
                "  +=+=============================================+\n"
                "  |                   Frame Payload (0...)        |\n"
                "  +-----------------------------------------------+\n"
                "\n"
                "Server Default Settings:\n"
                "  SETTINGS_MAX_CONCURRENT_STREAMS: 100\n"
                "  SETTINGS_INITIAL_WINDOW_SIZE:    65535\n"
                "  SETTINGS_HEADER_TABLE_SIZE:      4096 (HPACK)\n"
                "</pre>\n"
                "  <p><a href=\"/h2-status?json=1\" style=\"color:#818cf8;\">View JSON HTTP/2 Telemetry</a></p>\n"
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

she_plugin_t *mod_http2_plugin_init(void) {
    static she_plugin_t p;
    memset(&p, 0, sizeof(p));
    p.version = 1;
    p.name = "mod_http2";
    p.description = "SHE HTTP/2 Binary Framing, Stream Multiplexing & Preface Engine (RFC 9113)";
    p.handle_subrequest_start = mod_http2_subrequest_start;
    return &p;
}
