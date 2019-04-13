CFLAGS = --use-non-free -mpic14 -p16f690 --std-c11 --opt-code-speed
CC = sdcc
program = power
RPi_host=192.168.0.106

default: $(program)

$(program): $(program).o ../utils.o
	$(CC) $(CFLAGS) -o $@ $^

$(program).o: $(program).c
	$(CC) $(CFLAGS) -c -o $(program).o $(program).c

utils.o: ../utils.c ../utils.h
	$(CC) $(CFLAGS) -c -o ../utils.o ../utils.c


.PHONY: upload clean cleanexecs
upload: $(program)
	cat $(program).hex | ssh pi@$(RPi_host) "/usr/local/bin/p14 program"
#	cat $(program).hex | ssh -t pi@$(RPi_host) "/usr/local/bin/p14 program"

clean:
	rm -f *.asm *.o *.lst *.cod
	rm -f ../*.asm ../*.o ../*.lst ../*.cod

cleanexecs: clean
	rm -f $(program).hex
