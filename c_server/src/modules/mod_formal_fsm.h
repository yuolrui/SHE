#ifndef MOD_FORMAL_FSM_H
#define MOD_FORMAL_FSM_H

#include <stdint.h>
#include "modules/she_plugin.h"
#include "core/she_connection.h"

#define SHE_FORMAL_PROOF_RULES 6

typedef struct {
    char solver_engine[48];
    uint64_t invariants_checked;
    uint64_t violations_detected;
    uint64_t proof_runs;
    char last_proof_digest[65];
    char last_verdict[32];
} she_formal_state_t;

int she_fsm_verify_invariant(const she_connection_t *con);
int she_formal_run_proof_suite(uint32_t *out_checked, uint32_t *out_violations);
she_plugin_t *mod_formal_fsm_plugin_init(void);

#endif /* MOD_FORMAL_FSM_H */
