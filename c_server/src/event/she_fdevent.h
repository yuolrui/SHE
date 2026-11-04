#ifndef SHE_FDEVENT_H
#define SHE_FDEVENT_H

#include <stdint.h>
#include <sys/epoll.h>

#define SHE_FDEVENT_IN   (EPOLLIN)
#define SHE_FDEVENT_OUT  (EPOLLOUT)
#define SHE_FDEVENT_PRI  (EPOLLPRI)
#define SHE_FDEVENT_ERR  (EPOLLERR)
#define SHE_FDEVENT_HUP  (EPOLLHUP | EPOLLRDHUP)

/* Compatibility constants */
#define FDEVENT_IN   SHE_FDEVENT_IN
#define FDEVENT_OUT  SHE_FDEVENT_OUT
#define FDEVENT_PRI  SHE_FDEVENT_PRI
#define FDEVENT_ERR  SHE_FDEVENT_ERR
#define FDEVENT_HUP  SHE_FDEVENT_HUP

typedef struct she_fdevent_system she_fdevent_system_t;
typedef void (*she_fdevent_handler)(void *context, int revents);

typedef struct {
    int fd;
    int events;
    she_fdevent_handler handler;
    void *context;
} she_fd_entry_t;

struct she_fdevent_system {
    int epoll_fd;
    int max_fds;
    she_fd_entry_t *fdarray;
    struct epoll_event *events;
    int max_events;
};

/* Compatibility typedefs */
typedef she_fdevent_system_t fdevent_system_t;
typedef she_fdevent_handler fdevent_handler;
typedef she_fd_entry_t fd_entry_t;

she_fdevent_system_t *she_fdevent_init(int max_fds);
void she_fdevent_free(she_fdevent_system_t *ev);

int she_fdevent_register(she_fdevent_system_t *ev, int fd, she_fdevent_handler handler, void *context);
int she_fdevent_unregister(she_fdevent_system_t *ev, int fd);
int she_fdevent_event_set(she_fdevent_system_t *ev, int fd, int events);
int she_fdevent_event_add(she_fdevent_system_t *ev, int fd, int events);
int she_fdevent_event_del(she_fdevent_system_t *ev, int fd, int events);

int she_fdevent_poll(she_fdevent_system_t *ev, int timeout_ms);

/* Compatibility macros */
#define fdevent_init she_fdevent_init
#define fdevent_free she_fdevent_free
#define fdevent_register she_fdevent_register
#define fdevent_unregister she_fdevent_unregister
#define fdevent_event_set she_fdevent_event_set
#define fdevent_event_add she_fdevent_event_add
#define fdevent_event_del she_fdevent_event_del
#define fdevent_poll she_fdevent_poll

#endif /* SHE_FDEVENT_H */
