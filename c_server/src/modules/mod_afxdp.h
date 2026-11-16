#ifndef MOD_AFXDP_H
#define MOD_AFXDP_H

#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_AFXDP_MAX_RINGS 4

typedef struct {
    char ifname[24];
    uint32_t queue_id;
    uint32_t umem_frames;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t umem_recycled;
    int zero_copy_active;
} she_afxdp_ring_t;

typedef struct {
    char mode[48];
    uint32_t frame_size;
    uint64_t total_umem_frames;
    uint64_t total_zero_copy_pkts;
    she_afxdp_ring_t rings[SHE_AFXDP_MAX_RINGS];
} she_afxdp_state_t;

int she_afxdp_bind_umem_ring(const char *ifname, uint32_t queue_id, uint64_t *out_umem_frames);
she_plugin_t *mod_afxdp_plugin_init(void);

#endif /* MOD_AFXDP_H */
