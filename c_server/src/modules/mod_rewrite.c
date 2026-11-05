#include "modules/mod_rewrite.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char source[128];
    char target[256];
    int status_code; /* 301, 302, or 0 for internal rewrite */
} she_rewrite_rule_t;

#define MAX_REWRITE_RULES 16
static she_rewrite_rule_t g_rules[MAX_REWRITE_RULES];
static size_t g_rule_count = 0;

static void parse_rewrite_rules(const char *rewrite_str, const char *redirect_str) {
    g_rule_count = 0;
    
    /* 1. Parse internal rewrites: "src=>dst,src2=>dst2" */
    if (rewrite_str && *rewrite_str) {
        char buf[512];
        strncpy(buf, rewrite_str, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        
        char *tok = strtok(buf, ",");
        while (tok && g_rule_count < MAX_REWRITE_RULES) {
            char *arrow = strstr(tok, "=>");
            if (arrow) {
                *arrow = '\0';
                char *src = tok;
                char *dst = arrow + 2;
                while (*src == ' ') src++;
                while (*dst == ' ') dst++;
                
                strncpy(g_rules[g_rule_count].source, src, sizeof(g_rules[0].source) - 1);
                strncpy(g_rules[g_rule_count].target, dst, sizeof(g_rules[0].target) - 1);
                g_rules[g_rule_count].status_code = 0; /* internal */
                g_rule_count++;
            }
            tok = strtok(NULL, ",");
        }
    }
    
    /* 2. Parse redirects: "src=>301:dst,src2=>302:dst2" */
    if (redirect_str && *redirect_str) {
        char buf[512];
        strncpy(buf, redirect_str, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        
        char *tok = strtok(buf, ",");
        while (tok && g_rule_count < MAX_REWRITE_RULES) {
            char *arrow = strstr(tok, "=>");
            if (arrow) {
                *arrow = '\0';
                char *src = tok;
                char *dst = arrow + 2;
                while (*src == ' ') src++;
                while (*dst == ' ') dst++;
                
                int code = 302;
                if (strncmp(dst, "301:", 4) == 0) {
                    code = 301;
                    dst += 4;
                } else if (strncmp(dst, "302:", 4) == 0) {
                    code = 302;
                    dst += 4;
                }
                
                strncpy(g_rules[g_rule_count].source, src, sizeof(g_rules[0].source) - 1);
                strncpy(g_rules[g_rule_count].target, dst, sizeof(g_rules[0].target) - 1);
                g_rules[g_rule_count].status_code = code;
                g_rule_count++;
            }
            tok = strtok(NULL, ",");
        }
    }
    
    /* Defaults if no rules configured */
    if (g_rule_count == 0) {
        /* Default demo redirect: /legacy -> 301 /index.html */
        strncpy(g_rules[0].source, "/legacy", sizeof(g_rules[0].source) - 1);
        strncpy(g_rules[0].target, "/index.html", sizeof(g_rules[0].target) - 1);
        g_rules[0].status_code = 301;
        
        /* Default demo rewrite: /old-docs -> /docs/index.html */
        strncpy(g_rules[1].source, "/old-docs", sizeof(g_rules[1].source) - 1);
        strncpy(g_rules[1].target, "/docs/index.html", sizeof(g_rules[1].target) - 1);
        g_rules[1].status_code = 0;
        
        /* Default demo redirect: /guide -> 302 /docs/ */
        strncpy(g_rules[2].source, "/guide", sizeof(g_rules[2].source) - 1);
        strncpy(g_rules[2].target, "/docs/", sizeof(g_rules[2].target) - 1);
        g_rules[2].status_code = 302;
        
        g_rule_count = 3;
    }
}

static she_handler_t mod_rewrite_uri_clean(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)p_data;
    if (!srv || !con) return SHE_HANDLER_GO_ON;
    
    const char *uri = she_buffer_cstr(con->request.uri_path);
    if (!uri) return SHE_HANDLER_GO_ON;
    
    for (size_t i = 0; i < g_rule_count; i++) {
        const char *src = g_rules[i].source;
        size_t slen = strlen(src);
        
        /* Check exact match or prefix match */
        int matched = 0;
        if (strcmp(uri, src) == 0) {
            matched = 1;
        } else if (slen > 1 && strncmp(uri, src, slen) == 0 && (uri[slen] == '/' || uri[slen] == '\0')) {
            matched = 1;
        }
        
        if (matched) {
            if (g_rules[i].status_code == 301 || g_rules[i].status_code == 302) {
                int code = g_rules[i].status_code;
                con->http_status = code;
                if (code == 301) srv->stats.status_301++;
                else srv->stats.status_302++;
                
                she_buffer_reset(con->response_headers);
                she_chunkqueue_reset(con->write_queue);
                
                time_t now = time(NULL);
                struct tm tm;
                gmtime_r(&now, &tm);
                char date_buf[64];
                strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);
                
                const char *reason = (code == 301) ? "Moved Permanently" : "Found";
                const char *loc = g_rules[i].target;
                
                char body[512];
                int blen = snprintf(body, sizeof(body),
                    "<!DOCTYPE html><html><head><title>%d %s</title></head>\n"
                    "<body style=\"font-family:sans-serif;text-align:center;padding:50px;background:#0b0f19;color:#f1f5f9;\">\n"
                    "<h1>%d %s</h1>\n"
                    "<p>The document has moved <a href=\"%s\" style=\"color:#10b981;\">here</a>.</p>\n"
                    "</body></html>\n",
                    code, reason, code, reason, loc);
                    
                she_buffer_append_printf(con->response_headers,
                    "HTTP/1.1 %d %s\r\n"
                    "Date: %s\r\n"
                    "Server: %s\r\n"
                    "Location: %s\r\n"
                    "Content-Type: text/html; charset=utf-8\r\n"
                    "Content-Length: %d\r\n",
                    code, reason,
                    date_buf,
                    srv->config.server_tag,
                    loc,
                    blen);
                    
                if (srv->security) {
                    she_security_append_headers(srv->security, con);
                }
                
                she_buffer_append_printf(con->response_headers,
                    "Connection: %s\r\n\r\n",
                    con->keep_alive ? "keep-alive" : "close");
                    
                she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
                if (con->request.method != SHE_HTTP_METHOD_HEAD) {
                    she_chunkqueue_append_mem(con->write_queue, body, (size_t)blen);
                }
                
                con->state = SHE_CON_STATE_RESPONSE_START;
                return SHE_HANDLER_FINISHED;
            } else {
                /* Internal transparent rewrite */
                she_buffer_copy_string(con->request.uri_path, g_rules[i].target);
                return SHE_HANDLER_GO_ON;
            }
        }
    }
    
    return SHE_HANDLER_GO_ON;
}

static she_handler_t mod_rewrite_init(she_server_t *srv, she_plugin_t *p) {
    (void)p;
    parse_rewrite_rules(srv->config.rewrite_rules, srv->config.redirect_rules);
    return SHE_HANDLER_GO_ON;
}

she_plugin_t *mod_rewrite_plugin_init(void) {
    static she_plugin_t p;
    memset(&p, 0, sizeof(p));
    p.version = 1;
    p.name = "mod_rewrite";
    p.description = "SHE URL Rewriting & HTTP 301/302 Redirection Subsystem";
    p.init = mod_rewrite_init;
    p.handle_uri_clean = mod_rewrite_uri_clean;
    return &p;
}
