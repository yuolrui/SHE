#ifndef MOD_FASTCGI_H
#define MOD_FASTCGI_H

#include "modules/she_plugin.h"
#include <stdint.h>

#define FCGI_VERSION_1           1
#define FCGI_BEGIN_REQUEST       1
#define FCGI_ABORT_REQUEST       2
#define FCGI_END_REQUEST         3
#define FCGI_PARAMS              4
#define FCGI_STDIN               5
#define FCGI_STDOUT              6
#define FCGI_STDERR              7
#define FCGI_DATA                8
#define FCGI_GET_VALUES          9
#define FCGI_GET_VALUES_RESULT  10
#define FCGI_UNKNOWN_TYPE       11

#define FCGI_RESPONDER           1
#define FCGI_AUTHORIZER          2
#define FCGI_FILTER              3

typedef struct {
    uint8_t version;
    uint8_t type;
    uint16_t requestId;
    uint16_t contentLength;
    uint8_t paddingLength;
    uint8_t reserved;
} __attribute__((packed)) fcgi_header_t;

typedef struct {
    uint16_t role;
    uint8_t flags;
    uint8_t reserved[5];
} __attribute__((packed)) fcgi_begin_request_body_t;

typedef struct {
    uint32_t appStatus;
    uint8_t protocolStatus;
    uint8_t reserved[3];
} __attribute__((packed)) fcgi_end_request_body_t;

she_plugin_t *mod_fastcgi_plugin_init(void);

#endif /* MOD_FASTCGI_H */
