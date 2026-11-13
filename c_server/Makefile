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
MODULE_SRCS   = src/modules/she_plugin.c src/modules/mod_status.c src/modules/mod_indexfile.c src/modules/mod_dirlisting.c src/modules/mod_accesslog.c src/modules/mod_auth.c src/modules/mod_vhost.c src/modules/mod_fastcgi.c src/modules/mod_deflate.c src/modules/mod_rewrite.c src/modules/mod_proxy.c src/modules/mod_websocket.c src/modules/mod_ssl.c src/modules/mod_http2.c src/modules/mod_wasm.c src/modules/mod_ebpf.c src/modules/mod_acme.c src/modules/mod_quic.c src/modules/mod_wireguard.c src/modules/mod_grpc.c src/modules/mod_ai_gateway.c src/modules/mod_raft.c src/modules/mod_otel.c src/modules/mod_dpdk.c src/modules/mod_tc_flower.c src/modules/mod_hw_mesh.c src/modules/mod_pqc.c src/modules/mod_http4.c src/modules/mod_terabit_fabric.c src/modules/mod_qkd.c src/modules/mod_nanocore.c src/modules/mod_neural_routing.c src/modules/mod_waveguide.c src/modules/mod_dtn_bundle.c src/modules/mod_planetary_grid.c src/modules/mod_self_heal.c src/modules/mod_sandbox.c src/modules/mod_metrics.c src/modules/mod_proxy_pool.c src/modules/mod_mtls.c src/modules/mod_cloud_mesh.c src/modules/mod_io_uring.c src/modules/mod_xds.c src/modules/mod_profiler.c
MAIN_SRC      = src/main.c
TEST_SRC      = tests/she_unit_tests.c
TEST_BIN      = $(BUILD_DIR)/she_tests
ASAN_BIN      = $(BUILD_DIR)/she_tests_asan

SRCS = $(CORE_SRCS) $(EVENT_SRCS) $(NET_SRCS) $(HTTP_SRCS) $(SECURITY_SRCS) $(MODULE_SRCS) $(MAIN_SRC)
LIB_SRCS = $(CORE_SRCS) $(EVENT_SRCS) $(NET_SRCS) $(HTTP_SRCS) $(SECURITY_SRCS) $(MODULE_SRCS)
OBJS = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(SRCS))
LIB_OBJS = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(LIB_SRCS))

.PHONY: all clean run test asan

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

$(TEST_BIN): $(LIB_OBJS) $(TEST_SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(LIB_OBJS) $(TEST_SRC) $(LDFLAGS)

test: $(TARGET) $(TEST_BIN)
	./$(TEST_BIN)
	./$(TARGET) -h >/dev/null

asan: $(LIB_SRCS) $(TEST_SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) -Wall -Wextra -g -O1 -fsanitize=address,undefined -D_GNU_SOURCE -std=c11 $(INCLUDES) -o $(ASAN_BIN) $(LIB_SRCS) $(TEST_SRC) $(LDFLAGS)
	ASAN_OPTIONS=detect_leaks=1 ./$(ASAN_BIN)

