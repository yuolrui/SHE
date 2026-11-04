#include "core/she_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define SHE_BUFFER_DEFAULT_SIZE 128

she_buffer_t *she_buffer_init(void) {
    return she_buffer_init_size(SHE_BUFFER_DEFAULT_SIZE);
}

she_buffer_t *she_buffer_init_size(size_t initial_size) {
    she_buffer_t *b = (she_buffer_t *)malloc(sizeof(she_buffer_t));
    if (!b) return NULL;
    
    if (initial_size == 0) initial_size = SHE_BUFFER_DEFAULT_SIZE;
    
    b->ptr = (char *)malloc(initial_size);
    if (!b->ptr) {
        free(b);
        return NULL;
    }
    b->ptr[0] = '\0';
    b->used = 1;
    b->size = initial_size;
    return b;
}

void she_buffer_free(she_buffer_t *b) {
    if (!b) return;
    if (b->ptr) {
        free(b->ptr);
        b->ptr = NULL;
    }
    free(b);
}

void she_buffer_reset(she_buffer_t *b) {
    if (!b) return;
    if (b->ptr && b->size > 0) {
        b->ptr[0] = '\0';
    }
    b->used = 1;
}

int she_buffer_prepare_copy(she_buffer_t *b, size_t size) {
    if (!b) return -1;
    if (size > b->size) {
        size_t new_size = b->size ? b->size : SHE_BUFFER_DEFAULT_SIZE;
        while (new_size < size) {
            new_size *= 2;
        }
        char *new_ptr = (char *)realloc(b->ptr, new_size);
        if (!new_ptr) return -1;
        b->ptr = new_ptr;
        b->size = new_size;
    }
    return 0;
}

int she_buffer_prepare_append(she_buffer_t *b, size_t size) {
    if (!b) return -1;
    size_t req = b->used + size;
    return she_buffer_prepare_copy(b, req);
}

int she_buffer_copy_string_len(she_buffer_t *b, const char *s, size_t len) {
    if (!b || !s) return -1;
    if (she_buffer_prepare_copy(b, len + 1) != 0) return -1;
    memcpy(b->ptr, s, len);
    b->ptr[len] = '\0';
    b->used = len + 1;
    return 0;
}

int she_buffer_copy_string(she_buffer_t *b, const char *s) {
    if (!s) return she_buffer_copy_string_len(b, "", 0);
    return she_buffer_copy_string_len(b, s, strlen(s));
}

int she_buffer_append_string_len(she_buffer_t *b, const char *s, size_t len) {
    if (!b || !s || len == 0) return 0;
    if (she_buffer_prepare_append(b, len) != 0) return -1;
    
    size_t offset = (b->used > 0) ? b->used - 1 : 0;
    memcpy(b->ptr + offset, s, len);
    b->used = offset + len + 1;
    b->ptr[b->used - 1] = '\0';
    return 0;
}

int she_buffer_append_string(she_buffer_t *b, const char *s) {
    if (!s) return 0;
    return she_buffer_append_string_len(b, s, strlen(s));
}

int she_buffer_append_char(she_buffer_t *b, char c) {
    char s[2] = {c, '\0'};
    return she_buffer_append_string_len(b, s, 1);
}

int she_buffer_append_int(she_buffer_t *b, long long val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%lld", val);
    if (len < 0) return -1;
    return she_buffer_append_string_len(b, buf, (size_t)len);
}

int she_buffer_append_printf(she_buffer_t *b, const char *fmt, ...) {
    if (!b || !fmt) return -1;
    va_list ap;
    va_start(ap, fmt);
    va_list ap_copy;
    va_copy(ap_copy, ap);
    
    int len = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);
    
    if (len < 0) {
        va_end(ap);
        return -1;
    }
    
    if (she_buffer_prepare_append(b, (size_t)len) != 0) {
        va_end(ap);
        return -1;
    }
    
    size_t offset = (b->used > 0) ? b->used - 1 : 0;
    vsnprintf(b->ptr + offset, b->size - offset, fmt, ap);
    va_end(ap);
    
    b->used = offset + (size_t)len + 1;
    return 0;
}

int she_buffer_is_empty(const she_buffer_t *b) {
    return (!b || b->used <= 1);
}

const char *she_buffer_cstr(const she_buffer_t *b) {
    if (!b || !b->ptr) return "";
    return b->ptr;
}

size_t she_buffer_len(const she_buffer_t *b) {
    if (!b || b->used <= 1) return 0;
    return b->used - 1;
}
