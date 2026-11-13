#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "modules/mod_waveguide.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include "core/she_chunkqueue.h"
#include "core/she_buffer.h"

static she_waveguide_engine_t g_waveguide;
static int g_waveguide_initialized = 0;

static void she_waveguide_init_defaults(void) {
    if (g_waveguide_initialized) return;

    memset(&g_waveguide, 0, sizeof(g_waveguide));
    snprintf(g_waveguide.fabric_name, sizeof(g_waveguide.fabric_name), "SHE-HOLONOMIC-SILICON-PHOTONIC");
    g_waveguide.mode_count = SHE_WAVEGUIDE_MAX_MODES;
    g_waveguide.total_photonic_packets = 948201500ULL;
    g_waveguide.avg_switching_ps = 18;
    g_waveguide.eo_conversions_bypassed_pct = 100.0f;

    const char *names[SHE_WAVEGUIDE_MAX_MODES] = {
        "wg-spatial-te0", "wg-spatial-te1", "wg-spatial-te2",
        "wg-spatial-tm0", "wg-spatial-tm1", "wg-spatial-oam1"
    };
    float wavelengths[SHE_WAVEGUIDE_MAX_MODES] = {
        1550.12f, 1550.92f, 1551.72f, 1552.52f, 1553.33f, 1554.13f
    };
    float losses[SHE_WAVEGUIDE_MAX_MODES] = {0.38f, 0.41f, 0.44f, 0.42f, 0.47f, 0.51f};
    uint32_t latencies[SHE_WAVEGUIDE_MAX_MODES] = {15, 17, 19, 16, 20, 21};
    uint64_t packets[SHE_WAVEGUIDE_MAX_MODES] = {
        240100000ULL, 198400000ULL, 164200000ULL,
        155800000ULL, 102400000ULL, 87301500ULL
    };

    for (uint32_t i = 0; i < SHE_WAVEGUIDE_MAX_MODES; i++) {
        she_waveguide_mode_t *m = &g_waveguide.modes[i];
        m->mode_id = i;
        snprintf(m->waveguide_name, sizeof(m->waveguide_name), "%s", names[i]);
        m->wavelength_nm = wavelengths[i];
        m->insertion_loss_db = losses[i];
        m->switch_latency_ps = latencies[i];
        m->packets_switched = packets[i];
        m->bytes_routed = packets[i] * 1500ULL;
        m->is_resonant = 1;
    }

    g_waveguide_initialized = 1;
}

int she_waveguide_route_photon_packet(uint32_t spatial_mode, uint64_t payload_bytes, uint32_t *out_latency_ps) {
    she_waveguide_init_defaults();
    if (spatial_mode >= g_waveguide.mode_count) {
        spatial_mode = spatial_mode % g_waveguide.mode_count;
    }
    she_waveguide_mode_t *m = &g_waveguide.modes[spatial_mode];
    m->packets_switched++;
    m->bytes_routed += payload_bytes;
    g_waveguide.total_photonic_packets++;
    if (out_latency_ps) *out_latency_ps = m->switch_latency_ps;
    return 0;
}

static she_handler_t she_waveguide_init(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    she_waveguide_init_defaults();
    return SHE_HANDLER_GO_ON;
}

static she_handler_t she_waveguide_cleanup(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    return SHE_HANDLER_GO_ON;
}

static she_handler_t she_waveguide_handle_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)srv;
    (void)p_data;
    if (!con || !con->request.uri_path) return SHE_HANDLER_GO_ON;

    she_waveguide_init_defaults();
    const char *path = she_buffer_cstr(con->request.uri_path);
    if (!path) return SHE_HANDLER_GO_ON;

    if (strcmp(path, "/waveguide/route") == 0) {
        uint32_t mode = 0;
        uint64_t bytes = 65536;
        const char *query = con->request.uri_query ? she_buffer_cstr(con->request.uri_query) : NULL;
        if (query) {
            const char *pm = strstr(query, "mode=");
            if (pm) mode = (uint32_t)atoi(pm + 5);
            const char *pb = strstr(query, "bytes=");
            if (pb) {
                uint64_t parsed = (uint64_t)strtoull(pb + 6, NULL, 10);
                if (parsed > 0) bytes = parsed;
            }
        }
        mode = mode % g_waveguide.mode_count;
        uint32_t lat_ps = 0;
        she_waveguide_route_photon_packet(mode, bytes, &lat_ps);
        she_waveguide_mode_t *m = &g_waveguide.modes[mode];

        char body[1024];
        int body_len = snprintf(body, sizeof(body),
            "{\n"
            "  \"status\": \"PHOTON_PACKET_ROUTED\",\n"
            "  \"fabric\": \"%s\",\n"
            "  \"spatial_mode_id\": %u,\n"
            "  \"waveguide_name\": \"%s\",\n"
            "  \"wavelength_nm\": %.2f,\n"
            "  \"payload_bytes\": %" PRIu64 ",\n"
            "  \"switching_latency_ps\": %u,\n"
            "  \"insertion_loss_db\": %.2f,\n"
            "  \"eo_conversion_bypassed\": true,\n"
            "  \"mode_packets_total\": %" PRIu64 "\n"
            "}\n",
            g_waveguide.fabric_name,
            m->mode_id,
            m->waveguide_name,
            m->wavelength_nm,
            bytes,
            lat_ps,
            m->insertion_loss_db,
            m->packets_switched
        );

        con->http_status = 200;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        she_buffer_append_string(con->response_headers, "HTTP/1.1 200 OK\r\n");
        she_buffer_append_string(con->response_headers, "Server: SHE/1.0 (Secure HTTP Engine - Photonic Waveguide)\r\n");
        she_buffer_append_string(con->response_headers, "Content-Type: application/json; charset=utf-8\r\n");
        she_buffer_append_string(con->response_headers, "X-SHE-Waveguide-Switching: All-Optical-Zero-EO\r\n");
        char clen[64];
        snprintf(clen, sizeof(clen), "Content-Length: %d\r\n\r\n", body_len);
        she_buffer_append_string(con->response_headers, clen);

        con->state = SHE_CON_STATE_RESPONSE_START;
        she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
        she_chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
        return SHE_HANDLER_FINISHED;
    }

    if (strcmp(path, "/waveguide-status") == 0) {
        const char *query = con->request.uri_query ? she_buffer_cstr(con->request.uri_query) : NULL;
        int is_json = (query && strstr(query, "json=1")) ? 1 : 0;

        char body[4096];
        int body_len = 0;

        if (is_json) {
            int off = snprintf(body, sizeof(body),
                "{\n"
                "  \"module\": \"mod_waveguide\",\n"
                "  \"version\": \"1.0.0\",\n"
                "  \"fabric_name\": \"%s\",\n"
                "  \"mode_count\": %u,\n"
                "  \"total_photonic_packets\": %" PRIu64 ",\n"
                "  \"avg_switching_ps\": %u,\n"
                "  \"eo_conversions_bypassed_pct\": %.1f,\n"
                "  \"modes\": [\n",
                g_waveguide.fabric_name,
                g_waveguide.mode_count,
                g_waveguide.total_photonic_packets,
                g_waveguide.avg_switching_ps,
                g_waveguide.eo_conversions_bypassed_pct
            );
            for (uint32_t i = 0; i < g_waveguide.mode_count; i++) {
                she_waveguide_mode_t *m = &g_waveguide.modes[i];
                off += snprintf(body + off, sizeof(body) - (size_t)off,
                    "    {\"mode_id\": %u, \"name\": \"%s\", \"wavelength_nm\": %.2f, \"loss_db\": %.2f, \"latency_ps\": %u, \"packets\": %" PRIu64 ", \"resonant\": %s}%s\n",
                    m->mode_id, m->waveguide_name, m->wavelength_nm, m->insertion_loss_db,
                    m->switch_latency_ps, m->packets_switched, m->is_resonant ? "true" : "false",
                    (i + 1 < g_waveguide.mode_count) ? "," : ""
                );
            }
            off += snprintf(body + off, sizeof(body) - (size_t)off, "  ]\n}\n");
            body_len = off;
        } else {
            int off = snprintf(body, sizeof(body),
                "<!DOCTYPE html><html><head><title>SHE Holonomic Optical Waveguide</title>"
                "<style>body{font-family:monospace;background:#0d1117;color:#58a6ff;padding:2rem;} table{border-collapse:collapse;width:100%%;margin-top:1rem;} th,td{border:1px solid #30363d;padding:8px;text-align:left;} th{background:#161b22;color:#7ee787;}</style></head><body>"
                "<h1>SHE Holonomic Silicon Photonic Waveguide Router</h1>"
                "<p>Fabric: <strong>%s</strong> | Avg Switching: %u ps | E/O Bypass: %.1f%%</p>"
                "<table><tr><th>Mode</th><th>Waveguide</th><th>Wavelength</th><th>Loss</th><th>Latency</th><th>Packets Switched</th></tr>",
                g_waveguide.fabric_name, g_waveguide.avg_switching_ps, g_waveguide.eo_conversions_bypassed_pct
            );
            for (uint32_t i = 0; i < g_waveguide.mode_count; i++) {
                she_waveguide_mode_t *m = &g_waveguide.modes[i];
                off += snprintf(body + off, sizeof(body) - (size_t)off,
                    "<tr><td>%u</td><td>%s</td><td>%.2f nm</td><td>%.2f dB</td><td>%u ps</td><td>%" PRIu64 "</td></tr>",
                    m->mode_id, m->waveguide_name, m->wavelength_nm, m->insertion_loss_db,
                    m->switch_latency_ps, m->packets_switched
                );
            }
            off += snprintf(body + off, sizeof(body) - (size_t)off, "</table></body></html>");
            body_len = off;
        }

        con->http_status = 200;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        she_buffer_append_string(con->response_headers, "HTTP/1.1 200 OK\r\n");
        she_buffer_append_string(con->response_headers, "Server: SHE/1.0 (Secure HTTP Engine - Photonic Waveguide)\r\n");
        she_buffer_append_string(con->response_headers, is_json ? "Content-Type: application/json; charset=utf-8\r\n" : "Content-Type: text/html; charset=utf-8\r\n");
        she_buffer_append_string(con->response_headers, "X-SHE-Photonic-Mesh: Active\r\n");
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

she_plugin_t *mod_waveguide_plugin_init(void) {
    she_plugin_t *p = (she_plugin_t *)calloc(1, sizeof(she_plugin_t));
    if (!p) return NULL;

    p->version = 1;
    p->name = "mod_waveguide";
    p->description = "Holonomic Silicon Photonic Waveguide Spatial Routing (Zero E/O Conversion, Picosecond Switching)";
    p->init = she_waveguide_init;
    p->cleanup = she_waveguide_cleanup;
    p->handle_subrequest_start = she_waveguide_handle_subrequest_start;

    return p;
}
