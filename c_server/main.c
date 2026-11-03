#include "server.h"
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
    if (g_server) {
        g_server->is_running = 0;
    }
}

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -p <port>         Listen port (default: 8080)\n");
    printf("  -b <ip>           Bind IP address (default: 0.0.0.0)\n");
    printf("  -d <dir>          Document root directory (default: ./www)\n");
    printf("  -k <seconds>      Keep-alive timeout in seconds (default: 15)\n");
    printf("  -m <connections>  Max simultaneous connections (default: 1024)\n");
    printf("  -v                Verbose mode\n");
    printf("  -h                Show this help message\n");
}

int main(int argc, char *argv[]) {
    server_config_t config;
    memset(&config, 0, sizeof(config));
    
    strcpy(config.bind_ip, "0.0.0.0");
    config.port = 8080;
    strcpy(config.document_root, "./www");
    config.max_connections = 1024;
    config.keepalive_timeout_sec = 15;
    config.max_keepalive_requests = 100;
    strcpy(config.server_tag, "c-lighttpd/1.0");
    config.verbose = 1;
    
    int opt;
    while ((opt = getopt(argc, argv, "p:b:d:k:m:vh")) != -1) {
        switch (opt) {
            case 'p': config.port = atoi(optarg); break;
            case 'b': strncpy(config.bind_ip, optarg, sizeof(config.bind_ip) - 1); break;
            case 'd': strncpy(config.document_root, optarg, sizeof(config.document_root) - 1); break;
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
    signal(SIGPIPE, SIG_IGN); /* Ignore broken pipe signals */
    
    printf("=====================================================\n");
    printf("           c-lighttpd Web Server v1.0                 \n");
    printf("       Lighttpd Architecture in Pure C (Linux epoll)  \n");
    printf("=====================================================\n");
    printf(" Port:            %d\n", config.port);
    printf(" Bind Address:    %s\n", config.bind_ip);
    printf(" Document Root:   %s\n", config.document_root);
    printf(" Max Connections: %d\n", config.max_connections);
    printf(" Keep-Alive:      %d s (max %d reqs/con)\n", 
           config.keepalive_timeout_sec, config.max_keepalive_requests);
    printf(" I/O Model:       Non-blocking epoll + Zero-copy sendfile\n");
    printf(" State Machine:   Lighttpd CON_STATE 10-state engine\n");
    printf("=====================================================\n");
    
    server_t *srv = server_init(&config);
    if (!srv) {
        fprintf(stderr, "Failed to initialize server\n");
        return 1;
    }
    
    if (server_start(srv) != 0) {
        server_free(srv);
        return 1;
    }
    
    printf("[READY] Server started successfully. Press Ctrl+C to terminate.\n");
    
    while (!g_shutdown && srv->is_running) {
        server_step(srv, 100);
    }
    
    printf("\n[SHUTDOWN] Terminating gracefully...\n");
    printf(" Statistics summary:\n");
    printf("  Total Connections: %llu\n", (unsigned long long)srv->stats.total_connections);
    printf("  Total Requests:    %llu\n", (unsigned long long)srv->stats.total_requests);
    printf("  Bytes Read:        %llu\n", (unsigned long long)srv->stats.bytes_read);
    printf("  Bytes Written:     %llu\n", (unsigned long long)srv->stats.bytes_written);
    
    server_free(srv);
    printf("[DONE] Exited cleanly.\n");
    return 0;
}
