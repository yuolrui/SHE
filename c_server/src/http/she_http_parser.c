#include "http/she_http_parser.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>

void she_http_request_init(she_http_request_t *req) {
    if (!req) return;
    memset(req, 0, sizeof(she_http_request_t));
    req->uri_raw = she_buffer_init();
    req->uri_path = she_buffer_init();
    req->uri_query = she_buffer_init();
    req->host = she_buffer_init();
    req->if_modified_since = she_buffer_init();
    req->authorization = she_buffer_init();
    req->vhost_docroot = she_buffer_init();
    req->has_range = 0;
    req->range_start = 0;
    req->range_end = -1;
    req->content_length = -1;
    req->keep_alive = 0;
    
    for (size_t i = 0; i < SHE_MAX_HTTP_HEADERS; i++) {
        req->headers[i].key = she_buffer_init();
        req->headers[i].value = she_buffer_init();
    }
}

void she_http_request_reset(she_http_request_t *req) {
    if (!req) return;
    req->method = SHE_HTTP_METHOD_UNSET;
    req->version = SHE_HTTP_VERSION_UNSET;
    
    she_buffer_reset(req->uri_raw);
    she_buffer_reset(req->uri_path);
    she_buffer_reset(req->uri_query);
    she_buffer_reset(req->host);
    she_buffer_reset(req->if_modified_since);
    she_buffer_reset(req->authorization);
    she_buffer_reset(req->vhost_docroot);
    
    req->has_range = 0;
    req->range_start = 0;
    req->range_end = -1;
    req->content_length = -1;
    req->keep_alive = 0;
    
    for (size_t i = 0; i < req->header_count; i++) {
        she_buffer_reset(req->headers[i].key);
        she_buffer_reset(req->headers[i].value);
    }
    req->header_count = 0;
}

void she_http_request_free(she_http_request_t *req) {
    if (!req) return;
    she_buffer_free(req->uri_raw);
    she_buffer_free(req->uri_path);
    she_buffer_free(req->uri_query);
    she_buffer_free(req->host);
    she_buffer_free(req->if_modified_since);
    she_buffer_free(req->authorization);
    she_buffer_free(req->vhost_docroot);
    
    for (size_t i = 0; i < SHE_MAX_HTTP_HEADERS; i++) {
        she_buffer_free(req->headers[i].key);
        she_buffer_free(req->headers[i].value);
    }
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* URL decode and sanitize path to prevent directory traversal */
static int sanitize_path(const char *raw, size_t raw_len, she_buffer_t *out) {
    she_buffer_reset(out);
    
    she_buffer_t *decoded = she_buffer_init_size(raw_len + 1);
    if (!decoded) return -1;
    
    for (size_t i = 0; i < raw_len; i++) {
        if (raw[i] == '%' && i + 2 < raw_len) {
            int h1 = hex_val(raw[i + 1]);
            int h2 = hex_val(raw[i + 2]);
            if (h1 >= 0 && h2 >= 0) {
                char ch = (char)((h1 << 4) | h2);
                she_buffer_append_char(decoded, ch);
                i += 2;
                continue;
            }
        }
        she_buffer_append_char(decoded, raw[i]);
    }
    
    /* Canonicalize path & neutralize /../ */
    const char *src = she_buffer_cstr(decoded);
    size_t dlen = she_buffer_len(decoded);
    
    char *clean = (char *)malloc(dlen + 2);
    if (!clean) {
        she_buffer_free(decoded);
        return -1;
    }
    
    size_t ci = 0;
    clean[ci++] = '/';
    
    for (size_t i = 0; i < dlen; ) {
        while (i < dlen && src[i] == '/') i++;
        if (i >= dlen) break;
        
        size_t seg_start = i;
        while (i < dlen && src[i] != '/') i++;
        size_t seg_len = i - seg_start;
        
        if (seg_len == 1 && src[seg_start] == '.') {
            continue;
        } else if (seg_len == 2 && src[seg_start] == '.' && src[seg_start + 1] == '.') {
            if (ci > 1) {
                ci--;
                while (ci > 0 && clean[ci - 1] != '/') ci--;
            }
        } else {
            if (ci > 1 && clean[ci - 1] != '/') clean[ci++] = '/';
            memcpy(clean + ci, src + seg_start, seg_len);
            ci += seg_len;
        }
    }
    
    if (dlen > 1 && src[dlen - 1] == '/' && ci > 1 && clean[ci - 1] != '/') {
        clean[ci++] = '/';
    }
    clean[ci] = '\0';
    she_buffer_copy_string_len(out, clean, ci);
    
    free(clean);
    she_buffer_free(decoded);
    return 0;
}

static void parse_range_header(she_http_request_t *req, const char *val) {
    if (!val) return;
    while (*val == ' ') val++;
    if (strncasecmp(val, "bytes=", 6) != 0) return;
    val += 6;
    
    char *dash = strchr(val, '-');
    if (!dash) return;
    
    req->has_range = 1;
    if (dash == val) {
        req->range_start = -1;
        req->range_end = atoll(dash + 1);
    } else {
        req->range_start = atoll(val);
        if (*(dash + 1) != '\0') {
            req->range_end = atoll(dash + 1);
        } else {
            req->range_end = -1;
        }
    }
}

int she_http_request_parse(she_http_request_t *req, const char *raw_data, size_t len, size_t *header_len) {
    if (!req || !raw_data || len == 0) return 0;
    
    const char *end = NULL;
    size_t eol_size = 0;
    
    for (size_t i = 0; i + 3 < len; i++) {
        if (raw_data[i] == '\r' && raw_data[i + 1] == '\n' &&
            raw_data[i + 2] == '\r' && raw_data[i + 3] == '\n') {
            end = raw_data + i;
            eol_size = 4;
            break;
        }
    }
    if (!end) {
        for (size_t i = 0; i + 1 < len; i++) {
            if (raw_data[i] == '\n' && raw_data[i + 1] == '\n') {
                end = raw_data + i;
                eol_size = 2;
                break;
            }
        }
    }
    
    if (!end) {
        if (len > 32768) return -3;
        return 0;
    }
    
    *header_len = (size_t)(end - raw_data) + eol_size;
    
    /* 1. Parse Request Line */
    const char *p = raw_data;
    const char *line_end = strchr(p, '\n');
    if (!line_end || line_end > end) return -1;
    
    /* Method */
    if (strncmp(p, "GET ", 4) == 0) {
        req->method = SHE_HTTP_METHOD_GET;
        p += 4;
    } else if (strncmp(p, "HEAD ", 5) == 0) {
        req->method = SHE_HTTP_METHOD_HEAD;
        p += 5;
    } else if (strncmp(p, "POST ", 5) == 0) {
        req->method = SHE_HTTP_METHOD_POST;
        p += 5;
    } else if (strncmp(p, "PUT ", 4) == 0) {
        req->method = SHE_HTTP_METHOD_PUT;
        p += 4;
    } else if (strncmp(p, "DELETE ", 7) == 0) {
        req->method = SHE_HTTP_METHOD_DELETE;
        p += 7;
    } else if (strncmp(p, "OPTIONS ", 8) == 0) {
        req->method = SHE_HTTP_METHOD_OPTIONS;
        p += 8;
    } else {
        req->method = SHE_HTTP_METHOD_UNSUPPORTED;
        const char *sp = strchr(p, ' ');
        if (!sp || sp > line_end) return -1;
        p = sp + 1;
    }
    
    while (*p == ' ') p++;
    
    /* URI */
    const char *uri_start = p;
    const char *uri_end = strchr(p, ' ');
    if (!uri_end || uri_end > line_end) return -1;
    
    size_t uri_len = uri_end - uri_start;
    she_buffer_copy_string_len(req->uri_raw, uri_start, uri_len);
    
    /* Query string split */
    const char *qmark = memchr(uri_start, '?', uri_len);
    if (qmark) {
        size_t path_len = qmark - uri_start;
        sanitize_path(uri_start, path_len, req->uri_path);
        she_buffer_copy_string_len(req->uri_query, qmark + 1, (uri_start + uri_len) - (qmark + 1));
    } else {
        sanitize_path(uri_start, uri_len, req->uri_path);
        she_buffer_reset(req->uri_query);
    }
    
    p = uri_end + 1;
    while (*p == ' ') p++;
    
    /* Version */
    if (strncmp(p, "HTTP/1.1", 8) == 0) {
        req->version = SHE_HTTP_VERSION_1_1;
        req->keep_alive = 1;
    } else if (strncmp(p, "HTTP/1.0", 8) == 0) {
        req->version = SHE_HTTP_VERSION_1_0;
        req->keep_alive = 0;
    } else {
        return -1;
    }
    
    /* Move to headers */
    p = line_end + 1;
    
    /* 2. Parse Headers */
    while (p < end) {
        if (*p == '\r' && *(p + 1) == '\n') { p += 2; break; }
        if (*p == '\n') { p += 1; break; }
        
        const char *h_end = strchr(p, '\n');
        if (!h_end || h_end > end + eol_size) break;
        
        const char *colon = memchr(p, ':', h_end - p);
        if (!colon) {
            p = h_end + 1;
            continue;
        }
        
        if (req->header_count < SHE_MAX_HTTP_HEADERS) {
            size_t key_len = colon - p;
            const char *val_start = colon + 1;
            while (val_start < h_end && (*val_start == ' ' || *val_start == '\t')) {
                val_start++;
            }
            const char *val_end = h_end;
            if (val_end > val_start && *(val_end - 1) == '\r') val_end--;
            while (val_end > val_start && (*(val_end - 1) == ' ' || *(val_end - 1) == '\t')) {
                val_end--;
            }
            
            she_http_header_t *hdr = &req->headers[req->header_count++];
            she_buffer_copy_string_len(hdr->key, p, key_len);
            she_buffer_copy_string_len(hdr->value, val_start, val_end - val_start);
            
            const char *k = she_buffer_cstr(hdr->key);
            const char *v = she_buffer_cstr(hdr->value);
            
            if (strcasecmp(k, "Connection") == 0) {
                if (strcasecmp(v, "keep-alive") == 0) {
                    req->keep_alive = 1;
                } else if (strcasecmp(v, "close") == 0) {
                    req->keep_alive = 0;
                }
            } else if (strcasecmp(k, "Host") == 0) {
                she_buffer_copy_string(req->host, v);
            } else if (strcasecmp(k, "Content-Length") == 0) {
                req->content_length = atoll(v);
            } else if (strcasecmp(k, "If-Modified-Since") == 0) {
                she_buffer_copy_string(req->if_modified_since, v);
            } else if (strcasecmp(k, "Range") == 0) {
                parse_range_header(req, v);
            } else if (strcasecmp(k, "Authorization") == 0) {
                she_buffer_copy_string(req->authorization, v);
            }
        }
        p = h_end + 1;
    }
    
    return 1;
}

const char *she_http_request_get_header(const she_http_request_t *req, const char *name) {
    if (!req || !name) return NULL;
    for (size_t i = 0; i < req->header_count; i++) {
        if (strcasecmp(she_buffer_cstr(req->headers[i].key), name) == 0) {
            return she_buffer_cstr(req->headers[i].value);
        }
    }
    return NULL;
}

const char *she_http_method_to_str(she_http_method_t method) {
    switch (method) {
        case SHE_HTTP_METHOD_GET:         return "GET";
        case SHE_HTTP_METHOD_HEAD:        return "HEAD";
        case SHE_HTTP_METHOD_POST:        return "POST";
        case SHE_HTTP_METHOD_PUT:         return "PUT";
        case SHE_HTTP_METHOD_DELETE:      return "DELETE";
        case SHE_HTTP_METHOD_OPTIONS:     return "OPTIONS";
        default:                          return "UNKNOWN";
    }
}
