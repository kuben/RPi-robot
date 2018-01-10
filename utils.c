#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

void printButton(int g)
{
  if (GET_GPIO(g)) // !=0 <-> bit is 1 <- port is HIGH=3.3V
    printf("Button pressed!\n");
  else // port is LOW=0V
    printf("Button released!\n");
}

//Run until nsec have passed
//Busy wait
int delay(int nsec)
{
  struct timespec ts_start;
  struct timespec ts_end;
  long elapsed;
  int i;
  clock_gettime(CLOCK_MONOTONIC, &ts_start);
  for (i = 0;1;i++)
  //for (i = 0;i < 1000;i++)
  {
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    timespec_diff(&ts_start, &ts_end, &elapsed);
    if (elapsed > nsec) return i;
  }
  return 1;//Too many iterations
}

void pin_up(int pin)
{
  GPIO_SET = 1<<pin;
}

void pin_down(int pin)
{
  GPIO_CLR = 1<<pin;
}

void pin_set(int pin, int status)
{
  if(status) pin_up(pin);
  else pin_down(pin);
}
//Supports seconds, returns value in msec
void timespec_diff_ms(struct timespec *start, struct timespec *stop, long *msec)
{
  long sec_part = stop->tv_sec - start->tv_sec;
  sec_part *= 1000;
  *msec = (stop->tv_nsec - start->tv_nsec)/1000000 + sec_part;
  return;
}

//Supports seconds
void timespec_diff_s(struct timespec *start, struct timespec *stop, long *nsec)
{
  long sec_part = stop->tv_sec - start->tv_sec;
  sec_part *= 1000000000;
  *nsec = stop->tv_nsec - start->tv_nsec + sec_part;
  return;
}

//
void timespec_diff(struct timespec *start, struct timespec *stop, long *nsec)
{
  if ((stop->tv_nsec - start->tv_nsec) < 0) {
    *nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
  } else {
    *nsec = stop->tv_nsec - start->tv_nsec;
  }

  return;
}

//
// Set up a memory regions to access GPIO
//
void setup_io()
{
   /* open /dev/mem */
   if ((mem_fd = open("/dev/gpiomem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/gpiomem \n");
      exit(-1);
   }

   /* mmap GPIO */
   gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      BLOCK_SIZE,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      GPIO_BASE         //Offset to GPIO peripheral
   );

   close(mem_fd); //No need to keep mem_fd open after mmap

   if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      exit(-1);
   }

   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;


} // setup_io

void free_io()
{
  free(gpio_map);
}

int shift_into_array(long *array, int size, long element_in)
{
  //Will put element_in at index 0. Last element will disappear
  //Intended for type long
  memmove(array + 1, array, (size-1)*sizeof(long));
  array[0] = element_in;
  return 0;
}
