#ifndef MOD_TC_FLOWER_H
#define MOD_TC_FLOWER_H

#include <stddef.h>
#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_TC_MAX_RULES 16

typedef enum {
    SHE_TC_ACT_OK = 0,
    SHE_TC_ACT_SHOT = 1,
    SHE_TC_ACT_PIPE = 2
} she_tc_action_t;

typedef struct {
    uint32_t rule_id;
    uint32_t priority;
    char protocol[8];       /* "tcp", "udp", "any" */
    char src_cidr[32];      /* e.g. "0.0.0.0/0" */
    uint16_t dst_port;      /* 8899, 443, etc. */
    uint32_t rate_limit_mbps;
    uint32_t burst_kb;
    she_tc_action_t action;
    uint64_t matched_packets;
    uint64_t dropped_packets;
    char class_id[16];      /* e.g. "1:10" */
} she_tc_flower_rule_t;

typedef struct {
    char ifname[32];
    char qdisc[32];
    uint32_t rule_count;
    she_tc_flower_rule_t rules[SHE_TC_MAX_RULES];
    uint64_t total_policed_pkts;
    uint64_t total_microburst_drops;
    uint32_t sub_ns_eval_latency_ns;
} she_tc_flower_engine_t;

/* Plugin initialization */
she_plugin_t *mod_tc_flower_plugin_init(void);

/* Rule manipulation */
int she_tc_flower_add_rule(const char *proto, uint16_t dport, uint32_t rate_mbps, uint32_t burst_kb, she_tc_action_t act);

#endif /* MOD_TC_FLOWER_H */
