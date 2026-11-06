#ifndef SHE_MOD_EBPF_H
#define SHE_MOD_EBPF_H

#include "modules/she_plugin.h"
#include <stdint.h>
#include <stddef.h>

/**
 * eBPF / XDP (eXpress Data Path) Constants & Actions
 */
typedef enum {
    SHE_XDP_PASS = 0,
    SHE_XDP_DROP = 1,
    SHE_XDP_TX = 2,
    SHE_XDP_REDIRECT = 3
} she_xdp_action_t;

typedef enum {
    SHE_BPF_MAP_TYPE_HASH = 1,
    SHE_BPF_MAP_TYPE_ARRAY = 2,
    SHE_BPF_MAP_TYPE_LRU_HASH = 9
} she_bpf_map_type_t;

#define SHE_EBPF_MAX_BLACKLIST 128

typedef struct {
    char ip[48];
    uint64_t drop_count;
    uint64_t added_at;
} she_ebpf_blacklist_entry_t;

typedef struct {
    uint64_t total_packets_processed;
    uint64_t total_packets_dropped;
    uint64_t syn_flood_mitigated;
    uint32_t active_blacklist_count;
    const char *xdp_mode;        /* "native" | "generic" | "offload" */
    const char *prog_type;       /* "BPF_PROG_TYPE_XDP" */
    uint32_t bpf_bytecode_instructions;
} she_ebpf_stats_t;

/* Plugin initialization */
she_plugin_t *mod_ebpf_plugin_init(void);

/* eBPF / XDP Kernel Filter APIs */
int she_ebpf_inspect_packet(const char *src_ip, uint16_t src_port, int is_syn);
int she_ebpf_add_blacklist_ip(const char *ip);
int she_ebpf_remove_blacklist_ip(const char *ip);
int she_ebpf_is_blacklisted(const char *ip);
const she_ebpf_stats_t *she_ebpf_get_stats(void);

#endif /* SHE_MOD_EBPF_H */
