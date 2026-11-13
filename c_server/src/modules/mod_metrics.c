#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include "modules/mod_metrics.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include "core/she_chunkqueue.h"
#include "core/she_buffer.h"

static uint64_t g_metrics_scrapes = 0;

static she_handler_t she_metrics_init(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    g_metrics_scrapes = 0;
    return SHE_HANDLER_GO_ON;
}

static she_handler_t she_metrics_cleanup(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    return SHE_HANDLER_GO_ON;
}

static she_handler_t she_metrics_handle_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)p_data;
    if (!con || !con->request.uri_path) return SHE_HANDLER_GO_ON;

    const char *path = she_buffer_cstr(con->request.uri_path);
    if (!path) return SHE_HANDLER_GO_ON;

    if (strcmp(path, "/metrics") == 0 || strcmp(path, "/metrics-json") == 0) {
        g_metrics_scrapes++;
        int is_json = (strcmp(path, "/metrics-json") == 0);
        const char *query = con->request.uri_query ? she_buffer_cstr(con->request.uri_query) : NULL;
        if (query && strstr(query, "json=1")) {
            is_json = 1;
        }

        uint64_t total_con = srv ? srv->stats.total_connections : 0;
        uint64_t total_req = srv ? srv->stats.total_requests : 0;
        uint32_t active_con = srv ? srv->stats.active_connections : 0;
        uint64_t bytes_rx = srv ? srv->stats.bytes_read : 0;
        uint64_t bytes_tx = srv ? srv->stats.bytes_written : 0;
        uint64_t reloads = srv ? srv->stats.config_reloads : 0;
        uint64_t rate_blocks = (srv && srv->security) ? srv->security->total_rate_blocks : 0;
        uint64_t host_reject = (srv && srv->security) ? srv->security->total_host_rejections : 0;
        uint64_t st_200 = srv ? srv->stats.status_200 : 0;
        uint64_t st_401 = srv ? srv->stats.status_401 : 0;
        uint64_t st_404 = srv ? srv->stats.status_404 : 0;
        uint64_t st_429 = srv ? srv->stats.status_429 : 0;
        size_t plugin_cnt = srv ? srv->plugins.count : 0;
        long uptime = srv ? (long)(time(NULL) - srv->stats.start_time) : 0L;

        char body[4096];
        int body_len = 0;

        if (is_json) {
            body_len = snprintf(body, sizeof(body),
                "{\n"
                "  \"module\": \"mod_metrics\",\n"
                "  \"format\": \"OpenMetrics/Prometheus\",\n"
                "  \"scrapes_total\": %" PRIu64 ",\n"
                "  \"uptime_seconds\": %ld,\n"
                "  \"plugins_loaded\": %zu,\n"
                "  \"connections_total\": %" PRIu64 ",\n"
                "  \"connections_active\": %u,\n"
                "  \"requests_total\": %" PRIu64 ",\n"
                "  \"bytes_read_total\": %" PRIu64 ",\n"
                "  \"bytes_written_total\": %" PRIu64 ",\n"
                "  \"config_reloads_total\": %" PRIu64 ",\n"
                "  \"security_rate_blocks_total\": %" PRIu64 ",\n"
                "  \"security_host_rejections_total\": %" PRIu64 ",\n"
                "  \"http_status_200\": %" PRIu64 ",\n"
                "  \"http_status_401\": %" PRIu64 ",\n"
                "  \"http_status_404\": %" PRIu64 ",\n"
                "  \"http_status_429\": %" PRIu64 "\n"
                "}\n",
                g_metrics_scrapes, uptime, plugin_cnt, total_con, active_con,
                total_req, bytes_rx, bytes_tx, reloads, rate_blocks, host_reject,
                st_200, st_401, st_404, st_429
            );
        } else {
            body_len = snprintf(body, sizeof(body),
                "# HELP she_uptime_seconds Total uptime of the SHE C11 engine in seconds\n"
                "# TYPE she_uptime_seconds gauge\n"
                "she_uptime_seconds %ld\n"
                "# HELP she_plugins_loaded Number of active registered SHE plugins\n"
                "# TYPE she_plugins_loaded gauge\n"
                "she_plugins_loaded %zu\n"
                "# HELP she_connections_total Cumulative accepted TCP connections\n"
                "# TYPE she_connections_total counter\n"
                "she_connections_total %" PRIu64 "\n"
                "# HELP she_connections_active Current active open connections\n"
                "# TYPE she_connections_active gauge\n"
                "she_connections_active %u\n"
                "# HELP she_requests_total Cumulative HTTP requests processed\n"
                "# TYPE she_requests_total counter\n"
                "she_requests_total %" PRIu64 "\n"
                "# HELP she_bytes_read_total Total bytes read from sockets\n"
                "# TYPE she_bytes_read_total counter\n"
                "she_bytes_read_total %" PRIu64 "\n"
                "# HELP she_bytes_written_total Total bytes written to sockets\n"
                "# TYPE she_bytes_written_total counter\n"
                "she_bytes_written_total %" PRIu64 "\n"
                "# HELP she_config_reloads_total Zero-downtime SIGHUP/API config reloads\n"
                "# TYPE she_config_reloads_total counter\n"
                "she_config_reloads_total %" PRIu64 "\n"
                "# HELP she_security_rate_blocks_total Requests blocked by token-bucket rate limiter\n"
                "# TYPE she_security_rate_blocks_total counter\n"
                "she_security_rate_blocks_total %" PRIu64 "\n"
                "# HELP she_http_responses_total HTTP responses partitioned by status code\n"
                "# TYPE she_http_responses_total counter\n"
                "she_http_responses_total{code=\"200\"} %" PRIu64 "\n"
                "she_http_responses_total{code=\"401\"} %" PRIu64 "\n"
                "she_http_responses_total{code=\"404\"} %" PRIu64 "\n"
                "she_http_responses_total{code=\"429\"} %" PRIu64 "\n"
                "# HELP she_metrics_scrapes_total Total Prometheus /metrics scrapes served\n"
                "# TYPE she_metrics_scrapes_total counter\n"
                "she_metrics_scrapes_total %" PRIu64 "\n",
                uptime, plugin_cnt, total_con, active_con, total_req,
                bytes_rx, bytes_tx, reloads, rate_blocks,
                st_200, st_401, st_404, st_429, g_metrics_scrapes
            );
        }

        con->http_status = 200;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        she_buffer_append_string(con->response_headers, "HTTP/1.1 200 OK\r\n");
        she_buffer_append_string(con->response_headers, "Server: SHE/1.0 (Secure HTTP Engine - Prometheus Exporter)\r\n");
        she_buffer_append_string(con->response_headers,
            is_json ? "Content-Type: application/json; charset=utf-8\r\n"
                    : "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n");
        char clen[64];
        snprintf(clen, sizeof(clen), "Content-Length: %d\r\n\r\n", body_len);
        she_buffer_append_string(con->response_headers, clen);

        con->state = SHE_CON_STATE_RESPONSE_START;
        she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
        she_chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
        return SHE_HANDLER_FINISHED;
    }

    return SHE_HANDLER_GO_ON;
}

she_plugin_t *mod_metrics_plugin_init(void) {
    she_plugin_t *p = (she_plugin_t *)calloc(1, sizeof(she_plugin_t));
    if (!p) return NULL;

    p->version = 1;
    p->name = "mod_metrics";
    p->description = "Real-Time Prometheus OpenMetrics (/metrics) Exporter & Telemetry Snapshot";
    p->init = she_metrics_init;
    p->cleanup = she_metrics_cleanup;
    p->handle_subrequest_start = she_metrics_handle_subrequest_start;

    return p;
}
