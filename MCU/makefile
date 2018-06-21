CFLAGS = --use-non-free -mpic14 -p16f685 --std-c11
CC = sdcc

default: all
driver.hex: driver
blink_led.hex: blink

all: driver

driver: driver.c
	$(CC) $(CFLAGS) -o driver driver.c

blink: blink_led.c
	$(CC) $(CFLAGS) -o blink_led blink_led.c
clean:
	rm -f driver
