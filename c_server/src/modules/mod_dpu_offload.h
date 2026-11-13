#ifndef MOD_DPU_OFFLOAD_H
#define MOD_DPU_OFFLOAD_H

#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_DPU_MAX_FLOWS 4

typedef struct {
    uint32_t nic_flow_id;
    char policy_name[48];
    uint64_t packets_offloaded;
    int hardware_active;
} she_dpu_flow_t;

typedef struct {
    char dpu_device[48];
    uint32_t arm_cores;
    uint32_t waf_regex_rules;
    uint64_t total_flows_offloaded;
    uint64_t total_packets_fastpathed;
    she_dpu_flow_t active_flows[SHE_DPU_MAX_FLOWS];
} she_dpu_state_t;

int she_dpu_offload_tls_flow(uint32_t nic_flow_id, const char *policy_name, uint64_t *out_offloaded_flows);
she_plugin_t *mod_dpu_offload_plugin_init(void);

#endif /* MOD_DPU_OFFLOAD_H */
