#include "modules/mod_status.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

static void get_http_date(time_t t, char *buf, size_t max) {
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(buf, max, "%a, %d %b %Y %H:%M:%S GMT", &tm);
}

static she_handler_t mod_status_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)p_data;
    const char *uri = she_buffer_cstr(con->request.uri_path);
    if (!uri || strcmp(uri, "/server-status") != 0) {
        return SHE_HANDLER_GO_ON;
    }
    
    time_t now = time(NULL);
    double uptime = difftime(now, srv->stats.start_time);
    if (uptime <= 0) uptime = 1;
    
    double rps = (double)srv->stats.total_requests / uptime;
    const char *query = she_buffer_cstr(con->request.uri_query);
    int is_json = (query && (strstr(query, "json") != NULL || strstr(query, "format=json") != NULL));
    
    she_buffer_t *body = she_buffer_init_size(2048);
    
    uint64_t rate_blocks = srv->security ? srv->security->total_rate_blocks : 0;
    uint64_t host_blocks = srv->security ? srv->security->total_host_rejections : 0;
    
    if (is_json) {
        she_buffer_append_printf(body,
            "{\n"
            "  \"server\": \"%s\",\n"
            "  \"engine\": \"SHE (Secure HTTP Engine)\",\n"
            "  \"uptime_seconds\": %.0f,\n"
            "  \"total_connections\": %llu,\n"
            "  \"active_connections\": %u,\n"
            "  \"total_requests\": %llu,\n"
            "  \"requests_per_second\": %.2f,\n"
            "  \"bytes_read\": %llu,\n"
            "  \"bytes_written\": %llu,\n"
            "  \"security\": {\n"
            "    \"rate_limiting_active\": %d,\n"
            "    \"rate_limit_blocks\": %llu,\n"
            "    \"host_header_rejections\": %llu,\n"
            "    \"headers_injected\": %d\n"
            "  },\n"
            "  \"phase4\": {\n"
            "    \"deflate_active\": %d,\n"
            "    \"requests_compressed\": %llu,\n"
            "    \"bytes_saved\": %llu,\n"
            "    \"requests_proxied\": %llu,\n"
            "    \"redirects_301\": %llu,\n"
            "    \"redirects_302\": %llu\n"
            "  },\n"
            "  \"status_codes\": {\n"
            "    \"200\": %llu,\n"
            "    \"206\": %llu,\n"
            "    \"301\": %llu,\n"
            "    \"302\": %llu,\n"
            "    \"304\": %llu,\n"
            "    \"400\": %llu,\n"
            "    \"401\": %llu,\n"
            "    \"403\": %llu,\n"
            "    \"404\": %llu,\n"
            "    \"405\": %llu,\n"
            "    \"429\": %llu,\n"
            "    \"500\": %llu\n"
            "  }\n"
            "}\n",
            srv->config.server_tag,
            uptime,
            (unsigned long long)srv->stats.total_connections,
            srv->stats.active_connections,
            (unsigned long long)srv->stats.total_requests,
            rps,
            (unsigned long long)srv->stats.bytes_read,
            (unsigned long long)srv->stats.bytes_written,
            srv->security && srv->security->config.enable_rate_limit ? 1 : 0,
            (unsigned long long)rate_blocks,
            (unsigned long long)host_blocks,
            srv->security && srv->security->config.enable_security_headers ? 1 : 0,
            srv->config.deflate_enabled ? 1 : 0,
            (unsigned long long)srv->stats.requests_compressed,
            (unsigned long long)srv->stats.bytes_compressed_saved,
            (unsigned long long)srv->stats.requests_proxied,
            (unsigned long long)srv->stats.status_301,
            (unsigned long long)srv->stats.status_302,
            (unsigned long long)srv->stats.status_200,
            (unsigned long long)srv->stats.status_206,
            (unsigned long long)srv->stats.status_301,
            (unsigned long long)srv->stats.status_302,
            (unsigned long long)srv->stats.status_304,
            (unsigned long long)srv->stats.status_400,
            (unsigned long long)srv->stats.status_401,
            (unsigned long long)srv->stats.status_403,
            (unsigned long long)srv->stats.status_404,
            (unsigned long long)srv->stats.status_405,
            (unsigned long long)srv->stats.status_429,
            (unsigned long long)srv->stats.status_500
        );
    } else {
        she_buffer_append_printf(body,
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head>\n"
            "  <title>SHE Server Status</title>\n"
            "  <style>\n"
            "    body { font-family: system-ui, sans-serif; background: #0b0f19; color: #f1f5f9; padding: 40px; margin: 0; }\n"
            "    .card { max-width: 760px; margin: 0 auto; background: #111827; border: 1px solid #1f2937; border-radius: 12px; padding: 30px; }\n"
            "    h1 { color: #38bdf8; margin-top: 0; font-size: 24px; border-bottom: 1px solid #1f2937; padding-bottom: 12px; }\n"
            "    .grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 16px; margin: 20px 0; }\n"
            "    .item { background: #1e293b; padding: 14px; border-radius: 8px; border: 1px solid #334155; }\n"
            "    .item-val { font-family: monospace; font-size: 20px; font-weight: bold; color: #38bdf8; }\n"
            "    .item-lbl { font-size: 11px; color: #94a3b8; text-transform: uppercase; margin-top: 4px; }\n"
            "    table { width: 100%%; border-collapse: collapse; margin-top: 15px; font-family: monospace; font-size: 13px; }\n"
            "    td, th { padding: 8px 12px; border-bottom: 1px solid #1f2937; text-align: left; }\n"
            "    th { color: #94a3b8; }\n"
            "    a { color: #38bdf8; text-decoration: none; }\n"
            "  </style>\n"
            "</head>\n"
            "<body>\n"
            "  <div class=\"card\">\n"
            "    <h1>SHE Server Status (Secure HTTP Engine)</h1>\n"
            "    <p>Server Version: <strong>%s</strong> · Model: Single-process epoll + 10-state FSM</p>\n"
            "    <div class=\"grid\">\n"
            "      <div class=\"item\"><div class=\"item-val\">%.0f s</div><div class=\"item-lbl\">Uptime</div></div>\n"
            "      <div class=\"item\"><div class=\"item-val\">%u</div><div class=\"item-lbl\">Active Connections</div></div>\n"
            "      <div class=\"item\"><div class=\"item-val\">%llu</div><div class=\"item-lbl\">Total Requests</div></div>\n"
            "      <div class=\"item\"><div class=\"item-val\">%.1f req/s</div><div class=\"item-lbl\">Average Throughput</div></div>\n"
            "    </div>\n"
            "    <h3>Security Metrics (she_security)</h3>\n"
            "    <table>\n"
            "      <tr><th>Metric</th><th>Status</th></tr>\n"
            "      <tr><td>IP Rate Limiting Blocks (429)</td><td>%llu</td></tr>\n"
            "      <tr><td>Host Header Rejections</td><td>%llu</td></tr>\n"
            "      <tr><td>Security Headers Injection</td><td>Active (nosniff, SAMEORIGIN, XSS-1)</td></tr>\n"
            "    </table>\n"
            "    <h3>HTTP Response Distribution</h3>\n"
            "    <table>\n"
            "      <tr><th>Status Code</th><th>Meaning</th><th>Count</th></tr>\n"
            "      <tr><td>200</td><td>OK</td><td>%llu</td></tr>\n"
            "      <tr><td>206</td><td>Partial Content</td><td>%llu</td></tr>\n"
            "      <tr><td>304</td><td>Not Modified</td><td>%llu</td></tr>\n"
            "      <tr><td>400</td><td>Bad Request</td><td>%llu</td></tr>\n"
            "      <tr><td>401</td><td>Unauthorized</td><td>%llu</td></tr>\n"
            "      <tr><td>403</td><td>Forbidden</td><td>%llu</td></tr>\n"
            "      <tr><td>404</td><td>Not Found</td><td>%llu</td></tr>\n"
            "      <tr><td>429</td><td>Too Many Requests</td><td>%llu</td></tr>\n"
            "      <tr><td>500</td><td>Internal Error</td><td>%llu</td></tr>\n"
            "    </table>\n"
            "    <p style=\"margin-top:20px;\"><a href=\"/server-status?json=1\">View JSON format</a> | <a href=\"/\">Return Home</a></p>\n"
            "  </div>\n"
            "</body>\n"
            "</html>\n",
            srv->config.server_tag,
            uptime,
            srv->stats.active_connections,
            (unsigned long long)srv->stats.total_requests,
            rps,
            (unsigned long long)rate_blocks,
            (unsigned long long)host_blocks,
            (unsigned long long)srv->stats.status_200,
            (unsigned long long)srv->stats.status_206,
            (unsigned long long)srv->stats.status_304,
            (unsigned long long)srv->stats.status_400,
            (unsigned long long)srv->stats.status_401,
            (unsigned long long)srv->stats.status_403,
            (unsigned long long)srv->stats.status_404,
            (unsigned long long)srv->stats.status_429,
            (unsigned long long)srv->stats.status_500
        );
    }
    
    con->http_status = 200;
    srv->stats.status_200++;
    
    she_buffer_reset(con->response_headers);
    she_chunkqueue_reset(con->write_queue);
    
    char date_buf[64];
    get_http_date(now, date_buf, sizeof(date_buf));
    
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
    
    she_buffer_free(body);
    con->state = SHE_CON_STATE_RESPONSE_START;
    return SHE_HANDLER_FINISHED;
}

she_plugin_t *mod_status_plugin_init(void) {
    static she_plugin_t p;
    memset(&p, 0, sizeof(p));
    p.version = 1;
    p.name = "mod_status";
    p.description = "SHE runtime metrics and online status monitor (/server-status)";
    p.handle_subrequest_start = mod_status_subrequest_start;
    return &p;
}
