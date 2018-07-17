#ARCH := $(shell gcc -dumpmachine | awk -F- '{print $1}')
ARCH := $(shell uname -m)
DEPS_x86_64 = utils.h serial_lib.h
OBJECTS_x86_64 = RPi-robot.o utils.o serial_lib.o
DEPS_armv7l = utils.h serial_lib.h analog_sample.h
OBJECTS_armv7l = RPi-robot.o utils.o serial_lib.o analog_sample.o
DEPS = $(DEPS_$(ARCH))
OBJECTS = $(OBJECTS_$(ARCH))

CFLAGS = -Wall -lncurses -pthread -O3 -Wno-format-truncation
CC = gcc

default: all

all: RPi-robot

RPi-robot: $(OBJECTS)
	$(CC) $(CFLAGS) -o RPi-robot $(OBJECTS)

%.o: %.c $(DEPS)#Depends on all .h files now, unneccessary
	$(CC) -c $< $(CFLAGS) -o $@
clean:
	rm -f *.o
