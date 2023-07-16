PROGS=aesdsocket

CC?=$(CROSS_COMPILE)gcc
CFLAGS?=-Wall -Werror -g -O0
LFLAGS?=-lrt -pthread

.PHONY: all default clean

all: $(PROGS)
default: $(PROGS)

$(PROGS): $(PROGS).c
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

clean:
	rm -f $(PROGS).o
	rm -f $(PROGS)

