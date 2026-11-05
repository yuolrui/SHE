#include "core/she_connection.h"
#include "core/she_server.h"
#include "http/she_mime.h"
#include "net/she_net.h"
#include "modules/she_plugin.h"
#include "modules/mod_deflate.h"
#include "security/she_security.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#define SHE_READ_CHUNK_SIZE 4096

she_connection_t *she_connection_init(int client_fd, struct sockaddr_in *client_addr) {
    she_connection_t *con = (she_connection_t *)malloc(sizeof(she_connection_t));
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
    
    con->state = SHE_CON_STATE_CONNECT;
    con->state_timestamp = time(NULL);
    con->read_buf = she_buffer_init_size(4096);
    she_http_request_init(&con->request);
    
    con->http_status = 0;
    con->response_headers = she_buffer_init_size(1024);
    con->write_queue = she_chunkqueue_init();
    
    con->keep_alive = 1;
    con->keep_alive_max_requests = 100;
    con->request_count = 0;
    con->bytes_sent = 0;
    
    con->next = NULL;
    con->prev = NULL;
    
    return con;
}

void she_connection_reset(she_connection_t *con) {
    if (!con) return;
    she_buffer_reset(con->read_buf);
    she_http_request_reset(&con->request);
    con->http_status = 0;
    she_buffer_reset(con->response_headers);
    she_chunkqueue_reset(con->write_queue);
    con->bytes_sent = 0;
}

void she_connection_free(she_connection_t *con) {
    if (!con) return;
    if (con->read_buf) she_buffer_free(con->read_buf);
    she_http_request_free(&con->request);
    if (con->response_headers) she_buffer_free(con->response_headers);
    if (con->write_queue) she_chunkqueue_free(con->write_queue);
    free(con);
}

const char *she_connection_state_to_str(she_con_state_t state) {
    switch (state) {
        case SHE_CON_STATE_CONNECT:         return "CONNECT";
        case SHE_CON_STATE_REQUEST_START:   return "REQUEST_START";
        case SHE_CON_STATE_READ:            return "READ";
        case SHE_CON_STATE_REQUEST_END:     return "REQUEST_END";
        case SHE_CON_STATE_HANDLE_REQUEST:  return "HANDLE_REQUEST";
        case SHE_CON_STATE_RESPONSE_START:  return "RESPONSE_START";
        case SHE_CON_STATE_WRITE:           return "WRITE";
        case SHE_CON_STATE_RESPONSE_END:    return "RESPONSE_END";
        case SHE_CON_STATE_ERROR:           return "ERROR";
        case SHE_CON_STATE_CLOSE:           return "CLOSE";
        default:                            return "UNKNOWN";
    }
}

static const char *she_http_status_reason(int status) {
    switch (status) {
        case 200: return "OK";
        case 206: return "Partial Content";
        case 301: return "Moved Permanently";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 414: return "URI Too Long";
        case 416: return "Range Not Satisfiable";
        case 429: return "Too Many Requests";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown";
    }
}

static void she_update_stats_status(she_server_t *srv, int status) {
    if (!srv) return;
    switch (status) {
        case 200: srv->stats.status_200++; break;
        case 206: srv->stats.status_206++; break;
        case 304: srv->stats.status_304++; break;
        case 400: srv->stats.status_400++; break;
        case 401: srv->stats.status_401++; break;
        case 403: srv->stats.status_403++; break;
        case 404: srv->stats.status_404++; break;
        case 405: srv->stats.status_405++; break;
        case 429: srv->stats.status_429++; break;
        case 500: srv->stats.status_500++; break;
        default: break;
    }
}

static void she_get_http_date(time_t t, char *buf, size_t max) {
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(buf, max, "%a, %d %b %Y %H:%M:%S GMT", &tm);
}

static time_t she_parse_http_date(const char *str) {
    if (!str || !*str) return 0;
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    if (strptime(str, "%a, %d %b %Y %H:%M:%S GMT", &tm) != NULL) {
        return timegm(&tm);
    }
    return 0;
}

void she_generate_error_response(she_server_t *srv, she_connection_t *con, int status) {
    con->http_status = status;
    she_buffer_reset(con->response_headers);
    she_chunkqueue_reset(con->write_queue);
    
    char date_buf[64];
    she_get_http_date(time(NULL), date_buf, sizeof(date_buf));
    const char *reason = she_http_status_reason(status);
    
    char body[1024];
    int body_len = snprintf(body, sizeof(body),
        "<!DOCTYPE html>\n"
        "<html><head><title>%d %s</title></head>\n"
        "<body style=\"font-family:sans-serif;text-align:center;padding:50px;background:#0b0f19;color:#f1f5f9;\">\n"
        "<h1 style=\"color:#f43f5e;\">%d %s</h1>\n"
        "<hr style=\"border-color:#334155;max-width:500px;\">\n"
        "<p style=\"color:#94a3b8;\">%s</p>\n"
        "</body></html>\n",
        status, reason, status, reason, srv->config.server_tag);
    
    she_buffer_append_printf(con->response_headers,
        "HTTP/1.1 %d %s\r\n"
        "Date: %s\r\n"
        "Server: %s\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n",
        status, reason,
        date_buf,
        srv->config.server_tag,
        body_len);
        
    if (status == 429) {
        she_buffer_append_string(con->response_headers, "Retry-After: 1\r\n");
    }
    
    /* Inject SHE security headers */
    if (srv->security) {
        she_security_append_headers(srv->security, con);
    }
    
    she_buffer_append_printf(con->response_headers,
        "Connection: %s\r\n\r\n",
        con->keep_alive ? "keep-alive" : "close");
    
    she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
    if (con->request.method != SHE_HTTP_METHOD_HEAD) {
        she_chunkqueue_append_mem(con->write_queue, body, (size_t)body_len);
    }
}

int she_connection_state_machine(she_server_t *srv, she_connection_t *con) {
    int done = 0;
    
    while (!done) {
        con->state_timestamp = time(NULL);
        
        switch (con->state) {
            case SHE_CON_STATE_CONNECT: {
                con->state = SHE_CON_STATE_REQUEST_START;
                break;
            }
            
            case SHE_CON_STATE_REQUEST_START: {
                she_connection_reset(con);
                she_fdevent_event_set(srv->ev, con->fd, SHE_FDEVENT_IN);
                con->state = SHE_CON_STATE_READ;
                done = 1;
                break;
            }
            
            case SHE_CON_STATE_READ: {
                char buf[SHE_READ_CHUNK_SIZE];
                ssize_t bytes = recv(con->fd, buf, sizeof(buf), 0);
                
                if (bytes < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        done = 1;
                        break;
                    }
                    con->state = SHE_CON_STATE_ERROR;
                    break;
                } else if (bytes == 0) {
                    con->state = SHE_CON_STATE_CLOSE;
                    break;
                }
                
                srv->stats.bytes_read += bytes;
                she_buffer_append_string_len(con->read_buf, buf, (size_t)bytes);
                
                size_t header_len = 0;
                int parse_res = she_http_request_parse(&con->request, 
                                                       she_buffer_cstr(con->read_buf), 
                                                       she_buffer_len(con->read_buf), 
                                                       &header_len);
                if (parse_res == 1) {
                    con->request_count++;
                    srv->stats.total_requests++;
                    con->keep_alive = con->request.keep_alive && 
                                     (con->request_count < con->keep_alive_max_requests);
                    
                    /* 1. Security Check: Token-bucket IP Rate Limiting */
                    if (srv->security && !she_security_check_rate_limit(srv->security, con->client_ip)) {
                        she_generate_error_response(srv, con, 429);
                        she_update_stats_status(srv, 429);
                        con->keep_alive = 0;
                        con->state = SHE_CON_STATE_WRITE;
                        she_fdevent_event_set(srv->ev, con->fd, SHE_FDEVENT_OUT);
                        break;
                    }
                    
                    /* 2. Security Check: Host Header Validation */
                    if (srv->security && !she_security_validate_host(srv->security, she_buffer_cstr(con->request.host))) {
                        she_generate_error_response(srv, con, 400);
                        she_update_stats_status(srv, 400);
                        con->keep_alive = 0;
                        con->state = SHE_CON_STATE_WRITE;
                        she_fdevent_event_set(srv->ev, con->fd, SHE_FDEVENT_OUT);
                        break;
                    }
                    
                    /* 3. Security Check: Request Smuggling Check */
                    if (srv->security && she_security_check_smuggling(con) < 0) {
                        she_generate_error_response(srv, con, 400);
                        she_update_stats_status(srv, 400);
                        con->keep_alive = 0;
                        con->state = SHE_CON_STATE_WRITE;
                        she_fdevent_event_set(srv->ev, con->fd, SHE_FDEVENT_OUT);
                        break;
                    }
                    
                    /* 4. Security Check: Path Traversal / Illegal characters */
                    if (!she_security_validate_uri(she_buffer_cstr(con->request.uri_raw))) {
                        she_generate_error_response(srv, con, 403);
                        she_update_stats_status(srv, 403);
                        con->keep_alive = 0;
                        con->state = SHE_CON_STATE_WRITE;
                        she_fdevent_event_set(srv->ev, con->fd, SHE_FDEVENT_OUT);
                        break;
                    }
                    
                    con->state = SHE_CON_STATE_REQUEST_END;
                } else if (parse_res == 0) {
                    done = 1;
                } else if (parse_res == -1) {
                    she_generate_error_response(srv, con, 400);
                    she_update_stats_status(srv, 400);
                    con->keep_alive = 0;
                    con->state = SHE_CON_STATE_WRITE;
                    she_fdevent_event_set(srv->ev, con->fd, SHE_FDEVENT_OUT);
                } else if (parse_res == -2) {
                    she_generate_error_response(srv, con, 414);
                    she_update_stats_status(srv, 414);
                    con->keep_alive = 0;
                    con->state = SHE_CON_STATE_WRITE;
                    she_fdevent_event_set(srv->ev, con->fd, SHE_FDEVENT_OUT);
                } else if (parse_res == -3) {
                    she_generate_error_response(srv, con, 431);
                    she_update_stats_status(srv, 431);
                    con->keep_alive = 0;
                    con->state = SHE_CON_STATE_WRITE;
                    she_fdevent_event_set(srv->ev, con->fd, SHE_FDEVENT_OUT);
                }
                break;
            }
            
            case SHE_CON_STATE_REQUEST_END: {
                con->state = SHE_CON_STATE_HANDLE_REQUEST;
                break;
            }
            
            case SHE_CON_STATE_HANDLE_REQUEST: {
                /* SHE Plugin Pipeline: URI Clean & Canonicalization (mod_vhost) */
                if (she_plugins_call_handle_uri_clean(srv, con) == SHE_HANDLER_FINISHED) {
                    break;
                }
                
                /* SHE Plugin Pipeline: Subrequest Start (mod_auth, mod_status, mod_fastcgi, mod_indexfile, mod_dirlisting) */
                she_handler_t hook_res = she_plugins_call_handle_subrequest_start(srv, con);
                if (hook_res == SHE_HANDLER_FINISHED) {
                    break;
                } else if (hook_res == SHE_HANDLER_ERROR) {
                    she_generate_error_response(srv, con, 500);
                    she_update_stats_status(srv, 500);
                    con->state = SHE_CON_STATE_RESPONSE_START;
                    break;
                }
                
                if (con->request.method != SHE_HTTP_METHOD_GET && 
                    con->request.method != SHE_HTTP_METHOD_HEAD) {
                    she_generate_error_response(srv, con, 405);
                    she_update_stats_status(srv, 405);
                    con->state = SHE_CON_STATE_RESPONSE_START;
                    break;
                }
                
                /* Determine effective document root (vhost override or global) */
                const char *docroot = srv->config.document_root;
                if (!she_buffer_is_empty(con->request.vhost_docroot)) {
                    docroot = she_buffer_cstr(con->request.vhost_docroot);
                }
                
                const char *uri_path = she_buffer_cstr(con->request.uri_path);
                char full_path[2048];
                snprintf(full_path, sizeof(full_path), "%s%s", docroot, uri_path);
                
                struct stat st;
                if (stat(full_path, &st) != 0) {
                    she_generate_error_response(srv, con, 404);
                    she_update_stats_status(srv, 404);
                    con->state = SHE_CON_STATE_RESPONSE_START;
                    break;
                }
                
                /* Directory index resolution */
                if (S_ISDIR(st.st_mode)) {
                    char index_path[2048];
                    size_t path_len = strlen(full_path);
                    if (path_len + 16 >= sizeof(index_path)) {
                        she_generate_error_response(srv, con, 414);
                        con->state = SHE_CON_STATE_RESPONSE_START;
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
                        she_generate_error_response(srv, con, 403);
                        she_update_stats_status(srv, 403);
                        con->state = SHE_CON_STATE_RESPONSE_START;
                        break;
                    }
                }
                
                if (!S_ISREG(st.st_mode)) {
                    she_generate_error_response(srv, con, 403);
                    she_update_stats_status(srv, 403);
                    con->state = SHE_CON_STATE_RESPONSE_START;
                    break;
                }
                
                /* If-Modified-Since cache revalidation */
                if (!she_buffer_is_empty(con->request.if_modified_since)) {
                    time_t if_mod = she_parse_http_date(she_buffer_cstr(con->request.if_modified_since));
                    if (if_mod > 0 && st.st_mtime <= if_mod) {
                        con->http_status = 304;
                        she_update_stats_status(srv, 304);
                        she_buffer_reset(con->response_headers);
                        she_chunkqueue_reset(con->write_queue);
                        
                        char date_buf[64];
                        she_get_http_date(time(NULL), date_buf, sizeof(date_buf));
                        
                        she_buffer_append_printf(con->response_headers,
                            "HTTP/1.1 304 Not Modified\r\n"
                            "Date: %s\r\n"
                            "Server: %s\r\n",
                            date_buf,
                            srv->config.server_tag);
                            
                        if (srv->security) {
                            she_security_append_headers(srv->security, con);
                        }
                        
                        she_buffer_append_printf(con->response_headers,
                            "Connection: %s\r\n\r\n",
                            con->keep_alive ? "keep-alive" : "close");
                            
                        she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
                        con->state = SHE_CON_STATE_WRITE;
                        she_fdevent_event_set(srv->ev, con->fd, SHE_FDEVENT_OUT);
                        break;
                    }
                }
                
                int file_fd = open(full_path, O_RDONLY);
                if (file_fd < 0) {
                    she_generate_error_response(srv, con, 403);
                    she_update_stats_status(srv, 403);
                    con->state = SHE_CON_STATE_RESPONSE_START;
                    break;
                }
                
                /* HTTP Range request handling */
                off_t file_size = st.st_size;
                off_t range_start = 0;
                off_t range_len = file_size;
                int is_range = 0;
                
                if (con->request.has_range) {
                    if (con->request.range_start >= 0) {
                        range_start = con->request.range_start;
                        if (range_start >= file_size) {
                            close(file_fd);
                            she_generate_error_response(srv, con, 416);
                            she_update_stats_status(srv, 416);
                            con->state = SHE_CON_STATE_RESPONSE_START;
                            break;
                        }
                        if (con->request.range_end >= 0 && con->request.range_end < file_size) {
                            range_len = con->request.range_end - range_start + 1;
                        } else {
                            range_len = file_size - range_start;
                        }
                    } else if (con->request.range_end > 0) {
                        range_len = con->request.range_end;
                        if (range_len > file_size) range_len = file_size;
                        range_start = file_size - range_len;
                    }
                    is_range = 1;
                }
                
                int status_code = is_range ? 206 : 200;
                con->http_status = status_code;
                she_update_stats_status(srv, status_code);
                
                she_buffer_reset(con->response_headers);
                she_chunkqueue_reset(con->write_queue);
                
                char cur_date[64], mod_date[64];
                she_get_http_date(time(NULL), cur_date, sizeof(cur_date));
                she_get_http_date(st.st_mtime, mod_date, sizeof(mod_date));
                const char *mime = she_mime_type_get(full_path);
                
                /* Phase 4: Dynamic Gzip Compression (mod_deflate) */
                const char *accept_encoding = she_http_request_get_header(&con->request, "Accept-Encoding");
                int client_supports_gzip = (accept_encoding && strstr(accept_encoding, "gzip") != NULL);
                int is_compressible = (srv->config.deflate_enabled && she_deflate_is_compressible_mime(mime));
                int min_size = srv->config.deflate_min_size > 0 ? srv->config.deflate_min_size : 128;
                int should_compress = (!is_range && client_supports_gzip && is_compressible && 
                                       file_size >= min_size && file_size <= 8 * 1024 * 1024);
                
                if (should_compress) {
                    char *raw_buf = (char *)malloc((size_t)file_size);
                    if (raw_buf) {
                        ssize_t rd = pread(file_fd, raw_buf, (size_t)file_size, 0);
                        close(file_fd);
                        file_fd = -1;
                        if (rd == file_size) {
                            char *gz_data = NULL;
                            size_t gz_len = 0;
                            if (she_deflate_compress_gzip(raw_buf, (size_t)file_size, &gz_data, &gz_len) == 0 && gz_len < (size_t)file_size) {
                                free(raw_buf);
                                srv->stats.requests_compressed++;
                                srv->stats.bytes_compressed_saved += (file_size - gz_len);
                                
                                she_buffer_append_printf(con->response_headers,
                                    "HTTP/1.1 200 OK\r\n"
                                    "Date: %s\r\n"
                                    "Server: %s\r\n"
                                    "Content-Type: %s\r\n"
                                    "Content-Encoding: gzip\r\n"
                                    "Vary: Accept-Encoding\r\n"
                                    "Content-Length: %zu\r\n"
                                    "Last-Modified: %s\r\n"
                                    "Accept-Ranges: bytes\r\n",
                                    cur_date,
                                    srv->config.server_tag,
                                    mime,
                                    gz_len,
                                    mod_date);
                                    
                                if (srv->security) {
                                    she_security_append_headers(srv->security, con);
                                }
                                
                                she_buffer_append_printf(con->response_headers,
                                    "Connection: %s\r\n\r\n",
                                    con->keep_alive ? "keep-alive" : "close");
                                    
                                she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
                                if (con->request.method != SHE_HTTP_METHOD_HEAD) {
                                    she_chunkqueue_append_mem(con->write_queue, gz_data, gz_len);
                                }
                                free(gz_data);
                                con->state = SHE_CON_STATE_RESPONSE_START;
                                break;
                            }
                            if (gz_data) free(gz_data);
                        }
                        free(raw_buf);
                        file_fd = open(full_path, O_RDONLY);
                    }
                }
                
                if (is_range) {
                    she_buffer_append_printf(con->response_headers,
                        "HTTP/1.1 206 Partial Content\r\n"
                        "Date: %s\r\n"
                        "Server: %s\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %lld\r\n"
                        "Content-Range: bytes %lld-%lld/%lld\r\n"
                        "Last-Modified: %s\r\n"
                        "Accept-Ranges: bytes\r\n",
                        cur_date,
                        srv->config.server_tag,
                        mime,
                        (long long)range_len,
                        (long long)range_start,
                        (long long)(range_start + range_len - 1),
                        (long long)file_size,
                        mod_date);
                } else {
                    she_buffer_append_printf(con->response_headers,
                        "HTTP/1.1 200 OK\r\n"
                        "Date: %s\r\n"
                        "Server: %s\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %lld\r\n"
                        "Last-Modified: %s\r\n"
                        "Accept-Ranges: bytes\r\n",
                        cur_date,
                        srv->config.server_tag,
                        mime,
                        (long long)file_size,
                        mod_date);
                }
                
                /* Inject SHE security headers */
                if (srv->security) {
                    she_security_append_headers(srv->security, con);
                }
                
                she_buffer_append_printf(con->response_headers,
                    "Connection: %s\r\n\r\n",
                    con->keep_alive ? "keep-alive" : "close");
                
                she_chunkqueue_append_buffer(con->write_queue, con->response_headers);
                
                if (con->request.method != SHE_HTTP_METHOD_HEAD) {
                    she_chunkqueue_append_file(con->write_queue, file_fd, range_start, range_len);
                } else {
                    close(file_fd);
                }
                
                con->state = SHE_CON_STATE_RESPONSE_START;
                break;
            }
            
            case SHE_CON_STATE_RESPONSE_START: {
                she_fdevent_event_set(srv->ev, con->fd, SHE_FDEVENT_OUT);
                con->state = SHE_CON_STATE_WRITE;
                break;
            }
            
            case SHE_CON_STATE_WRITE: {
                ssize_t w = she_chunkqueue_write_chunk_to_socket(con->write_queue, con->fd);
                if (w > 0) {
                    con->bytes_sent += w;
                    srv->stats.bytes_written += w;
                }
                
                if (she_chunkqueue_is_empty(con->write_queue)) {
                    con->state = SHE_CON_STATE_RESPONSE_END;
                } else if (w == -1) {
                    done = 1;
                } else if (w == -2) {
                    con->state = SHE_CON_STATE_ERROR;
                }
                break;
            }
            
            case SHE_CON_STATE_RESPONSE_END: {
                she_plugins_call_handle_response_done(srv, con);
                
                if (con->keep_alive) {
                    con->state = SHE_CON_STATE_REQUEST_START;
                } else {
                    con->state = SHE_CON_STATE_CLOSE;
                }
                break;
            }
            
            case SHE_CON_STATE_ERROR:
            case SHE_CON_STATE_CLOSE: {
                she_plugins_call_handle_response_done(srv, con);
                she_server_close_connection(srv, con);
                done = 1;
                return 0;
            }
        }
    }
    
    return 0;
}

void she_connection_handle_fdevent(void *context, int revents) {
    she_connection_t *con = (she_connection_t *)context;
    if (!con || !g_she_server) return;
    
    if (revents & (SHE_FDEVENT_HUP | SHE_FDEVENT_ERR)) {
        con->state = SHE_CON_STATE_CLOSE;
        she_connection_state_machine(g_she_server, con);
        return;
    }
    
    she_connection_state_machine(g_she_server, con);
}
