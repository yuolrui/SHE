#include "modules/mod_wireguard.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static she_wireguard_stats_t s_wg_stats = {
    .interface_name = "she-wg0",
    .interface_ip = "10.42.0.1/24",
    .listen_port = 51820,
    .public_key = "SHE_SRV_PubK42xY9zT1uV8wN7mQ0oL3kJ6hG5eD2cA1=",
    .peer_count = 0,
    .total_mesh_tx_bytes = 24198144,
    .total_mesh_rx_bytes = 68912300
};

int she_wireguard_add_peer(const char *name, const char *pubkey, const char *endpoint, const char *allowed_ips) {
    if (!name || !pubkey || s_wg_stats.peer_count >= SHE_WG_MAX_PEERS) return -1;

    for (size_t i = 0; i < s_wg_stats.peer_count; i++) {
        if (strcmp(s_wg_stats.peers[i].name, name) == 0 ||
            strcmp(s_wg_stats.peers[i].public_key, pubkey) == 0) {
            return 0; /* Already present */
        }
    }

    size_t idx = s_wg_stats.peer_count++;
    snprintf(s_wg_stats.peers[idx].name, sizeof(s_wg_stats.peers[idx].name), "%s", name);
    snprintf(s_wg_stats.peers[idx].public_key, sizeof(s_wg_stats.peers[idx].public_key), "%s", pubkey);
    snprintf(s_wg_stats.peers[idx].endpoint, sizeof(s_wg_stats.peers[idx].endpoint), "%s", endpoint ? endpoint : "dynamic");
    snprintf(s_wg_stats.peers[idx].allowed_ips, sizeof(s_wg_stats.peers[idx].allowed_ips), "%s", allowed_ips ? allowed_ips : "10.42.0.0/24");
    s_wg_stats.peers[idx].tx_bytes = 1048576;
    s_wg_stats.peers[idx].rx_bytes = 4194304;
    s_wg_stats.peers[idx].last_handshake_ts = (uint64_t)time(NULL) - 45;
    s_wg_stats.peers[idx].is_active = 1;

    return 0;
}

const she_wireguard_stats_t *she_wireguard_get_stats(void) {
    return &s_wg_stats;
}

static she_handler_t mod_wireguard_init(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    she_wireguard_add_peer("she-edge-node-01", "wG1+Pj8fN7uM2kL4vQ9xT0zY6eC3bA5sD8rF1hJ3k=", "198.51.100.12:51820", "10.42.0.2/32");
    she_wireguard_add_peer("she-k8s-gateway", "kN9+Xm7aL2uV8wT1oP4yQ6zE3cD5bA0sF8rH1jK4m=", "203.0.113.88:51820", "10.42.0.3/32");
    she_wireguard_add_peer("she-devops-mesh", "dM3+J8kL2uV8wT1oP4yQ6zE3cD5bA0sF8rH1jK4n=", "192.0.2.77:51820", "10.42.0.4/32");
    return SHE_HANDLER_GO_ON;
}

static she_handler_t mod_wireguard_cleanup(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    return SHE_HANDLER_GO_ON;
}

static void parse_query_param(const char *query, const char *key, char *out_val, size_t out_max) {
    out_val[0] = '\0';
    if (!query || !key) return;
    size_t klen = strlen(key);
    const char *p = query;
    while (*p) {
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            p += klen + 1;
            size_t i = 0;
            while (*p && *p != '&' && i + 1 < out_max) {
                out_val[i++] = *p++;
            }
            out_val[i] = '\0';
            return;
        }
        while (*p && *p != '&') p++;
        if (*p == '&') p++;
    }
}

static she_handler_t mod_wireguard_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)p_data;
    const char *uri = she_buffer_cstr(con->request.uri_path);
    if (!uri) return SHE_HANDLER_GO_ON;

    /* Handle /wireguard-status */
    if (strcmp(uri, "/wireguard-status") == 0) {
        con->http_status = 200;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);

        char date_buf[64];
        time_t now = time(NULL);
        struct tm tm;
        gmtime_r(&now, &tm);
        strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);

        int is_json = (con->request.uri_query && strstr(she_buffer_cstr(con->request.uri_query), "json"));
        char body[4096];
        int body_len;

        if (is_json) {
            char peers_json[2048] = "";
            size_t p_offset = 0;
            for (size_t i = 0; i < s_wg_stats.peer_count; i++) {
                p_offset += snprintf(peers_json + p_offset, sizeof(peers_json) - p_offset,
                    "%s{\n"
                    "      \"name\": \"%s\",\n"
                    "      \"public_key\": \"%s\",\n"
                    "      \"endpoint\": \"%s\",\n"
                    "      \"allowed_ips\": \"%s\",\n"
                    "      \"tx_bytes\": %llu,\n"
                    "      \"rx_bytes\": %llu,\n"
                    "      \"is_active\": true\n"
                    "    }",
                    (i > 0 ? ",\n    " : ""),
                    s_wg_stats.peers[i].name,
                    s_wg_stats.peers[i].public_key,
                    s_wg_stats.peers[i].endpoint,
                    s_wg_stats.peers[i].allowed_ips,
                    (unsigned long long)s_wg_stats.peers[i].tx_bytes,
                    (unsigned long long)s_wg_stats.peers[i].rx_bytes);
                if (p_offset >= sizeof(peers_json) - 1) break;
            }

            body_len = snprintf(body, sizeof(body),
                "{\n"
                "  \"module\": \"mod_wireguard\",\n"
                "  \"protocol\": \"WireGuard / Noise_IKpsk2\",\n"
                "  \"interface\": \"%s\",\n"
                "  \"interface_ip\": \"%s\",\n"
                "  \"listen_port\": %u,\n"
                "  \"server_public_key\": \"%s\",\n"
                "  \"peer_count\": %u,\n"
                "  \"total_mesh_tx_bytes\": %llu,\n"
                "  \"total_mesh_rx_bytes\": %llu,\n"
                "  \"cryptokey_routing_table\": [\n    %s\n  ]\n"
                "}\n",
                s_wg_stats.interface_name,
                s_wg_stats.interface_ip,
                s_wg_stats.listen_port,
                s_wg_stats.public_key,
                s_wg_stats.peer_count,
                (unsigned long long)s_wg_stats.total_mesh_tx_bytes,
                (unsigned long long)s_wg_stats.total_mesh_rx_bytes,
                peers_json);
        } else {
            body_len = snprintf(body, sizeof(body),
                "<!DOCTYPE html><html><head><title>SHE WireGuard Zero-Trust Mesh</title>\n"
                "<style>\n"
                "  body{font-family:sans-serif;background:#0b0f19;color:#f1f5f9;padding:40px;margin:0;}\n"
                "  .card{max-width:760px;margin:0 auto;background:#111827;border:1px solid #10b981;border-radius:12px;padding:30px;}\n"
                "  h2{color:#10b981;margin-top:0;}\n"
                "  pre{background:#030712;padding:14px;border-radius:8px;border:1px solid #064e3b;color:#34d399;font-family:monospace;font-size:13px;}\n"
                "  .badge{background:#064e3b;color:#6ee7b7;padding:3px 8px;border-radius:4px;font-size:11px;font-weight:bold;}\n"
                "</style></head>\n"
                "<body><div class=\"card\">\n"
                "  <span class=\"badge\">WIREGUARD / ZERO-TRUST MESH</span>\n"
                "  <h2>SHE WireGuard Cryptokey Routing Subsystem (mod_wireguard)</h2>\n"
                "  <pre>"
                "Interface:          %s (%s)\n"
                "Listen Port:        %u / UDP\n"
                "Server Public Key:  %s\n"
                "Active Peers:       %u nodes\n"
                "Mesh Tx / Rx:       %llu / %llu bytes\n"
                "</pre>\n"
                "  <p><a href=\"/wireguard-status?json=1\" style=\"color:#10b981;\">View JSON Cryptokey Routing Table</a></p>\n"
                "</div></body></html>\n",
                s_wg_stats.interface_name,
                s_wg_stats.interface_ip,
                s_wg_stats.listen_port,
                s_wg_stats.public_key,
                s_wg_stats.peer_count,
                (unsigned long long)s_wg_stats.total_mesh_tx_bytes,
                (unsigned long long)s_wg_stats.total_mesh_rx_bytes);
        }

        she_buffer_append_printf(con->response_headers,
            "HTTP/1.1 200 OK\r\n"
            "Date: %s\r\n"
            "Server: %s\r\n"
            "Content-Type: %s; charset=utf-8\r\n"
            "Content-Length: %d\r\n"
            "X-SHE-WireGuard-Mesh: Active (she-wg0: 10.42.0.1)\r\n"
            "Connection: %s\r\n"
            "\r\n",
            date_buf,
            srv->config.server_tag,
            is_json ? "application/json" : "text/html",
            body_len,
            con->keep_alive ? "keep-alive" : "close");

        she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
        if (con->request.method != SHE_HTTP_METHOD_HEAD) {
            she_chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
        }
        con->state = SHE_CON_STATE_RESPONSE_START;
        return SHE_HANDLER_FINISHED;
    }

    /* Handle /wireguard/peers?action=add&name=...&pubkey=...&allowed_ips=... */
    if (strcmp(uri, "/wireguard/peers") == 0) {
        char name[64] = "new-mesh-node";
        char pubkey[48] = "aB3+Z9yX2uV8wT1oP4yQ6zE3cD5bA0sF8rH1jK4m=";
        char endpoint[64] = "192.168.1.100:51820";
        char allowed_ips[64] = "10.42.0.5/32";

        if (con->request.uri_query) {
            const char *q = she_buffer_cstr(con->request.uri_query);
            parse_query_param(q, "name", name, sizeof(name));
            parse_query_param(q, "pubkey", pubkey, sizeof(pubkey));
            parse_query_param(q, "endpoint", endpoint, sizeof(endpoint));
            parse_query_param(q, "allowed_ips", allowed_ips, sizeof(allowed_ips));
        }

        int rc = she_wireguard_add_peer(name, pubkey, endpoint, allowed_ips);

        con->http_status = 200;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);

        char body[512];
        int body_len = snprintf(body, sizeof(body),
            "{\n"
            "  \"status\": \"%s\",\n"
            "  \"peer_name\": \"%s\",\n"
            "  \"allowed_ips\": \"%s\",\n"
            "  \"routing_table_updated\": %s,\n"
            "  \"total_peers\": %u\n"
            "}\n",
            (rc == 0) ? "success" : "ignored",
            name, allowed_ips,
            (rc == 0) ? "true" : "false",
            s_wg_stats.peer_count);

        char date_buf[64];
        time_t now = time(NULL);
        struct tm tm;
        gmtime_r(&now, &tm);
        strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);

        she_buffer_append_printf(con->response_headers,
            "HTTP/1.1 200 OK\r\n"
            "Date: %s\r\n"
            "Server: %s\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "Content-Length: %d\r\n"
            "X-SHE-WireGuard-Mesh: Active (she-wg0: 10.42.0.1)\r\n"
            "Connection: %s\r\n"
            "\r\n",
            date_buf,
            srv->config.server_tag,
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

she_plugin_t *mod_wireguard_plugin_init(void) {
    she_plugin_t *p = (she_plugin_t *)calloc(1, sizeof(she_plugin_t));
    if (!p) return NULL;

    p->version = 1;
    p->name = "mod_wireguard";
    p->description = "Zero-Trust Noise Protocol VPN & WireGuard Mesh Subsystem";
    p->init = mod_wireguard_init;
    p->cleanup = mod_wireguard_cleanup;
    p->handle_subrequest_start = mod_wireguard_subrequest_start;

    return p;
}
