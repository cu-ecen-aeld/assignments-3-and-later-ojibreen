.PHONY: all, clean

CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra -g
SRCS = aesdsocket.c
OBJS = $(SRCS:.c=.o)
TARGET = aesdsocket

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

clean:
	rm -f $(TARGET) $(OBJS)
