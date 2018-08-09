CFLAGS = --use-non-free -mpic14 -p16f690 --std-c11 --opt-code-speed --cyclomatic
CC = sdcc

default: all
driver.hex: driver
blink_led.hex: blink

all: driver

driver: driver.o utils.o
	$(CC) $(CFLAGS) -o driver driver.o utils.o

utils.o: utils.c
	$(CC) $(CFLAGS) -co utils.o utils.c


blink: blink_led.c
	$(CC) $(CFLAGS) -o blink_led blink_led.c
clean:
	rm -f driver
