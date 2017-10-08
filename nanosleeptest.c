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

#define MAX 1000

int main(int argc, char **argv)
{
  double d1 = 0.0;
  double d2 = -fabs(d1);
  printf("%lf %lf\n",d1,d2);
  printf("%u %u\n",signbit(d1),signbit(d2));
  return;
  int i;
  long sleeps[MAX];
  struct timespec time[MAX];
  for(i = 0;i < MAX;i++)//First value isn't used
  {
    sleeps[i] = 100 + ((float)i/MAX)*1000000;
  }
  clock_gettime(CLOCK_MONOTONIC, &time[0]);
  for(i = 1;i < MAX;i++)
  {
    clock_gettime(CLOCK_MONOTONIC, &time[i]);
    nanosleep((const struct timespec[]){{0, sleeps[i]}}, NULL);
  }   
  for(i = 1;i < 1000;i++)
  {
    printf("%ld %ld\n", sleeps[i], time[i].tv_nsec);
  }
}
