CFLAGS = -Wall -lncurses -pthread -O3 -Wno-format-truncation
CC = gcc

default: all

all: RPitest

RPitest: RPitest.c
	$(CC) $(CFLAGS) -o RPitest RPitest.c utils.c rpm_sensor.c spi_lib.c
clean:
	rm -f RPitest
