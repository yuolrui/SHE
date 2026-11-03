CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_GNU_SOURCE -std=c11
TARGET = c_lighttpd

SRCS = main.c server.c connection.c http_parser.c chunkqueue.c buffer.c fdevent.c network.c mime_types.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET) -p 8080 -d ./www

test: $(TARGET)
	@echo "Running basic sanity test..."
	./$(TARGET) -h
