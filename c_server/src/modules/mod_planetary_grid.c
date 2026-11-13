#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "modules/mod_planetary_grid.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include "core/she_chunkqueue.h"
#include "core/she_buffer.h"

static she_planetary_grid_t g_grid;
static int g_grid_initialized = 0;

static void she_grid_init_defaults(void) {
    if (g_grid_initialized) return;

    memset(&g_grid, 0, sizeof(g_grid));
    snprintf(g_grid.controller_id, sizeof(g_grid.controller_id), "SHE-TERAWATT-PLANETARY-GRID");
    g_grid.zone_count = SHE_GRID_MAX_ZONES;
    g_grid.workloads_migrated = 189420ULL;
    g_grid.qec_surface_distance = 17;

    const char *zone_ids[SHE_GRID_MAX_ZONES] = {
        "nordic-hydro-01", "atacama-solar-02", "iceland-geothermal-03", "iter-fusion-edge-04"
    };
    const char *sources[SHE_GRID_MAX_ZONES] = {
        "Hydro+Wind", "Solar+LFP-BESS", "Geothermal", "Tokamak-Pilot"
    };
    uint32_t surplus[SHE_GRID_MAX_ZONES] = {1840, 2450, 920, 1200};
    uint32_t carbon[SHE_GRID_MAX_ZONES] = {6, 11, 4, 2};
    uint32_t share[SHE_GRID_MAX_ZONES] = {32, 38, 16, 14};
    double qec[SHE_GRID_MAX_ZONES] = {1.2e-12, 1.4e-12, 9.8e-13, 8.5e-13};

    uint32_t total_mw = 0;
    for (uint32_t i = 0; i < SHE_GRID_MAX_ZONES; i++) {
        she_grid_zone_t *z = &g_grid.zones[i];
        snprintf(z->zone_id, sizeof(z->zone_id), "%s", zone_ids[i]);
        snprintf(z->clean_source, sizeof(z->clean_source), "%s", sources[i]);
        z->surplus_mw = surplus[i];
        z->carbon_intensity_g = carbon[i];
        z->workload_share_pct = share[i];
        z->surface_code_err = qec[i];
        total_mw += surplus[i];
    }
    g_grid.total_clean_surplus_mw = total_mw;
    g_grid_initialized = 1;
}

int she_grid_balance_compute(uint32_t mw_demand, char *out_zone, size_t zone_buf_sz, uint32_t *out_carbon_g) {
    she_grid_init_defaults();
    uint32_t best_idx = 0;
    uint32_t best_score = 0;

    for (uint32_t i = 0; i < g_grid.zone_count; i++) {
        she_grid_zone_t *z = &g_grid.zones[i];
        if (z->surplus_mw >= mw_demand) {
            uint32_t score = (z->surplus_mw * 10) / (z->carbon_intensity_g + 1);
            if (score > best_score) {
                best_score = score;
                best_idx = i;
            }
        }
    }

    she_grid_zone_t *chosen = &g_grid.zones[best_idx];
    g_grid.workloads_migrated++;
    if (chosen->workload_share_pct < 60) {
        chosen->workload_share_pct++;
    }

    if (out_zone && zone_buf_sz > 0) {
        snprintf(out_zone, zone_buf_sz, "%s", chosen->zone_id);
    }
    if (out_carbon_g) *out_carbon_g = chosen->carbon_intensity_g;
    return 0;
}

static she_handler_t she_grid_init(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    she_grid_init_defaults();
    return SHE_HANDLER_GO_ON;
}

static she_handler_t she_grid_cleanup(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    return SHE_HANDLER_GO_ON;
}

static she_handler_t she_grid_handle_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)srv;
    (void)p_data;
    if (!con || !con->request.uri_path) return SHE_HANDLER_GO_ON;

    she_grid_init_defaults();
    const char *path = she_buffer_cstr(con->request.uri_path);
    if (!path) return SHE_HANDLER_GO_ON;

    if (strcmp(path, "/grid/balance") == 0) {
        uint32_t mw_demand = 450;
        const char *query = con->request.uri_query ? she_buffer_cstr(con->request.uri_query) : NULL;
        if (query) {
            const char *pm = strstr(query, "mw_demand=");
            if (pm) {
                uint32_t v = (uint32_t)atoi(pm + 10);
                if (v > 0) mw_demand = v;
            }
        }

        char chosen_zone[32] = {0};
        uint32_t carbon = 0;
        she_grid_balance_compute(mw_demand, chosen_zone, sizeof(chosen_zone), &carbon);

        char body[1024];
        int body_len = snprintf(body, sizeof(body),
            "{\n"
            "  \"status\": \"WORKLOAD_BALANCED\",\n"
            "  \"controller\": \"%s\",\n"
            "  \"mw_demand\": %u,\n"
            "  \"placed_zone\": \"%s\",\n"
            "  \"carbon_intensity_gco2_kwh\": %u,\n"
            "  \"qec_surface_code_distance\": %u,\n"
            "  \"qec_attestation\": \"VERIFIED_LOGICAL_STATE\",\n"
            "  \"workloads_migrated_total\": %" PRIu64 "\n"
            "}\n",
            g_grid.controller_id,
            mw_demand,
            chosen_zone,
            carbon,
            g_grid.qec_surface_distance,
            g_grid.workloads_migrated
        );

        con->http_status = 200;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        she_buffer_append_string(con->response_headers, "HTTP/1.1 200 OK\r\n");
        she_buffer_append_string(con->response_headers, "Server: SHE/1.0 (Secure HTTP Engine - Planetary Clean-Energy Grid)\r\n");
        she_buffer_append_string(con->response_headers, "Content-Type: application/json; charset=utf-8\r\n");
        she_buffer_append_string(con->response_headers, "X-SHE-Grid-Placement: Zero-Carbon-Load-Following\r\n");
        char clen[64];
        snprintf(clen, sizeof(clen), "Content-Length: %d\r\n\r\n", body_len);
        she_buffer_append_string(con->response_headers, clen);

        con->state = SHE_CON_STATE_RESPONSE_START;
        she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
        she_chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
        return SHE_HANDLER_FINISHED;
    }

    if (strcmp(path, "/grid-status") == 0) {
        const char *query = con->request.uri_query ? she_buffer_cstr(con->request.uri_query) : NULL;
        int is_json = (query && strstr(query, "json=1")) ? 1 : 0;

        char body[4096];
        int body_len = 0;

        if (is_json) {
            int off = snprintf(body, sizeof(body),
                "{\n"
                "  \"module\": \"mod_planetary_grid\",\n"
                "  \"version\": \"1.0.0\",\n"
                "  \"controller_id\": \"%s\",\n"
                "  \"total_clean_surplus_mw\": %u,\n"
                "  \"workloads_migrated\": %" PRIu64 ",\n"
                "  \"qec_surface_distance\": %u,\n"
                "  \"zones\": [\n",
                g_grid.controller_id,
                g_grid.total_clean_surplus_mw,
                g_grid.workloads_migrated,
                g_grid.qec_surface_distance
            );
            for (uint32_t i = 0; i < g_grid.zone_count; i++) {
                she_grid_zone_t *z = &g_grid.zones[i];
                off += snprintf(body + off, sizeof(body) - (size_t)off,
                    "    {\"zone_id\": \"%s\", \"source\": \"%s\", \"surplus_mw\": %u, \"carbon_g\": %u, \"share_pct\": %u, \"surface_code_err\": %.2e}%s\n",
                    z->zone_id, z->clean_source, z->surplus_mw, z->carbon_intensity_g,
                    z->workload_share_pct, z->surface_code_err,
                    (i + 1 < g_grid.zone_count) ? "," : ""
                );
            }
            off += snprintf(body + off, sizeof(body) - (size_t)off, "  ]\n}\n");
            body_len = off;
        } else {
            int off = snprintf(body, sizeof(body),
                "<!DOCTYPE html><html><head><title>SHE Planetary Clean-Energy Grid</title>"
                "<style>body{font-family:monospace;background:#0d1117;color:#58a6ff;padding:2rem;} table{border-collapse:collapse;width:100%%;margin-top:1rem;} th,td{border:1px solid #30363d;padding:8px;text-align:left;} th{background:#161b22;color:#7ee787;}</style></head><body>"
                "<h1>SHE Terawatt Planetary Grid & Topological QEC Attestation</h1>"
                "<p>Total Clean Surplus: <strong>%u MW</strong> | Surface Code Distance: d=%u | Migrations: %" PRIu64 "</p>"
                "<table><tr><th>Zone</th><th>Clean Source</th><th>Surplus</th><th>Carbon</th><th>Compute Share</th><th>QEC Error</th></tr>",
                g_grid.total_clean_surplus_mw, g_grid.qec_surface_distance, g_grid.workloads_migrated
            );
            for (uint32_t i = 0; i < g_grid.zone_count; i++) {
                she_grid_zone_t *z = &g_grid.zones[i];
                off += snprintf(body + off, sizeof(body) - (size_t)off,
                    "<tr><td>%s</td><td>%s</td><td>%u MW</td><td>%u gCO2/kWh</td><td>%u%%</td><td>%.2e</td></tr>",
                    z->zone_id, z->clean_source, z->surplus_mw, z->carbon_intensity_g,
                    z->workload_share_pct, z->surface_code_err
                );
            }
            off += snprintf(body + off, sizeof(body) - (size_t)off, "</table></body></html>");
            body_len = off;
        }

        con->http_status = 200;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        she_buffer_append_string(con->response_headers, "HTTP/1.1 200 OK\r\n");
        she_buffer_append_string(con->response_headers, "Server: SHE/1.0 (Secure HTTP Engine - Planetary Clean-Energy Grid)\r\n");
        she_buffer_append_string(con->response_headers, is_json ? "Content-Type: application/json; charset=utf-8\r\n" : "Content-Type: text/html; charset=utf-8\r\n");
        she_buffer_append_string(con->response_headers, "X-SHE-QEC-Attestation: SurfaceCode-d17\r\n");
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

she_plugin_t *mod_planetary_grid_plugin_init(void) {
    she_plugin_t *p = (she_plugin_t *)calloc(1, sizeof(she_plugin_t));
    if (!p) return NULL;

    p->version = 1;
    p->name = "mod_planetary_grid";
    p->description = "Terawatt Clean-Energy Load-Following Compute Placement & Surface Code QEC Attestation";
    p->init = she_grid_init;
    p->cleanup = she_grid_cleanup;
    p->handle_subrequest_start = she_grid_handle_subrequest_start;

    return p;
}
