#include "modules/mod_deflate.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include <zlib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char *default_compressible_mimes[] = {
    "text/html",
    "text/plain",
    "text/css",
    "application/javascript",
    "text/javascript",
    "application/json",
    "application/xml",
    "text/xml",
    NULL
};

int she_deflate_is_compressible_mime(const char *mime_type) {
    if (!mime_type || !*mime_type) return 0;
    
    for (int i = 0; default_compressible_mimes[i] != NULL; i++) {
        if (strncasecmp(mime_type, default_compressible_mimes[i], strlen(default_compressible_mimes[i])) == 0) {
            return 1;
        }
    }
    return 0;
}

int she_deflate_compress_gzip(const char *in_data, size_t in_len, char **out_data, size_t *out_len) {
    if (!in_data || in_len == 0 || !out_data || !out_len) return -1;
    
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    
    /* windowBits: 15 + 16 indicates standard gzip wrapper with header and CRC32 */
    int ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        return -1;
    }
    
    /* Upper bound for gzip compressed payload */
    size_t max_out = deflateBound(&strm, (uLong)in_len) + 64;
    char *buf = (char *)malloc(max_out);
    if (!buf) {
        deflateEnd(&strm);
        return -1;
    }
    
    strm.next_in = (Bytef *)in_data;
    strm.avail_in = (uInt)in_len;
    strm.next_out = (Bytef *)buf;
    strm.avail_out = (uInt)max_out;
    
    ret = deflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&strm);
        free(buf);
        return -1;
    }
    
    *out_len = (size_t)strm.total_out;
    *out_data = buf;
    deflateEnd(&strm);
    return 0;
}

static she_handler_t mod_deflate_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)srv;
    (void)p_data;
    /* Optional endpoint: /deflate-status */
    const char *uri = she_buffer_cstr(con->request.uri_path);
    if (!uri) return SHE_HANDLER_GO_ON;
    
    if (strcmp(uri, "/deflate-status") == 0) {
        con->http_status = 200;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        
        char date_buf[64];
        time_t now = time(NULL);
        struct tm tm;
        gmtime_r(&now, &tm);
        strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        
        char body[1024];
        int body_len = snprintf(body, sizeof(body),
            "{\n"
            "  \"module\": \"mod_deflate\",\n"
            "  \"status\": \"active\",\n"
            "  \"encoding\": \"gzip\",\n"
            "  \"rfc\": \"RFC 1952\",\n"
            "  \"zlib_version\": \"%s\",\n"
            "  \"min_compress_size\": %d,\n"
            "  \"requests_compressed\": %llu,\n"
            "  \"bytes_saved\": %llu\n"
            "}\n",
            zlibVersion(),
            srv->config.deflate_min_size > 0 ? srv->config.deflate_min_size : 128,
            (unsigned long long)srv->stats.requests_compressed,
            (unsigned long long)srv->stats.bytes_compressed_saved);
            
        she_buffer_append_printf(con->response_headers,
            "HTTP/1.1 200 OK\r\n"
            "Date: %s\r\n"
            "Server: %s\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "Content-Length: %d\r\n"
            "Connection: %s\r\n"
            "\r\n",
            date_buf,
            srv->config.server_tag,
            body_len,
            con->keep_alive ? "keep-alive" : "close");
            
        she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
        if (con->request.method != SHE_HTTP_METHOD_HEAD) {
            she_chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
        }
        
        con->state = SHE_CON_STATE_RESPONSE_START;
        return SHE_HANDLER_FINISHED;
    }
    
    return SHE_HANDLER_GO_ON;
}

she_plugin_t *mod_deflate_plugin_init(void) {
    static she_plugin_t p;
    memset(&p, 0, sizeof(p));
    p.version = 1;
    p.name = "mod_deflate";
    p.description = "SHE On-the-fly Gzip HTTP Transfer Compression (RFC 1952 via zlib)";
    p.handle_subrequest_start = mod_deflate_subrequest_start;
    return &p;
}
