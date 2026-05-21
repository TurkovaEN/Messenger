CC=gcc
CFLAGS=-Wall -Wextra -O2 -I../common -pthread
TARGET=client
SRCS=src/main.c src/client_threads.c ../common/platform.c ../common/net_frame.c ../common/kv.c
OBJS=$(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(TARGET) $(OBJS)
