#include "core/she_server.h"
#include "core/she_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>

static volatile sig_atomic_t g_shutdown = 0;

static void sig_handler(int sig) {
    (void)sig;
    g_shutdown = 1;
    if (g_she_server) {
        g_she_server->is_running = 0;
    }
}

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -c <conf>         Configuration file path (e.g. she.conf)\n");
    printf("  -p <port>         Listen port (default: 8899)\n");
    printf("  -b <ip>           Bind IP address (default: 0.0.0.0)\n");
    printf("  -d <dir>          Document root directory (default: ./www)\n");
    printf("  -w <workers>      Worker process count with SO_REUSEPORT (default: 1)\n");
    printf("  -k <seconds>      Keep-alive timeout in seconds (default: 15)\n");
    printf("  -m <connections>  Max simultaneous connections (default: 1024)\n");
    printf("  -v                Verbose mode\n");
    printf("  -h                Show this help message\n");
}

int main(int argc, char *argv[]) {
    she_server_config_t config;
    memset(&config, 0, sizeof(config));
    
    strcpy(config.bind_ip, "0.0.0.0");
    config.port = 8899;
    strcpy(config.document_root, "./www");
    config.max_connections = 1024;
    config.keepalive_timeout_sec = 15;
    config.max_keepalive_requests = 100;
    strcpy(config.server_tag, "SHE/1.0 (Secure HTTP Engine)");
    config.verbose = 1;
    config.workers = 1;
    
    config.security_cfg.enable_rate_limit = 1;
    config.security_cfg.rate_limit_rps = 40.0;
    config.security_cfg.rate_limit_burst = 30.0;
    config.security_cfg.enable_security_headers = 1;
    config.security_cfg.strict_host_check = 1;
    config.security_cfg.block_smuggling = 1;
    
    config.auth_enabled = 1;
    strcpy(config.auth_path, "/secure/");
    strcpy(config.auth_user, "admin");
    strcpy(config.auth_pass, "she_secret123");
    
    config.vhost_enabled = 1;
    strcpy(config.vhost_map, "docs.local:./www/docs,api.local:./www/api,secure.local:./www/secure");
    
    /* Phase 4 defaults */
    config.deflate_enabled = 1;
    config.deflate_min_size = 128;
    config.rewrite_enabled = 1;
    strcpy(config.rewrite_rules, "/old-docs=>/docs/index.html");
    strcpy(config.redirect_rules, "/legacy=>301:/index.html,/guide=>302:/docs/");
    config.proxy_enabled = 1;
    strcpy(config.proxy_path, "/proxy");
    strcpy(config.proxy_backend, "127.0.0.1:3000");
    
    /* Auto-load she.conf if present */
    if (access("she.conf", R_OK) == 0) {
        she_config_load_file(&config, "she.conf");
    }
    
    int opt;
    while ((opt = getopt(argc, argv, "c:p:b:d:w:k:m:vh")) != -1) {
        switch (opt) {
            case 'c': she_config_load_file(&config, optarg); break;
            case 'p': config.port = atoi(optarg); break;
            case 'b': strncpy(config.bind_ip, optarg, sizeof(config.bind_ip) - 1); break;
            case 'd': strncpy(config.document_root, optarg, sizeof(config.document_root) - 1); break;
            case 'w': config.workers = atoi(optarg); break;
            case 'k': config.keepalive_timeout_sec = atoi(optarg); break;
            case 'm': config.max_connections = atoi(optarg); break;
            case 'v': config.verbose = 1; break;
            case 'h': print_usage(argv[0]); return 0;
            default:  print_usage(argv[0]); return 1;
        }
    }
    
    /* Setup signal handlers */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
    
    printf("=====================================================\n");
    printf("            SHE - Secure HTTP Engine v1.0             \n");
    printf("       Lighttpd Architecture in Pure C (Linux epoll)  \n");
    printf("=====================================================\n");
    printf(" Port:            %d\n", config.port);
    printf(" Bind Address:    %s\n", config.bind_ip);
    printf(" Document Root:   %s\n", config.document_root);
    printf(" Workers:         %d (SO_REUSEPORT socket balancing)\n", config.workers > 0 ? config.workers : 1);
    printf(" Max Connections: %d\n", config.max_connections);
    printf(" Keep-Alive:      %d s (max %d reqs/con)\n", 
           config.keepalive_timeout_sec, config.max_keepalive_requests);
    printf(" Security Engine: Active (Token-bucket Rate Limit: %.0f rps, Burst: %.0f)\n",
           config.security_cfg.rate_limit_rps, config.security_cfg.rate_limit_burst);
    printf(" Security Headers: nosniff, SAMEORIGIN, XSS-Protection, Referrer\n");
    printf(" Basic Auth:      Active (%s, default admin:she_secret123)\n", config.auth_path);
    printf(" Virtual Hosts:   Active (docs.local, api.local, secure.local)\n");
    printf(" FastCGI Gateway: Active (/fastcgi, FastCGI 1.0 upstream client)\n");
    printf(" Gzip Deflate:    Active (RFC 1952 dynamic compression via zlib)\n");
    printf(" URL Rewrite:     Active (/old-docs => /docs/index.html, /legacy => 301)\n");
    printf(" Reverse Proxy:   Active (/proxy, Upstream X-Forwarded-* headers)\n");
    printf(" I/O Model:       Non-blocking epoll + Zero-copy sendfile\n");
    printf(" State Machine:   Lighttpd 10-state non-blocking FSM\n");
    printf("=====================================================\n");
    
    she_server_t *srv = she_server_init(&config);
    if (!srv) {
        fprintf(stderr, "Failed to initialize SHE server\n");
        return 1;
    }
    
    if (she_server_start(srv) != 0) {
        she_server_free(srv);
        return 1;
    }
    
    printf("[READY] SHE server started successfully. Press Ctrl+C to terminate.\n");
    
    while (!g_shutdown && srv->is_running) {
        she_server_step(srv, 100);
    }
    
    printf("\n[SHUTDOWN] Terminating SHE gracefully...\n");
    printf(" Statistics summary:\n");
    printf("  Total Connections: %llu\n", (unsigned long long)srv->stats.total_connections);
    printf("  Total Requests:    %llu\n", (unsigned long long)srv->stats.total_requests);
    printf("  Bytes Read:        %llu\n", (unsigned long long)srv->stats.bytes_read);
    printf("  Bytes Written:     %llu\n", (unsigned long long)srv->stats.bytes_written);
    if (srv->security) {
        printf("  Rate Limit Blocks: %llu\n", (unsigned long long)srv->security->total_rate_blocks);
        printf("  Host Rejections:   %llu\n", (unsigned long long)srv->security->total_host_rejections);
    }
    
    she_server_free(srv);
    printf("[DONE] Exited cleanly.\n");
    return 0;
}
