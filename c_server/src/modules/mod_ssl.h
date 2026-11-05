#ifndef MOD_SSL_H
#define MOD_SSL_H

#include "modules/she_plugin.h"
#include <openssl/ssl.h>

/**
 * mod_ssl: SHE TLS 1.3 / 1.2 Transport Layer Security Subsystem
 *
 * Implements:
 * 1. OpenSSL 3.0 server context initialization
 * 2. TLS 1.3 & TLS 1.2 cipher suite configuration
 * 3. ALPN (Application-Layer Protocol Negotiation) for "h2" and "http/1.1"
 * 4. Runtime certificate and cryptographic status telemetry (/ssl-status)
 */
she_plugin_t *mod_ssl_plugin_init(void);

SSL_CTX *she_ssl_get_context(void);

#endif /* MOD_SSL_H */
