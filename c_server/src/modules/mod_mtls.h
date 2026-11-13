#ifndef MOD_MTLS_H
#define MOD_MTLS_H

#include <stddef.h>
#include <stdint.h>
#include "modules/she_plugin.h"

typedef struct {
    char trust_domain[48];       /* "spiffe://she.mesh" */
    char ca_subject[64];         /* "CN=SHE Root Mesh CA,O=Secure HTTP Engine" */
    int require_client_cert;
    int ocsp_stapling_enabled;
    uint64_t certs_verified;
    uint64_t certs_rejected;
    char last_spiffe_id[96];
    char last_sha256_fp[65];
} she_mtls_state_t;

/* Plugin initialization */
she_plugin_t *mod_mtls_plugin_init(void);

/* Verify a SPIFFE/X.509 client certificate identity using OpenSSL SHA-256 */
int she_ssl_verify_mtls_client_cert(const char *spiffe_id, char *out_fingerprint, size_t fp_buf_sz);

#endif /* MOD_MTLS_H */
