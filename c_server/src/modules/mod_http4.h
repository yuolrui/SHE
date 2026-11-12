#ifndef MOD_HTTP4_H
#define MOD_HTTP4_H

#include <stddef.h>
#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_HTTP4_MAX_CARRIERS 4

typedef struct {
    char name[32];
    char medium[32];         /* "Terrestrial Fiber", "5G mmWave", "LEO Satellite", "WiFi 7 MLO" */
    uint32_t capacity_mbps;
    float rtt_ms;
    float jitter_ms;
    int link_up;
    uint64_t bytes_sent;
    uint64_t packets_sent;
    uint32_t traffic_weight_pct;
} she_http4_carrier_t;

typedef struct {
    char protocol_version[16];   /* "HTTP/4-draft02" */
    uint32_t carrier_count;
    she_http4_carrier_t carriers[SHE_HTTP4_MAX_CARRIERS];
    uint32_t aggregate_capacity_mbps;
    uint64_t total_multipath_bytes;
    uint64_t total_reordered_frames;
    uint32_t vhe_compression_ratio_pct; /* Vectorized Header Encoding savings */
} she_http4_engine_t;

/* Plugin initialization */
she_plugin_t *mod_http4_plugin_init(void);

/* Multi-carrier transmission */
int she_http4_dispatch_payload(uint64_t payload_bytes, uint64_t carrier_shares[SHE_HTTP4_MAX_CARRIERS]);

#endif /* MOD_HTTP4_H */
