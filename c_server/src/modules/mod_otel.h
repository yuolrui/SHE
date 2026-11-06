#ifndef SHE_MOD_OTEL_H
#define SHE_MOD_OTEL_H

#include "modules/she_plugin.h"
#include <stdint.h>
#include <stddef.h>

#define SHE_OTEL_MAX_RECENT_SPANS 32

typedef struct {
    char trace_id[36];
    char span_id[20];
    char name[64];
    uint64_t start_time_us;
    uint64_t duration_us;
    uint16_t status_code;
} she_otel_span_t;

typedef struct {
    uint64_t total_spans_recorded;
    uint64_t total_traces_created;
    uint32_t sampling_rate_pct;
    char otlp_endpoint[128];
    uint32_t recent_span_count;
    she_otel_span_t recent_spans[SHE_OTEL_MAX_RECENT_SPANS];
} she_otel_stats_t;

/* Plugin initialization */
she_plugin_t *mod_otel_plugin_init(void);

/* OpenTelemetry APIs */
int she_otel_generate_traceparent(char *out_buf, size_t out_max, char *out_trace_id, char *out_span_id);
int she_otel_record_span(const char *name, uint64_t duration_us, uint16_t status_code);
const she_otel_stats_t *she_otel_get_stats(void);

#endif /* SHE_MOD_OTEL_H */
