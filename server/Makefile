CC=gcc
CFLAGS=-Wall -Wextra -O2 -I../common
TARGET=server
SRCS=src/main.c src/logger.c ../common/platform.c ../common/net_frame.c ../common/kv.c ../common/urlcodec.c ../common/crypto.c
OBJS=$(SRCS:.c=.o)
LIBS=-lssl -lcrypto

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(TARGET) $(OBJS)
