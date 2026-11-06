#ifndef SHE_MOD_QUIC_H
#define SHE_MOD_QUIC_H

#include "modules/she_plugin.h"
#include <stdint.h>
#include <stddef.h>

/**
 * RFC 9000 QUIC Transport Protocol & RFC 9114 HTTP/3 Constants
 */
#define SHE_QUIC_VERSION_1 0x00000001
#define SHE_QUIC_VERSION_DRAFT29 0xff00001d

/* QUIC Long Header Packet Types */
typedef enum {
    SHE_QUIC_PKT_INITIAL   = 0x00,
    SHE_QUIC_PKT_0RTT      = 0x01,
    SHE_QUIC_PKT_HANDSHAKE = 0x02,
    SHE_QUIC_PKT_RETRY     = 0x03,
    SHE_QUIC_PKT_1RTT      = 0x40  /* Short Header */
} she_quic_pkt_type_t;

/* HTTP/3 RFC 9114 Frame Types */
typedef enum {
    SHE_H3_FRAME_DATA          = 0x00,
    SHE_H3_FRAME_HEADERS       = 0x01,
    SHE_H3_FRAME_CANCEL_PUSH   = 0x03,
    SHE_H3_FRAME_SETTINGS      = 0x04,
    SHE_H3_FRAME_PUSH_PROMISE  = 0x05,
    SHE_H3_FRAME_GOAWAY        = 0x07,
    SHE_H3_FRAME_MAX_PUSH_ID   = 0x0D
} she_h3_frame_type_t;

#define SHE_QUIC_MAX_CID_LEN 20

typedef struct {
    uint8_t data[SHE_QUIC_MAX_CID_LEN];
    uint8_t len;
} she_quic_cid_t;

typedef struct {
    uint8_t first_byte;
    uint32_t version;
    she_quic_cid_t dcid;
    she_quic_cid_t scid;
    uint64_t packet_number;
    size_t payload_len;
} she_quic_header_t;

typedef struct {
    uint64_t total_udp_packets_rx;
    uint64_t total_udp_packets_tx;
    uint64_t active_quic_connections;
    uint64_t zero_rtt_handshakes;
    uint64_t connection_migrations;
    uint64_t h3_requests_served;
    uint32_t qpack_max_table_capacity;
    uint32_t max_field_section_size;
} she_quic_stats_t;

/* Plugin initialization */
she_plugin_t *mod_quic_plugin_init(void);

/* QUIC APIs */
int she_quic_parse_header(const uint8_t *buf, size_t len, she_quic_header_t *out_hdr);
const she_quic_stats_t *she_quic_get_stats(void);

#endif /* SHE_MOD_QUIC_H */
