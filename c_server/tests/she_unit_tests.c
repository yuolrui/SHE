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
