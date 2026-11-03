#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>
#include <sys/types.h>

/**
 * buffer_t is modeled directly after Lighttpd's iconic memory buffer.
 * It provides a dynamically sized character buffer with tracked capacity
 * and length, avoiding repeated allocations and buffer overruns.
 */
typedef struct {
    char *ptr;
    size_t used; /* Current length including terminating null byte */
    size_t size; /* Allocated capacity in bytes */
} buffer_t;

/* Lifecycle */
buffer_t *buffer_init(void);
buffer_t *buffer_init_size(size_t initial_size);
void buffer_free(buffer_t *b);
void buffer_reset(buffer_t *b);

/* Capacity management */
int buffer_prepare_copy(buffer_t *b, size_t size);
int buffer_prepare_append(buffer_t *b, size_t size);

/* String operations */
int buffer_copy_string(buffer_t *b, const char *s);
int buffer_copy_string_len(buffer_t *b, const char *s, size_t len);
int buffer_append_string(buffer_t *b, const char *s);
int buffer_append_string_len(buffer_t *b, const char *s, size_t len);
int buffer_append_char(buffer_t *b, char c);
int buffer_append_int(buffer_t *b, long long val);
int buffer_append_printf(buffer_t *b, const char *fmt, ...);

/* Helpers */
int buffer_is_empty(const buffer_t *b);
const char *buffer_cstr(const buffer_t *b);
size_t buffer_len(const buffer_t *b);

#endif /* BUFFER_H */
