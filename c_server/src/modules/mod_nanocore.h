#ifndef MOD_NANOCORE_H
#define MOD_NANOCORE_H

#include <stddef.h>
#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_NANOCORE_MAX_POOLS 6

typedef struct {
    uint32_t slab_size;          /* 64, 128, 256, 512, 1024, 4096 */
    uint32_t total_slabs;
    uint32_t free_slabs;
    uint64_t total_allocations;
    uint64_t total_deallocations;
    uint32_t avg_alloc_latency_ns;
    uint32_t cacheline_hit_ratio_pct;
} she_nanocore_slab_pool_t;

typedef struct {
    char allocator_name[32];     /* "SHE-NANOCORE-LOCKFREE-V1" */
    uint32_t pool_count;
    she_nanocore_slab_pool_t pools[SHE_NANOCORE_MAX_POOLS];
    uint64_t cumulative_allocs;
    uint64_t cumulative_bytes_served;
    uint32_t global_avg_latency_ns;
    float memory_efficiency_pct;
} she_nanocore_state_t;

/* Plugin initialization */
she_plugin_t *mod_nanocore_plugin_init(void);

/* Benchmarking lock-free slab allocations */
int she_nanocore_run_bench(uint32_t iterations, uint64_t *out_duration_ns, uint32_t *out_ns_per_op);

#endif /* MOD_NANOCORE_H */
