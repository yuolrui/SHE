#ifndef MOD_DPDK_H
#define MOD_DPDK_H

#include <stddef.h>
#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_DPDK_MAX_QUEUES 8
#define SHE_DPDK_BURST_SIZE 64
#define SHE_DPDK_RING_SIZE  4096

typedef struct {
    uint32_t buf_len;
    uint32_t data_len;
    uint16_t port_id;
    uint16_t queue_id;
    uint32_t rss_hash;
    uint64_t ol_flags; /* Offload flags (PKT_RX_L4_CKSUM_GOOD, etc.) */
    uint8_t  data[1518];
} she_dpdk_mbuf_t;

typedef struct {
    uint16_t queue_id;
    uint32_t head;
    uint32_t tail;
    uint32_t size;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
} she_dpdk_queue_t;

typedef struct {
    uint16_t port_id;
    char pcie_addr[32];
    char driver_name[32];
    uint32_t link_speed_mbps;
    int link_up;
    uint16_t nb_rx_queues;
    uint16_t nb_tx_queues;
    she_dpdk_queue_t rx_queues[SHE_DPDK_MAX_QUEUES];
    she_dpdk_queue_t tx_queues[SHE_DPDK_MAX_QUEUES];
    uint64_t total_rx_pkts;
    uint64_t total_tx_pkts;
    uint64_t total_rx_bytes;
    uint64_t total_tx_bytes;
    uint64_t total_dropped;
    uint32_t mpps_rate;
} she_dpdk_port_t;

/* Plugin initialization */
she_plugin_t *mod_dpdk_plugin_init(void);

/* DPDK Packet burst processor */
int she_dpdk_process_burst(she_dpdk_port_t *port, uint32_t count, uint32_t pkt_size);

#endif /* MOD_DPDK_H */
