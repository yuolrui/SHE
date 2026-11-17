#ifndef MOD_MPC_TELEMETRY_H
#define MOD_MPC_TELEMETRY_H

#include <stdint.h>
#include <stddef.h>
#include "modules/she_plugin.h"

typedef struct {
    char protocol_name[48];
    uint32_t party_quorum;
    uint64_t rounds_aggregated;
    uint64_t cumulative_blinded_sum;
    char last_cohort[48];
    char last_transcript_hex[65];
} she_mpc_state_t;

int she_mpc_aggregate_telemetry(const char *cohort, uint64_t tenant_metric, char *out_share_digest, size_t d_sz, uint64_t *out_cohort_sum);
she_plugin_t *mod_mpc_telemetry_plugin_init(void);

#endif /* MOD_MPC_TELEMETRY_H */
