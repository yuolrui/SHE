#ifndef SHE_MOD_AI_GATEWAY_H
#define SHE_MOD_AI_GATEWAY_H

#include "modules/she_plugin.h"
#include <stdint.h>
#include <stddef.h>

#define SHE_AI_MAX_TENANTS 16

typedef enum {
    SHE_AI_TIER_COMMUNITY  = 0,
    SHE_AI_TIER_PRO        = 1,
    SHE_AI_TIER_ENTERPRISE = 2
} she_ai_tier_t;

typedef struct {
    char tenant_id[64];
    char api_key[64];
    she_ai_tier_t tier;
    uint64_t token_quota_hourly;
    uint64_t tokens_used_current_hour;
    uint64_t total_requests;
    uint8_t is_active;
} she_ai_tenant_t;

typedef struct {
    uint64_t total_prompts_routed;
    uint64_t total_tokens_streamed;
    uint64_t total_prompt_tokens;
    uint64_t total_completion_tokens;
    uint64_t sse_chunks_delivered;
    uint64_t semantic_cache_hits;
    uint32_t active_tenants_count;
    double avg_first_token_latency_ms;
} she_ai_gateway_stats_t;

/* Plugin initialization */
she_plugin_t *mod_ai_gateway_plugin_init(void);

/* AI Gateway APIs */
int she_ai_authenticate_tenant(const char *api_key, she_ai_tenant_t **out_tenant);
int she_ai_record_tokens(const char *tenant_id, uint32_t prompt_tokens, uint32_t completion_tokens);
const she_ai_gateway_stats_t *she_ai_get_stats(void);

#endif /* SHE_MOD_AI_GATEWAY_H */
