#include "modules/mod_status.h"
#include "core/server.h"
#include "core/connection.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

static void get_http_date(time_t t, char *buf, size_t max) {
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(buf, max, "%a, %d %b %Y %H:%M:%S GMT", &tm);
}

static handler_t mod_status_subrequest_start(server_t *srv, connection_t *con, void *p_data) {
    (void)p_data;
    const char *uri = buffer_cstr(con->request.uri_path);
    if (!uri || strcmp(uri, "/server-status") != 0) {
        return HANDLER_GO_ON;
    }
    
    time_t now = time(NULL);
    double uptime = difftime(now, srv->stats.start_time);
    if (uptime <= 0) uptime = 1;
    
    double rps = (double)srv->stats.total_requests / uptime;
    const char *query = buffer_cstr(con->request.uri_query);
    int is_json = (query && (strstr(query, "json") != NULL || strstr(query, "format=json") != NULL));
    
    buffer_t *body = buffer_init_size(2048);
    
    if (is_json) {
        buffer_append_printf(body,
            "{\n"
            "  \"server\": \"%s\",\n"
            "  \"uptime_seconds\": %.0f,\n"
            "  \"total_connections\": %llu,\n"
            "  \"active_connections\": %u,\n"
            "  \"total_requests\": %llu,\n"
            "  \"requests_per_second\": %.2f,\n"
            "  \"bytes_read\": %llu,\n"
            "  \"bytes_written\": %llu,\n"
            "  \"status_codes\": {\n"
            "    \"200\": %llu,\n"
            "    \"206\": %llu,\n"
            "    \"304\": %llu,\n"
            "    \"400\": %llu,\n"
            "    \"403\": %llu,\n"
            "    \"404\": %llu,\n"
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
            (unsigned long long)srv->stats.status_200,
            (unsigned long long)srv->stats.status_206,
            (unsigned long long)srv->stats.status_304,
            (unsigned long long)srv->stats.status_400,
            (unsigned long long)srv->stats.status_403,
            (unsigned long long)srv->stats.status_404,
            (unsigned long long)srv->stats.status_500
        );
    } else {
        buffer_append_printf(body,
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head>\n"
            "  <title>c-lighttpd Server Status</title>\n"
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
            "    <h1>c-lighttpd Status (mod_status)</h1>\n"
            "    <p>Server Version: <strong>%s</strong> · Model: Single-process epoll + 10-state FSM</p>\n"
            "    <div class=\"grid\">\n"
            "      <div class=\"item\"><div class=\"item-val\">%.0f s</div><div class=\"item-lbl\">Uptime</div></div>\n"
            "      <div class=\"item\"><div class=\"item-val\">%u</div><div class=\"item-lbl\">Active Connections</div></div>\n"
            "      <div class=\"item\"><div class=\"item-val\">%llu</div><div class=\"item-lbl\">Total Requests</div></div>\n"
            "      <div class=\"item\"><div class=\"item-val\">%.1f req/s</div><div class=\"item-lbl\">Average Throughput</div></div>\n"
            "    </div>\n"
            "    <h3>HTTP Response Distribution</h3>\n"
            "    <table>\n"
            "      <tr><th>Status Code</th><th>Meaning</th><th>Count</th></tr>\n"
            "      <tr><td style=\"color:#34d399;\">200</td><td>OK</td><td>%llu</td></tr>\n"
            "      <tr><td style=\"color:#34d399;\">206</td><td>Partial Content (Range)</td><td>%llu</td></tr>\n"
            "      <tr><td style=\"color:#60a5fa;\">304</td><td>Not Modified (Cache)</td><td>%llu</td></tr>\n"
            "      <tr><td style=\"color:#fbbf24;\">400</td><td>Bad Request</td><td>%llu</td></tr>\n"
            "      <tr><td style=\"color:#fbbf24;\">403</td><td>Forbidden</td><td>%llu</td></tr>\n"
            "      <tr><td style=\"color:#fbbf24;\">404</td><td>Not Found</td><td>%llu</td></tr>\n"
            "      <tr><td style=\"color:#f87171;\">500</td><td>Internal Error</td><td>%llu</td></tr>\n"
            "    </table>\n"
            "    <p style=\"margin-top:20px; font-size:12px; color:#64748b;\">Format toggle: <a href=\"/server-status?json\">View Raw JSON API</a></p>\n"
            "  </div>\n"
            "</body>\n"
            "</html>\n",
            srv->config.server_tag,
            uptime,
            srv->stats.active_connections,
            (unsigned long long)srv->stats.total_requests,
            rps,
            (unsigned long long)srv->stats.status_200,
            (unsigned long long)srv->stats.status_206,
            (unsigned long long)srv->stats.status_304,
            (unsigned long long)srv->stats.status_400,
            (unsigned long long)srv->stats.status_403,
            (unsigned long long)srv->stats.status_404,
            (unsigned long long)srv->stats.status_500
        );
    }
    
    char date_buf[64];
    get_http_date(time(NULL), date_buf, sizeof(date_buf));
    
    con->http_status = 200;
    srv->stats.status_200++;
    
    buffer_reset(con->response_headers);
    chunkqueue_reset(con->write_queue);
    
    buffer_append_printf(con->response_headers,
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
        buffer_len(body),
        con->keep_alive ? "keep-alive" : "close"
    );
    
    chunkqueue_append_buffer(con->write_queue, con->response_headers);
    if (con->request.method != HTTP_METHOD_HEAD) {
        chunkqueue_append_buffer(con->write_queue, body);
    }
    
    buffer_free(body);
    con->state = CON_STATE_RESPONSE_START;
    return HANDLER_FINISHED;
}

static plugin_t g_mod_status = {
    .version = 1,
    .name = "mod_status",
    .description = "Online server runtime metrics and status dashboard (/server-status)",
    .init = NULL,
    .cleanup = NULL,
    .handle_uri_clean = NULL,
    .handle_subrequest_start = mod_status_subrequest_start,
    .handle_response_done = NULL,
    .data = NULL,
};

plugin_t *mod_status_plugin_init(void) {
    return &g_mod_status;
}
