#include "http_parser.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>

void http_request_init(http_request_t *req) {
    if (!req) return;
    memset(req, 0, sizeof(http_request_t));
    req->uri_raw = buffer_init();
    req->uri_path = buffer_init();
    req->uri_query = buffer_init();
    req->host = buffer_init();
    req->if_modified_since = buffer_init();
    req->has_range = 0;
    req->range_start = 0;
    req->range_end = -1;
    req->content_length = -1;
    req->keep_alive = 0;
    
    for (size_t i = 0; i < MAX_HTTP_HEADERS; i++) {
        req->headers[i].key = buffer_init();
        req->headers[i].value = buffer_init();
    }
}

void http_request_reset(http_request_t *req) {
    if (!req) return;
    req->method = HTTP_METHOD_UNSET;
    req->version = HTTP_VERSION_UNSET;
    
    buffer_reset(req->uri_raw);
    buffer_reset(req->uri_path);
    buffer_reset(req->uri_query);
    buffer_reset(req->host);
    buffer_reset(req->if_modified_since);
    
    req->has_range = 0;
    req->range_start = 0;
    req->range_end = -1;
    req->content_length = -1;
    req->keep_alive = 0;
    
    for (size_t i = 0; i < req->header_count; i++) {
        buffer_reset(req->headers[i].key);
        buffer_reset(req->headers[i].value);
    }
    req->header_count = 0;
}

void http_request_free(http_request_t *req) {
    if (!req) return;
    buffer_free(req->uri_raw);
    buffer_free(req->uri_path);
    buffer_free(req->uri_query);
    buffer_free(req->host);
    buffer_free(req->if_modified_since);
    
    for (size_t i = 0; i < MAX_HTTP_HEADERS; i++) {
        buffer_free(req->headers[i].key);
        buffer_free(req->headers[i].value);
    }
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* URL decode and sanitize path to prevent directory traversal */
static int sanitize_path(const char *raw, size_t raw_len, buffer_t *out) {
    buffer_reset(out);
    
    /* Decode %XX */
    buffer_t *decoded = buffer_init_size(raw_len + 1);
    if (!decoded) return -1;
    
    for (size_t i = 0; i < raw_len; i++) {
        if (raw[i] == '%' && i + 2 < raw_len) {
            int h1 = hex_val(raw[i + 1]);
            int h2 = hex_val(raw[i + 2]);
            if (h1 >= 0 && h2 >= 0) {
                char ch = (char)((h1 << 4) | h2);
                buffer_append_char(decoded, ch);
                i += 2;
                continue;
            }
        }
        buffer_append_char(decoded, raw[i]);
    }
    
    /* Canonicalize path & neutralize /../ */
    const char *src = buffer_cstr(decoded);
    size_t dlen = buffer_len(decoded);
    
    char *clean = (char *)malloc(dlen + 2);
    if (!clean) {
        buffer_free(decoded);
        return -1;
    }
    
    size_t ci = 0;
    clean[ci++] = '/';
    
    for (size_t i = 0; i < dlen; ) {
        /* Skip multiple consecutive slashes */
        while (i < dlen && src[i] == '/') i++;
        if (i >= dlen) break;
        
        size_t seg_start = i;
        while (i < dlen && src[i] != '/') i++;
        size_t seg_len = i - seg_start;
        
        if (seg_len == 1 && src[seg_start] == '.') {
            /* Current directory '.' ignore */
            continue;
        } else if (seg_len == 2 && src[seg_start] == '.' && src[seg_start + 1] == '.') {
            /* Parent directory '..' pop last component */
            if (ci > 1) {
                ci--; /* remove trailing slash if any */
                while (ci > 0 && clean[ci - 1] != '/') ci--;
            }
        } else {
            /* Normal segment */
            if (ci > 1 && clean[ci - 1] != '/') clean[ci++] = '/';
            memcpy(clean + ci, src + seg_start, seg_len);
            ci += seg_len;
        }
    }
    
    clean[ci] = '\0';
    buffer_copy_string_len(out, clean, ci);
    
    free(clean);
    buffer_free(decoded);
    return 0;
}

static void parse_range_header(http_request_t *req, const char *val) {
    if (!val) return;
    while (*val == ' ') val++;
    if (strncasecmp(val, "bytes=", 6) != 0) return;
    val += 6;
    
    char *dash = strchr(val, '-');
    if (!dash) return;
    
    req->has_range = 1;
    if (dash == val) {
        /* Suffix range: bytes=-500 */
        req->range_start = -1;
        req->range_end = atoll(dash + 1);
    } else {
        req->range_start = atoll(val);
        if (*(dash + 1) != '\0') {
            req->range_end = atoll(dash + 1);
        } else {
            req->range_end = -1; /* Unbounded */
        }
    }
}

int http_request_parse(http_request_t *req, const char *raw_data, size_t len, size_t *header_len) {
    if (!req || !raw_data || len == 0) return 0;
    
    /* Find double CRLF or double LF indicating end of headers */
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
        /* Incomplete request headers */
        if (len > 32768) return -3; /* Request header too large (431) */
        return 0;
    }
    
    *header_len = (size_t)(end - raw_data) + eol_size;
    
    /* 1. Parse Request Line */
    const char *p = raw_data;
    const char *line_end = strchr(p, '\n');
    if (!line_end || line_end > end) return -1;
    
    /* Method */
    if (strncmp(p, "GET ", 4) == 0) {
        req->method = HTTP_METHOD_GET;
        p += 4;
    } else if (strncmp(p, "HEAD ", 5) == 0) {
        req->method = HTTP_METHOD_HEAD;
        p += 5;
    } else if (strncmp(p, "POST ", 5) == 0) {
        req->method = HTTP_METHOD_POST;
        p += 5;
    } else if (strncmp(p, "OPTIONS ", 8) == 0) {
        req->method = HTTP_METHOD_OPTIONS;
        p += 8;
    } else {
        req->method = HTTP_METHOD_UNSUPPORTED;
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
    buffer_copy_string_len(req->uri_raw, uri_start, uri_len);
    
    /* Query string split */
    const char *qmark = memchr(uri_start, '?', uri_len);
    if (qmark) {
        size_t path_len = qmark - uri_start;
        sanitize_path(uri_start, path_len, req->uri_path);
        buffer_copy_string_len(req->uri_query, qmark + 1, (uri_start + uri_len) - (qmark + 1));
    } else {
        sanitize_path(uri_start, uri_len, req->uri_path);
        buffer_reset(req->uri_query);
    }
    
    p = uri_end + 1;
    while (*p == ' ') p++;
    
    /* Version */
    if (strncmp(p, "HTTP/1.1", 8) == 0) {
        req->version = HTTP_VERSION_1_1;
        req->keep_alive = 1; /* HTTP/1.1 defaults to keep-alive */
    } else if (strncmp(p, "HTTP/1.0", 8) == 0) {
        req->version = HTTP_VERSION_1_0;
        req->keep_alive = 0; /* HTTP/1.0 defaults to close */
    } else {
        return -1;
    }
    
    /* 2. Parse Headers */
    p = line_end + 1;
    while (p < end && req->header_count < MAX_HTTP_HEADERS) {
        const char *next_line = strchr(p, '\n');
        if (!next_line) break;
        
        size_t cur_line_len = next_line - p;
        if (cur_line_len > 0 && p[cur_line_len - 1] == '\r') {
            cur_line_len--;
        }
        
        if (cur_line_len == 0) {
            break; /* Empty line before body */
        }
        
        const char *colon = memchr(p, ':', cur_line_len);
        if (colon) {
            size_t klen = colon - p;
            const char *vstart = colon + 1;
            while (vstart < p + cur_line_len && *vstart == ' ') vstart++;
            size_t vlen = (p + cur_line_len) - vstart;
            
            http_header_t *hdr = &req->headers[req->header_count];
            buffer_copy_string_len(hdr->key, p, klen);
            buffer_copy_string_len(hdr->value, vstart, vlen);
            req->header_count++;
            
            const char *key_str = buffer_cstr(hdr->key);
            const char *val_str = buffer_cstr(hdr->value);
            
            if (strcasecmp(key_str, "Connection") == 0) {
                if (strcasecmp(val_str, "keep-alive") == 0) {
                    req->keep_alive = 1;
                } else if (strcasecmp(val_str, "close") == 0) {
                    req->keep_alive = 0;
                }
            } else if (strcasecmp(key_str, "Host") == 0) {
                buffer_copy_string(req->host, val_str);
            } else if (strcasecmp(key_str, "If-Modified-Since") == 0) {
                buffer_copy_string(req->if_modified_since, val_str);
            } else if (strcasecmp(key_str, "Content-Length") == 0) {
                req->content_length = atoll(val_str);
            } else if (strcasecmp(key_str, "Range") == 0) {
                parse_range_header(req, val_str);
            }
        }
        
        p = next_line + 1;
    }
    
    return 1;
}

const char *http_request_get_header(const http_request_t *req, const char *name) {
    if (!req || !name) return NULL;
    for (size_t i = 0; i < req->header_count; i++) {
        if (strcasecmp(buffer_cstr(req->headers[i].key), name) == 0) {
            return buffer_cstr(req->headers[i].value);
        }
    }
    return NULL;
}

const char *http_method_to_str(http_method_t method) {
    switch (method) {
        case HTTP_METHOD_GET: return "GET";
        case HTTP_METHOD_HEAD: return "HEAD";
        case HTTP_METHOD_POST: return "POST";
        case HTTP_METHOD_OPTIONS: return "OPTIONS";
        default: return "UNKNOWN";
    }
}
