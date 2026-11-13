#ifndef MOD_QKD_H
#define MOD_QKD_H

#include <stddef.h>
#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_QKD_MAX_CHANNELS 4
#define SHE_QKD_KEY_POOL_SIZE 65536 /* 64 KB quantum entropy key pool */

typedef enum {
    SHE_QKD_PROTOCOL_BB84 = 0,    /* Single-photon polarization BB84 */
    SHE_QKD_PROTOCOL_E91          /* Entangled photon pair E91 */
} she_qkd_protocol_t;

typedef struct {
    char link_name[32];           /* e.g. "alice-bob-dark-fiber-01" */
    char remote_station[32];      /* e.g. "Datacenter-Bob-East" */
    she_qkd_protocol_t protocol;
    float fiber_distance_km;
    float qber_pct;               /* Quantum Bit Error Rate (QBER < 11% is secure) */
    float qber_threshold_pct;     /* Max allowed QBER threshold before abort (11.0%) */
    uint32_t key_rate_kbps;       /* Secret key generation rate (e.g. 2560 kbps) */
    uint32_t dark_count_rate_hz;  /* SPAD detector dark counts (e.g. 25 Hz) */
    uint64_t total_keys_generated_bits;
    int is_quantum_secure;        /* 1 = Secure, 0 = Eavesdropping detected */
} she_qkd_channel_t;

typedef struct {
    char station_id[32];          /* "SHE-QKD-NODE-ALICE" */
    uint32_t channel_count;
    she_qkd_channel_t channels[SHE_QKD_MAX_CHANNELS];
    uint32_t key_pool_available_bytes;
    uint32_t key_pool_capacity_bytes;
    uint64_t total_consumed_keys_bits;
    int continuous_rekey_active;
} she_qkd_engine_t;

/* Plugin initialization */
she_plugin_t *mod_qkd_plugin_init(void);

/* Consume quantum key material from entropy pool */
int she_qkd_consume_key(size_t key_bytes, uint8_t *out_key);

#endif /* MOD_QKD_H */
