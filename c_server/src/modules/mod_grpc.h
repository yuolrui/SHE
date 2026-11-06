#ifndef SHE_MOD_GRPC_H
#define SHE_MOD_GRPC_H

#include "modules/she_plugin.h"
#include <stdint.h>
#include <stddef.h>

/**
 * gRPC over HTTP/2 & Protocol Buffers Binary Framing
 */
typedef enum {
    SHE_GRPC_STATUS_OK                 = 0,
    SHE_GRPC_STATUS_CANCELLED          = 1,
    SHE_GRPC_STATUS_UNKNOWN            = 2,
    SHE_GRPC_STATUS_INVALID_ARGUMENT   = 3,
    SHE_GRPC_STATUS_DEADLINE_EXCEEDED  = 4,
    SHE_GRPC_STATUS_NOT_FOUND          = 5,
    SHE_GRPC_STATUS_ALREADY_EXISTS     = 6,
    SHE_GRPC_STATUS_PERMISSION_DENIED  = 7,
    SHE_GRPC_STATUS_UNAUTHENTICATED    = 16
} she_grpc_status_t;

typedef struct {
    uint8_t compressed;
    uint32_t message_len;
    uint8_t *payload;
} she_grpc_frame_t;

typedef struct {
    uint64_t unary_calls;
    uint64_t server_streaming_calls;
    uint64_t client_streaming_calls;
    uint64_t bidi_streaming_calls;
    uint64_t total_transcoded_bytes;
    uint32_t active_services;
} she_grpc_stats_t;

/* Plugin initialization */
she_plugin_t *mod_grpc_plugin_init(void);

/* gRPC framing APIs */
int she_grpc_pack_frame(const uint8_t *msg, uint32_t len, uint8_t compressed, uint8_t *out_frame, size_t out_max);
int she_grpc_unpack_frame(const uint8_t *frame, size_t len, uint8_t *out_compressed, uint32_t *out_len, const uint8_t **out_msg);
const she_grpc_stats_t *she_grpc_get_stats(void);

#endif /* SHE_MOD_GRPC_H */
