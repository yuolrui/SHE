#ifndef MOD_ENCLAVE_H
#define MOD_ENCLAVE_H

#include <stdint.h>
#include <stddef.h>
#include "modules/she_plugin.h"

typedef struct {
    char tee_platform[32];
    uint32_t tcb_version;
    int debug_disabled;
    uint64_t quotes_verified;
    uint64_t quotes_rejected;
    char last_measurement_hex[65];
    char last_nonce[48];
} she_enclave_state_t;

int she_enclave_verify_quote(const char *platform, const char *report_data, char *out_measurement, size_t meas_sz);
she_plugin_t *mod_enclave_plugin_init(void);

#endif /* MOD_ENCLAVE_H */
