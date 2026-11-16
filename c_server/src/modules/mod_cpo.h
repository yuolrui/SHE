#ifndef MOD_CPO_H
#define MOD_CPO_H

#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_CPO_MAX_LANES 8

typedef struct {
    uint32_t lane_id;
    uint32_t wavelength_nm;
    int tx_power_dbm;
    uint32_t gbps;
    uint64_t frames_switched;
    int locked;
} she_cpo_lane_t;

typedef struct {
    char fabric_name[48];
    uint32_t total_lanes;
    uint64_t aggregate_gbps;
    uint64_t tuning_ops;
    double avg_ber_exponent;
    she_cpo_lane_t lanes[SHE_CPO_MAX_LANES];
} she_cpo_state_t;

int she_cpo_tune_optical_lane(uint32_t lane_id, int target_power_dbm, uint64_t *out_total_gbps);
she_plugin_t *mod_cpo_plugin_init(void);

#endif /* MOD_CPO_H */
