#include "modules/mod_accesslog.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    FILE *log_file;
    char log_path[512];
} mod_accesslog_data_t;

static mod_accesslog_data_t g_accesslog_data;

static she_handler_t mod_accesslog_init(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)mkdir("logs", 0755);
    g_accesslog_data.log_file = fopen("logs/access.log", "a");
    if (!g_accesslog_data.log_file) {
        g_accesslog_data.log_file = fopen("access.log", "a");
    }
    p->data = &g_accesslog_data;
    return SHE_HANDLER_GO_ON;
}

static she_handler_t mod_accesslog_cleanup(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    if (g_accesslog_data.log_file) {
        fflush(g_accesslog_data.log_file);
        fclose(g_accesslog_data.log_file);
        g_accesslog_data.log_file = NULL;
    }
    return SHE_HANDLER_GO_ON;
}

static she_handler_t mod_accesslog_response_done(she_server_t *srv, she_connection_t *con, void *p_data) {
    mod_accesslog_data_t *d = (mod_accesslog_data_t *)p_data;
    if (!d || !d->log_file || con->http_status == 0) return SHE_HANDLER_GO_ON;
    
    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%d/%b/%Y:%H:%M:%S +0000", &tm);
    
    const char *method_str = she_http_method_to_str(con->request.method);
    const char *uri_raw = she_buffer_cstr(con->request.uri_raw);
    if (!uri_raw || !*uri_raw) uri_raw = "/";
    
    const char *user_agent = she_http_request_get_header(&con->request, "User-Agent");
    if (!user_agent) user_agent = "-";
    
    const char *referer = she_http_request_get_header(&con->request, "Referer");
    if (!referer) referer = "-";
    
    /* Standard Apache / Lighttpd Combined Log Format:
     * %h %l %u [%t] "%r" %>s %b "%{Referer}i" "%{User-agent}i"
     */
    fprintf(d->log_file, "%s - - [%s] \"%s %s HTTP/1.1\" %d %lld \"%s\" \"%s\"\n",
            con->client_ip[0] ? con->client_ip : "127.0.0.1",
            time_str,
            method_str,
            uri_raw,
            con->http_status,
            (long long)con->bytes_sent,
            referer,
            user_agent
    );
    fflush(d->log_file);
    
    if (srv->config.verbose) {
        printf("[ACCESS] %s - \"%s %s\" %d (%lld bytes)\n",
               con->client_ip, method_str, uri_raw, con->http_status, (long long)con->bytes_sent);
    }
    
    return SHE_HANDLER_GO_ON;
}

she_plugin_t *mod_accesslog_plugin_init(void) {
    static she_plugin_t p;
    memset(&p, 0, sizeof(p));
    p.version = 1;
    p.name = "mod_accesslog";
    p.description = "Combined/Common Log Format access logger to logs/access.log";
    p.init = mod_accesslog_init;
    p.cleanup = mod_accesslog_cleanup;
    p.handle_response_done = mod_accesslog_response_done;
    p.data = &g_accesslog_data;
    return &p;
}
