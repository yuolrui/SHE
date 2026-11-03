#include "connection.h"
#include "server.h"
#include "mime_types.h"
#include "network.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#define READ_CHUNK_SIZE 4096

connection_t *connection_init(int client_fd, struct sockaddr_in *client_addr) {
    connection_t *con = (connection_t *)malloc(sizeof(connection_t));
    if (!con) return NULL;
    
    con->fd = client_fd;
    if (client_addr) {
        con->client_addr = *client_addr;
        inet_ntop(AF_INET, &client_addr->sin_addr, con->client_ip, sizeof(con->client_ip));
        con->client_port = ntohs(client_addr->sin_port);
    } else {
        memset(&con->client_addr, 0, sizeof(con->client_addr));
        strcpy(con->client_ip, "127.0.0.1");
        con->client_port = 0;
    }
    
    con->state = CON_STATE_CONNECT;
    con->state_timestamp = time(NULL);
    con->read_buf = buffer_init_size(4096);
    http_request_init(&con->request);
    
    con->http_status = 0;
    con->response_headers = buffer_init_size(1024);
    con->write_queue = chunkqueue_init();
    
    con->keep_alive = 1;
    con->keep_alive_max_requests = 100;
    con->request_count = 0;
    con->bytes_sent = 0;
    
    con->next = NULL;
    con->prev = NULL;
    
    return con;
}

void connection_reset(connection_t *con) {
    if (!con) return;
    buffer_reset(con->read_buf);
    http_request_reset(&con->request);
    con->http_status = 0;
    buffer_reset(con->response_headers);
    chunkqueue_reset(con->write_queue);
    con->bytes_sent = 0;
}

void connection_free(connection_t *con) {
    if (!con) return;
    if (con->read_buf) buffer_free(con->read_buf);
    http_request_free(&con->request);
    if (con->response_headers) buffer_free(con->response_headers);
    if (con->write_queue) chunkqueue_free(con->write_queue);
    free(con);
}

const char *connection_state_to_str(connection_state_t state) {
    switch (state) {
        case CON_STATE_CONNECT:         return "CONNECT";
        case CON_STATE_REQUEST_START:   return "REQUEST_START";
        case CON_STATE_READ:            return "READ";
        case CON_STATE_REQUEST_END:     return "REQUEST_END";
        case CON_STATE_HANDLE_REQUEST:  return "HANDLE_REQUEST";
        case CON_STATE_RESPONSE_START:  return "RESPONSE_START";
        case CON_STATE_WRITE:           return "WRITE";
        case CON_STATE_RESPONSE_END:    return "RESPONSE_END";
        case CON_STATE_ERROR:           return "ERROR";
        case CON_STATE_CLOSE:           return "CLOSE";
        default:                        return "UNKNOWN";
    }
}

static const char *http_status_reason(int status) {
    switch (status) {
        case 200: return "OK";
        case 206: return "Partial Content";
        case 301: return "Moved Permanently";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 414: return "URI Too Long";
        case 416: return "Range Not Satisfiable";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown";
    }
}

static void update_stats_status(server_t *srv, int status) {
    if (!srv) return;
    switch (status) {
        case 200: srv->stats.status_200++; break;
        case 206: srv->stats.status_206++; break;
        case 304: srv->stats.status_304++; break;
        case 400: srv->stats.status_400++; break;
        case 403: srv->stats.status_403++; break;
        case 404: srv->stats.status_404++; break;
        case 500: srv->stats.status_500++; break;
        default: break;
    }
}

/* Format RFC 1123 HTTP date: "Sun, 06 Nov 1994 08:49:37 GMT" */
static void get_http_date(time_t t, char *buf, size_t max) {
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(buf, max, "%a, %d %b %Y %H:%M:%S GMT", &tm);
}

/* Parse HTTP date from If-Modified-Since header */
static time_t parse_http_date(const char *str) {
    if (!str || !*str) return 0;
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    if (strptime(str, "%a, %d %b %Y %H:%M:%S GMT", &tm) != NULL) {
        return timegm(&tm);
    }
    return 0;
}

static void generate_error_response(connection_t *con, int status, const char *server_tag) {
    con->http_status = status;
    buffer_reset(con->response_headers);
    chunkqueue_reset(con->write_queue);
    
    char date_buf[64];
    get_http_date(time(NULL), date_buf, sizeof(date_buf));
    
    const char *reason = http_status_reason(status);
    
    /* HTML error body */
    char body[512];
    int body_len = snprintf(body, sizeof(body),
        "<!DOCTYPE html>\n"
        "<html><head><title>%d %s</title></head>\n"
        "<body style=\"font-family:sans-serif;text-align:center;padding:50px;\">\n"
        "<h1>%d %s</h1>\n"
        "<hr><p>%s</p>\n"
        "</body></html>\n",
        status, reason, status, reason, server_tag);
    
    buffer_append_printf(con->response_headers,
        "HTTP/1.1 %d %s\r\n"
        "Date: %s\r\n"
        "Server: %s\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: %s\r\n"
        "\r\n",
        status, reason,
        date_buf,
        server_tag,
        body_len,
        con->keep_alive ? "keep-alive" : "close");
    
    chunkqueue_append_buffer(con->write_queue, con->response_headers);
    if (con->request.method != HTTP_METHOD_HEAD) {
        chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
    }
}

int connection_state_machine(server_t *srv, connection_t *con) {
    int done = 0;
    
    while (!done) {
        con->state_timestamp = time(NULL);
        
        switch (con->state) {
            case CON_STATE_CONNECT: {
                con->state = CON_STATE_REQUEST_START;
                break;
            }
            
            case CON_STATE_REQUEST_START: {
                connection_reset(con);
                fdevent_event_set(srv->ev, con->fd, FDEVENT_IN);
                con->state = CON_STATE_READ;
                done = 1; /* Wait for read event */
                break;
            }
            
            case CON_STATE_READ: {
                char buf[READ_CHUNK_SIZE];
                ssize_t bytes = recv(con->fd, buf, sizeof(buf), 0);
                
                if (bytes < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        done = 1;
                        break;
                    }
                    con->state = CON_STATE_ERROR;
                    break;
                } else if (bytes == 0) {
                    /* Client closed connection */
                    con->state = CON_STATE_CLOSE;
                    break;
                }
                
                srv->stats.bytes_read += bytes;
                buffer_append_string_len(con->read_buf, buf, (size_t)bytes);
                
                size_t header_len = 0;
                int parse_res = http_request_parse(&con->request, 
                                                   buffer_cstr(con->read_buf), 
                                                   buffer_len(con->read_buf), 
                                                   &header_len);
                if (parse_res == 1) {
                    /* Headers complete */
                    con->request_count++;
                    srv->stats.total_requests++;
                    con->keep_alive = con->request.keep_alive && 
                                     (con->request_count < con->keep_alive_max_requests);
                    con->state = CON_STATE_REQUEST_END;
                } else if (parse_res == 0) {
                    /* Incomplete request, wait for more data */
                    done = 1;
                } else if (parse_res == -1) {
                    /* Bad request */
                    generate_error_response(con, 400, srv->config.server_tag);
                    update_stats_status(srv, 400);
                    con->keep_alive = 0;
                    con->state = CON_STATE_WRITE;
                    fdevent_event_set(srv->ev, con->fd, FDEVENT_OUT);
                } else if (parse_res == -2) {
                    generate_error_response(con, 414, srv->config.server_tag);
                    update_stats_status(srv, 414);
                    con->keep_alive = 0;
                    con->state = CON_STATE_WRITE;
                    fdevent_event_set(srv->ev, con->fd, FDEVENT_OUT);
                } else if (parse_res == -3) {
                    generate_error_response(con, 431, srv->config.server_tag);
                    update_stats_status(srv, 431);
                    con->keep_alive = 0;
                    con->state = CON_STATE_WRITE;
                    fdevent_event_set(srv->ev, con->fd, FDEVENT_OUT);
                }
                break;
            }
            
            case CON_STATE_REQUEST_END: {
                con->state = CON_STATE_HANDLE_REQUEST;
                break;
            }
            
            case CON_STATE_HANDLE_REQUEST: {
                if (con->request.method != HTTP_METHOD_GET && 
                    con->request.method != HTTP_METHOD_HEAD) {
                    generate_error_response(con, 405, srv->config.server_tag);
                    update_stats_status(srv, 405);
                    con->state = CON_STATE_RESPONSE_START;
                    break;
                }
                
                /* Build full filesystem path */
                const char *uri_path = buffer_cstr(con->request.uri_path);
                char full_path[2048];
                snprintf(full_path, sizeof(full_path), "%s%s", srv->config.document_root, uri_path);
                
                struct stat st;
                if (stat(full_path, &st) != 0) {
                    generate_error_response(con, 404, srv->config.server_tag);
                    update_stats_status(srv, 404);
                    con->state = CON_STATE_RESPONSE_START;
                    break;
                }
                
                /* If path is a directory, look for index.html */
                if (S_ISDIR(st.st_mode)) {
                    char index_path[2048];
                    size_t path_len = strlen(full_path);
                    if (path_len + 16 >= sizeof(index_path)) {
                        generate_error_response(con, 414, srv->config.server_tag);
                        con->state = CON_STATE_RESPONSE_START;
                        break;
                    }
                    memcpy(index_path, full_path, path_len);
                    if (path_len > 0 && index_path[path_len - 1] == '/') {
                        memcpy(index_path + path_len, "index.html", 11);
                    } else {
                        memcpy(index_path + path_len, "/index.html", 12);
                    }
                    
                    if (stat(index_path, &st) == 0 && S_ISREG(st.st_mode)) {
                        snprintf(full_path, sizeof(full_path), "%s", index_path);
                    } else {
                        generate_error_response(con, 403, srv->config.server_tag);
                        update_stats_status(srv, 403);
                        con->state = CON_STATE_RESPONSE_START;
                        break;
                    }
                }
                
                if (!S_ISREG(st.st_mode)) {
                    generate_error_response(con, 403, srv->config.server_tag);
                    update_stats_status(srv, 403);
                    con->state = CON_STATE_RESPONSE_START;
                    break;
                }
                
                /* Check If-Modified-Since */
                if (!buffer_is_empty(con->request.if_modified_since)) {
                    time_t if_mod = parse_http_date(buffer_cstr(con->request.if_modified_since));
                    if (if_mod > 0 && st.st_mtime <= if_mod) {
                        /* 304 Not Modified */
                        con->http_status = 304;
                        update_stats_status(srv, 304);
                        buffer_reset(con->response_headers);
                        chunkqueue_reset(con->write_queue);
                        
                        char date_buf[64];
                        get_http_date(time(NULL), date_buf, sizeof(date_buf));
                        
                        buffer_append_printf(con->response_headers,
                            "HTTP/1.1 304 Not Modified\r\n"
                            "Date: %s\r\n"
                            "Server: %s\r\n"
                            "Connection: %s\r\n"
                            "\r\n",
                            date_buf,
                            srv->config.server_tag,
                            con->keep_alive ? "keep-alive" : "close");
                            
                        chunkqueue_append_buffer(con->write_queue, con->response_headers);
                        con->state = CON_STATE_WRITE;
                        fdevent_event_set(srv->ev, con->fd, FDEVENT_OUT);
                        break;
                    }
                }
                
                /* Open file */
                int file_fd = open(full_path, O_RDONLY);
                if (file_fd < 0) {
                    generate_error_response(con, 403, srv->config.server_tag);
                    update_stats_status(srv, 403);
                    con->state = CON_STATE_RESPONSE_START;
                    break;
                }
                
                /* Handle Range requests */
                off_t file_size = st.st_size;
                off_t range_start = 0;
                off_t range_len = file_size;
                int is_range = 0;
                
                if (con->request.has_range) {
                    if (con->request.range_start >= 0) {
                        range_start = con->request.range_start;
                        if (range_start >= file_size) {
                            close(file_fd);
                            generate_error_response(con, 416, srv->config.server_tag);
                            update_stats_status(srv, 416);
                            con->state = CON_STATE_RESPONSE_START;
                            break;
                        }
                        if (con->request.range_end >= 0 && con->request.range_end < file_size) {
                            range_len = con->request.range_end - range_start + 1;
                        } else {
                            range_len = file_size - range_start;
                        }
                    } else if (con->request.range_end > 0) {
                        /* Suffix range: last N bytes */
                        range_len = con->request.range_end;
                        if (range_len > file_size) range_len = file_size;
                        range_start = file_size - range_len;
                    }
                    is_range = 1;
                }
                
                int status_code = is_range ? 206 : 200;
                con->http_status = status_code;
                update_stats_status(srv, status_code);
                
                buffer_reset(con->response_headers);
                chunkqueue_reset(con->write_queue);
                
                char cur_date[64], mod_date[64];
                get_http_date(time(NULL), cur_date, sizeof(cur_date));
                get_http_date(st.st_mtime, mod_date, sizeof(mod_date));
                const char *mime = mime_type_get(full_path);
                
                if (is_range) {
                    buffer_append_printf(con->response_headers,
                        "HTTP/1.1 206 Partial Content\r\n"
                        "Date: %s\r\n"
                        "Server: %s\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %lld\r\n"
                        "Content-Range: bytes %lld-%lld/%lld\r\n"
                        "Last-Modified: %s\r\n"
                        "Accept-Ranges: bytes\r\n"
                        "Connection: %s\r\n"
                        "\r\n",
                        cur_date,
                        srv->config.server_tag,
                        mime,
                        (long long)range_len,
                        (long long)range_start,
                        (long long)(range_start + range_len - 1),
                        (long long)file_size,
                        mod_date,
                        con->keep_alive ? "keep-alive" : "close");
                } else {
                    buffer_append_printf(con->response_headers,
                        "HTTP/1.1 200 OK\r\n"
                        "Date: %s\r\n"
                        "Server: %s\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %lld\r\n"
                        "Last-Modified: %s\r\n"
                        "Accept-Ranges: bytes\r\n"
                        "Connection: %s\r\n"
                        "\r\n",
                        cur_date,
                        srv->config.server_tag,
                        mime,
                        (long long)file_size,
                        mod_date,
                        con->keep_alive ? "keep-alive" : "close");
                }
                
                chunkqueue_append_buffer(con->write_queue, con->response_headers);
                
                if (con->request.method != HTTP_METHOD_HEAD) {
                    chunkqueue_append_file(con->write_queue, file_fd, range_start, range_len);
                } else {
                    close(file_fd);
                }
                
                con->state = CON_STATE_RESPONSE_START;
                break;
            }
            
            case CON_STATE_RESPONSE_START: {
                fdevent_event_set(srv->ev, con->fd, FDEVENT_OUT);
                con->state = CON_STATE_WRITE;
                break;
            }
            
            case CON_STATE_WRITE: {
                ssize_t w = chunkqueue_write_chunk_to_socket(con->write_queue, con->fd);
                if (w > 0) {
                    con->bytes_sent += w;
                    srv->stats.bytes_written += w;
                }
                
                if (chunkqueue_is_empty(con->write_queue)) {
                    /* Full response sent! */
                    con->state = CON_STATE_RESPONSE_END;
                } else if (w == -1) {
                    /* EAGAIN / EWOULDBLOCK, wait for next EPOLLOUT */
                    done = 1;
                } else if (w == -2) {
                    /* Broken pipe / connection error */
                    con->state = CON_STATE_ERROR;
                }
                break;
            }
            
            case CON_STATE_RESPONSE_END: {
                if (con->keep_alive) {
                    /* Reset and wait for next request on this connection */
                    con->state = CON_STATE_REQUEST_START;
                } else {
                    con->state = CON_STATE_CLOSE;
                }
                break;
            }
            
            case CON_STATE_ERROR:
            case CON_STATE_CLOSE: {
                server_close_connection(srv, con);
                done = 1;
                return 0;
            }
        }
    }
    
    return 0;
}

void connection_handle_fdevent(void *context, int revents) {
    connection_t *con = (connection_t *)context;
    if (!con) return;
    
    /* We will retrieve the server instance from con or server_t pointer */
    extern server_t *g_server;
    if (!g_server) return;
    
    if (revents & (FDEVENT_HUP | FDEVENT_ERR)) {
        con->state = CON_STATE_CLOSE;
        connection_state_machine(g_server, con);
        return;
    }
    
    connection_state_machine(g_server, con);
}
