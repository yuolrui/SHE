#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "modules/mod_orbital_mesh.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include "core/she_chunkqueue.h"
#include "core/she_buffer.h"

static she_orbital_state_t g_orbital;
static int g_orbital_initialized = 0;

static void she_orbital_sanitize(const char *src, char *dst, size_t dst_sz) {
    size_t j = 0;
    for (size_t i = 0; src && src[i] && src[i] != '&' && j + 1 < dst_sz; i++) {
        unsigned char c = (unsigned char)src[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_') {
            dst[j++] = (char)c;
        }
    }
    dst[j] = '\0';
}

static void she_orbital_init_defaults(void) {
    if (g_orbital_initialized) return;

    memset(&g_orbital, 0, sizeof(g_orbital));
    snprintf(g_orbital.constellation_id, sizeof(g_orbital.constellation_id), "SHE-ORBITAL-OISL-1550NM");
    g_orbital.frames_relayed = 741200ULL;
    g_orbital.doppler_resyncs = 1280ULL;

    const char *sats[SHE_ORBITAL_MAX_LINKS] = {
        "LEO-STAR-01", "LEO-STAR-02", "MEO-RELAY-03", "L1-SOLAR-04"
    };
    const char *shells[SHE_ORBITAL_MAX_LINKS] = {
        "LEO-550KM", "LEO-550KM", "MEO-8000KM", "L1-HALO"
    };
    const uint32_t dopplers[SHE_ORBITAL_MAX_LINKS] = { 420U, 680U, 190U, 45U };
    const uint32_t snrs[SHE_ORBITAL_MAX_LINKS] = { 32U, 30U, 28U, 26U };
    const uint32_t gbps[SHE_ORBITAL_MAX_LINKS] = { 400U, 400U, 200U, 100U };

    for (uint32_t i = 0; i < SHE_ORBITAL_MAX_LINKS; i++) {
        snprintf(g_orbital.links[i].sat_id, sizeof(g_orbital.links[i].sat_id), "%s", sats[i]);
        snprintf(g_orbital.links[i].orbit_shell, sizeof(g_orbital.links[i].orbit_shell), "%s", shells[i]);
        g_orbital.links[i].doppler_khz = dopplers[i];
        g_orbital.links[i].snr_db = snrs[i];
        g_orbital.links[i].link_gbps = gbps[i];
        snprintf(g_orbital.links[i].wave_lock, sizeof(g_orbital.links[i].wave_lock), "OPLL_LOCKED");
    }

    g_orbital_initialized = 1;
}

int she_orbital_route_laser_link(const char *sat_id, uint32_t doppler_khz, uint32_t *out_snr_db, char *out_wave_lock, size_t w_sz) {
    she_orbital_init_defaults();
    if (!sat_id || !sat_id[0] || doppler_khz > 50000U) return -1;
    if (strncmp(sat_id, "LEO-", 4) != 0 && strncmp(sat_id, "MEO-", 4) != 0 && strncmp(sat_id, "L1-", 3) != 0) {
        return -1;
    }

    uint32_t snr = (doppler_khz < 1000U) ? 34U : (doppler_khz < 10000U ? 29U : 24U);
    const char *lock_state = (doppler_khz <= 25000U) ? "OPLL_LOCKED" : "OPLL_SLEW_COMP";

    int slot = 0;
    for (int i = 0; i < SHE_ORBITAL_MAX_LINKS; i++) {
        if (strcmp(g_orbital.links[i].sat_id, sat_id) == 0) {
            slot = i;
            break;
        }
    }

    snprintf(g_orbital.links[slot].sat_id, sizeof(g_orbital.links[slot].sat_id), "%s", sat_id);
    g_orbital.links[slot].doppler_khz = doppler_khz;
    g_orbital.links[slot].snr_db = snr;
    snprintf(g_orbital.links[slot].wave_lock, sizeof(g_orbital.links[slot].wave_lock), "%s", lock_state);

    g_orbital.frames_relayed += 256ULL;
    g_orbital.doppler_resyncs++;

    if (out_snr_db) *out_snr_db = snr;
    if (out_wave_lock && w_sz > 0) snprintf(out_wave_lock, w_sz, "%s", lock_state);
    return 0;
}

static she_handler_t she_orbital_init(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    she_orbital_init_defaults();
    return SHE_HANDLER_GO_ON;
}

static she_handler_t she_orbital_cleanup(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    return SHE_HANDLER_GO_ON;
}

static she_handler_t she_orbital_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)srv;
    (void)p_data;
    if (!con || !con->request.uri_path) return SHE_HANDLER_GO_ON;

    she_orbital_init_defaults();
    const char *path = she_buffer_cstr(con->request.uri_path);
    if (!path) return SHE_HANDLER_GO_ON;

    if (strcmp(path, "/orbital/route") == 0) {
        char sat[32] = "LEO-STAR-01";
        uint32_t doppler_khz = 420U;
        const char *query = con->request.uri_query ? she_buffer_cstr(con->request.uri_query) : NULL;
        if (query) {
            const char *ps = strstr(query, "sat=");
            if (ps) {
                char parsed[32];
                she_orbital_sanitize(ps + 4, parsed, sizeof(parsed));
                if (parsed[0]) snprintf(sat, sizeof(sat), "%s", parsed);
            }
            const char *pd = strstr(query, "doppler_khz=");
            if (pd) {
                unsigned long v = strtoul(pd + 12, NULL, 10);
                if (v <= 0xFFFFFFFFUL) doppler_khz = (uint32_t)v;
            }
        }

        uint32_t snr_db = 0U;
        char wave_lock[24] = {0};
        int rc = she_orbital_route_laser_link(sat, doppler_khz, &snr_db, wave_lock, sizeof(wave_lock));

        char body[1024];
        int body_len = snprintf(body, sizeof(body),
            "{\n"
            "  \"status\": \"%s\",\n"
            "  \"constellation_id\": \"%s\",\n"
            "  \"sat_id\": \"%s\",\n"
            "  \"doppler_khz\": %u,\n"
            "  \"snr_db\": %u,\n"
            "  \"wave_lock\": \"%s\",\n"
            "  \"doppler_resyncs\": %" PRIu64 "\n"
            "}\n",
            rc == 0 ? "ORBITAL_OISL_ROUTED" : "ORBITAL_LINK_REJECTED",
            g_orbital.constellation_id,
            sat,
            doppler_khz,
            snr_db,
            rc == 0 ? wave_lock : "UNLOCKED",
            g_orbital.doppler_resyncs
        );

        con->http_status = (rc == 0) ? 200 : 400;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        she_buffer_append_string(con->response_headers,
            rc == 0 ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 400 Bad Request\r\n");
        she_buffer_append_string(con->response_headers, "Server: SHE/1.0 (Secure HTTP Engine - Orbital OISL)\r\n");
        she_buffer_append_string(con->response_headers, "Content-Type: application/json; charset=utf-8\r\n");
        she_buffer_append_string(con->response_headers, "X-SHE-Orbital-OPLL: 1550nm-Coherent\r\n");
        char clen[64];
        snprintf(clen, sizeof(clen), "Content-Length: %d\r\n\r\n", body_len);
        she_buffer_append_string(con->response_headers, clen);

        con->state = SHE_CON_STATE_RESPONSE_START;
        she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
        she_chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
        return SHE_HANDLER_FINISHED;
    }

    if (strcmp(path, "/orbital-status") == 0) {
        const char *query = con->request.uri_query ? she_buffer_cstr(con->request.uri_query) : NULL;
        int is_json = (query && strstr(query, "json=1")) ? 1 : 0;

        char body[4096];
        int body_len = 0;

        if (is_json) {
            int off = snprintf(body, sizeof(body),
                "{\n"
                "  \"module\": \"mod_orbital_mesh\",\n"
                "  \"version\": \"1.0.0\",\n"
                "  \"constellation_id\": \"%s\",\n"
                "  \"frames_relayed\": %" PRIu64 ",\n"
                "  \"doppler_resyncs\": %" PRIu64 ",\n"
                "  \"links\": [\n",
                g_orbital.constellation_id,
                g_orbital.frames_relayed,
                g_orbital.doppler_resyncs
            );
            for (uint32_t i = 0; i < SHE_ORBITAL_MAX_LINKS; i++) {
                she_orbital_link_t *lk = &g_orbital.links[i];
                off += snprintf(body + off, sizeof(body) - (size_t)off,
                    "    {\"sat_id\": \"%s\", \"orbit_shell\": \"%s\", \"doppler_khz\": %u, \"snr_db\": %u, \"link_gbps\": %u, \"wave_lock\": \"%s\"}%s\n",
                    lk->sat_id, lk->orbit_shell, lk->doppler_khz, lk->snr_db, lk->link_gbps, lk->wave_lock,
                    (i + 1U < SHE_ORBITAL_MAX_LINKS) ? "," : ""
                );
            }
            off += snprintf(body + off, sizeof(body) - (size_t)off, "  ]\n}\n");
            body_len = off;
        } else {
            body_len = snprintf(body, sizeof(body),
                "<!DOCTYPE html><html><head><title>SHE Orbital Optical Mesh</title>"
                "<style>body{font-family:monospace;background:#0d1117;color:#58a6ff;padding:2rem;}</style></head><body>"
                "<h1>SHE Optical Inter-Satellite Laser Link Mesh</h1>"
                "<p>Constellation: <strong>%s</strong> | Frames Relayed: %" PRIu64 "</p>"
                "</body></html>",
                g_orbital.constellation_id, g_orbital.frames_relayed
            );
        }

        con->http_status = 200;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        she_buffer_append_string(con->response_headers, "HTTP/1.1 200 OK\r\n");
        she_buffer_append_string(con->response_headers, "Server: SHE/1.0 (Secure HTTP Engine - Orbital OISL)\r\n");
        she_buffer_append_string(con->response_headers, is_json ? "Content-Type: application/json; charset=utf-8\r\n" : "Content-Type: text/html; charset=utf-8\r\n");
        she_buffer_append_string(con->response_headers, "X-SHE-Orbital-OPLL: 1550nm-Coherent\r\n");
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

she_plugin_t *mod_orbital_mesh_plugin_init(void) {
    she_plugin_t *p = (she_plugin_t *)calloc(1, sizeof(she_plugin_t));
    if (!p) return NULL;

    p->version = 1;
    p->name = "mod_orbital_mesh";
    p->description = "Optical LEO/MEO Inter-Satellite Laser Link Routing with Doppler Compensation";
    p->init = she_orbital_init;
    p->cleanup = she_orbital_cleanup;
    p->handle_subrequest_start = she_orbital_subrequest_start;

    return p;
}
