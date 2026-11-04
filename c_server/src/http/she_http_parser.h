#ifndef SHE_HTTP_PARSER_H
#define SHE_HTTP_PARSER_H

#include <stddef.h>
#include <sys/types.h>
#include "core/she_buffer.h"

typedef enum {
    SHE_HTTP_METHOD_UNSET = 0,
    SHE_HTTP_METHOD_GET,
    SHE_HTTP_METHOD_HEAD,
    SHE_HTTP_METHOD_POST,
    SHE_HTTP_METHOD_PUT,
    SHE_HTTP_METHOD_DELETE,
    SHE_HTTP_METHOD_OPTIONS,
    SHE_HTTP_METHOD_UNSUPPORTED
} she_http_method_t;

typedef enum {
    SHE_HTTP_VERSION_UNSET = 0,
    SHE_HTTP_VERSION_1_0,
    SHE_HTTP_VERSION_1_1
} she_http_version_t;

typedef struct {
    she_buffer_t *key;
    she_buffer_t *value;
} she_http_header_t;

#define SHE_MAX_HTTP_HEADERS 64

typedef struct {
    she_http_method_t method;
    she_http_version_t version;
    
    she_buffer_t *uri_raw;       /* Raw URI from request line */
    she_buffer_t *uri_path;      /* Decoded & sanitized path */
    she_buffer_t *uri_query;     /* Query string after '?' if present */
    
    she_http_header_t headers[SHE_MAX_HTTP_HEADERS];
    size_t header_count;
    
    /* Pre-parsed common headers */
    int keep_alive;
    off_t content_length;
    she_buffer_t *host;
    she_buffer_t *if_modified_since;
    she_buffer_t *authorization; /* Authorization: Basic ... */
    
    /* Range header: Range: bytes=start-end */
    int has_range;
    off_t range_start;
    off_t range_end;
    
    /* FastCGI / Vhost virtual mapping */
    she_buffer_t *vhost_docroot;
} she_http_request_t;

/* Compatibility typedefs */
typedef she_http_method_t http_method_t;
typedef she_http_version_t http_version_t;
typedef she_http_header_t http_header_t;
typedef she_http_request_t http_request_t;

#define HTTP_METHOD_UNSET        SHE_HTTP_METHOD_UNSET
#define HTTP_METHOD_GET          SHE_HTTP_METHOD_GET
#define HTTP_METHOD_HEAD         SHE_HTTP_METHOD_HEAD
#define HTTP_METHOD_POST         SHE_HTTP_METHOD_POST
#define HTTP_METHOD_OPTIONS      SHE_HTTP_METHOD_OPTIONS
#define HTTP_METHOD_UNSUPPORTED  SHE_HTTP_METHOD_UNSUPPORTED

#define HTTP_VERSION_UNSET       SHE_HTTP_VERSION_UNSET
#define HTTP_VERSION_1_0         SHE_HTTP_VERSION_1_0
#define HTTP_VERSION_1_1         SHE_HTTP_VERSION_1_1

#define MAX_HTTP_HEADERS         SHE_MAX_HTTP_HEADERS

void she_http_request_init(she_http_request_t *req);
void she_http_request_reset(she_http_request_t *req);
void she_http_request_free(she_http_request_t *req);

int she_http_request_parse(she_http_request_t *req, const char *raw_data, size_t len, size_t *header_len);
const char *she_http_request_get_header(const she_http_request_t *req, const char *name);
const char *she_http_method_to_str(she_http_method_t method);

/* Compatibility macros */
#define http_request_init she_http_request_init
#define http_request_reset she_http_request_reset
#define http_request_free she_http_request_free
#define http_request_parse she_http_request_parse
#define http_request_get_header she_http_request_get_header
#define http_method_to_str she_http_method_to_str

#endif /* SHE_HTTP_PARSER_H */
