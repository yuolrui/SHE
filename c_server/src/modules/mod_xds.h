#ifndef MOD_XDS_H
#define MOD_XDS_H

#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_XDS_MAX_REGIONS 4

typedef struct {
    char region_id[32];
    char endpoint[64];
    uint32_t primary_weight_pct;
    uint32_t canary_weight_pct;
    uint32_t rtt_ms;
    int circuit_breaker_tripped;
    uint64_t routed_requests;
} she_xds_cluster_t;

typedef struct {
    char control_plane_uri[96];
    uint64_t snapshot_version;
    uint64_t sync_count;
    char nonce[32];
    uint32_t region_count;
    she_xds_cluster_t regions[SHE_XDS_MAX_REGIONS];
} she_xds_state_t;

int she_cloud_mesh_sync_xds_snapshot(const char *control_plane_uri, uint64_t *out_version);
int she_xds_set_canary_weight(const char *region_id, uint32_t canary_pct);
she_plugin_t *mod_xds_plugin_init(void);

#endif /* MOD_XDS_H */
