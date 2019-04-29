# Some files can only be compiled on device with gpio and spi and so on..
ARCH := $(shell uname -m)
OBJECTS_x86_64 = RPi-robot.o utils.o serial_lib.o
OBJECTS_armv7l = RPi-robot.o utils.o serial_lib.o analog_sample.o
OBJECTS_armv6l = RPi-robot.o utils.o serial_lib.o analog_sample.o
OBJECTS = $(OBJECTS_$(ARCH))
DEPS = $(OBJECTS:.o=.d)  # one dependency file for each source

CFLAGS = -Wall -lncurses -pthread -O3 -Wno-format-truncation
CC = gcc
CPP = cpp

default: RPi-robot

RPi-robot: $(OBJECTS)
	$(CC) $(CFLAGS) -o RPi-robot $(OBJECTS)

# Rules for building objects from dependency files
-include $(DEPS)   # include all dep files in the makefile

# rule to generate a dep file by using the C preprocessor
%.d: %.c
	@$(CPP) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

.PHONY: clean
clean:
	rm -f $(OBJECTS)

.PHONY: cleandeps
cleandep:
	rm -f $(DEPS)
