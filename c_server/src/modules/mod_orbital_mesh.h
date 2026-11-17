#ifndef MOD_ORBITAL_MESH_H
#define MOD_ORBITAL_MESH_H

#include <stdint.h>
#include <stddef.h>
#include "modules/she_plugin.h"

#define SHE_ORBITAL_MAX_LINKS 4

typedef struct {
    char sat_id[32];
    char orbit_shell[24];
    uint32_t doppler_khz;
    uint32_t snr_db;
    uint32_t link_gbps;
    char wave_lock[24];
} she_orbital_link_t;

typedef struct {
    char constellation_id[48];
    uint64_t frames_relayed;
    uint64_t doppler_resyncs;
    she_orbital_link_t links[SHE_ORBITAL_MAX_LINKS];
} she_orbital_state_t;

int she_orbital_route_laser_link(const char *sat_id, uint32_t doppler_khz, uint32_t *out_snr_db, char *out_wave_lock, size_t w_sz);
she_plugin_t *mod_orbital_mesh_plugin_init(void);

#endif /* MOD_ORBITAL_MESH_H */
