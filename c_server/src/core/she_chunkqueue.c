#include "core/she_chunkqueue.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>

she_chunkqueue_t *she_chunkqueue_init(void) {
    she_chunkqueue_t *cq = (she_chunkqueue_t *)malloc(sizeof(she_chunkqueue_t));
    if (!cq) return NULL;
    cq->first = NULL;
    cq->last = NULL;
    cq->bytes_in = 0;
    cq->bytes_out = 0;
    return cq;
}

static void she_chunk_free_item(she_chunk_t *c) {
    if (!c) return;
    if (c->type == SHE_CHUNK_TYPE_MEM && c->mem) {
        she_buffer_free(c->mem);
        c->mem = NULL;
    } else if (c->type == SHE_CHUNK_TYPE_FILE && c->file.fd >= 0) {
        close(c->file.fd);
        c->file.fd = -1;
    }
    free(c);
}

void she_chunkqueue_reset(she_chunkqueue_t *cq) {
    if (!cq) return;
    she_chunk_t *curr = cq->first;
    while (curr) {
        she_chunk_t *next = curr->next;
        she_chunk_free_item(curr);
        curr = next;
    }
    cq->first = NULL;
    cq->last = NULL;
    cq->bytes_in = 0;
    cq->bytes_out = 0;
}

void she_chunkqueue_free(she_chunkqueue_t *cq) {
    if (!cq) return;
    she_chunkqueue_reset(cq);
    free(cq);
}

static void she_chunkqueue_append_chunk(she_chunkqueue_t *cq, she_chunk_t *c) {
    c->next = NULL;
    if (!cq->first) {
        cq->first = c;
        cq->last = c;
    } else {
        cq->last->next = c;
        cq->last = c;
    }
}

int she_chunkqueue_append_mem(she_chunkqueue_t *cq, const char *data, size_t len) {
    if (!cq || !data || len == 0) return 0;
    
    she_chunk_t *c = (she_chunk_t *)malloc(sizeof(she_chunk_t));
    if (!c) return -1;
    
    c->type = SHE_CHUNK_TYPE_MEM;
    c->mem = she_buffer_init_size(len + 1);
    if (!c->mem) {
        free(c);
        return -1;
    }
    she_buffer_copy_string_len(c->mem, data, len);
    c->offset = 0;
    c->next = NULL;
    
    cq->bytes_in += len;
    she_chunkqueue_append_chunk(cq, c);
    return 0;
}

int she_chunkqueue_append_buffer(she_chunkqueue_t *cq, she_buffer_t *b) {
    if (!cq || !b || she_buffer_is_empty(b)) return 0;
    return she_chunkqueue_append_mem(cq, she_buffer_cstr(b), she_buffer_len(b));
}

int she_chunkqueue_append_file(she_chunkqueue_t *cq, int fd, off_t offset, off_t length) {
    if (!cq || fd < 0 || length <= 0) return -1;
    
    she_chunk_t *c = (she_chunk_t *)malloc(sizeof(she_chunk_t));
    if (!c) return -1;
    
    c->type = SHE_CHUNK_TYPE_FILE;
    c->mem = NULL;
    c->file.fd = fd;
    c->file.start_offset = offset;
    c->file.length = length;
    c->offset = 0;
    c->next = NULL;
    
    cq->bytes_in += length;
    she_chunkqueue_append_chunk(cq, c);
    return 0;
}

int she_chunkqueue_is_empty(const she_chunkqueue_t *cq) {
    return (!cq || cq->first == NULL);
}

off_t she_chunkqueue_length(const she_chunkqueue_t *cq) {
    if (!cq) return 0;
    return cq->bytes_in - cq->bytes_out;
}

ssize_t she_chunkqueue_write_chunk_to_socket(she_chunkqueue_t *cq, int socket_fd) {
    if (!cq || !cq->first) return 0;
    
    ssize_t total_written = 0;
    
    while (cq->first) {
        she_chunk_t *c = cq->first;
        
        if (c->type == SHE_CHUNK_TYPE_MEM) {
            size_t chunk_len = she_buffer_len(c->mem);
            size_t remaining = chunk_len - (size_t)c->offset;
            const char *src = she_buffer_cstr(c->mem) + c->offset;
            
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
                she_chunk_free_item(c);
            } else {
                /* Partial write */
                return total_written;
            }
        } else if (c->type == SHE_CHUNK_TYPE_FILE) {
            off_t remaining = c->file.length - c->offset;
            off_t file_cur_offset = c->file.start_offset + c->offset;
            
            /* Linux zero-copy sendfile(2) */
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
                she_chunk_free_item(c);
            } else {
                return total_written;
            }
        }
    }
    
    return total_written;
}
