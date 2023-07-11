PROGS=aesdsocket

CC=$(CROSS_COMPILE)gcc
CFLAGS=-Wall -Werror
CFLAGS+= -g -O0

.PHONY: all default clean

all: $(PROGS)
default: $(PROGS)

$(PROGS): $(PROGS).c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(PROGS).o
	rm -f $(PROGS)

