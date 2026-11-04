#ifndef SHE_BUFFER_H
#define SHE_BUFFER_H

#include <stddef.h>
#include <sys/types.h>

/**
 * SHE (Secure HTTP Engine) Buffer Subsystem.
 * Provides dynamically expanding, memory-safe byte/string buffers
 * with bounded operations to prevent buffer overflow vulnerabilities.
 */
typedef struct {
    char *ptr;
    size_t used; /* Current length including terminating null byte */
    size_t size; /* Allocated capacity in bytes */
} she_buffer_t;

/* Backward-compatible alias */
typedef she_buffer_t buffer_t;

/* Lifecycle */
she_buffer_t *she_buffer_init(void);
she_buffer_t *she_buffer_init_size(size_t initial_size);
void she_buffer_free(she_buffer_t *b);
void she_buffer_reset(she_buffer_t *b);

/* Capacity management */
int she_buffer_prepare_copy(she_buffer_t *b, size_t size);
int she_buffer_prepare_append(she_buffer_t *b, size_t size);

/* Safe string & data manipulation */
int she_buffer_copy_string(she_buffer_t *b, const char *s);
int she_buffer_copy_string_len(she_buffer_t *b, const char *s, size_t len);
int she_buffer_append_string(she_buffer_t *b, const char *s);
int she_buffer_append_string_len(she_buffer_t *b, const char *s, size_t len);
int she_buffer_append_char(she_buffer_t *b, char c);
int she_buffer_append_int(she_buffer_t *b, long long val);
int she_buffer_append_printf(she_buffer_t *b, const char *fmt, ...);

/* Inspection helpers */
int she_buffer_is_empty(const she_buffer_t *b);
const char *she_buffer_cstr(const she_buffer_t *b);
size_t she_buffer_len(const she_buffer_t *b);

/* Compatibility macros */
#define buffer_init she_buffer_init
#define buffer_init_size she_buffer_init_size
#define buffer_free she_buffer_free
#define buffer_reset she_buffer_reset
#define buffer_prepare_copy she_buffer_prepare_copy
#define buffer_prepare_append she_buffer_prepare_append
#define buffer_copy_string she_buffer_copy_string
#define buffer_copy_string_len she_buffer_copy_string_len
#define buffer_append_string she_buffer_append_string
#define buffer_append_string_len she_buffer_append_string_len
#define buffer_append_char she_buffer_append_char
#define buffer_append_int she_buffer_append_int
#define buffer_append_printf she_buffer_append_printf
#define buffer_is_empty she_buffer_is_empty
#define buffer_cstr she_buffer_cstr
#define buffer_len she_buffer_len

#endif /* SHE_BUFFER_H */
