c-lighttpd: A high-performance Web Server written in pure C
Inspired by the clean architecture of lighttpd.

Key Components:
1. buffer.c: Dynamically resized memory buffers
2. chunkqueue.c: Zero-copy memory and file chunk queue using sendfile()
3. fdevent.c: Linux epoll abstraction layer
4. network.c: Non-blocking socket configuration with TCP_NODELAY
5. http_parser.c: HTTP/1.1 request line and header parser with Range support
6. connection.c: The 10-state Connection State Machine
7. server.c: Epoll event loop and connection management
