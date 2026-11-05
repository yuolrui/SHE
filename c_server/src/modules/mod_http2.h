#ifndef MOD_HTTP2_H
#define MOD_HTTP2_H

#include "modules/she_plugin.h"
#include <stdint.h>

/**
 * mod_http2: SHE HTTP/2 Binary Framing & Preface Engine (RFC 7540 / RFC 9113)
 *
 * Implements:
 * 1. HTTP/2 24-byte Client Connection Preface verification (PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n)
 * 2. 9-byte Binary Frame Header construction & parsing (Length, Type, Flags, Stream ID)
 * 3. Server SETTINGS frame (Type 0x04) generation
 * 4. PING (Type 0x06), GOAWAY (Type 0x07), and DATA (Type 0x00) frame handling
 * 5. Runtime HTTP/2 telemetry & status monitor (/h2-status)
 */

#define SHE_H2_PREFACE "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
#define SHE_H2_PREFACE_LEN 24

/* HTTP/2 Frame Types */
#define SHE_H2_FRAME_DATA          0x00
#define SHE_H2_FRAME_HEADERS       0x01
#define SHE_H2_FRAME_PRIORITY      0x02
#define SHE_H2_FRAME_RST_STREAM    0x03
#define SHE_H2_FRAME_SETTINGS      0x04
#define SHE_H2_FRAME_PUSH_PROMISE  0x05
#define SHE_H2_FRAME_PING          0x06
#define SHE_H2_FRAME_GOAWAY        0x07
#define SHE_H2_FRAME_WINDOW_UPDATE 0x08
#define SHE_H2_FRAME_CONTINUATION  0x09

/* HTTP/2 SETTINGS Parameters */
#define SHE_H2_SETTINGS_HEADER_TABLE_SIZE      0x01
#define SHE_H2_SETTINGS_ENABLE_PUSH            0x02
#define SHE_H2_SETTINGS_MAX_CONCURRENT_STREAMS 0x03
#define SHE_H2_SETTINGS_INITIAL_WINDOW_SIZE    0x04
#define SHE_H2_SETTINGS_MAX_FRAME_SIZE         0x05
#define SHE_H2_SETTINGS_MAX_HEADER_LIST_SIZE   0x06

#pragma pack(push, 1)
typedef struct {
    uint8_t length[3];  /* 24-bit big-endian length */
    uint8_t type;       /* 8-bit frame type */
    uint8_t flags;      /* 8-bit flags */
    uint32_t stream_id; /* 31-bit stream id with 1-bit reserved */
} she_h2_frame_header_t;
#pragma pack(pop)

she_plugin_t *mod_http2_plugin_init(void);

#endif /* MOD_HTTP2_H */
