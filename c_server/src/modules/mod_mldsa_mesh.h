#ifndef MOD_MLDSA_MESH_H
#define MOD_MLDSA_MESH_H

#include <stdint.h>
#include <stddef.h>
#include "modules/she_plugin.h"

typedef struct {
    char suite_name[48];
    char pqc_standard[32];
    uint64_t envelopes_verified;
    uint64_t envelopes_rejected;
    char last_signer[48];
    char last_signature_hex[65];
} she_mldsa_state_t;

int she_pqc_mldsa_verify_envelope(const char *signer_id, const char *payload, char *out_sig_hex, size_t sig_sz);
she_plugin_t *mod_mldsa_mesh_plugin_init(void);

#endif /* MOD_MLDSA_MESH_H */
