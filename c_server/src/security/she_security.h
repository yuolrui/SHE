#ifndef SHE_SECURITY_H
#define SHE_SECURITY_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* Forward declarations */
typedef struct she_server she_server_t;
typedef struct she_connection she_connection_t;

#define SHE_RATE_LIMIT_BUCKET_SIZE 256
#define SHE_MAX_SECURITY_HEADERS   8

typedef struct she_rate_entry {
    char ip[48];
    double tokens;
    time_t last_update;
    uint32_t violations;
    struct she_rate_entry *next;
} she_rate_entry_t;

typedef struct she_security_config {
    int enable_rate_limit;         /* 1 = enable, 0 = disable */
    double rate_limit_rps;         /* Allowed requests per second (e.g., 50.0) */
    double rate_limit_burst;       /* Max burst tokens (e.g., 30.0) */
    
    int enable_security_headers;   /* Inject X-Frame-Options, X-Content-Type-Options, etc. */
    int strict_host_check;         /* Validate Host header syntax and allowed domain */
    char allowed_host_suffix[128]; /* Optional allowed domain suffix (e.g. "local" or empty for all) */
    
    int block_smuggling;           /* Check for conflicting Content-Length / Transfer-Encoding */
} she_security_config_t;

typedef struct she_security {
    she_security_config_t config;
    she_rate_entry_t *rate_buckets[SHE_RATE_LIMIT_BUCKET_SIZE];
    uint64_t total_rate_blocks;
    uint64_t total_host_rejections;
    uint64_t total_traversal_blocks;
} she_security_t;

/* Lifecycle */
she_security_t *she_security_init(const she_security_config_t *cfg);
void she_security_free(she_security_t *sec);

/* Security checks */
int she_security_check_rate_limit(she_security_t *sec, const char *client_ip);
int she_security_validate_host(she_security_t *sec, const char *host);
int she_security_validate_uri(const char *uri);
int she_security_check_smuggling(const she_connection_t *con);

/* Security header injection */
int she_security_append_headers(she_security_t *sec, she_connection_t *con);

#endif /* SHE_SECURITY_H */
