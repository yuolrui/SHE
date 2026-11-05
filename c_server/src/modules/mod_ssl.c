#include "modules/mod_ssl.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static SSL_CTX *g_ssl_ctx = NULL;

static int alpn_select_cb(SSL *ssl, const unsigned char **out, unsigned char *outlen,
                          const unsigned char *in, unsigned int inlen, void *arg) {
    (void)ssl;
    (void)arg;
    /* Client advertises protocols: look for "h2" then "http/1.1" */
    unsigned int i = 0;
    while (i < inlen) {
        unsigned char len = in[i++];
        if (i + len > inlen) break;
        if (len == 2 && memcmp(&in[i], "h2", 2) == 0) {
            *out = &in[i];
            *outlen = 2;
            return SSL_TLSEXT_ERR_OK;
        }
        i += len;
    }
    
    /* Default to http/1.1 */
    i = 0;
    while (i < inlen) {
        unsigned char len = in[i++];
        if (i + len > inlen) break;
        if (len == 8 && memcmp(&in[i], "http/1.1", 8) == 0) {
            *out = &in[i];
            *outlen = 8;
            return SSL_TLSEXT_ERR_OK;
        }
        i += len;
    }
    
    return SSL_TLSEXT_ERR_NOACK;
}

static she_handler_t mod_ssl_init(she_server_t *srv, she_plugin_t *p) {
    (void)p;
    (void)srv;
    
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    
    const SSL_METHOD *method = TLS_server_method();
    g_ssl_ctx = SSL_CTX_new(method);
    if (!g_ssl_ctx) {
        fprintf(stderr, "[WARN] mod_ssl: Failed to initialize OpenSSL SSL_CTX\n");
        return SHE_HANDLER_GO_ON;
    }
    
    /* Enforce modern TLS: Minimum TLS 1.2, preferred TLS 1.3 */
    SSL_CTX_set_min_proto_version(g_ssl_ctx, TLS1_2_VERSION);
    
    /* Configure TLS 1.3 Ciphersuites */
    SSL_CTX_set_ciphersuites(g_ssl_ctx, 
        "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256");
        
    /* Configure ALPN callback for HTTP/2 and HTTP/1.1 */
    SSL_CTX_set_alpn_select_cb(g_ssl_ctx, alpn_select_cb, NULL);
    
    return SHE_HANDLER_GO_ON;
}

static she_handler_t mod_ssl_cleanup(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    if (g_ssl_ctx) {
        SSL_CTX_free(g_ssl_ctx);
        g_ssl_ctx = NULL;
    }
    return SHE_HANDLER_GO_ON;
}

SSL_CTX *she_ssl_get_context(void) {
    return g_ssl_ctx;
}

static she_handler_t mod_ssl_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)p_data;
    const char *uri = she_buffer_cstr(con->request.uri_path);
    if (!uri) return SHE_HANDLER_GO_ON;
    
    if (strcmp(uri, "/ssl-status") == 0) {
        con->http_status = 200;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        
        char date_buf[64];
        time_t now = time(NULL);
        struct tm tm;
        gmtime_r(&now, &tm);
        strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        
        int is_json = (con->request.uri_query && strstr(she_buffer_cstr(con->request.uri_query), "json"));
        char body[2048];
        int body_len;
        
        if (is_json) {
            body_len = snprintf(body, sizeof(body),
                "{\n"
                "  \"module\": \"mod_ssl\",\n"
                "  \"status\": \"%s\",\n"
                "  \"openssl_version\": \"%s\",\n"
                "  \"min_tls_version\": \"TLSv1.2\",\n"
                "  \"max_tls_version\": \"TLSv1.3\",\n"
                "  \"alpn_protocols\": [\"h2\", \"http/1.1\"],\n"
                "  \"tls13_ciphers\": [\n"
                "    \"TLS_AES_256_GCM_SHA384\",\n"
                "    \"TLS_CHACHA20_POLY1305_SHA256\",\n"
                "    \"TLS_AES_128_GCM_SHA256\"\n"
                "  ]\n"
                "}\n",
                g_ssl_ctx ? "active" : "disabled",
                OpenSSL_version(OPENSSL_VERSION));
        } else {
            body_len = snprintf(body, sizeof(body),
                "<!DOCTYPE html><html><head><title>SHE TLS 1.3 Transport Security</title>\n"
                "<style>\n"
                "  body{font-family:sans-serif;background:#0b0f19;color:#f1f5f9;padding:40px;margin:0;}\n"
                "  .card{max-width:720px;margin:0 auto;background:#111827;border:1px solid #059669;border-radius:12px;padding:30px;}\n"
                "  h2{color:#10b981;margin-top:0;}\n"
                "  pre{background:#030712;padding:14px;border-radius:8px;border:1px solid #064e3b;color:#34d399;font-family:monospace;font-size:13px;}\n"
                "  .badge{background:#064e3b;color:#6ee7b7;padding:3px 8px;border-radius:4px;font-size:11px;font-weight:bold;}\n"
                "</style></head>\n"
                "<body><div class=\"card\">\n"
                "  <span class=\"badge\">TLS 1.3 / OPENSSL 3.0</span>\n"
                "  <h2>SHE Transport Layer Security Subsystem (mod_ssl)</h2>\n"
                "  <pre>"
                "Engine:             %s\n"
                "OpenSSL Version:    %s\n"
                "Supported Versions: TLSv1.2, TLSv1.3\n"
                "ALPN Protocols:     h2, http/1.1\n"
                "TLS 1.3 Ciphers:    TLS_AES_256_GCM_SHA384\n"
                "                    TLS_CHACHA20_POLY1305_SHA256\n"
                "                    TLS_AES_128_GCM_SHA256\n"
                "</pre>\n"
                "  <p><a href=\"/ssl-status?json=1\" style=\"color:#10b981;\">View JSON Security Telemetry</a></p>\n"
                "</div></body></html>\n",
                srv->config.server_tag,
                OpenSSL_version(OPENSSL_VERSION));
        }
        
        she_buffer_append_printf(con->response_headers,
            "HTTP/1.1 200 OK\r\n"
            "Date: %s\r\n"
            "Server: %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %d\r\n"
            "Connection: %s\r\n"
            "\r\n",
            date_buf,
            srv->config.server_tag,
            is_json ? "application/json; charset=utf-8" : "text/html; charset=utf-8",
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

she_plugin_t *mod_ssl_plugin_init(void) {
    static she_plugin_t p;
    memset(&p, 0, sizeof(p));
    p.version = 1;
    p.name = "mod_ssl";
    p.description = "SHE TLS 1.3 & TLS 1.2 Transport Security & ALPN Subsystem";
    p.init = mod_ssl_init;
    p.cleanup = mod_ssl_cleanup;
    p.handle_subrequest_start = mod_ssl_subrequest_start;
    return &p;
}
