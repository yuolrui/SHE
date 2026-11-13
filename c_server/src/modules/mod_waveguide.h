#ifndef MOD_WAVEGUIDE_H
#define MOD_WAVEGUIDE_H

#include <stddef.h>
#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_WAVEGUIDE_MAX_MODES 6

typedef struct {
    uint32_t mode_id;
    char waveguide_name[32];     /* "wg-spatial-te0", "wg-spatial-tm1", etc. */
    float wavelength_nm;         /* e.g. 1550.12 nm */
    float insertion_loss_db;     /* e.g. 0.42 dB */
    uint32_t switch_latency_ps;  /* picoseconds, zero E/O conversion */
    uint64_t packets_switched;
    uint64_t bytes_routed;
    int is_resonant;
} she_waveguide_mode_t;

typedef struct {
    char fabric_name[32];        /* "SHE-HOLONOMIC-SILICON-PHOTONIC" */
    uint32_t mode_count;
    she_waveguide_mode_t modes[SHE_WAVEGUIDE_MAX_MODES];
    uint64_t total_photonic_packets;
    uint32_t avg_switching_ps;
    float eo_conversions_bypassed_pct;
} she_waveguide_engine_t;

/* Plugin initialization */
she_plugin_t *mod_waveguide_plugin_init(void);

/* Route a packet through a spatial silicon photonic waveguide mode */
int she_waveguide_route_photon_packet(uint32_t spatial_mode, uint64_t payload_bytes, uint32_t *out_latency_ps);

#endif /* MOD_WAVEGUIDE_H */
