#ifndef SHE_MOD_WASM_H
#define SHE_MOD_WASM_H

#include "modules/she_plugin.h"
#include <stdint.h>
#include <stddef.h>

/**
 * WebAssembly Binary Format (WASM 1.0) Constants
 */
#define SHE_WASM_MAGIC_0 0x00
#define SHE_WASM_MAGIC_1 0x61
#define SHE_WASM_MAGIC_2 0x73
#define SHE_WASM_MAGIC_3 0x6D
#define SHE_WASM_VERSION 0x01

/* WASM Section IDs */
typedef enum {
    SHE_WASM_SEC_CUSTOM   = 0,
    SHE_WASM_SEC_TYPE     = 1,
    SHE_WASM_SEC_IMPORT   = 2,
    SHE_WASM_SEC_FUNCTION = 3,
    SHE_WASM_SEC_TABLE    = 4,
    SHE_WASM_SEC_MEMORY   = 5,
    SHE_WASM_SEC_GLOBAL   = 6,
    SHE_WASM_SEC_EXPORT   = 7,
    SHE_WASM_SEC_START    = 8,
    SHE_WASM_SEC_ELEMENT  = 9,
    SHE_WASM_SEC_CODE     = 10,
    SHE_WASM_SEC_DATA     = 11
} she_wasm_section_id_t;

typedef struct {
    char name[64];
    uint32_t func_index;
} she_wasm_export_t;

typedef struct {
    uint8_t magic[4];
    uint32_t version;
    size_t bytecode_size;
    uint32_t memory_pages;     /* 64KB per page */
    uint32_t function_count;
    she_wasm_export_t exports[8];
    size_t export_count;
    uint8_t is_valid;
} she_wasm_module_t;

typedef struct {
    uint64_t total_invocations;
    uint64_t total_exec_time_us;
    uint64_t memory_allocated_bytes;
    uint32_t active_instances;
} she_wasm_metrics_t;

/* Plugin initialization */
she_plugin_t *mod_wasm_plugin_init(void);

/* WASM Verification & Execution APIs */
int she_wasm_validate_binary(const uint8_t *bytecode, size_t len, she_wasm_module_t *out_mod);
int she_wasm_execute_function(const char *func_name, const char *arg, char *out_buf, size_t out_max, uint64_t *out_duration_us);
const she_wasm_metrics_t *she_wasm_get_metrics(void);

#endif /* SHE_MOD_WASM_H */
