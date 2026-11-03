#include "chunkqueue.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>

chunkqueue_t *chunkqueue_init(void) {
    chunkqueue_t *cq = (chunkqueue_t *)malloc(sizeof(chunkqueue_t));
    if (!cq) return NULL;
    cq->first = NULL;
    cq->last = NULL;
    cq->bytes_in = 0;
    cq->bytes_out = 0;
    return cq;
}

static void chunk_free(chunk_t *c) {
    if (!c) return;
    if (c->type == CHUNK_TYPE_MEM && c->mem) {
        buffer_free(c->mem);
        c->mem = NULL;
    } else if (c->type == CHUNK_TYPE_FILE && c->file.fd >= 0) {
        close(c->file.fd);
        c->file.fd = -1;
    }
    free(c);
}

void chunkqueue_reset(chunkqueue_t *cq) {
    if (!cq) return;
    chunk_t *curr = cq->first;
    while (curr) {
        chunk_t *next = curr->next;
        chunk_free(curr);
        curr = next;
    }
    cq->first = NULL;
    cq->last = NULL;
    cq->bytes_in = 0;
    cq->bytes_out = 0;
}

void chunkqueue_free(chunkqueue_t *cq) {
    if (!cq) return;
    chunkqueue_reset(cq);
    free(cq);
}

static void chunkqueue_append_chunk(chunkqueue_t *cq, chunk_t *c) {
    c->next = NULL;
    if (!cq->first) {
        cq->first = c;
        cq->last = c;
    } else {
        cq->last->next = c;
        cq->last = c;
    }
}

int chunkqueue_append_mem(chunkqueue_t *cq, const char *data, size_t len) {
    if (!cq || !data || len == 0) return 0;
    
    chunk_t *c = (chunk_t *)malloc(sizeof(chunk_t));
    if (!c) return -1;
    
    c->type = CHUNK_TYPE_MEM;
    c->mem = buffer_init_size(len + 1);
    if (!c->mem) {
        free(c);
        return -1;
    }
    buffer_copy_string_len(c->mem, data, len);
    c->offset = 0;
    c->next = NULL;
    
    cq->bytes_in += len;
    chunkqueue_append_chunk(cq, c);
    return 0;
}

int chunkqueue_append_buffer(chunkqueue_t *cq, buffer_t *b) {
    if (!cq || !b || buffer_is_empty(b)) return 0;
    return chunkqueue_append_mem(cq, buffer_cstr(b), buffer_len(b));
}

int chunkqueue_append_file(chunkqueue_t *cq, int fd, off_t offset, off_t length) {
    if (!cq || fd < 0 || length <= 0) return -1;
    
    chunk_t *c = (chunk_t *)malloc(sizeof(chunk_t));
    if (!c) return -1;
    
    c->type = CHUNK_TYPE_FILE;
    c->mem = NULL;
    c->file.fd = fd;
    c->file.start_offset = offset;
    c->file.length = length;
    c->offset = 0;
    c->next = NULL;
    
    cq->bytes_in += length;
    chunkqueue_append_chunk(cq, c);
    return 0;
}

int chunkqueue_is_empty(const chunkqueue_t *cq) {
    return (!cq || cq->first == NULL);
}

off_t chunkqueue_length(const chunkqueue_t *cq) {
    if (!cq) return 0;
    return cq->bytes_in - cq->bytes_out;
}

ssize_t chunkqueue_write_chunk_to_socket(chunkqueue_t *cq, int socket_fd) {
    if (!cq || !cq->first) return 0;
    
    ssize_t total_written = 0;
    
    while (cq->first) {
        chunk_t *c = cq->first;
        
        if (c->type == CHUNK_TYPE_MEM) {
            size_t chunk_len = buffer_len(c->mem);
            size_t remaining = chunk_len - (size_t)c->offset;
            const char *src = buffer_cstr(c->mem) + c->offset;
            
            ssize_t written = send(socket_fd, src, remaining, MSG_NOSIGNAL);
            if (written < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return (total_written > 0) ? total_written : -1;
                }
                return -2; /* Fatal write error */
            }
            
            c->offset += written;
            cq->bytes_out += written;
            total_written += written;
            
            if (c->offset >= (off_t)chunk_len) {
                /* Finished this mem chunk */
                cq->first = c->next;
                if (!cq->first) cq->last = NULL;
                chunk_free(c);
            } else {
                /* Partial write */
                return total_written;
            }
        } else if (c->type == CHUNK_TYPE_FILE) {
            off_t remaining = c->file.length - c->offset;
            off_t file_cur_offset = c->file.start_offset + c->offset;
            
            /* Use Linux sendfile for zero-copy kernel transfer */
            ssize_t written = sendfile(socket_fd, c->file.fd, &file_cur_offset, remaining);
            if (written < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return (total_written > 0) ? total_written : -1;
                }
                return -2;
            }
            
            c->offset += written;
            cq->bytes_out += written;
            total_written += written;
            
            if (c->offset >= c->file.length) {
                /* Finished file chunk */
                cq->first = c->next;
                if (!cq->first) cq->last = NULL;
                chunk_free(c);
            } else {
                return total_written;
            }
        }
    }
    
    return total_written;
}
