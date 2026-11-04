#include "security/she_security.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static unsigned int ip_hash(const char *ip) {
    unsigned int hash = 5381;
    int c;
    while ((c = (unsigned char)*ip++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % SHE_RATE_LIMIT_BUCKET_SIZE;
}

she_security_t *she_security_init(const she_security_config_t *cfg) {
    she_security_t *sec = (she_security_t *)calloc(1, sizeof(she_security_t));
    if (!sec) return NULL;
    
    if (cfg) {
        sec->config = *cfg;
    } else {
        sec->config.enable_rate_limit = 1;
        sec->config.rate_limit_rps = 40.0;
        sec->config.rate_limit_burst = 30.0;
        sec->config.enable_security_headers = 1;
        sec->config.strict_host_check = 1;
        sec->config.block_smuggling = 1;
        strcpy(sec->config.allowed_host_suffix, "");
    }
    
    sec->total_rate_blocks = 0;
    sec->total_host_rejections = 0;
    sec->total_traversal_blocks = 0;
    
    return sec;
}

void she_security_free(she_security_t *sec) {
    if (!sec) return;
    
    for (int i = 0; i < SHE_RATE_LIMIT_BUCKET_SIZE; i++) {
        she_rate_entry_t *entry = sec->rate_buckets[i];
        while (entry) {
            she_rate_entry_t *next = entry->next;
            free(entry);
            entry = next;
        }
        sec->rate_buckets[i] = NULL;
    }
    
    free(sec);
}

int she_security_check_rate_limit(she_security_t *sec, const char *client_ip) {
    if (!sec || !sec->config.enable_rate_limit || !client_ip || !*client_ip) {
        return 1; /* Allowed */
    }
    
    time_t now = time(NULL);
    unsigned int bucket = ip_hash(client_ip);
    she_rate_entry_t *entry = sec->rate_buckets[bucket];
    
    while (entry) {
        if (strcmp(entry->ip, client_ip) == 0) {
            break;
        }
        entry = entry->next;
    }
    
    if (!entry) {
        entry = (she_rate_entry_t *)calloc(1, sizeof(she_rate_entry_t));
        if (!entry) return 1; /* Allow on OOM */
        strncpy(entry->ip, client_ip, sizeof(entry->ip) - 1);
        entry->tokens = sec->config.rate_limit_burst;
        entry->last_update = now;
        entry->violations = 0;
        entry->next = sec->rate_buckets[bucket];
        sec->rate_buckets[bucket] = entry;
    }
    
    /* Token bucket replenishment */
    double elapsed = difftime(now, entry->last_update);
    if (elapsed > 0) {
        entry->tokens += elapsed * sec->config.rate_limit_rps;
        if (entry->tokens > sec->config.rate_limit_burst) {
            entry->tokens = sec->config.rate_limit_burst;
        }
        entry->last_update = now;
    }
    
    if (entry->tokens >= 1.0) {
        entry->tokens -= 1.0;
        return 1; /* Allowed */
    } else {
        entry->violations++;
        sec->total_rate_blocks++;
        return 0; /* Rate limit exceeded (429) */
    }
}

int she_security_validate_host(she_security_t *sec, const char *host) {
    if (!sec || !sec->config.strict_host_check) return 1;
    if (!host || !*host) return 1; /* HTTP/1.0 may omit host */
    
    /* Disallow control characters, whitespace, slashes or CRLF */
    size_t len = strlen(host);
    if (len > 255) {
        sec->total_host_rejections++;
        return 0;
    }
    
    for (size_t i = 0; i < len; i++) {
        char c = host[i];
        if (c <= 32 || c >= 127 || c == '/' || c == '\\' || c == '@') {
            sec->total_host_rejections++;
            return 0;
        }
    }
    
    return 1;
}

int she_security_validate_uri(const char *uri) {
    if (!uri || !*uri) return 0;
    
    /* Null byte injection in URL encoding */
    if (strstr(uri, "%00") != NULL || strstr(uri, "%0D") != NULL || strstr(uri, "%0A") != NULL) {
        return 0;
    }
    
    /* Double dot traversal escape attempt */
    if (strstr(uri, "/../") != NULL || strstr(uri, "\\..\\") != NULL ||
        strstr(uri, "/..") != NULL || strstr(uri, "..\\") != NULL) {
        return 0;
    }
    
    return 1;
}

int she_security_check_smuggling(const she_connection_t *con) {
    if (!con) return 0;
    
    const char *te = she_http_request_get_header(&con->request, "Transfer-Encoding");
    const char *cl = she_http_request_get_header(&con->request, "Content-Length");
    
    /* RFC 7230: Request must not contain both TE and CL */
    if (te && cl) {
        return -1; /* Smuggling risk detected */
    }
    
    return 0;
}

int she_security_append_headers(she_security_t *sec, she_connection_t *con) {
    if (!sec || !sec->config.enable_security_headers || !con || !con->response_headers) {
        return 0;
    }
    
    she_buffer_append_string(con->response_headers,
        "X-Content-Type-Options: nosniff\r\n"
        "X-Frame-Options: SAMEORIGIN\r\n"
        "X-XSS-Protection: 1; mode=block\r\n"
        "Referrer-Policy: strict-origin-when-cross-origin\r\n"
        "X-SHE-Engine: Secure HTTP Engine v1.0\r\n"
    );
    
    return 0;
}
