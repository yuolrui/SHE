#include "core/she_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

static void unquote(char *s) {
    size_t len = strlen(s);
    if (len >= 2 && ((s[0] == '"' && s[len - 1] == '"') || (s[0] == '\'' && s[len - 1] == '\''))) {
        memmove(s, s + 1, len - 2);
        s[len - 2] = '\0';
    }
}

int she_config_load_file(she_server_config_t *cfg, const char *filepath) {
    if (!cfg || !filepath) return -1;
    
    FILE *f = fopen(filepath, "r");
    if (!f) return -1;
    
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = trim(line);
        if (*p == '#' || *p == ';' || *p == '\0') continue;
        
        char *eq = strchr(p, '=');
        if (!eq) continue;
        
        *eq = '\0';
        char *key = trim(p);
        char *val = trim(eq + 1);
        unquote(val);
        
        if (strcmp(key, "server.port") == 0) {
            cfg->port = atoi(val);
        } else if (strcmp(key, "server.bind") == 0) {
            strncpy(cfg->bind_ip, val, sizeof(cfg->bind_ip) - 1);
        } else if (strcmp(key, "server.document-root") == 0) {
            strncpy(cfg->document_root, val, sizeof(cfg->document_root) - 1);
        } else if (strcmp(key, "server.tag") == 0) {
            strncpy(cfg->server_tag, val, sizeof(cfg->server_tag) - 1);
        } else if (strcmp(key, "server.max-connections") == 0) {
            cfg->max_connections = atoi(val);
        } else if (strcmp(key, "server.keepalive-timeout") == 0) {
            cfg->keepalive_timeout_sec = atoi(val);
        } else if (strcmp(key, "server.max-keepalive-requests") == 0) {
            cfg->max_keepalive_requests = atoi(val);
        } else if (strcmp(key, "security.rate-limit-enable") == 0) {
            cfg->security_cfg.enable_rate_limit = atoi(val);
        } else if (strcmp(key, "security.rate-limit-rate") == 0) {
            cfg->security_cfg.rate_limit_rps = atof(val);
        } else if (strcmp(key, "security.rate-limit-burst") == 0) {
            cfg->security_cfg.rate_limit_burst = atof(val);
        } else if (strcmp(key, "security.enable-headers") == 0) {
            cfg->security_cfg.enable_security_headers = atoi(val);
        } else if (strcmp(key, "security.strict-host-check") == 0) {
            cfg->security_cfg.strict_host_check = atoi(val);
        } else if (strcmp(key, "auth.enable") == 0) {
            cfg->auth_enabled = atoi(val);
        } else if (strcmp(key, "auth.path") == 0) {
            strncpy(cfg->auth_path, val, sizeof(cfg->auth_path) - 1);
        } else if (strcmp(key, "auth.user") == 0) {
            strncpy(cfg->auth_user, val, sizeof(cfg->auth_user) - 1);
        } else if (strcmp(key, "auth.password") == 0) {
            strncpy(cfg->auth_pass, val, sizeof(cfg->auth_pass) - 1);
        } else if (strcmp(key, "vhost.enable") == 0) {
            cfg->vhost_enabled = atoi(val);
        } else if (strcmp(key, "vhost.map") == 0) {
            strncpy(cfg->vhost_map, val, sizeof(cfg->vhost_map) - 1);
        }
    }
    
    fclose(f);
    strncpy(cfg->config_file, filepath, sizeof(cfg->config_file) - 1);
    return 0;
}
