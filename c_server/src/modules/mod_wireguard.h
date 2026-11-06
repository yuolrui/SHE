#ifndef SHE_MOD_WIREGUARD_H
#define SHE_MOD_WIREGUARD_H

#include "modules/she_plugin.h"
#include <stdint.h>
#include <stddef.h>

/**
 * WireGuard / Noise_IKpsk2 Protocol Constants
 */
typedef enum {
    SHE_WG_MSG_INITIATION = 1,
    SHE_WG_MSG_RESPONSE   = 2,
    SHE_WG_MSG_COOKIE     = 3,
    SHE_WG_MSG_DATA       = 4
} she_wg_msg_type_t;

#define SHE_WG_MAX_PEERS 16

typedef struct {
    char name[64];
    char public_key[48];
    char endpoint[64];
    char allowed_ips[64];
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    uint64_t last_handshake_ts;
    uint8_t is_active;
} she_wireguard_peer_t;

typedef struct {
    char interface_name[16];     /* "she-wg0" */
    char interface_ip[32];       /* "10.42.0.1/24" */
    uint16_t listen_port;        /* 51820 */
    char public_key[48];
    uint32_t peer_count;
    uint64_t total_mesh_tx_bytes;
    uint64_t total_mesh_rx_bytes;
    she_wireguard_peer_t peers[SHE_WG_MAX_PEERS];
} she_wireguard_stats_t;

/* Plugin initialization */
she_plugin_t *mod_wireguard_plugin_init(void);

/* WireGuard APIs */
int she_wireguard_add_peer(const char *name, const char *pubkey, const char *endpoint, const char *allowed_ips);
const she_wireguard_stats_t *she_wireguard_get_stats(void);

#endif /* SHE_MOD_WIREGUARD_H */
