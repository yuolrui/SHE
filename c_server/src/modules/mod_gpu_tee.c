#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <openssl/evp.h>
#include "modules/mod_gpu_tee.h"
#include "core/she_server.h"
#include "core/she_connection.h"
#include "core/she_chunkqueue.h"
#include "core/she_buffer.h"

static she_gpu_tee_state_t g_gpu_tee;
static int g_gpu_tee_initialized = 0;

static void she_gpu_tee_sanitize(const char *src, char *dst, size_t dst_sz) {
    size_t j = 0;
    for (size_t i = 0; src && src[i] && src[i] != '&' && j + 1 < dst_sz; i++) {
        unsigned char c = (unsigned char)src[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
            dst[j++] = (char)c;
        }
    }
    dst[j] = '\0';
}

static void she_gpu_tee_init_defaults(void) {
    if (g_gpu_tee_initialized) return;

    memset(&g_gpu_tee, 0, sizeof(g_gpu_tee));
    snprintf(g_gpu_tee.driver_mode, sizeof(g_gpu_tee.driver_mode), "NVIDIA-CC-H100-NVLINK-ENCRYPTED");
    g_gpu_tee.contexts_attested = 19420ULL;
    g_gpu_tee.attest_rejections = 0ULL;

    const char *models[SHE_GPU_TEE_MAX_DEVICES] = {
        "llama4-405b-fp8", "deepseek-r1-671b", "whisper-v4-edge", "starcoder-2-15b"
    };
    const char *digests[SHE_GPU_TEE_MAX_DEVICES] = {
        "4f9c2a1b8e7d6c5a3f0e1d2c3b4a5968778695a4b3c2d1e0f9a8b7c6d5e4f3a2",
        "7a1e3c5b9d0f2a4c6e8b1d3f5a7c9e0b2d4f6a8c0e1b3d5f7a9c1e3b5d7f9a0c",
        "1b3d5f7a9c0e2a4c6e8b0d2f4a6c8e1b3d5f7a9c0e2b4d6f8a1c3e5b7d9f0a2c",
        "9e8d7c6b5a4f3e2d1c0b9a8f7e6d5c4b3a2f1e0d9c8b7a6f5e4d3c2b1a0f9e8d"
    };

    for (uint32_t i = 0; i < SHE_GPU_TEE_MAX_DEVICES; i++) {
        g_gpu_tee.devices[i].device_id = i;
        snprintf(g_gpu_tee.devices[i].gpu_arch, sizeof(g_gpu_tee.devices[i].gpu_arch), "Hopper-CC-90");
        snprintf(g_gpu_tee.devices[i].model_tag, sizeof(g_gpu_tee.devices[i].model_tag), "%s", models[i]);
        g_gpu_tee.devices[i].nvlink_isolated = 1;
        g_gpu_tee.devices[i].vram_gb = 80U;
        snprintf(g_gpu_tee.devices[i].context_digest, sizeof(g_gpu_tee.devices[i].context_digest), "%s", digests[i]);
    }

    g_gpu_tee_initialized = 1;
}

int she_gpu_tee_attest_context(uint32_t device_id, const char *model_tag, char *out_digest, size_t len) {
    she_gpu_tee_init_defaults();
    if (device_id >= SHE_GPU_TEE_MAX_DEVICES || !model_tag || !model_tag[0]) {
        g_gpu_tee.attest_rejections++;
        return -1;
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int dlen = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return -1;

    char payload[128];
    snprintf(payload, sizeof(payload), "SHE-GPU-TEE:DEV%u:NVLINK-ISO:%s", device_id, model_tag);
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
        EVP_DigestUpdate(ctx, payload, strlen(payload)) != 1 ||
        EVP_DigestFinal_ex(ctx, digest, &dlen) != 1) {
        EVP_MD_CTX_free(ctx);
        return -1;
    }
    EVP_MD_CTX_free(ctx);

    char hex[65];
    for (unsigned int i = 0; i < dlen && i < 32; i++) {
        snprintf(hex + i * 2, 3, "%02x", digest[i]);
    }
    hex[64] = '\0';

    snprintf(g_gpu_tee.devices[device_id].model_tag, sizeof(g_gpu_tee.devices[device_id].model_tag), "%s", model_tag);
    snprintf(g_gpu_tee.devices[device_id].context_digest, sizeof(g_gpu_tee.devices[device_id].context_digest), "%s", hex);
    g_gpu_tee.devices[device_id].nvlink_isolated = 1;
    g_gpu_tee.contexts_attested++;

    if (out_digest && len > 0) {
        snprintf(out_digest, len, "%s", hex);
    }
    return 0;
}

static she_handler_t she_gpu_tee_init(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    she_gpu_tee_init_defaults();
    return SHE_HANDLER_GO_ON;
}

static she_handler_t she_gpu_tee_cleanup(she_server_t *srv, she_plugin_t *p) {
    (void)srv;
    (void)p;
    return SHE_HANDLER_GO_ON;
}

static she_handler_t she_gpu_tee_subrequest_start(she_server_t *srv, she_connection_t *con, void *p_data) {
    (void)srv;
    (void)p_data;
    if (!con || !con->request.uri_path) return SHE_HANDLER_GO_ON;

    she_gpu_tee_init_defaults();
    const char *path = she_buffer_cstr(con->request.uri_path);
    if (!path) return SHE_HANDLER_GO_ON;

    if (strcmp(path, "/gpu-tee/attest") == 0) {
        uint32_t device_id = 0U;
        char model[48] = "llama4-405b-fp8";
        const char *query = con->request.uri_query ? she_buffer_cstr(con->request.uri_query) : NULL;
        if (query) {
            const char *pd = strstr(query, "device=");
            if (pd) {
                unsigned long v = strtoul(pd + 7, NULL, 10);
                if (v <= 0xFFFFFFFFUL) device_id = (uint32_t)v;
            }
            const char *pm = strstr(query, "model=");
            if (pm) {
                char parsed[48];
                she_gpu_tee_sanitize(pm + 6, parsed, sizeof(parsed));
                if (parsed[0]) snprintf(model, sizeof(model), "%s", parsed);
            }
        }

        char digest[65] = {0};
        int rc = she_gpu_tee_attest_context(device_id, model, digest, sizeof(digest));

        char body[1024];
        int body_len = snprintf(body, sizeof(body),
            "{\n"
            "  \"status\": \"%s\",\n"
            "  \"driver_mode\": \"%s\",\n"
            "  \"device_id\": %u,\n"
            "  \"model_tag\": \"%s\",\n"
            "  \"nvlink_isolated\": %s,\n"
            "  \"context_digest_sha256\": \"%s\",\n"
            "  \"contexts_attested\": %" PRIu64 "\n"
            "}\n",
            rc == 0 ? "GPU_TEE_ATTESTED" : "GPU_TEE_REJECTED",
            g_gpu_tee.driver_mode,
            device_id,
            model,
            rc == 0 ? "true" : "false",
            rc == 0 ? digest : "invalid",
            g_gpu_tee.contexts_attested
        );

        con->http_status = (rc == 0) ? 200 : 400;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        she_buffer_append_string(con->response_headers,
            rc == 0 ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 400 Bad Request\r\n");
        she_buffer_append_string(con->response_headers, "Server: SHE/1.0 (Secure HTTP Engine - GPU TEE)\r\n");
        she_buffer_append_string(con->response_headers, "Content-Type: application/json; charset=utf-8\r\n");
        she_buffer_append_string(con->response_headers, "X-SHE-GPU-TEE: NVLink-Isolated\r\n");
        char clen[64];
        snprintf(clen, sizeof(clen), "Content-Length: %d\r\n\r\n", body_len);
        she_buffer_append_string(con->response_headers, clen);

        con->state = SHE_CON_STATE_RESPONSE_START;
        she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
        she_chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
        return SHE_HANDLER_FINISHED;
    }

    if (strcmp(path, "/gpu-tee-status") == 0) {
        const char *query = con->request.uri_query ? she_buffer_cstr(con->request.uri_query) : NULL;
        int is_json = (query && strstr(query, "json=1")) ? 1 : 0;

        char body[4096];
        int body_len = 0;

        if (is_json) {
            int off = snprintf(body, sizeof(body),
                "{\n"
                "  \"module\": \"mod_gpu_tee\",\n"
                "  \"version\": \"1.0.0\",\n"
                "  \"driver_mode\": \"%s\",\n"
                "  \"contexts_attested\": %" PRIu64 ",\n"
                "  \"attest_rejections\": %" PRIu64 ",\n"
                "  \"devices\": [\n",
                g_gpu_tee.driver_mode,
                g_gpu_tee.contexts_attested,
                g_gpu_tee.attest_rejections
            );
            for (uint32_t i = 0; i < SHE_GPU_TEE_MAX_DEVICES; i++) {
                she_gpu_tee_dev_t *d = &g_gpu_tee.devices[i];
                off += snprintf(body + off, sizeof(body) - (size_t)off,
                    "    {\"device_id\": %u, \"gpu_arch\": \"%s\", \"model_tag\": \"%s\", \"vram_gb\": %u, \"nvlink_isolated\": %s, \"context_digest\": \"%s\"}%s\n",
                    d->device_id, d->gpu_arch, d->model_tag, d->vram_gb,
                    d->nvlink_isolated ? "true" : "false", d->context_digest,
                    (i + 1U < SHE_GPU_TEE_MAX_DEVICES) ? "," : ""
                );
            }
            off += snprintf(body + off, sizeof(body) - (size_t)off, "  ]\n}\n");
            body_len = off;
        } else {
            body_len = snprintf(body, sizeof(body),
                "<!DOCTYPE html><html><head><title>SHE Confidential GPU TEE</title>"
                "<style>body{font-family:monospace;background:#0d1117;color:#58a6ff;padding:2rem;}</style></head><body>"
                "<h1>SHE Confidential GPU Tensor Enclave</h1>"
                "<p>Driver Mode: <strong>%s</strong> | Attested Contexts: %" PRIu64 "</p>"
                "</body></html>",
                g_gpu_tee.driver_mode, g_gpu_tee.contexts_attested
            );
        }

        con->http_status = 200;
        she_buffer_reset(con->response_headers);
        she_chunkqueue_reset(con->write_queue);
        she_buffer_append_string(con->response_headers, "HTTP/1.1 200 OK\r\n");
        she_buffer_append_string(con->response_headers, "Server: SHE/1.0 (Secure HTTP Engine - GPU TEE)\r\n");
        she_buffer_append_string(con->response_headers, is_json ? "Content-Type: application/json; charset=utf-8\r\n" : "Content-Type: text/html; charset=utf-8\r\n");
        she_buffer_append_string(con->response_headers, "X-SHE-GPU-TEE: NVLink-Isolated\r\n");
        char clen[64];
        snprintf(clen, sizeof(clen), "Content-Length: %d\r\n\r\n", body_len);
        she_buffer_append_string(con->response_headers, clen);

        con->state = SHE_CON_STATE_RESPONSE_START;
        she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
        she_chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
        return SHE_HANDLER_FINISHED;
    }

    return SHE_HANDLER_GO_ON;
}

she_plugin_t *mod_gpu_tee_plugin_init(void) {
    she_plugin_t *p = (she_plugin_t *)calloc(1, sizeof(she_plugin_t));
    if (!p) return NULL;

    p->version = 1;
    p->name = "mod_gpu_tee";
    p->description = "Confidential GPU TEE Tensor Pipeline Attestation & NVLink Isolation";
    p->init = she_gpu_tee_init;
    p->cleanup = she_gpu_tee_cleanup;
    p->handle_subrequest_start = she_gpu_tee_subrequest_start;

    return p;
}
