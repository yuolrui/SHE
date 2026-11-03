#include "network.h"
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <string.h>
#include <stdio.h>

int network_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int network_create_server_socket(const char *bind_addr, int port, int backlog) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
#ifdef SO_REUSEPORT
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
#endif
    
    /* Disable Nagle's algorithm for low-latency HTTP transfers */
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));
    
    if (network_set_nonblocking(fd) != 0) {
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

int network_accept_client(int server_fd, struct sockaddr_in *client_addr) {
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int client_fd = accept(server_fd, (struct sockaddr *)client_addr, &addr_len);
    if (client_fd < 0) return -1;
    
    network_set_nonblocking(client_fd);
    
    int val = 1;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));
    
    return client_fd;
}
