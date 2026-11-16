#ifndef MOD_FEDERATION_H
#define MOD_FEDERATION_H

#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_FED_MAX_CLUSTERS 4

typedef struct {
    char cluster_id[32];
    char region[24];
    char status[16];
    uint32_t sla_ms;
    uint64_t policy_epoch;
    char policy_digest[65];
} she_fed_cluster_t;

typedef struct {
    char mesh_id[48];
    uint64_t global_epoch;
    uint64_t failovers_executed;
    she_fed_cluster_t clusters[SHE_FED_MAX_CLUSTERS];
} she_federation_state_t;

int she_federation_sync_policy(const char *cluster_id, const char *policy_digest, uint64_t *out_epoch);
she_plugin_t *mod_federation_plugin_init(void);

#endif /* MOD_FEDERATION_H */
