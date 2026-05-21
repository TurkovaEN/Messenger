CC=gcc
CFLAGS=-Wall -Wextra -O2 -I../common
TARGET=server
SRCS=src/main.c src/logger.c ../common/platform.c ../common/net_frame.c ../common/kv.c ../common/urlcodec.c
OBJS=$(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(TARGET) $(OBJS)
