#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "modules/mod_cpo.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include "core/she_chunkqueue.h"
#include "core/she_buffer.h"

static she_cpo_state_t g_cpo;
static int g_cpo_initialized = 0;

static void she_cpo_init_defaults(void) {
    if (g_cpo_initialized) return;

    memset(&g_cpo, 0, sizeof(g_cpo));
    snprintf(g_cpo.fabric_name, sizeof(g_cpo.fabric_name), "SHE-CPO-3.2T-SILICON-PHOTONIC");
    g_cpo.total_lanes = SHE_CPO_MAX_LANES;
    g_cpo.aggregate_gbps = 0ULL;
    g_cpo.tuning_ops = 4820ULL;
    g_cpo.avg_ber_exponent = -15.4;

    for (uint32_t i = 0; i < SHE_CPO_MAX_LANES; i++) {
        g_cpo.lanes[i].lane_id = i + 1U;
        g_cpo.lanes[i].wavelength_nm = 1310U + i * 2U;
        g_cpo.lanes[i].tx_power_dbm = -2;
        g_cpo.lanes[i].gbps = 400U;
        g_cpo.lanes[i].frames_switched = 125000000ULL + (uint64_t)i * 4200000ULL;
        g_cpo.lanes[i].locked = 1;
        g_cpo.aggregate_gbps += g_cpo.lanes[i].gbps;
    }

    g_cpo_initialized = 1;
}

int she_cpo_tune_optical_lane(uint32_t lane_id, int target_power_dbm, uint64_t *out_total_gbps) {
    she_cpo_init_defaults();
    if (lane_id == 0U || lane_id > SHE_CPO_MAX_LANES) return -1;
    if (target_power_dbm < -10 || target_power_dbm > 6) return -1;

    she_cpo_lane_t *lane = &g_cpo.lanes[lane_id - 1U];
    lane->tx_power_dbm = target_power_dbm;
    lane->locked = 1;
    lane->frames_switched += 65536ULL;
    g_cpo.tuning_ops++;

    if (out_total_gbps) {
        *out_total_gbps = g_cpo.aggregate_gbps;
    }
    return 0;
}

static she_handler_t she_cpo_init(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    she_cpo_init_defaults();
    return SHE_HANDLER_GO_ON;
}

static she_handler_t she_cpo_cleanup(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    return SHE_HANDLER_GO_ON;
}

static she_handler_t she_cpo_handle_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)srv;
    (void)p_data;
    if (!con || !con->request.uri_path) return SHE_HANDLER_GO_ON;

    she_cpo_init_defaults();
    const char *path = she_buffer_cstr(con->request.uri_path);
    if (!path) return SHE_HANDLER_GO_ON;

    if (strcmp(path, "/cpo/tune") == 0) {
        uint32_t lane_id = 1U;
        int power_dbm = -2;
        const char *query = con->request.uri_query ? she_buffer_cstr(con->request.uri_query) : NULL;
        if (query) {
            const char *pl = strstr(query, "lane=");
            if (pl) {
                unsigned long v = strtoul(pl + 5, NULL, 10);
                if (v <= 0xFFFFFFFFUL) lane_id = (uint32_t)v;
            }
            const char *pp = strstr(query, "power=");
            if (pp) {
                power_dbm = (int)strtol(pp + 6, NULL, 10);
            }
        }

        uint64_t total_gbps = 0ULL;
        int rc = she_cpo_tune_optical_lane(lane_id, power_dbm, &total_gbps);

        char body[1024];
        int body_len = snprintf(body, sizeof(body),
            "{\n"
            "  \"status\": \"%s\",\n"
            "  \"fabric\": \"%s\",\n"
            "  \"lane_id\": %u,\n"
            "  \"target_power_dbm\": %d,\n"
            "  \"aggregate_gbps\": %" PRIu64 ",\n"
            "  \"tuning_ops\": %" PRIu64 "\n"
            "}\n",
            rc == 0 ? "CPO_OPTICAL_LANE_LOCKED" : "CPO_INVALID_PARAMETERS",
            g_cpo.fabric_name,
            lane_id,
            power_dbm,
            rc == 0 ? total_gbps : g_cpo.aggregate_gbps,
            g_cpo.tuning_ops
        );

        con->http_status = (rc == 0) ? 200 : 400;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        she_buffer_append_string(con->response_headers,
            rc == 0 ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 400 Bad Request\r\n");
        she_buffer_append_string(con->response_headers, "Server: SHE/1.0 (Secure HTTP Engine - Co-Packaged Optics)\r\n");
        she_buffer_append_string(con->response_headers, "Content-Type: application/json; charset=utf-8\r\n");
        she_buffer_append_string(con->response_headers, "X-SHE-CPO-Fabric: 3.2Tbps-Silicon-Photonics\r\n");
        char clen[64];
        snprintf(clen, sizeof(clen), "Content-Length: %d\r\n\r\n", body_len);
        she_buffer_append_string(con->response_headers, clen);

        con->state = SHE_CON_STATE_RESPONSE_START;
        she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
        she_chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
        return SHE_HANDLER_FINISHED;
    }

    if (strcmp(path, "/cpo-status") == 0) {
        const char *query = con->request.uri_query ? she_buffer_cstr(con->request.uri_query) : NULL;
        int is_json = (query && strstr(query, "json=1")) ? 1 : 0;

        char body[4096];
        int body_len = 0;

        if (is_json) {
            int off = snprintf(body, sizeof(body),
                "{\n"
                "  \"module\": \"mod_cpo\",\n"
                "  \"version\": \"1.0.0\",\n"
                "  \"fabric_name\": \"%s\",\n"
                "  \"total_lanes\": %u,\n"
                "  \"aggregate_gbps\": %" PRIu64 ",\n"
                "  \"tuning_ops\": %" PRIu64 ",\n"
                "  \"avg_ber_exponent\": %.1f,\n"
                "  \"lanes\": [\n",
                g_cpo.fabric_name,
                g_cpo.total_lanes,
                g_cpo.aggregate_gbps,
                g_cpo.tuning_ops,
                g_cpo.avg_ber_exponent
            );
            for (uint32_t i = 0; i < SHE_CPO_MAX_LANES; i++) {
                she_cpo_lane_t *l = &g_cpo.lanes[i];
                off += snprintf(body + off, sizeof(body) - (size_t)off,
                    "    {\"lane_id\": %u, \"wavelength_nm\": %u, \"tx_power_dbm\": %d, \"gbps\": %u, \"frames_switched\": %" PRIu64 ", \"locked\": %s}%s\n",
                    l->lane_id, l->wavelength_nm, l->tx_power_dbm, l->gbps, l->frames_switched,
                    l->locked ? "true" : "false",
                    (i + 1U < SHE_CPO_MAX_LANES) ? "," : ""
                );
            }
            off += snprintf(body + off, sizeof(body) - (size_t)off, "  ]\n}\n");
            body_len = off;
        } else {
            body_len = snprintf(body, sizeof(body),
                "<!DOCTYPE html><html><head><title>SHE Co-Packaged Optics</title>"
                "<style>body{font-family:monospace;background:#0d1117;color:#58a6ff;padding:2rem;}</style></head><body>"
                "<h1>SHE 3.2 Tbps Co-Packaged Silicon Optics Fabric</h1>"
                "<p>Fabric: <strong>%s</strong> | Lanes: %u | Aggregate: %" PRIu64 " Gbps</p>"
                "<p>Tuning Ops: %" PRIu64 " | Mean BER: 10^(%.1f)</p>"
                "</body></html>",
                g_cpo.fabric_name, g_cpo.total_lanes, g_cpo.aggregate_gbps,
                g_cpo.tuning_ops, g_cpo.avg_ber_exponent
            );
        }

        con->http_status = 200;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        she_buffer_append_string(con->response_headers, "HTTP/1.1 200 OK\r\n");
        she_buffer_append_string(con->response_headers, "Server: SHE/1.0 (Secure HTTP Engine - Co-Packaged Optics)\r\n");
        she_buffer_append_string(con->response_headers, is_json ? "Content-Type: application/json; charset=utf-8\r\n" : "Content-Type: text/html; charset=utf-8\r\n");
        she_buffer_append_string(con->response_headers, "X-SHE-CPO-Fabric: 3.2Tbps-Silicon-Photonics\r\n");
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

she_plugin_t *mod_cpo_plugin_init(void) {
    she_plugin_t *p = (she_plugin_t *)calloc(1, sizeof(she_plugin_t));
    if (!p) return NULL;

    p->version = 1;
    p->name = "mod_cpo";
    p->description = "Co-Packaged Optics (CPO) 3.2 Tbps Rack-Scale Silicon Photonics Interconnect & Laser Tuner";
    p->init = she_cpo_init;
    p->cleanup = she_cpo_cleanup;
    p->handle_subrequest_start = she_cpo_handle_subrequest_start;

    return p;
}
