#ifndef MOD_TERABIT_FABRIC_H
#define MOD_TERABIT_FABRIC_H

#include <stddef.h>
#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_FABRIC_MAX_CHANNELS 4

typedef struct {
    char channel_id[16];     /* "lambda-1", "lambda-2", ... */
    float frequency_thz;     /* e.g. 193.10 THz */
    float wavelength_nm;     /* e.g. 1552.52 nm */
    uint32_t speed_gbps;     /* 400 Gbps */
    float tx_power_dbm;
    float rx_power_dbm;
    float osnr_db;           /* Optical Signal-to-Noise Ratio */
    int is_active;
    int is_protection_standby;
    uint64_t transferred_bytes;
} she_optical_channel_t;

typedef struct {
    char fabric_id[32];      /* "SHE-TERABIT-FABRIC-01" */
    uint32_t total_capacity_gbps; /* 1600 Gbps (1.6 Tbps) */
    uint32_t channel_count;
    she_optical_channel_t channels[SHE_FABRIC_MAX_CHANNELS];
    uint32_t protection_switch_time_ms; /* e.g. 8 ms */
    uint64_t total_protection_switches;
    uint64_t total_fabric_bytes;
} she_terabit_fabric_t;

/* Plugin initialization */
she_plugin_t *mod_terabit_fabric_plugin_init(void);

/* Optical channel protection switchover */
int she_fabric_trigger_protection_switch(const char *channel_id, int activate);

#endif /* MOD_TERABIT_FABRIC_H */
