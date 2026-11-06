#ifndef SHE_MOD_ACME_H
#define SHE_MOD_ACME_H

#include "modules/she_plugin.h"
#include <stdint.h>
#include <stddef.h>

/**
 * RFC 8555 ACME (Automated Certificate Management Environment)
 */
typedef enum {
    SHE_ACME_STATE_INITIAL = 0,
    SHE_ACME_STATE_CHALLENGE_READY = 1,
    SHE_ACME_STATE_VALID = 2,
    SHE_ACME_STATE_RENEWAL_DUE = 3,
    SHE_ACME_STATE_REVOKED = 4
} she_acme_state_t;

#define SHE_ACME_MAX_CHALLENGES 16

typedef struct {
    char token[128];
    char key_authorization[256];
    char domain[128];
    uint64_t created_at;
} she_acme_challenge_t;

typedef struct {
    char primary_domain[128];
    char san_domains[256];
    char acme_directory[128];
    she_acme_state_t state;
    uint32_t cert_days_remaining;
    uint32_t auto_renew_threshold_days;
    uint64_t last_renewal_timestamp;
    she_acme_challenge_t challenges[SHE_ACME_MAX_CHALLENGES];
    size_t challenge_count;
} she_acme_cert_info_t;

/* Plugin initialization */
she_plugin_t *mod_acme_plugin_init(void);

/* ACME API */
int she_acme_set_http01_challenge(const char *token, const char *key_auth, const char *domain);
const char *she_acme_get_http01_challenge(const char *token);
const she_acme_cert_info_t *she_acme_get_cert_info(void);

#endif /* SHE_MOD_ACME_H */
