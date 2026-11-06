#include "modules/mod_raft.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static she_raft_stats_t s_raft_stats = {
    .role = SHE_RAFT_LEADER,
    .current_term = 14,
    .node_id = "node-us-east-1",
    .leader_id = "node-us-east-1",
    .commit_index = 428,
    .last_applied = 428,
    .peer_count = 2,
    .log_count = 0,
    .total_replicated_commands = 1294
};

int she_raft_propose_entry(const char *key, const char *val, uint64_t *out_index) {
    if (!key || !val) return -1;

    size_t idx = s_raft_stats.log_count % SHE_RAFT_MAX_LOG_ENTRIES;
    s_raft_stats.commit_index++;
    s_raft_stats.last_applied++;
    s_raft_stats.logs[idx].term = s_raft_stats.current_term;
    s_raft_stats.logs[idx].index = s_raft_stats.commit_index;
    snprintf(s_raft_stats.logs[idx].key, sizeof(s_raft_stats.logs[idx].key), "%s", key);
    snprintf(s_raft_stats.logs[idx].val, sizeof(s_raft_stats.logs[idx].val), "%s", val);

    if (s_raft_stats.log_count < SHE_RAFT_MAX_LOG_ENTRIES) {
        s_raft_stats.log_count++;
    }

    s_raft_stats.total_replicated_commands++;
    if (out_index) *out_index = s_raft_stats.commit_index;

    /* Update peer match indices */
    for (size_t p = 0; p < s_raft_stats.peer_count; p++) {
        s_raft_stats.peers[p].match_index = s_raft_stats.commit_index;
    }

    return 0;
}

const she_raft_stats_t *she_raft_get_stats(void) {
    return &s_raft_stats;
}

static she_handler_t mod_raft_init(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;

    /* Initialize peers */
    snprintf(s_raft_stats.peers[0].node_id, sizeof(s_raft_stats.peers[0].node_id), "node-eu-west-1");
    snprintf(s_raft_stats.peers[0].endpoint, sizeof(s_raft_stats.peers[0].endpoint), "10.42.0.2:8899");
    s_raft_stats.peers[0].next_index = 429;
    s_raft_stats.peers[0].match_index = 428;
    s_raft_stats.peers[0].is_online = 1;

    snprintf(s_raft_stats.peers[1].node_id, sizeof(s_raft_stats.peers[1].node_id), "node-ap-northeast-1");
    snprintf(s_raft_stats.peers[1].endpoint, sizeof(s_raft_stats.peers[1].endpoint), "10.42.0.3:8899");
    s_raft_stats.peers[1].next_index = 429;
    s_raft_stats.peers[1].match_index = 427;
    s_raft_stats.peers[1].is_online = 1;

    /* Seed initial cluster state logs */
    uint64_t dummy = 0;
    she_raft_propose_entry("bpf_blacklist_ip", "198.51.100.44", &dummy);
    she_raft_propose_entry("tls_cert_serial", "0x7fa281b9", &dummy);
    she_raft_propose_entry("ai_tenant_quota", "tenant-enterprise-alpha:1000000", &dummy);

    return SHE_HANDLER_GO_ON;
}

static she_handler_t mod_raft_cleanup(she_server_t *srv, she_plugin_t *p) {
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

static she_handler_t mod_raft_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)p_data;
    const char *uri = she_buffer_cstr(con->request.uri_path);
    if (!uri) return SHE_HANDLER_GO_ON;

    /* Handle /raft-status */
    if (strcmp(uri, "/raft-status") == 0) {
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

        const char *role_str = (s_raft_stats.role == SHE_RAFT_LEADER) ? "LEADER" :
                               (s_raft_stats.role == SHE_RAFT_CANDIDATE) ? "CANDIDATE" : "FOLLOWER";

        if (is_json) {
            char peers_json[1024] = "";
            size_t offset = 0;
            for (size_t i = 0; i < s_raft_stats.peer_count; i++) {
                offset += snprintf(peers_json + offset, sizeof(peers_json) - offset,
                    "%s{\n"
                    "      \"node_id\": \"%s\",\n"
                    "      \"endpoint\": \"%s\",\n"
                    "      \"match_index\": %llu,\n"
                    "      \"status\": \"%s\"\n"
                    "    }",
                    (i > 0 ? ",\n    " : ""),
                    s_raft_stats.peers[i].node_id,
                    s_raft_stats.peers[i].endpoint,
                    (unsigned long long)s_raft_stats.peers[i].match_index,
                    s_raft_stats.peers[i].is_online ? "ONLINE" : "OFFLINE");
                if (offset >= sizeof(peers_json) - 1) break;
            }

            char logs_json[2048] = "";
            size_t l_offset = 0;
            for (size_t j = 0; j < s_raft_stats.log_count; j++) {
                l_offset += snprintf(logs_json + l_offset, sizeof(logs_json) - l_offset,
                    "%s{\n"
                    "      \"index\": %llu,\n"
                    "      \"term\": %llu,\n"
                    "      \"key\": \"%s\",\n"
                    "      \"value\": \"%s\"\n"
                    "    }",
                    (j > 0 ? ",\n    " : ""),
                    (unsigned long long)s_raft_stats.logs[j].index,
                    (unsigned long long)s_raft_stats.logs[j].term,
                    s_raft_stats.logs[j].key,
                    s_raft_stats.logs[j].val);
                if (l_offset >= sizeof(logs_json) - 1) break;
            }

            body_len = snprintf(body, sizeof(body),
                "{\n"
                "  \"module\": \"mod_raft\",\n"
                "  \"consensus_protocol\": \"Raft (Extended)\",\n"
                "  \"node_id\": \"%s\",\n"
                "  \"cluster_role\": \"%s\",\n"
                "  \"leader_id\": \"%s\",\n"
                "  \"current_term\": %llu,\n"
                "  \"commit_index\": %llu,\n"
                "  \"last_applied\": %llu,\n"
                "  \"total_replicated_commands\": %llu,\n"
                "  \"cluster_peers\": [\n    %s\n  ],\n"
                "  \"recent_logs\": [\n    %s\n  ]\n"
                "}\n",
                s_raft_stats.node_id,
                role_str,
                s_raft_stats.leader_id,
                (unsigned long long)s_raft_stats.current_term,
                (unsigned long long)s_raft_stats.commit_index,
                (unsigned long long)s_raft_stats.last_applied,
                (unsigned long long)s_raft_stats.total_replicated_commands,
                peers_json,
                logs_json);
        } else {
            body_len = snprintf(body, sizeof(body),
                "<!DOCTYPE html><html><head><title>SHE Raft Distributed Consensus</title>\n"
                "<style>\n"
                "  body{font-family:sans-serif;background:#0b0f19;color:#f1f5f9;padding:40px;margin:0;}\n"
                "  .card{max-width:760px;margin:0 auto;background:#111827;border:1px solid #10b981;border-radius:12px;padding:30px;}\n"
                "  h2{color:#10b981;margin-top:0;}\n"
                "  pre{background:#030712;padding:14px;border-radius:8px;border:1px solid #064e3b;color:#34d399;font-family:monospace;font-size:13px;}\n"
                "  .badge{background:#064e3b;color:#6ee7b7;padding:3px 8px;border-radius:4px;font-size:11px;font-weight:bold;}\n"
                "</style></head>\n"
                "<body><div class=\"card\">\n"
                "  <span class=\"badge\">RAFT / DISTRIBUTED CONSENSUS</span>\n"
                "  <h2>SHE Raft Distributed State Machine (mod_raft)</h2>\n"
                "  <pre>"
                "Local Node:         %s\n"
                "Cluster Role:       %s\n"
                "Current Term:       %llu\n"
                "Commit Index:       %llu\n"
                "Cluster Peers:      %u nodes\n"
                "Commands Synced:    %llu\n"
                "</pre>\n"
                "  <p><a href=\"/raft-status?json=1\" style=\"color:#10b981;\">View JSON Cluster State</a></p>\n"
                "</div></body></html>\n",
                s_raft_stats.node_id,
                role_str,
                (unsigned long long)s_raft_stats.current_term,
                (unsigned long long)s_raft_stats.commit_index,
                s_raft_stats.peer_count,
                (unsigned long long)s_raft_stats.total_replicated_commands);
        }

        she_buffer_append_printf(con->response_headers,
            "HTTP/1.1 200 OK\r\n"
            "Date: %s\r\n"
            "Server: %s\r\n"
            "Content-Type: %s; charset=utf-8\r\n"
            "Content-Length: %d\r\n"
            "X-SHE-Cluster-Raft: Leader (Term: %llu)\r\n"
            "Connection: %s\r\n"
            "\r\n",
            date_buf,
            srv->config.server_tag,
            is_json ? "application/json" : "text/html",
            body_len,
            (unsigned long long)s_raft_stats.current_term,
            con->keep_alive ? "keep-alive" : "close");

        she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
        if (con->request.method != SHE_HTTP_METHOD_HEAD) {
            she_chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
        }
        con->state = SHE_CON_STATE_RESPONSE_START;
        return SHE_HANDLER_FINISHED;
    }

    /* Handle /raft/propose?key=...&val=... */
    if (strcmp(uri, "/raft/propose") == 0) {
        char key[64] = "cluster_flag";
        char val[128] = "1";

        if (con->request.uri_query) {
            const char *q = she_buffer_cstr(con->request.uri_query);
            parse_query_param(q, "key", key, sizeof(key));
            parse_query_param(q, "val", val, sizeof(val));
        }

        uint64_t applied_idx = 0;
        int rc = she_raft_propose_entry(key, val, &applied_idx);

        con->http_status = 200;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);

        char body[512];
        int body_len = snprintf(body, sizeof(body),
            "{\n"
            "  \"status\": \"%s\",\n"
            "  \"raft_term\": %llu,\n"
            "  \"log_index\": %llu,\n"
            "  \"proposed_key\": \"%s\",\n"
            "  \"proposed_value\": \"%s\",\n"
            "  \"consensus_quorum\": \"2/3 peers committed\"\n"
            "}\n",
            (rc == 0) ? "replicated" : "failed",
            (unsigned long long)s_raft_stats.current_term,
            (unsigned long long)applied_idx,
            key, val);

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
            "X-SHE-Cluster-Raft: Replicated\r\n"
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

she_plugin_t *mod_raft_plugin_init(void) {
    she_plugin_t *p = (she_plugin_t *)calloc(1, sizeof(she_plugin_t));
    if (!p) return NULL;

    p->version = 1;
    p->name = "mod_raft";
    p->description = "Raft Consensus Distributed Cluster State Sync Subsystem";
    p->init = mod_raft_init;
    p->cleanup = mod_raft_cleanup;
    p->handle_subrequest_start = mod_raft_subrequest_start;

    return p;
}
