#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <stddef.h>
#include <sys/types.h>
#include "buffer.h"

typedef enum {
    HTTP_METHOD_UNSET = 0,
    HTTP_METHOD_GET,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_POST,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_UNSUPPORTED
} http_method_t;

typedef enum {
    HTTP_VERSION_UNSET = 0,
    HTTP_VERSION_1_0,
    HTTP_VERSION_1_1
} http_version_t;

typedef struct {
    buffer_t *key;
    buffer_t *value;
} http_header_t;

#define MAX_HTTP_HEADERS 64

typedef struct {
    http_method_t method;
    http_version_t version;
    
    buffer_t *uri_raw;       /* Raw URI from request line */
    buffer_t *uri_path;      /* Decoded & sanitized path */
    buffer_t *uri_query;     /* Query string after '?' if present */
    
    http_header_t headers[MAX_HTTP_HEADERS];
    size_t header_count;
    
    /* Commonly extracted headers for high performance */
    int keep_alive;          /* 1 if keep-alive requested, 0 otherwise */
    off_t content_length;
    buffer_t *host;
    buffer_t *if_modified_since;
    
    /* HTTP Range header: Range: bytes=start-end */
    int has_range;
    off_t range_start;
    off_t range_end;         /* -1 if unbounded (e.g., bytes=100-) */
} http_request_t;

void http_request_init(http_request_t *req);
void http_request_reset(http_request_t *req);
void http_request_free(http_request_t *req);

/* Returns:
 *   1 : Request headers completely parsed
 *   0 : Incomplete request, need more data
 *  -1 : Bad request syntax (400 Bad Request)
 *  -2 : Request URI too long (414)
 *  -3 : Request header too large (431)
 */
int http_request_parse(http_request_t *req, const char *raw_data, size_t len, size_t *header_len);

const char *http_request_get_header(const http_request_t *req, const char *name);
const char *http_method_to_str(http_method_t method);

#endif /* HTTP_PARSER_H */
