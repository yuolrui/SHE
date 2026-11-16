#ifndef MOD_NEURO_EDGE_H
#define MOD_NEURO_EDGE_H

#include <stdint.h>
#include <stddef.h>
#include "modules/she_plugin.h"

#define SHE_NEURO_MAX_SYNAPSES 4

typedef struct {
    uint32_t flow_id;
    uint32_t isi_us;
    uint32_t membrane_mv;
    char verdict[24];
} she_neuro_neuron_t;

typedef struct {
    char model_name[48];
    uint32_t fire_threshold_mv;
    uint64_t spikes_evaluated;
    uint64_t anomalies_fired;
    she_neuro_neuron_t neurons[SHE_NEURO_MAX_SYNAPSES];
} she_neuro_state_t;

int she_neuro_classify_spike(uint32_t flow_id, uint32_t isi_us, uint32_t *out_membrane_mv, char *out_verdict, size_t v_sz);
she_plugin_t *mod_neuro_edge_plugin_init(void);

#endif /* MOD_NEURO_EDGE_H */
