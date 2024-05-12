.PHONY: all clean	
CC ?= gcc	
CROSS_COMPILE ?=

CFLAGS ?= -g -Wall -Werror 
SRCS = aesdsocket.c
TARGET ?= aesdsocket
LDFLAGS ?= -pthread -lrt

$(TARGET):$(TARGET).o
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $(LDFLAGS) -I/ $(TARGET).o -o $(TARGET)	

$(TARGET).o:$(TARGET).c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $(LDFLAGS) -c $(TARGET).c -o $(TARGET).o	


all: $(TARGET)

clean:	
	rm -f *.o $(TARGET)	
