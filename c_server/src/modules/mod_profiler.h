#ifndef MOD_PROFILER_H
#define MOD_PROFILER_H

#include <stdint.h>
#include <stddef.h>
#include "modules/she_plugin.h"

#define SHE_PROFILER_MAX_STACKS 5

typedef struct {
    char folded_stack[96];
    uint64_t samples;
    double cpu_pct;
} she_profiler_stack_t;

typedef struct {
    char alertmanager_sink[96];
    uint32_t sample_freq_hz;
    uint64_t total_samples;
    uint64_t captures_run;
    uint64_t alerts_dispatched;
    char alert_state[32];
    she_profiler_stack_t stacks[SHE_PROFILER_MAX_STACKS];
} she_profiler_state_t;

int she_profiler_capture_flamegraph(uint32_t duration_ms, uint64_t *out_samples);
int she_profiler_evaluate_alerts(const she_server_t *srv, char *out_status, size_t status_sz);
she_plugin_t *mod_profiler_plugin_init(void);

#endif /* MOD_PROFILER_H */
