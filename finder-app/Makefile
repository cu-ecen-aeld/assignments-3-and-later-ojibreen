CC=gcc
CFLAGS=-I.

ifeq ($(CROSS_COMPILE), aarch64-none-linux-gnu-)
	CC=$(CROSS_COMPILE)gcc
endif

writermake: writer.c
	$(CC) -o writer writer.c -I.

clean:
	rm -f ./writer
	rm -f ./*.o
