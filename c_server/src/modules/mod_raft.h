#ifndef SHE_MOD_RAFT_H
#define SHE_MOD_RAFT_H

#include "modules/she_plugin.h"
#include <stdint.h>
#include <stddef.h>

#define SHE_RAFT_MAX_PEERS 8
#define SHE_RAFT_MAX_LOG_ENTRIES 32

typedef enum {
    SHE_RAFT_FOLLOWER  = 0,
    SHE_RAFT_CANDIDATE = 1,
    SHE_RAFT_LEADER    = 2
} she_raft_role_t;

typedef struct {
    char node_id[64];
    char endpoint[64];
    uint64_t next_index;
    uint64_t match_index;
    uint8_t is_online;
} she_raft_peer_t;

typedef struct {
    uint64_t term;
    uint64_t index;
    char key[64];
    char val[128];
} she_raft_log_entry_t;

typedef struct {
    she_raft_role_t role;
    uint64_t current_term;
    char node_id[64];
    char leader_id[64];
    uint64_t commit_index;
    uint64_t last_applied;
    uint32_t peer_count;
    she_raft_peer_t peers[SHE_RAFT_MAX_PEERS];
    uint32_t log_count;
    she_raft_log_entry_t logs[SHE_RAFT_MAX_LOG_ENTRIES];
    uint64_t total_replicated_commands;
} she_raft_stats_t;

/* Plugin initialization */
she_plugin_t *mod_raft_plugin_init(void);

/* Raft consensus APIs */
int she_raft_propose_entry(const char *key, const char *val, uint64_t *out_index);
const she_raft_stats_t *she_raft_get_stats(void);

#endif /* SHE_MOD_RAFT_H */
