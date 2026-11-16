#ifndef MOD_REPLAY_H
#define MOD_REPLAY_H

#include <stdint.h>
#include <stddef.h>
#include "modules/she_plugin.h"

#define SHE_REPLAY_RING_SIZE 8

typedef struct {
    uint64_t seq;
    char method[12];
    char path[64];
    int status;
    char trace_hash[17];
} she_replay_event_t;

typedef struct {
    char engine_name[48];
    uint64_t events_recorded;
    uint64_t replays_verified;
    uint32_t head;
    she_replay_event_t ring[SHE_REPLAY_RING_SIZE];
} she_replay_state_t;

int she_replay_record_event(const char *method, const char *path, int status, uint64_t *out_seq);
int she_replay_verify_trace(uint64_t seq, char *out_checksum, size_t chk_sz);
she_plugin_t *mod_replay_plugin_init(void);

#endif /* MOD_REPLAY_H */
