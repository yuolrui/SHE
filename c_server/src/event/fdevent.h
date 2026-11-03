#ifndef FDEVENT_H
#define FDEVENT_H

#include <stdint.h>
#include <sys/epoll.h>

#define FDEVENT_IN   (EPOLLIN)
#define FDEVENT_OUT  (EPOLLOUT)
#define FDEVENT_PRI  (EPOLLPRI)
#define FDEVENT_ERR  (EPOLLERR)
#define FDEVENT_HUP  (EPOLLHUP | EPOLLRDHUP)

typedef struct fdevent_system fdevent_system_t;

typedef void (*fdevent_handler)(void *context, int revents);

typedef struct {
    int fd;
    int events;
    fdevent_handler handler;
    void *context;
} fd_entry_t;

struct fdevent_system {
    int epoll_fd;
    int max_fds;
    fd_entry_t *fdarray;
    struct epoll_event *events;
    int max_events;
};

fdevent_system_t *fdevent_init(int max_fds);
void fdevent_free(fdevent_system_t *ev);

int fdevent_register(fdevent_system_t *ev, int fd, fdevent_handler handler, void *context);
int fdevent_unregister(fdevent_system_t *ev, int fd);
int fdevent_event_set(fdevent_system_t *ev, int fd, int events);
int fdevent_event_add(fdevent_system_t *ev, int fd, int events);
int fdevent_event_del(fdevent_system_t *ev, int fd, int events);

int fdevent_poll(fdevent_system_t *ev, int timeout_ms);

#endif /* FDEVENT_H */
