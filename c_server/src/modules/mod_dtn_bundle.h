#ifndef MOD_DTN_BUNDLE_H
#define MOD_DTN_BUNDLE_H

#include <stddef.h>
#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_DTN_MAX_PEERS 4

typedef struct {
    char eid[48];                 /* e.g. "dtn://mars-relay-01/core" */
    char region[32];              /* "Earth-LEO", "Lunar-Gateway", "Mars-Orbit", "Sun-Earth-L2" */
    uint32_t one_way_light_sec;   /* Light-speed propagation delay */
    uint32_t contact_window_pct;  /* 0 - 100 */
    uint32_t queued_bundles;
    uint64_t delivered_bundles;
    int custody_active;
} she_dtn_peer_t;

typedef struct {
    char local_eid[48];           /* "dtn://she-earth-hub-01/gateway" */
    uint32_t protocol_version;    /* BPv7 (RFC 9171) / RFC 5050 compat */
    uint32_t peer_count;
    she_dtn_peer_t peers[SHE_DTN_MAX_PEERS];
    uint64_t total_bundles_forwarded;
    uint64_t custody_store_bytes;
} she_dtn_engine_t;

/* Plugin initialization */
she_plugin_t *mod_dtn_bundle_plugin_init(void);

/* Enqueue and dispatch a DTN Bundle with custody transfer */
int she_dtn_dispatch_bundle(const char *dest_eid, uint32_t payload_bytes, char *out_bundle_id, size_t id_buf_sz);

#endif /* MOD_DTN_BUNDLE_H */
