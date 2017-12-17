CFLAGS = -Wall -lncurses -pthread -O3
CC = gcc

default: all

all: RPitest

RPitest: RPitest.c
	$(CC) $(CFLAGS) -o RPitest RPitest.c utils.c rpm_sensor.c
clean:
	rm -f RPitest
