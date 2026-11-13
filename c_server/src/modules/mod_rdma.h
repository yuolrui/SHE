#ifndef MOD_RDMA_H
#define MOD_RDMA_H

#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_RDMA_MAX_QPS 4

typedef struct {
    uint32_t qp_num;
    char remote_rack[32];
    uint64_t remote_rkey;
    uint64_t dma_bytes_sent;
    uint64_t wr_posted;
} she_rdma_qp_t;

typedef struct {
    char hca_device[32];
    char transport[32];
    uint32_t link_gbps;
    uint64_t total_dma_bytes;
    uint64_t total_wr_completed;
    she_rdma_qp_t qps[SHE_RDMA_MAX_QPS];
} she_rdma_state_t;

int she_rdma_post_send_chunkqueue(uint32_t qp_num, uint64_t remote_rkey, uint64_t dma_bytes);
she_plugin_t *mod_rdma_plugin_init(void);

#endif /* MOD_RDMA_H */
