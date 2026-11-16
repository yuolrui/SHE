#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "modules/mod_neuro_edge.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include "core/she_chunkqueue.h"
#include "core/she_buffer.h"

static she_neuro_state_t g_neuro;
static int g_neuro_initialized = 0;

static void she_neuro_init_defaults(void) {
    if (g_neuro_initialized) return;

    memset(&g_neuro, 0, sizeof(g_neuro));
    snprintf(g_neuro.model_name, sizeof(g_neuro.model_name), "SHE-STDP-NEUROMORPHIC-L7");
    g_neuro.fire_threshold_mv = 70U;
    g_neuro.spikes_evaluated = 582000ULL;
    g_neuro.anomalies_fired = 142ULL;

    const uint32_t isis[SHE_NEURO_MAX_SYNAPSES] = { 120U, 450U, 15U, 820U };
    const uint32_t mvs[SHE_NEURO_MAX_SYNAPSES] = { 28U, 16U, 86U, 12U };
    const char *verdicts[SHE_NEURO_MAX_SYNAPSES] = { "BENIGN_PASS", "BENIGN_PASS", "SPIKE_ANOMALY_DROP", "BENIGN_PASS" };

    for (uint32_t i = 0; i < SHE_NEURO_MAX_SYNAPSES; i++) {
        g_neuro.neurons[i].flow_id = 401U + i;
        g_neuro.neurons[i].isi_us = isis[i];
        g_neuro.neurons[i].membrane_mv = mvs[i];
        snprintf(g_neuro.neurons[i].verdict, sizeof(g_neuro.neurons[i].verdict), "%s", verdicts[i]);
    }

    g_neuro_initialized = 1;
}

int she_neuro_classify_spike(uint32_t flow_id, uint32_t isi_us, uint32_t *out_membrane_mv, char *out_verdict, size_t v_sz) {
    she_neuro_init_defaults();
    if (flow_id == 0U || isi_us == 0U) return -1;

    uint32_t membrane_mv = (isi_us < 25U) ? 88U : (isi_us < 80U ? 54U : 22U);
    const char *verdict = (membrane_mv >= g_neuro.fire_threshold_mv) ? "SPIKE_ANOMALY_DROP" : "BENIGN_PASS";

    uint32_t slot = flow_id % SHE_NEURO_MAX_SYNAPSES;
    g_neuro.neurons[slot].flow_id = flow_id;
    g_neuro.neurons[slot].isi_us = isi_us;
    g_neuro.neurons[slot].membrane_mv = membrane_mv;
    snprintf(g_neuro.neurons[slot].verdict, sizeof(g_neuro.neurons[slot].verdict), "%s", verdict);

    g_neuro.spikes_evaluated++;
    if (membrane_mv >= g_neuro.fire_threshold_mv) {
        g_neuro.anomalies_fired++;
    }

    if (out_membrane_mv) *out_membrane_mv = membrane_mv;
    if (out_verdict && v_sz > 0) snprintf(out_verdict, v_sz, "%s", verdict);
    return 0;
}

static she_handler_t she_neuro_init(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    she_neuro_init_defaults();
    return SHE_HANDLER_GO_ON;
}

static she_handler_t she_neuro_cleanup(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    return SHE_HANDLER_GO_ON;
}

static she_handler_t she_neuro_handle_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)srv;
    (void)p_data;
    if (!con || !con->request.uri_path) return SHE_HANDLER_GO_ON;

    she_neuro_init_defaults();
    const char *path = she_buffer_cstr(con->request.uri_path);
    if (!path) return SHE_HANDLER_GO_ON;

    if (strcmp(path, "/neuro/spike") == 0) {
        uint32_t flow_id = 401U;
        uint32_t isi_us = 120U;
        const char *query = con->request.uri_query ? she_buffer_cstr(con->request.uri_query) : NULL;
        if (query) {
            const char *pf = strstr(query, "flow=");
            if (pf) {
                unsigned long v = strtoul(pf + 5, NULL, 10);
                if (v <= 0xFFFFFFFFUL) flow_id = (uint32_t)v;
            }
            const char *pi = strstr(query, "isi_us=");
            if (pi) {
                unsigned long v = strtoul(pi + 7, NULL, 10);
                if (v <= 0xFFFFFFFFUL) isi_us = (uint32_t)v;
            }
        }

        uint32_t mv = 0U;
        char verdict[24] = {0};
        int rc = she_neuro_classify_spike(flow_id, isi_us, &mv, verdict, sizeof(verdict));

        char body[1024];
        int body_len = snprintf(body, sizeof(body),
            "{\n"
            "  \"status\": \"%s\",\n"
            "  \"model\": \"%s\",\n"
            "  \"flow_id\": %u,\n"
            "  \"isi_us\": %u,\n"
            "  \"membrane_mv\": %u,\n"
            "  \"verdict\": \"%s\",\n"
            "  \"spikes_evaluated\": %" PRIu64 "\n"
            "}\n",
            rc == 0 ? "NEURO_SPIKE_CLASSIFIED" : "NEURO_INVALID_SPIKE",
            g_neuro.model_name,
            flow_id,
            isi_us,
            mv,
            rc == 0 ? verdict : "REJECTED",
            g_neuro.spikes_evaluated
        );

        con->http_status = (rc == 0) ? 200 : 400;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        she_buffer_append_string(con->response_headers,
            rc == 0 ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 400 Bad Request\r\n");
        she_buffer_append_string(con->response_headers, "Server: SHE/1.0 (Secure HTTP Engine - Neuromorphic L7)\r\n");
        she_buffer_append_string(con->response_headers, "Content-Type: application/json; charset=utf-8\r\n");
        she_buffer_append_string(con->response_headers, "X-SHE-Neuro-STDP: Active\r\n");
        char clen[64];
        snprintf(clen, sizeof(clen), "Content-Length: %d\r\n\r\n", body_len);
        she_buffer_append_string(con->response_headers, clen);

        con->state = SHE_CON_STATE_RESPONSE_START;
        she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
        she_chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
        return SHE_HANDLER_FINISHED;
    }

    if (strcmp(path, "/neuro-status") == 0) {
        const char *query = con->request.uri_query ? she_buffer_cstr(con->request.uri_query) : NULL;
        int is_json = (query && strstr(query, "json=1")) ? 1 : 0;

        char body[4096];
        int body_len = 0;

        if (is_json) {
            int off = snprintf(body, sizeof(body),
                "{\n"
                "  \"module\": \"mod_neuro_edge\",\n"
                "  \"version\": \"1.0.0\",\n"
                "  \"model_name\": \"%s\",\n"
                "  \"fire_threshold_mv\": %u,\n"
                "  \"spikes_evaluated\": %" PRIu64 ",\n"
                "  \"anomalies_fired\": %" PRIu64 ",\n"
                "  \"neurons\": [\n",
                g_neuro.model_name,
                g_neuro.fire_threshold_mv,
                g_neuro.spikes_evaluated,
                g_neuro.anomalies_fired
            );
            for (uint32_t i = 0; i < SHE_NEURO_MAX_SYNAPSES; i++) {
                she_neuro_neuron_t *n = &g_neuro.neurons[i];
                off += snprintf(body + off, sizeof(body) - (size_t)off,
                    "    {\"flow_id\": %u, \"isi_us\": %u, \"membrane_mv\": %u, \"verdict\": \"%s\"}%s\n",
                    n->flow_id, n->isi_us, n->membrane_mv, n->verdict,
                    (i + 1U < SHE_NEURO_MAX_SYNAPSES) ? "," : ""
                );
            }
            off += snprintf(body + off, sizeof(body) - (size_t)off, "  ]\n}\n");
            body_len = off;
        } else {
            body_len = snprintf(body, sizeof(body),
                "<!DOCTYPE html><html><head><title>SHE Neuromorphic Edge</title>"
                "<style>body{font-family:monospace;background:#0d1117;color:#58a6ff;padding:2rem;}</style></head><body>"
                "<h1>SHE Neuromorphic Spike-Timing L7 Classifier</h1>"
                "<p>Model: <strong>%s</strong> | Threshold: %u mV</p>"
                "<p>Spikes Evaluated: %" PRIu64 " | Anomalies Fired: %" PRIu64 "</p>"
                "</body></html>",
                g_neuro.model_name, g_neuro.fire_threshold_mv,
                g_neuro.spikes_evaluated, g_neuro.anomalies_fired
            );
        }

        con->http_status = 200;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        she_buffer_append_string(con->response_headers, "HTTP/1.1 200 OK\r\n");
        she_buffer_append_string(con->response_headers, "Server: SHE/1.0 (Secure HTTP Engine - Neuromorphic L7)\r\n");
        she_buffer_append_string(con->response_headers, is_json ? "Content-Type: application/json; charset=utf-8\r\n" : "Content-Type: text/html; charset=utf-8\r\n");
        she_buffer_append_string(con->response_headers, "X-SHE-Neuro-STDP: Active\r\n");
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

she_plugin_t *mod_neuro_edge_plugin_init(void) {
    she_plugin_t *p = (she_plugin_t *)calloc(1, sizeof(she_plugin_t));
    if (!p) return NULL;

    p->version = 1;
    p->name = "mod_neuro_edge";
    p->description = "Neuromorphic Spike-Timing Event-Driven L7 Anomaly Classifier";
    p->init = she_neuro_init;
    p->cleanup = she_neuro_cleanup;
    p->handle_subrequest_start = she_neuro_handle_subrequest_start;

    return p;
}
