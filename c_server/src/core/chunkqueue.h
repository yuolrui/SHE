#ifndef CHUNKQUEUE_H
#define CHUNKQUEUE_H

#include <stddef.h>
#include <sys/types.h>
#include "buffer.h"

typedef enum {
    CHUNK_TYPE_MEM,
    CHUNK_TYPE_FILE
} chunk_type_t;

typedef struct chunk {
    chunk_type_t type;
    
    /* Memory chunk data */
    buffer_t *mem;
    
    /* File chunk data */
    struct {
        int fd;
        off_t start_offset;
        off_t length;
    } file;
    
    /* Bytes already written from this chunk */
    off_t offset;
    
    struct chunk *next;
} chunk_t;

typedef struct {
    chunk_t *first;
    chunk_t *last;
    
    off_t bytes_in;
    off_t bytes_out;
} chunkqueue_t;

chunkqueue_t *chunkqueue_init(void);
void chunkqueue_free(chunkqueue_t *cq);
void chunkqueue_reset(chunkqueue_t *cq);

int chunkqueue_append_mem(chunkqueue_t *cq, const char *data, size_t len);
int chunkqueue_append_buffer(chunkqueue_t *cq, buffer_t *b);
int chunkqueue_append_file(chunkqueue_t *cq, int fd, off_t offset, off_t length);

int chunkqueue_is_empty(const chunkqueue_t *cq);
off_t chunkqueue_length(const chunkqueue_t *cq);

/* Writes as many bytes as possible non-blockingly to socket fd.
 * Returns:
 *   > 0 : bytes written
 *   = 0 : write queue completed
 *   -1  : EAGAIN / EWOULDBLOCK (socket full, wait for EPOLLOUT)
 *   -2  : fatal socket error or client disconnect
 */
ssize_t chunkqueue_write_chunk_to_socket(chunkqueue_t *cq, int socket_fd);

#endif /* CHUNKQUEUE_H */
