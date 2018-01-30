#ifndef ANALOG_SAMPLE
#define ANALOG_SAMPLE

#include <pthread.h>

struct a_s_arg_struct
{
  float *sample_buffer;
  struct timespec *time_buffer;
  float *sample_freq;
  int buffer_size;
  int *channel;
  int *running;
};

int open_oscilloscope();
void *poll_analog(void *arguments);
#endif
