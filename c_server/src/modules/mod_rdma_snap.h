#ifndef MOD_RDMA_SNAP_H
#define MOD_RDMA_SNAP_H

#include <stdint.h>
#include <stddef.h>
#include "modules/she_plugin.h"

#define SHE_RDMA_SNAP_MAX_SLOTS 4

typedef struct {
    char snap_id[48];
    char region[32];
    uint64_t state_seq;
    char merkle_root[65];
} she_rdma_snap_entry_t;

typedef struct {
    char transport[48];
    uint64_t snapshots_committed;
    uint64_t bytes_replicated;
    she_rdma_snap_entry_t entries[SHE_RDMA_SNAP_MAX_SLOTS];
} she_rdma_snap_state_t;

int she_rdma_snap_commit(const char *region, uint64_t state_seq, char *out_snap_id, size_t id_sz, char *out_merkle, size_t m_sz);
she_plugin_t *mod_rdma_snap_plugin_init(void);

#endif /* MOD_RDMA_SNAP_H */
