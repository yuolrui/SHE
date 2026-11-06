CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -D_GNU_SOURCE -std=c11
LDFLAGS ?= -lz -lssl -lcrypto
INCLUDES = -Isrc -Isrc/core -Isrc/event -Isrc/net -Isrc/http -Isrc/security -Isrc/modules

TARGET = she
COMPAT_TARGET = c_lighttpd
BUILD_DIR = build

# Source files grouped by SHE architectural subsystems
CORE_SRCS     = src/core/she_buffer.c src/core/she_chunkqueue.c src/core/she_connection.c src/core/she_server.c src/core/she_config.c
EVENT_SRCS    = src/event/she_fdevent.c
NET_SRCS      = src/net/she_net.c
HTTP_SRCS     = src/http/she_http_parser.c src/http/she_mime.c
SECURITY_SRCS = src/security/she_security.c
MODULE_SRCS   = src/modules/she_plugin.c src/modules/mod_status.c src/modules/mod_indexfile.c src/modules/mod_dirlisting.c src/modules/mod_accesslog.c src/modules/mod_auth.c src/modules/mod_vhost.c src/modules/mod_fastcgi.c src/modules/mod_deflate.c src/modules/mod_rewrite.c src/modules/mod_proxy.c src/modules/mod_websocket.c src/modules/mod_ssl.c src/modules/mod_http2.c src/modules/mod_wasm.c src/modules/mod_ebpf.c src/modules/mod_acme.c
MAIN_SRC      = src/main.c

SRCS = $(CORE_SRCS) $(EVENT_SRCS) $(NET_SRCS) $(HTTP_SRCS) $(SECURITY_SRCS) $(MODULE_SRCS) $(MAIN_SRC)
OBJS = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(SRCS))

.PHONY: all clean run test

all: $(TARGET) $(COMPAT_TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(OBJS) $(LDFLAGS)
	@chmod +x $(TARGET)
	@echo "Build complete: $(TARGET) (SHE - Secure HTTP Engine)"

$(COMPAT_TARGET): $(TARGET)
	@ln -sf $(TARGET) $(COMPAT_TARGET)
	@echo "Created compatibility symlink: $(COMPAT_TARGET) -> $(TARGET)"

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(COMPAT_TARGET)
	@echo "Cleaned build artifacts."

run: $(TARGET)
	./$(TARGET) -p 8899 -d ./www -v

test: $(TARGET)
	./$(TARGET) -h
