#ifndef RPM_SENSOR
#define RPM_SENSOR

//Define it here, because we need it in main file
struct rpm_arg_struct
{
  int pin_read;
  int pin_write;
  double *rpm;
  long *sleep_duration;
  int *running;
};

int poll_sensor(int pin_read, int pin_write, int last_state, long *sleep_duration);
void *read_rpm(void *arguments);
double rpm_from_diffarray(long array[], int size);
int adjust_sleep_dur(long *sleep_duration, double *rpm);
#endif
