#ifndef MOD_PQC_H
#define MOD_PQC_H

#include <stddef.h>
#include <stdint.h>
#include "modules/she_plugin.h"

#define SHE_PQC_SHARED_SECRET_LEN 32
#define SHE_PQC_CIPHERTEXT_LEN    1088 /* ML-KEM-768 ciphertext size */
#define SHE_PQC_PUBLIC_KEY_LEN    1184 /* ML-KEM-768 public key size */

typedef enum {
    SHE_PQC_KEM_ML_KEM_768 = 0,   /* NIST FIPS 203 Category 3 (AES-192 equivalent) */
    SHE_PQC_KEM_ML_KEM_1024,      /* NIST FIPS 203 Category 5 (AES-256 equivalent) */
    SHE_PQC_KEM_HYBRID_X25519_ML_KEM /* Hybrid Classical ECDH + Post-Quantum KEM */
} she_pqc_kem_type_t;

typedef enum {
    SHE_PQC_SIG_ML_DSA_65 = 0,    /* NIST FIPS 204 Dilithium-3 */
    SHE_PQC_SIG_ML_DSA_87         /* NIST FIPS 204 Dilithium-5 */
} she_pqc_sig_type_t;

typedef struct {
    she_pqc_kem_type_t kem_type;
    uint32_t public_key_len;
    uint32_t ciphertext_len;
    uint32_t shared_secret_len;
    uint8_t ephemeral_pubkey[SHE_PQC_PUBLIC_KEY_LEN];
    uint8_t ciphertext[SHE_PQC_CIPHERTEXT_LEN];
    uint8_t shared_secret[SHE_PQC_SHARED_SECRET_LEN];
    uint64_t encapsulation_latency_us;
} she_pqc_session_t;

typedef struct {
    uint64_t total_pqc_handshakes;
    uint64_t total_hybrid_handshakes;
    uint64_t total_quantum_rejected;
    uint32_t active_pqc_sessions;
    uint32_t avg_handshake_latency_us;
    char default_kem[32];
    char default_sig[32];
    int quantum_safe_enforced;
} she_pqc_state_t;

/* Plugin initialization */
she_plugin_t *mod_pqc_plugin_init(void);

/* PQC Key encapsulation mechanism */
int she_pqc_encapsulate(she_pqc_kem_type_t type, she_pqc_session_t *out_session);

#endif /* MOD_PQC_H */
