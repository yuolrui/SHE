#ifndef MOD_NEURAL_ROUTING_H
#define MOD_NEURAL_ROUTING_H

#include <stddef.h>
#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_NEURAL_MAX_NODES 6

typedef struct {
    char node_id[16];           /* "edge-us-east", "edge-eu-west", etc. */
    uint32_t current_load_pct;  /* 0 - 100 */
    uint32_t predicted_load_500ms;
    uint32_t congestion_risk_pct;
    char primary_path[32];
    char deflection_path[32];
    uint32_t packets_deflected;
    uint32_t latency_saved_ms;
} she_neural_node_state_t;

typedef struct {
    char model_name[32];        /* "SHE-CONGESTION-NET-INT8" */
    uint32_t prediction_horizon_ms; /* 500 ms */
    float inference_accuracy_pct;   /* 98.7% */
    uint64_t total_inferences;
    uint32_t active_deflections;
    she_neural_node_state_t nodes[SHE_NEURAL_MAX_NODES];
} she_neural_routing_engine_t;

/* Plugin initialization */
she_plugin_t *mod_neural_routing_plugin_init(void);

/* Neural inference prediction for a target destination AS/node */
int she_neural_predict_congestion(uint32_t dest_as, uint32_t current_load, uint32_t *out_risk, char *out_route, size_t route_buf_sz);

#endif /* MOD_NEURAL_ROUTING_H */
