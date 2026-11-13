#ifndef MOD_SANDBOX_H
#define MOD_SANDBOX_H

#include <stddef.h>
#include <stdint.h>
#include "modules/she_plugin.h"

typedef struct {
    char profile_name[32];       /* "SHE-SECCOMP-LANDLOCK-STRICT" */
    int seccomp_status_proc;     /* Read from /proc/self/status */
    int no_new_privs_proc;       /* Read from /proc/self/status */
    int landlock_abi_version;    /* Detected kernel ABI or configured */
    uint32_t allowed_syscalls;   /* Strict allowlist vector size */
    uint32_t blocked_syscalls;   /* Blocked dangerous syscalls count */
    uint64_t audit_runs;
    int formal_tests_verified;
} she_sandbox_state_t;

/* Plugin initialization */
she_plugin_t *mod_sandbox_plugin_init(void);

/* Audit syscall capability & sandbox readiness */
int she_sandbox_audit_profile(she_sandbox_state_t *out_state);

#endif /* MOD_SANDBOX_H */
