#include "fdevent.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define DEFAULT_MAX_EVENTS 256

fdevent_system_t *fdevent_init(int max_fds) {
    if (max_fds <= 0) max_fds = 1024;
    
    fdevent_system_t *ev = (fdevent_system_t *)malloc(sizeof(fdevent_system_t));
    if (!ev) return NULL;
    
    ev->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (ev->epoll_fd < 0) {
        /* Fallback if EPOLL_CLOEXEC fails on old kernels */
        ev->epoll_fd = epoll_create(max_fds);
        if (ev->epoll_fd < 0) {
            free(ev);
            return NULL;
        }
    }
    
    ev->max_fds = max_fds;
    ev->fdarray = (fd_entry_t *)calloc(max_fds, sizeof(fd_entry_t));
    if (!ev->fdarray) {
        close(ev->epoll_fd);
        free(ev);
        return NULL;
    }
    
    for (int i = 0; i < max_fds; i++) {
        ev->fdarray[i].fd = -1;
    }
    
    ev->max_events = DEFAULT_MAX_EVENTS;
    ev->events = (struct epoll_event *)calloc(ev->max_events, sizeof(struct epoll_event));
    if (!ev->events) {
        free(ev->fdarray);
        close(ev->epoll_fd);
        free(ev);
        return NULL;
    }
    
    return ev;
}

void fdevent_free(fdevent_system_t *ev) {
    if (!ev) return;
    if (ev->epoll_fd >= 0) {
        close(ev->epoll_fd);
        ev->epoll_fd = -1;
    }
    if (ev->fdarray) free(ev->fdarray);
    if (ev->events) free(ev->events);
    free(ev);
}

int fdevent_register(fdevent_system_t *ev, int fd, fdevent_handler handler, void *context) {
    if (!ev || fd < 0 || fd >= ev->max_fds) return -1;
    
    ev->fdarray[fd].fd = fd;
    ev->fdarray[fd].events = 0;
    ev->fdarray[fd].handler = handler;
    ev->fdarray[fd].context = context;
    
    struct epoll_event ep;
    memset(&ep, 0, sizeof(ep));
    ep.events = 0;
    ep.data.fd = fd;
    
    if (epoll_ctl(ev->epoll_fd, EPOLL_CTL_ADD, fd, &ep) != 0) {
        if (errno == EEXIST) {
            return epoll_ctl(ev->epoll_fd, EPOLL_CTL_MOD, fd, &ep);
        }
        return -1;
    }
    return 0;
}

int fdevent_unregister(fdevent_system_t *ev, int fd) {
    if (!ev || fd < 0 || fd >= ev->max_fds) return -1;
    
    epoll_ctl(ev->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    ev->fdarray[fd].fd = -1;
    ev->fdarray[fd].events = 0;
    ev->fdarray[fd].handler = NULL;
    ev->fdarray[fd].context = NULL;
    return 0;
}

int fdevent_event_set(fdevent_system_t *ev, int fd, int events) {
    if (!ev || fd < 0 || fd >= ev->max_fds) return -1;
    
    struct epoll_event ep;
    memset(&ep, 0, sizeof(ep));
    ep.events = events;
    ep.data.fd = fd;
    
    if (epoll_ctl(ev->epoll_fd, EPOLL_CTL_MOD, fd, &ep) != 0) {
        return -1;
    }
    ev->fdarray[fd].events = events;
    return 0;
}

int fdevent_event_add(fdevent_system_t *ev, int fd, int events) {
    if (!ev || fd < 0 || fd >= ev->max_fds) return -1;
    int cur = ev->fdarray[fd].events;
    return fdevent_event_set(ev, fd, cur | events);
}

int fdevent_event_del(fdevent_system_t *ev, int fd, int events) {
    if (!ev || fd < 0 || fd >= ev->max_fds) return -1;
    int cur = ev->fdarray[fd].events;
    return fdevent_event_set(ev, fd, cur & ~events);
}

int fdevent_poll(fdevent_system_t *ev, int timeout_ms) {
    if (!ev || ev->epoll_fd < 0) return -1;
    
    int nfds = epoll_wait(ev->epoll_fd, ev->events, ev->max_events, timeout_ms);
    if (nfds < 0) {
        if (errno == EINTR) return 0;
        return -1;
    }
    
    for (int i = 0; i < nfds; i++) {
        int fd = ev->events[i].data.fd;
        uint32_t revents = ev->events[i].events;
        
        if (fd >= 0 && fd < ev->max_fds && ev->fdarray[fd].handler) {
            ev->fdarray[fd].handler(ev->fdarray[fd].context, (int)revents);
        }
    }
    return nfds;
}
