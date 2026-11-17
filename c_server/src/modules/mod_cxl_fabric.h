#ifndef MOD_CXL_FABRIC_H
#define MOD_CXL_FABRIC_H

#include <stdint.h>
#include <stddef.h>
#include "modules/she_plugin.h"

#define SHE_CXL_MAX_POOLS 4

typedef struct {
    uint32_t pool_id;
    char pool_label[32];
    uint64_t capacity_gb;
    uint64_t allocated_bytes;
    uint32_t access_ns;
    int coherent_cache;
} she_cxl_pool_t;

typedef struct {
    char cxl_spec[32];
    uint64_t total_alloc_ops;
    uint64_t total_alloc_bytes;
    uint64_t last_fabric_addr;
    she_cxl_pool_t pools[SHE_CXL_MAX_POOLS];
} she_cxl_state_t;

int she_cxl_alloc_coherent_page(uint32_t pool_id, uint64_t bytes, uint64_t *out_fabric_addr);
she_plugin_t *mod_cxl_fabric_plugin_init(void);

#endif /* MOD_CXL_FABRIC_H */
