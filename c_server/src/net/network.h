#ifndef NETWORK_H
#define NETWORK_H

#include <sys/socket.h>
#include <netinet/in.h>

int network_set_nonblocking(int fd);
int network_create_server_socket(const char *bind_addr, int port, int backlog);
int network_accept_client(int server_fd, struct sockaddr_in *client_addr);

#endif /* NETWORK_H */
