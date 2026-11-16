#ifndef MOD_GPU_TEE_H
#define MOD_GPU_TEE_H

#include <stdint.h>
#include <stddef.h>
#include "modules/she_plugin.h"

#define SHE_GPU_TEE_MAX_DEVICES 4

typedef struct {
    uint32_t device_id;
    char gpu_arch[32];
    char model_tag[48];
    int nvlink_isolated;
    uint32_t vram_gb;
    char context_digest[65];
} she_gpu_tee_dev_t;

typedef struct {
    char driver_mode[48];
    uint64_t contexts_attested;
    uint64_t attest_rejections;
    she_gpu_tee_dev_t devices[SHE_GPU_TEE_MAX_DEVICES];
} she_gpu_tee_state_t;

int she_gpu_tee_attest_context(uint32_t device_id, const char *model_tag, char *out_digest, size_t len);
she_plugin_t *mod_gpu_tee_plugin_init(void);

#endif /* MOD_GPU_TEE_H */
