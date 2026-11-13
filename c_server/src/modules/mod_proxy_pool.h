#ifndef MOD_PROXY_POOL_H
#define MOD_PROXY_POOL_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "modules/she_plugin.h"

#define SHE_PROXY_POOL_MAX_SLOTS 8

typedef struct {
    int slot_id;
    char backend_addr[64];       /* e.g. "127.0.0.1:3000" */
    int in_use;
    int ebpf_sockmap_bound;      /* Kernel eBPF SOCKMAP zero-copy splice */
    uint64_t requests_reused;
    uint64_t bytes_spliced;
    time_t last_used;
} she_proxy_pool_slot_t;

typedef struct {
    char pool_name[32];          /* "SHE-EBPF-SOCKMAP-POOL-V1" */
    uint32_t slot_count;
    she_proxy_pool_slot_t slots[SHE_PROXY_POOL_MAX_SLOTS];
    uint64_t total_acquires;
    uint64_t pool_hits;
    uint64_t pool_misses;
} she_proxy_pool_t;

/* Plugin initialization */
she_plugin_t *mod_proxy_pool_plugin_init(void);

/* Persistent upstream connection pool API */
int she_proxy_pool_acquire_upstream(const char *backend_addr, int *out_slot_id);
int she_proxy_pool_release_upstream(int slot_id);

#endif /* MOD_PROXY_POOL_H */
