PROGS=writer

CC=$(CROSS_COMPILE)gcc
CFLAGS=-Wall -Werror -g

all: $(PROGS)

$(PROGS): $(PROGS).c
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f $(PROGS).o
	rm -f $(PROGS)

