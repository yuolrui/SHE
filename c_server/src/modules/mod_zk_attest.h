#ifndef MOD_ZK_ATTEST_H
#define MOD_ZK_ATTEST_H

#include <stdint.h>
#include <stddef.h>
#include "modules/she_plugin.h"

typedef struct {
    char circuit_id[48];
    char curve[32];
    uint64_t proofs_generated;
    uint64_t proofs_verified;
    uint32_t avg_verify_us;
    char last_claim[64];
    char last_commitment_hex[65];
} she_zk_state_t;

int she_zk_generate_compliance_proof(const char *tenant_claim, char *out_commitment, size_t commit_sz, uint32_t *out_verify_us);
she_plugin_t *mod_zk_attest_plugin_init(void);

#endif /* MOD_ZK_ATTEST_H */
