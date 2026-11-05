#ifndef MOD_DEFLATE_H
#define MOD_DEFLATE_H

#include "modules/she_plugin.h"
#include <stddef.h>

/**
 * mod_deflate: Dynamic Gzip Content-Encoding Compression Subsystem (RFC 1952)
 *
 * Compresses outbound HTTP response bodies on-the-fly for clients declaring
 * 'Accept-Encoding: gzip', reducing bandwidth by up to 75%.
 */
she_plugin_t *mod_deflate_plugin_init(void);

/* Helper to compress in-memory buffer with standard gzip header and crc32 trailer */
int she_deflate_compress_gzip(const char *in_data, size_t in_len, char **out_data, size_t *out_len);

/* Check whether a given MIME type is eligible for compression */
int she_deflate_is_compressible_mime(const char *mime_type);

#endif /* MOD_DEFLATE_H */
