CC=gcc
CFLAGS=-Wall -Wextra -O2 -I../common
TARGET=client
SRCS=src/main.c ../common/platform.c
OBJS=$(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(TARGET) $(OBJS)
