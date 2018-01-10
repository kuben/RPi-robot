#include "rpm_sensor.h"
#include "utils.h"
#include <unistd.h>
#include <stdio.h>

#define MAX_TRIES 10
#define DIFF_LENGTH 100

int poll_sensor(int pin_read, int pin_write, int last_state, long *sleep_duration)
{
  pin_set(pin_write,!last_state);
  int i;
  for(i = 0;i < MAX_TRIES;i++)//Max tries is low, so we can react to no pulse also
  {
    //printf("gpio %d  ",!(GET_GPIO(pin_read)));
    if ((GET_GPIO(pin_read) > 0) != last_state) return 1;//Pulse
    nanosleep((const struct timespec[]){{0, *sleep_duration}}, NULL);
  }
  return -1;//Too many loop iterations
}

void* read_rpm(void *arguments)
{
  struct rpm_arg_struct *args = (struct rpm_arg_struct *)arguments;
  int pin_read = args->pin_read;
  int pin_write = args->pin_write;
  double *rpm = args->rpm;
  long *sleep_duration = args->sleep_duration;
  int *running = args->running;
  INP_GPIO(pin_read);
  INP_GPIO(pin_write);
  OUT_GPIO(pin_write);

  struct timespec ts_last;
  struct timespec ts_now;
  int last_state = 0;
  //diff_array saves times in msec
  long diff_array[DIFF_LENGTH] = {0};
  long diff = 0;
  clock_gettime(CLOCK_MONOTONIC, &ts_last);
  clock_gettime(CLOCK_MONOTONIC, &ts_now);
  while (*running)
  {
    int res = poll_sensor(pin_read, pin_write, last_state, sleep_duration);
    if (res == 1)//Pulse detected
    {
      ts_last = ts_now;//Only write over the old value on new pulse
      clock_gettime(CLOCK_MONOTONIC, &ts_now);
      timespec_diff_ms(&ts_last, &ts_now, &diff);//Put a new value in the array
      shift_into_array(diff_array,DIFF_LENGTH,diff);
      last_state = !last_state;
    } else if(res == -1) {
      clock_gettime(CLOCK_MONOTONIC, &ts_now);//'Last' still indicates last pulse
      timespec_diff_ms(&ts_last, &ts_now, diff_array);//This overwrites the first value in the array
    }
    snprintf(debug_text, 70, "diff {%ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld...\n", diff_array[0], diff_array[1], diff_array[2], diff_array[3], diff_array[4], diff_array[5], diff_array[6], diff_array[7], diff_array[8], diff_array[9], diff_array[10], diff_array[11], diff_array[12], diff_array[13], diff_array[14], diff_array[15], diff_array[16], diff_array[17], diff_array[18], diff_array[19], diff_array[20], diff_array[21], diff_array[22] );
    *rpm = rpm_from_diffarray(diff_array,DIFF_LENGTH);

    adjust_sleep_dur(sleep_duration, rpm);
  }
  return 0;
}

// TODO: detect too short pulses
double rpm_from_diffarray(long array[], int size)
{
  if (array[0] > 2000) return 0.0;//Rpm is 0 if still for > 2 sec. Will cause discontinuity
  int n = 0;
  long sum = 0;
  for (int i = 0; n < 15 && i < DIFF_LENGTH; i++)//Only integrate over last 15
  {
    if (array[i] > 40)//If less than 40 msec (25 rot per sec), then discard
    {
      n++;
      sum += array[i];
    }
  }
  if (sum == 0) return 0.0;
  return (double)1000*n/sum;
}

//Adjust own sleep_duration based on last rpm
//Let sleep duration be 1/20 of the frequency
int adjust_sleep_dur(long *sleep_duration, double *rpm)
{
  if (*rpm <= 0)
  {
    *sleep_duration = 500000000;//Let default be 0.5 sec
    return 0;
  }
  *sleep_duration = 50000000/ *rpm;//1/20 of frequency
  if (*sleep_duration > 500000000)//Don't let it be longer than 0.5 sec
  {
    *sleep_duration = 500000000;
  }
  return 0;
}
