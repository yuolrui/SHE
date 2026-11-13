#ifndef MOD_IO_URING_H
#define MOD_IO_URING_H

#include <stdint.h>
#include "modules/she_plugin.h"

typedef struct {
    char ring_id[48];
    uint32_t sq_entries;
    uint32_t cq_entries;
    uint32_t registered_fixed_buffers;
    int sqpoll_enabled;
    int ioring_send_zc_enabled;
    uint64_t submissions_total;
    uint64_t completions_total;
    uint64_t zero_copy_send_bytes;
    uint64_t syscall_batches_saved;
    uint32_t avg_batch_depth;
} she_uring_ring_t;

int she_fdevent_uring_submit_sendfile(int fd, uint64_t offset, uint32_t len, uint64_t *out_cqe_seq);
she_plugin_t *mod_io_uring_plugin_init(void);

#endif /* MOD_IO_URING_H */
