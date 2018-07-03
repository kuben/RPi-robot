CFLAGS = --use-non-free -mpic14 -p16f685 --std-c11
CC = sdcc

default: all
driver.hex: driver
blink_led.hex: blink

all: driver

driver: driver.o utils.o
	$(CC) $(CFLAGS) -o driver driver.o utils.o

utils.o: utils.c
	$(CC) $(CFLAGS) -c utils utils.c


blink: blink_led.c
	$(CC) $(CFLAGS) -o blink_led blink_led.c
clean:
	rm -f driver
