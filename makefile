CFLAGS = -Wall -lncurses -pthread -O3 -Wno-format-truncation
CC = gcc
DEPS = utils.h rpm_sensor.h spi_lib.h analog_sample.h
OBJECTS = RPitest.o utils.o rpm_sensor.o spi_lib.o analog_sample.o

default: all

all: RPi-robot

RPitest: $(OBJECTS)
	$(CC) $(CFLAGS) -o RPi-robot $(OBJECTS)

%.o: %.c $(DEPS)#Depends on all .h files now, unneccessary
	$(CC) -c $< $(CFLAGS) -o $@
clean:
	rm -f *.o
