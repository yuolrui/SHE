#ifndef MOD_KEYVAULT_H
#define MOD_KEYVAULT_H

#include <stdint.h>
#include <stddef.h>
#include "modules/she_plugin.h"

typedef struct {
    char vault_name[48];
    char sealing_cipher[32];
    uint64_t sealed_keys_total;
    uint64_t rotation_epoch;
    char last_tenant[48];
    char last_sealed_key_id[33];
} she_keyvault_state_t;

int she_keyvault_seal_session_key(const char *tenant, uint32_t ttl_sec, char *out_key_id, size_t id_sz, uint64_t *out_epoch);
she_plugin_t *mod_keyvault_plugin_init(void);

#endif /* MOD_KEYVAULT_H */
