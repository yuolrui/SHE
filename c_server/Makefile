CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -D_GNU_SOURCE -std=c11
INCLUDES = -Isrc -Isrc/core -Isrc/event -Isrc/net -Isrc/http -Isrc/modules

TARGET = c_lighttpd
BUILD_DIR = build

# Source files grouped by architectural subsystems
CORE_SRCS    = src/core/buffer.c src/core/chunkqueue.c src/core/connection.c src/core/server.c
EVENT_SRCS   = src/event/fdevent.c
NET_SRCS     = src/net/network.c
HTTP_SRCS    = src/http/http_parser.c src/http/mime_types.c
MODULE_SRCS  = src/modules/plugin.c src/modules/mod_status.c src/modules/mod_indexfile.c src/modules/mod_dirlisting.c src/modules/mod_accesslog.c
MAIN_SRC     = src/main.c

SRCS = $(CORE_SRCS) $(EVENT_SRCS) $(NET_SRCS) $(HTTP_SRCS) $(MODULE_SRCS) $(MAIN_SRC)
OBJS = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(SRCS))

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(OBJS)
	@chmod +x $(TARGET)
	@echo "Build complete: $(TARGET)"

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
	@echo "Cleaned build artifacts."

run: $(TARGET)
	./$(TARGET) -p 8080 -d ./www -v

test: $(TARGET)
	./$(TARGET) -h
