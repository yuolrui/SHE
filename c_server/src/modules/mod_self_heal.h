#ifndef MOD_SELF_HEAL_H
#define MOD_SELF_HEAL_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "modules/she_plugin.h"

typedef struct {
    char supervisor_id[32];        /* "SHE-SELF-HEAL-WATCHDOG" */
    uint64_t health_checks_passed;
    uint64_t anomalies_recovered;
    uint64_t fd_leaks_prevented;
    uint64_t watchdog_ticks;
    time_t last_health_check;
    int auto_heal_enabled;
} she_self_heal_state_t;

/* Plugin initialization */
she_plugin_t *mod_self_heal_plugin_init(void);

/* Run watchdog check & optional hot reload */
int she_self_heal_run_check(she_server_t *srv);
int she_self_heal_trigger_reload(she_server_t *srv);

#endif /* MOD_SELF_HEAL_H */
