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
  char l_str[6] = "Left";
  char r_str[6] = "Right";
  l_str[2] = 'e';
  char *str = l_str;
  printf("%s%s%s\n",str,str,str);
  for (int j = 0; j < 6; j++ ) {
     printf("Element[%d] = %d: %c\n", j, str[j], str[j] );
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
