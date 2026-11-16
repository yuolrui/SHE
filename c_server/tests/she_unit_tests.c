#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "core/she_buffer.h"
#include "core/she_config.h"
#include "core/she_server.h"
#include "http/she_http_parser.h"
#include "security/she_security.h"
#include "modules/mod_nanocore.h"
#include "modules/mod_neural_routing.h"
#include "modules/mod_waveguide.h"
#include "modules/mod_dtn_bundle.h"
#include "modules/mod_planetary_grid.h"
#include "modules/mod_sandbox.h"
#include "modules/mod_proxy_pool.h"
#include "modules/mod_mtls.h"
#include "modules/mod_io_uring.h"
#include "modules/mod_xds.h"
#include "modules/mod_profiler.h"
#include "modules/mod_dpu_offload.h"
#include "modules/mod_zk_attest.h"
#include "modules/mod_rdma.h"
#include "modules/mod_cpo.h"
#include "modules/mod_bbrv3.h"
#include "modules/mod_formal_fsm.h"
#include "modules/mod_afxdp.h"
#include "modules/mod_federation.h"
#include "modules/mod_replay.h"
#include "modules/mod_enclave.h"
#include "modules/mod_neuro_edge.h"
#include "modules/mod_keyvault.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define SHE_TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, (msg)); \
        exit(1); \
    } \
    g_tests_passed++; \
} while (0)

static void test_buffer_subsystem(void) {
    she_buffer_t *b = she_buffer_init();
    SHE_TEST_ASSERT(b != NULL, "she_buffer_init returns non-NULL");
    SHE_TEST_ASSERT(she_buffer_is_empty(b) == 1, "new buffer is empty");

    SHE_TEST_ASSERT(she_buffer_copy_string(b, "SHE-") == 0, "copy string succeeds");
    SHE_TEST_ASSERT(she_buffer_append_string(b, "Phase13") == 0, "append string succeeds");
    SHE_TEST_ASSERT(strcmp(she_buffer_cstr(b), "SHE-Phase13") == 0, "buffer string matches");
    SHE_TEST_ASSERT(she_buffer_len(b) == 11, "buffer length matches 11");

    she_buffer_reset(b);
    SHE_TEST_ASSERT(she_buffer_is_empty(b) == 1, "reset buffer is empty");
    she_buffer_free(b);
}

static void test_http_parser_and_sanitization(void) {
    she_http_request_t req;
    she_http_request_init(&req);

    const char *raw_get = "GET /docs/index.html?v=13 HTTP/1.1\r\nHost: localhost:8899\r\nConnection: keep-alive\r\n\r\n";
    size_t hlen = 0;
    int rc = she_http_request_parse(&req, raw_get, strlen(raw_get), &hlen);
    SHE_TEST_ASSERT(rc == 1, "valid HTTP/1.1 GET parses with rc=1");
    SHE_TEST_ASSERT(req.method == SHE_HTTP_METHOD_GET, "method is GET");
    SHE_TEST_ASSERT(strcmp(she_buffer_cstr(req.uri_path), "/docs/index.html") == 0, "path extracted");
    SHE_TEST_ASSERT(strcmp(she_buffer_cstr(req.uri_query), "v=13") == 0, "query extracted");
    SHE_TEST_ASSERT(req.keep_alive == 1, "keep_alive detected");

    she_http_request_reset(&req);
    const char *raw_trav = "GET /../../etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n";
    rc = she_http_request_parse(&req, raw_trav, strlen(raw_trav), &hlen);
    SHE_TEST_ASSERT(rc == 1, "traversal request parses for sanitization");
    SHE_TEST_ASSERT(strstr(she_buffer_cstr(req.uri_path), "..") == NULL, "path traversal '..' stripped by sanitizer");

    she_http_request_free(&req);
}

static void test_security_engine(void) {
    she_security_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enable_rate_limit = 1;
    cfg.rate_limit_rps = 5.0;
    cfg.rate_limit_burst = 3.0;
    cfg.strict_host_check = 1;
    cfg.enable_security_headers = 1;

    she_security_t *sec = she_security_init(&cfg);
    SHE_TEST_ASSERT(sec != NULL, "she_security_init succeeds");

    /* 3 burst tokens allowed, 4th blocked */
    SHE_TEST_ASSERT(she_security_check_rate_limit(sec, "192.0.2.10") == 1, "req 1 allowed");
    SHE_TEST_ASSERT(she_security_check_rate_limit(sec, "192.0.2.10") == 1, "req 2 allowed");
    SHE_TEST_ASSERT(she_security_check_rate_limit(sec, "192.0.2.10") == 1, "req 3 allowed");
    SHE_TEST_ASSERT(she_security_check_rate_limit(sec, "192.0.2.10") == 0, "req 4 blocked by token bucket");
    SHE_TEST_ASSERT(sec->total_rate_blocks == 1, "total_rate_blocks incremented");

    SHE_TEST_ASSERT(she_security_validate_host(sec, "localhost:8899") == 1, "valid host accepted");
    SHE_TEST_ASSERT(she_security_validate_host(sec, "bad.host/evil") == 0, "malicious host with slash rejected");

    she_security_free(sec);
}

static void test_config_loader_and_plugins(void) {
    she_server_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strcpy(cfg.bind_ip, "0.0.0.0");
    cfg.port = 8899;

    int loaded = she_config_load_file(&cfg, "she.conf");
    SHE_TEST_ASSERT(loaded == 0, "she.conf loads cleanly");
    SHE_TEST_ASSERT(cfg.port == 8899, "configured port is 8899");

    uint64_t dur = 0;
    uint32_t ns_op = 0;
    SHE_TEST_ASSERT(she_nanocore_run_bench(10000, &dur, &ns_op) == 0, "nanocore bench runs cleanly");
    SHE_TEST_ASSERT(ns_op > 0, "nanocore latency > 0 ns");

    uint32_t risk = 0;
    char route[64] = {0};
    SHE_TEST_ASSERT(she_neural_predict_congestion(65002, 92, &risk, route, sizeof(route)) == 0, "neural prediction succeeds");
    SHE_TEST_ASSERT(risk >= 80, "high load yields high risk");

    uint32_t lat_ps = 0;
    SHE_TEST_ASSERT(she_waveguide_route_photon_packet(2, 4096, &lat_ps) == 0, "waveguide routes packet");
    SHE_TEST_ASSERT(lat_ps > 0 && lat_ps < 100, "waveguide latency is picosecond scale");

    char bid[64] = {0};
    SHE_TEST_ASSERT(she_dtn_dispatch_bundle("dtn://mars-relay-01/telemetry", 1024, bid, sizeof(bid)) == 0, "dtn bundle dispatches");
    SHE_TEST_ASSERT(strncmp(bid, "bpv7-", 5) == 0, "bundle id has bpv7- prefix");

    char zone[32] = {0};
    uint32_t carbon = 0;
    SHE_TEST_ASSERT(she_grid_balance_compute(450, zone, sizeof(zone), &carbon) == 0, "grid balance succeeds");
    SHE_TEST_ASSERT(carbon <= 15, "placed zone satisfies clean carbon intensity");

    she_sandbox_state_t sb;
    SHE_TEST_ASSERT(she_sandbox_audit_profile(&sb) == 0, "sandbox audit succeeds");
    SHE_TEST_ASSERT(sb.allowed_syscalls > 0, "sandbox allowlist populated");

    int slot = -1;
    SHE_TEST_ASSERT(she_proxy_pool_acquire_upstream("127.0.0.1:3000", &slot) == 0, "proxy pool acquires upstream");
    SHE_TEST_ASSERT(slot >= 0, "proxy pool returns valid slot id");
    SHE_TEST_ASSERT(she_proxy_pool_release_upstream(slot) == 0, "proxy pool releases slot cleanly");

    char fp[65] = {0};
    SHE_TEST_ASSERT(she_ssl_verify_mtls_client_cert("spiffe://she.mesh/ns/prod/sa/api", fp, sizeof(fp)) == 0, "valid SPIFFE ID verified");
    SHE_TEST_ASSERT(strlen(fp) == 64, "mTLS SHA-256 fingerprint is 64 hex chars");
    SHE_TEST_ASSERT(she_ssl_verify_mtls_client_cert("spiffe://untrusted.org/evil", fp, sizeof(fp)) != 0, "untrusted SPIFFE domain rejected");

    uint64_t cqe_seq = 0;
    SHE_TEST_ASSERT(she_fdevent_uring_submit_sendfile(1, 0, 65536U, &cqe_seq) == 0, "io_uring sendfile SQE succeeds");
    SHE_TEST_ASSERT(cqe_seq > 0ULL, "io_uring returns valid CQE completion sequence");

    uint64_t xds_ver = 0;
    SHE_TEST_ASSERT(she_cloud_mesh_sync_xds_snapshot("xds://global.she.mesh:18000", &xds_ver) == 0, "xDS delta snapshot sync succeeds");
    SHE_TEST_ASSERT(xds_ver > 0ULL, "xDS snapshot version incremented");
    SHE_TEST_ASSERT(she_xds_set_canary_weight("eu-central-1", 30U) == 0, "xDS canary weight updated for valid region");
    SHE_TEST_ASSERT(she_xds_set_canary_weight("nonexistent-region", 10U) != 0, "xDS rejects unknown region");

    uint64_t samples = 0;
    char alert_st[32] = {0};
    SHE_TEST_ASSERT(she_profiler_capture_flamegraph(100U, &samples) == 0, "flamegraph capture succeeds");
    SHE_TEST_ASSERT(samples == 1000ULL, "100ms capture yields 1000 samples");
    SHE_TEST_ASSERT(she_profiler_evaluate_alerts(NULL, alert_st, sizeof(alert_st)) == 0, "alertmanager evaluator runs cleanly");
    SHE_TEST_ASSERT(strcmp(alert_st, "OK_NOMINAL") == 0, "nominal alert state confirmed");

    uint64_t offloaded_flows = 0;
    SHE_TEST_ASSERT(she_dpu_offload_tls_flow(1024U, "strict-waf", &offloaded_flows) == 0, "DPU HW flow offload succeeds");
    SHE_TEST_ASSERT(offloaded_flows > 0ULL, "DPU offloaded flows counter incremented");
    SHE_TEST_ASSERT(she_dpu_offload_tls_flow(0U, "", &offloaded_flows) != 0, "chaos: DPU rejects invalid flow_id=0");

    char zk_commit[65] = {0};
    uint32_t verify_us = 0;
    SHE_TEST_ASSERT(she_zk_generate_compliance_proof("eu-gdpr-sovereign", zk_commit, sizeof(zk_commit), &verify_us) == 0, "ZK compliance proof generated");
    SHE_TEST_ASSERT(strlen(zk_commit) == 64, "ZK commitment is 64-char SHA-256 hex");
    SHE_TEST_ASSERT(verify_us > 0U && verify_us < 1000U, "ZK verification under 1ms");
    SHE_TEST_ASSERT(she_zk_generate_compliance_proof("", zk_commit, sizeof(zk_commit), &verify_us) != 0, "chaos: ZK rejects empty claim");

    SHE_TEST_ASSERT(she_rdma_post_send_chunkqueue(201U, 0x7A000010ULL, 1048576ULL) == 0, "RDMA RoCEv2 post-send succeeds");
    SHE_TEST_ASSERT(she_rdma_post_send_chunkqueue(0U, 0ULL, 0ULL) != 0, "chaos: RDMA rejects zero qp_num/bytes");

    uint64_t cpo_gbps = 0ULL;
    SHE_TEST_ASSERT(she_cpo_tune_optical_lane(1U, -2, &cpo_gbps) == 0, "CPO optical lane 1 tunes cleanly");
    SHE_TEST_ASSERT(cpo_gbps == 3200ULL, "CPO 8x400G aggregate equals 3200 Gbps");
    SHE_TEST_ASSERT(she_cpo_tune_optical_lane(99U, 20, &cpo_gbps) != 0, "chaos: CPO rejects out-of-range lane/power");

    uint64_t pacing_bps = 0ULL;
    SHE_TEST_ASSERT(she_bbrv3_pace_flow(101U, 180U, 40000U, &pacing_bps) == 0, "BBRv3 neural pacer succeeds");
    SHE_TEST_ASSERT(pacing_bps == 50000000000ULL, "BBRv3 ProbeBW UP applies 1.25x gain on 40Gbps");
    SHE_TEST_ASSERT(she_bbrv3_pace_flow(0U, 0U, 0U, &pacing_bps) != 0, "chaos: BBRv3 rejects zero flow/rtt/bw");

    uint32_t formal_checked = 0U;
    uint32_t formal_violations = 99U;
    SHE_TEST_ASSERT(she_formal_run_proof_suite(&formal_checked, &formal_violations) == 0, "Formal FSM proof suite verifies all states");
    SHE_TEST_ASSERT(formal_checked == 6U && formal_violations == 0U, "Formal FSM reports 6 verified rules and 0 violations");
    SHE_TEST_ASSERT(she_fsm_verify_invariant(NULL) != 0, "chaos: Formal FSM catches NULL connection invariant");

    uint64_t umem_frames = 0ULL;
    SHE_TEST_ASSERT(she_afxdp_bind_umem_ring("eth0", 0U, &umem_frames) == 0, "AF_XDP binds eth0 queue 0 UMEM cleanly");
    SHE_TEST_ASSERT(umem_frames == 16384ULL, "AF_XDP reports 16384 registered UMEM frames");
    SHE_TEST_ASSERT(she_afxdp_bind_umem_ring("", 99U, &umem_frames) != 0, "chaos: AF_XDP rejects empty ifname / out-of-range queue");

    uint64_t fed_epoch = 0ULL;
    SHE_TEST_ASSERT(she_federation_sync_policy("eu-west-mesh", "zero-trust-strict", &fed_epoch) == 0, "Federation syncs eu-west-mesh policy");
    SHE_TEST_ASSERT(fed_epoch > 1042ULL, "Federation increments global Raft policy epoch");
    SHE_TEST_ASSERT(she_federation_sync_policy("unknown-cluster", "p", &fed_epoch) != 0, "chaos: Federation rejects unknown cluster_id");

    uint64_t replay_seq = 0ULL;
    char replay_chk[17] = {0};
    SHE_TEST_ASSERT(she_replay_record_event("GET", "/federation-status", 200, &replay_seq) == 0, "Replay tracer records event cleanly");
    SHE_TEST_ASSERT(she_replay_verify_trace(replay_seq, replay_chk, sizeof(replay_chk)) == 0 && strlen(replay_chk) == 16, "Replay verifies deterministic 16-hex hash");
    SHE_TEST_ASSERT(she_replay_verify_trace(0ULL, replay_chk, sizeof(replay_chk)) != 0, "chaos: Replay rejects invalid seq=0");

    char tee_meas[65] = {0};
    SHE_TEST_ASSERT(she_enclave_verify_quote("AMD-SEV-SNP", "prod-quote-01", tee_meas, sizeof(tee_meas)) == 0 && strlen(tee_meas) == 64, "Enclave verifies AMD-SEV-SNP attestation quote");
    SHE_TEST_ASSERT(she_enclave_verify_quote("INTEL-TDX", "tdx-nonce-02", tee_meas, sizeof(tee_meas)) == 0 && strlen(tee_meas) == 64, "Enclave verifies INTEL-TDX attestation quote");
    SHE_TEST_ASSERT(she_enclave_verify_quote("UNTRUSTED-CPU", "nonce", tee_meas, sizeof(tee_meas)) != 0, "chaos: Enclave rejects unsupported TEE platform");

    uint32_t neuro_mv = 0U;
    char neuro_verdict[24] = {0};
    SHE_TEST_ASSERT(she_neuro_classify_spike(401U, 120U, &neuro_mv, neuro_verdict, sizeof(neuro_verdict)) == 0 && strcmp(neuro_verdict, "BENIGN_PASS") == 0, "Neuromorphic edge classifies nominal ISI as BENIGN_PASS");
    SHE_TEST_ASSERT(she_neuro_classify_spike(403U, 15U, &neuro_mv, neuro_verdict, sizeof(neuro_verdict)) == 0 && neuro_mv >= 70U && strcmp(neuro_verdict, "SPIKE_ANOMALY_DROP") == 0, "Neuromorphic STDP fires SPIKE_ANOMALY_DROP on rapid burst ISI < 25us");
    SHE_TEST_ASSERT(she_neuro_classify_spike(0U, 0U, &neuro_mv, neuro_verdict, sizeof(neuro_verdict)) != 0, "chaos: Neuromorphic classifier rejects flow_id=0 / isi_us=0");

    char kv_key_id[33] = {0};
    uint64_t kv_epoch = 0ULL;
    SHE_TEST_ASSERT(she_keyvault_seal_session_key("sovereign-ai", 3600U, kv_key_id, sizeof(kv_key_id), &kv_epoch) == 0 && strncmp(kv_key_id, "kv-", 3) == 0, "KeyVault seals ephemeral key with kv- prefix");
    SHE_TEST_ASSERT(kv_epoch > 880ULL, "KeyVault increments rotation epoch cleanly");
    SHE_TEST_ASSERT(she_keyvault_seal_session_key("sovereign-ai", 99999U, kv_key_id, sizeof(kv_key_id), &kv_epoch) != 0, "chaos: KeyVault rejects out-of-bounds TTL > 86400");
}

int main(void) {
    printf("Running SHE C11 Formal Verification & Unit Test Suite...\n");
    test_buffer_subsystem();
    test_http_parser_and_sanitization();
    test_security_engine();
    test_config_loader_and_plugins();
    printf("[PASS] %d/%d C11 assertions passed cleanly.\n", g_tests_passed, g_tests_run);
    return 0;
}
