#ifndef MOD_WEBSOCKET_H
#define MOD_WEBSOCKET_H

#include "modules/she_plugin.h"

/**
 * mod_websocket: SHE RFC 6455 WebSocket Handshake & Frame Processing Subsystem
 *
 * Implements:
 * 1. HTTP/1.1 Upgrade: websocket negotiation
 * 2. Sec-WebSocket-Accept computation via SHA-1 and Base64 (RFC 6455 GUID)
 * 3. 101 Switching Protocols response generation
 * 4. RFC 6455 binary frame validation (Opcodes: Text 0x1, Binary 0x2, Close 0x8, Ping 0x9, Pong 0xA)
 */
she_plugin_t *mod_websocket_plugin_init(void);

/* Helper to compute RFC 6455 Sec-WebSocket-Accept token from Sec-WebSocket-Key */
int she_websocket_compute_accept(const char *key, char *out_accept, size_t out_max);

#endif /* MOD_WEBSOCKET_H */
