#include "net/she_net.h"
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <string.h>
#include <stdio.h>

int she_net_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int she_net_set_tcp_nodelay(int fd) {
    int val = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));
}

int she_net_create_server_socket(const char *bind_addr, int port, int backlog) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
#ifdef SO_REUSEPORT
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
#endif
    
    /* Disable Nagle's algorithm for low-latency HTTP transfers */
    she_net_set_tcp_nodelay(fd);
    
    if (she_net_set_nonblocking(fd) != 0) {
        close(fd);
        return -1;
    }
    
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((uint16_t)port);
    if (!bind_addr || strcmp(bind_addr, "0.0.0.0") == 0 || bind_addr[0] == '\0') {
        sin.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, bind_addr, &sin.sin_addr) <= 0) {
            sin.sin_addr.s_addr = htonl(INADDR_ANY);
        }
    }
    
    if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        close(fd);
        return -1;
    }
    
    if (listen(fd, backlog > 0 ? backlog : 1024) != 0) {
        close(fd);
        return -1;
    }
    
    return fd;
}

int she_net_accept_client(int server_fd, struct sockaddr_in *client_addr) {
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int client_fd = accept(server_fd, (struct sockaddr *)client_addr, &addr_len);
    if (client_fd < 0) return -1;
    
    she_net_set_nonblocking(client_fd);
    she_net_set_tcp_nodelay(client_fd);
    
    return client_fd;
}
