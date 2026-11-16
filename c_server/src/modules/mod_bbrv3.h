#ifndef MOD_BBRV3_H
#define MOD_BBRV3_H

#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_BBRV3_MAX_FLOWS 4

typedef struct {
    uint32_t flow_id;
    char phase[16];
    uint32_t min_rtt_us;
    uint32_t btlbw_mbps;
    uint64_t pacing_rate_bps;
    double pacing_gain;
    double loss_rate_pct;
} she_bbrv3_flow_t;

typedef struct {
    char algorithm[48];
    uint64_t total_pacing_updates;
    uint64_t microbursts_smoothed;
    uint32_t active_flows;
    she_bbrv3_flow_t flows[SHE_BBRV3_MAX_FLOWS];
} she_bbrv3_state_t;

int she_bbrv3_pace_flow(uint32_t flow_id, uint32_t rtt_us, uint32_t bw_mbps, uint64_t *out_pacing_bps);
she_plugin_t *mod_bbrv3_plugin_init(void);

#endif /* MOD_BBRV3_H */
