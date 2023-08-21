CC?=$(CROSS_COMPILE)gcc
CFLAGS?=-Wall -Werror -g -O0
LDFLAGS?=-lrt -pthread

TARGET?=aesdsocket

.PHONY: all clean


all: $(TARGET)


$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) $(INC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET).o
	rm -f $(TARGET)

