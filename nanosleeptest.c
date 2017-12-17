#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <curses.h>
#include <math.h>

#include "utils.h"

#define MAX 1000

int main(int argc, char **argv)
{
  long test[4] = {1,2,3,4};
  for (int j = 0; j < 4; j++ ) {
     printf("Element[%d] = %d\n", j, test[j] );
  }
  shift_into_array(test,4,5);
  for (int j = 0; j < 4; j++ ) {
     printf("Element[%d] = %d\n", j, test[j] );
  }
  /*
  setup_io();
  INP_GPIO(5);
  int i;
  //long sleeps[MAX];
  struct timespec time[MAX];
  //for(i = 0;i < MAX;i++)//First value isn't used
  //{
  //  sleeps[i] = 100 + ((float)i/MAX)*1000000;
  //}
  clock_gettime(CLOCK_MONOTONIC, &time[0]);
  for(i = 1;i < MAX;i++)
  {
    clock_gettime(CLOCK_MONOTONIC, &time[i]);
//    nanosleep((const struct timespec[]){{0, sleeps[i]}}, NULL);
    nanosleep((const struct timespec[]){{0, 100000000}}, NULL);
    printButton(5);
  }   
//  for(i = 1;i < 1000;i++)
//  {
//    printf("%ld %ld\n", sleeps[i], time[i].tv_nsec);
//  }
    */
}
