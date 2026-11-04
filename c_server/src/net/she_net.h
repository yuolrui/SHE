#ifndef SHE_NET_H
#define SHE_NET_H

#include <sys/socket.h>
#include <netinet/in.h>

int she_net_set_nonblocking(int fd);
int she_net_set_tcp_nodelay(int fd);
int she_net_create_server_socket(const char *bind_addr, int port, int backlog);
int she_net_accept_client(int server_fd, struct sockaddr_in *client_addr);

/* Compatibility macros */
#define network_set_nonblocking she_net_set_nonblocking
#define network_create_server_socket she_net_create_server_socket
#define network_accept_client she_net_accept_client

#endif /* SHE_NET_H */
