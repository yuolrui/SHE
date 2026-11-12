#ifndef MOD_HW_MESH_H
#define MOD_HW_MESH_H

#include <stddef.h>
#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_MESH_MAX_ROUTES 8
#define SHE_MESH_MAX_PEERS  4

typedef struct {
    char peer_name[32];
    char peer_ip[32];
    uint32_t peer_as;
    int is_established;
    uint32_t keepalive_sec;
    uint64_t routes_received;
} she_bgp_peer_t;

typedef struct {
    char prefix[32];
    char next_hop[32];
    uint32_t as_path[4];
    uint32_t as_path_len;
    uint32_t local_pref;
    uint32_t med;
    int is_active;
} she_anycast_route_t;

typedef struct {
    /* kTLS Hardware Crypto */
    uint64_t ktls_active_sessions;
    uint64_t ktls_offloaded_bytes;
    uint32_t ktls_crypto_driver_id;
    char ktls_driver_name[32];
    char supported_ciphers[64];
    
    /* BGP EVPN Anycast */
    uint32_t local_as;
    char router_id[32];
    uint32_t peer_count;
    she_bgp_peer_t peers[SHE_MESH_MAX_PEERS];
    uint32_t route_count;
    she_anycast_route_t routes[SHE_MESH_MAX_ROUTES];
    uint32_t convergence_latency_ms;
} she_hw_mesh_state_t;

/* Plugin initialization */
she_plugin_t *mod_hw_mesh_plugin_init(void);

/* Mesh routing manipulation */
int she_hw_mesh_announce_route(const char *prefix, uint32_t next_as);

#endif /* MOD_HW_MESH_H */
