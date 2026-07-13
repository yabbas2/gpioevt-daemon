CROSS_COMPILE?=
CC?=$(CROSS_COMPILE)gcc
TARGET?=gpioevt_client
CFLAGS+=-Wall -Wextra -g -O2

all: gpioevt_client.c
	$(CC) $(CFLAGS) $^ -o $(TARGET)

clean:
	rm -f $(TARGET)
