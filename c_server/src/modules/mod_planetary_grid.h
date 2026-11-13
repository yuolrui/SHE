#ifndef MOD_PLANETARY_GRID_H
#define MOD_PLANETARY_GRID_H

#include <stddef.h>
#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_GRID_MAX_ZONES 4

typedef struct {
    char zone_id[32];            /* "nordic-hydro-01", "atacama-solar-02", etc. */
    char clean_source[24];       /* "Hydro+Geothermal", "Solar+Storage", "Fusion-Pilot" */
    uint32_t surplus_mw;         /* Available clean surplus MW */
    uint32_t carbon_intensity_g; /* gCO2eq/kWh */
    uint32_t workload_share_pct; /* % of global SHE compute */
    double surface_code_err;     /* Topological QEC logical error rate */
} she_grid_zone_t;

typedef struct {
    char controller_id[32];      /* "SHE-TERAWATT-PLANETARY-GRID" */
    uint32_t zone_count;
    she_grid_zone_t zones[SHE_GRID_MAX_ZONES];
    uint32_t total_clean_surplus_mw;
    uint64_t workloads_migrated;
    uint32_t qec_surface_distance; /* e.g. d=17 */
} she_planetary_grid_t;

/* Plugin initialization */
she_plugin_t *mod_planetary_grid_plugin_init(void);

/* Balance clean-energy compute placement */
int she_grid_balance_compute(uint32_t mw_demand, char *out_zone, size_t zone_buf_sz, uint32_t *out_carbon_g);

#endif /* MOD_PLANETARY_GRID_H */
