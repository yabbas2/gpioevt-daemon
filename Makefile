CROSS_COMPILE?=
CC?=$(CROSS_COMPILE)gcc
TARGET?=gpioevtd
CFLAGS+=-Wall -Wextra -g -O2
LDFLAGS+=-lpthread

all: gpioevtd.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $(TARGET)

clean:
	rm -f $(TARGET)
