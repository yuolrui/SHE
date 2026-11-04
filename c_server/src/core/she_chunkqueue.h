#ifndef SHE_CHUNKQUEUE_H
#define SHE_CHUNKQUEUE_H

#include <stddef.h>
#include <sys/types.h>
#include "core/she_buffer.h"

typedef enum {
    SHE_CHUNK_TYPE_MEM,
    SHE_CHUNK_TYPE_FILE
} she_chunk_type_t;

typedef struct she_chunk {
    she_chunk_type_t type;
    
    /* Memory chunk data */
    she_buffer_t *mem;
    
    /* File chunk data (zero-copy sendfile) */
    struct {
        int fd;
        off_t start_offset;
        off_t length;
    } file;
    
    /* Bytes already written from this chunk */
    off_t offset;
    
    struct she_chunk *next;
} she_chunk_t;

typedef struct {
    she_chunk_t *first;
    she_chunk_t *last;
    
    off_t bytes_in;
    off_t bytes_out;
} she_chunkqueue_t;

/* Backward compatibility typedefs */
typedef she_chunk_type_t chunk_type_t;
typedef she_chunk_t chunk_t;
typedef she_chunkqueue_t chunkqueue_t;

#define CHUNK_TYPE_MEM  SHE_CHUNK_TYPE_MEM
#define CHUNK_TYPE_FILE SHE_CHUNK_TYPE_FILE

she_chunkqueue_t *she_chunkqueue_init(void);
void she_chunkqueue_free(she_chunkqueue_t *cq);
void she_chunkqueue_reset(she_chunkqueue_t *cq);

int she_chunkqueue_append_mem(she_chunkqueue_t *cq, const char *data, size_t len);
int she_chunkqueue_append_buffer(she_chunkqueue_t *cq, she_buffer_t *b);
int she_chunkqueue_append_file(she_chunkqueue_t *cq, int fd, off_t offset, off_t length);

int she_chunkqueue_is_empty(const she_chunkqueue_t *cq);
off_t she_chunkqueue_length(const she_chunkqueue_t *cq);

ssize_t she_chunkqueue_write_chunk_to_socket(she_chunkqueue_t *cq, int socket_fd);

/* Compatibility macros */
#define chunkqueue_init she_chunkqueue_init
#define chunkqueue_free she_chunkqueue_free
#define chunkqueue_reset she_chunkqueue_reset
#define chunkqueue_append_mem she_chunkqueue_append_mem
#define chunkqueue_append_buffer she_chunkqueue_append_buffer
#define chunkqueue_append_file she_chunkqueue_append_file
#define chunkqueue_is_empty she_chunkqueue_is_empty
#define chunkqueue_length she_chunkqueue_length
#define chunkqueue_write_chunk_to_socket she_chunkqueue_write_chunk_to_socket

#endif /* SHE_CHUNKQUEUE_H */
