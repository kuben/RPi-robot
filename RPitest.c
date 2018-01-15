//
//  How to access GPIO registers from C-code on the Raspberry-Pi
//  Example program
//  15-January-2012
//  Dom and Gert //  Revised: 15-Feb-2013


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <curses.h>
#include <math.h>

#include "spi_lib.h"
#include "utils.h"
#include "rpm_sensor.h"

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define NSLEEPDELAY 65129
#define PIN_FREQ_POLL 4
#define PIN_PROX 3
#define PIN_L_A 17
#define PIN_L_B 18
#define PIN_R_A 22
#define PIN_R_B 27
#define PIN_RPM_RR 5
#define PIN_RPM_RW 6
#define PIN_RPM_LR 12
#define PIN_RPM_LW 13

struct arg_struct
{
  double *duty_cycle;//From 0.0 to 1.0 for A. From -0.0 to -1.0 for B
  int g_A;
  int g_B;
  int *running;
};

struct poll_arg_struct
{
  int g;
  double *measured_freq;
  long *sleep_duration;
};

void set_max(double *val, double max);
void set_min(double *val, double min);
void increase_by(double *val, double inc, double min, double max);

void create_thread(pthread_t * thread, void * (*start_routine)(void *), void *arg);

void manual_input(double *duty_l, double *duty_r);
int format_motor_text(char *text, int length, double *duty_cycle);
void set_pin(int g,int state);
void *toggle_pins(void * arguments);
int poll_pin_n_waves(int g, int n, double *return_freq, int max_loop_iters);
void *poll_pin(void * arguments);

int keypress_mode_stepwise(char c, double *l_val, double *r_val,
    double step, double min, double max);
int keypress_mode_dynamic (char c, double *l_val, double *r_val,
    double step_small, double step_big, double min, double max);
char debug_text[100] = { 0 };

int *running;
int main(int argc, char **argv)
{
  // Set up gpi pointer for direct register access
  setup_io();

  running = malloc(sizeof(int));
  *running = 1;

  double *duty_l = malloc(sizeof(double));
  double *duty_r = malloc(sizeof(double));
  double *rpm_r = malloc(sizeof(double));
  double *measured_freq = malloc(sizeof(double));
  long *sleep_duration = malloc(sizeof(long));
  long *rpm_sleep_duration = malloc(sizeof(long));//Will need one more
  double *sensor_voltage = malloc(sizeof(double));

  *duty_l = 0.0;//(double){atof(argv[1])};
  *duty_r = 0.0;//(double){atof(argv[2])};
  *rpm_r = 0.0;
  *sleep_duration = 900000000;
  *rpm_sleep_duration = 1000000;//1ms
  *measured_freq = 0.0;
  *sensor_voltage = 0.0;

  double temp;

  pthread_t motor_l, motor_r, poll_thread, rpm_thread;
  struct arg_struct *args_l = malloc(sizeof(struct arg_struct));
  *args_l = (struct arg_struct){.duty_cycle = duty_l, .g_A = PIN_L_A,
	  .g_B = PIN_L_B, .running = running};

  struct arg_struct *args_r = malloc(sizeof(struct arg_struct));
  *args_r = (struct arg_struct){.duty_cycle = duty_r, .g_A = PIN_R_A,
	  .g_B = PIN_R_B, .running = running};

  struct poll_arg_struct *args_poll = malloc(sizeof(struct poll_arg_struct));
  *args_poll = (struct poll_arg_struct){.measured_freq = measured_freq, .g = PIN_FREQ_POLL,
	  .sleep_duration = sleep_duration};

  struct rpm_arg_struct *args_rpm = malloc(sizeof(struct rpm_arg_struct));
  *args_rpm = (struct rpm_arg_struct){.rpm = rpm_r, .pin_read = PIN_RPM_RR,
	  .pin_write = PIN_RPM_RW, .sleep_duration = rpm_sleep_duration, .running = running};

  /* Create independent threads each of which will execute function */
  create_thread(&motor_l, toggle_pins, (void *)args_l);
  create_thread(&motor_r, toggle_pins, (void *)args_r);
  //create_thread(&poll_thread, poll_pin, (void *)args_poll);
  create_thread(&rpm_thread, read_rpm, (void *)args_rpm);

  /* Initialize Curses*/
  initscr();
  noecho();
  raw();
  timeout(100);//Refresh every 0.1s
  int mode = 0;//0 - Increments, 1 - Hold for speed
  INP_GPIO(3);
  INP_GPIO(2);
  OUT_GPIO(2);

  while (*running)
  {
    char l_text[10];
    char r_text[10];
    char status_text[50];

    format_motor_text(l_text,sizeof(l_text),duty_l);
    format_motor_text(r_text,sizeof(r_text),duty_r);
    snprintf(status_text, 50, "%lf", *measured_freq);
    mvprintw(0,0,"LEFT %-10s   RIGHT %s  READ FREQ %s  (mode %d)\n",l_text,r_text,status_text,mode);

    snprintf(l_text, 10, "%lf", *rpm_r);
    snprintf(r_text, 10, "%lf", *rpm_r);

    mvprintw(1,0,"     %-10s         %s\n",l_text,r_text);

    //Read sensors

    //Channel 0 - Temperature
    read_mcp3008(0, sensor_voltage);
    temp = *sensor_voltage/0.11;//In celcius
    mvprintw(2,0,"Temp %-4.1lf°C (%-4lfV)", temp, *sensor_voltage);

    //Channel 1 - Proximity
    read_mcp3008(1, sensor_voltage);
    mvprintw(4,0,"In proximity? (%-4lfV) ", *sensor_voltage);

    //Debugging string
    mvprintw(3,0,"%-50s\n",debug_text);
    refresh();

    move(3,0);
    int c = getch();
    if (c==-1) continue;//No input
    if (mode == 0)
    {
      int res = keypress_mode_stepwise(c, duty_l, duty_r, 0.1, 0.0, 1.0);
      if (res == 1)
      {
        mode = 1;
        timeout(100);
      }
    } else {
      int res = keypress_mode_dynamic(c, duty_l, duty_r, 0.05, 0.2, 0.0, 1.0);
      if (res == 1)
      {
        mode = 0;
        timeout(100);//Refresh every 0.1s
      }
    }
  }
  *sleep_duration = -1;
  *rpm_sleep_duration = -1;

  endwin();
  //Unneccessary
  pthread_join( motor_l, NULL);
  pthread_join( motor_r, NULL);
  pthread_join( rpm_thread, NULL);

  free(running);
  free(duty_l);
  free(duty_r);
  free(rpm_r);
  free(measured_freq);
  free(sleep_duration);
  free(rpm_sleep_duration);
  free(sensor_voltage);
  free(args_l);
  free(args_r);
  free(args_poll);
  free(args_rpm);

  GPIO_CLR = 1<<PIN_FREQ_POLL;
  GPIO_CLR = 1<<PIN_PROX;
  GPIO_CLR = 1<<PIN_L_A;
  GPIO_CLR = 1<<PIN_L_B;
  GPIO_CLR = 1<<PIN_R_A;
  GPIO_CLR = 1<<PIN_R_B;

  free_io();
  exit(EXIT_SUCCESS);

  return 0;

} // main

/*
 * Choose appropriate action depending on mode and key pressed
 * Duty cycle or setpoint depending on arguments
 * Return 1 when changing mode
 */
int keypress_mode_stepwise(char c, double *l_val, double *r_val,
    double step, double min, double max)
{
  char l_str[6] = "Left";
  char r_str[6] = "Right";
  char *side;//Pointer to "Left" or "Right"
  double *val;
  switch(c) {
    case ' ':
      endwin();
      manual_input(l_val, r_val);
      initscr();
      return 0;
    case '`':
      endwin();
      *running = 0;
      return 0;
    case 9://Tab
      return 1;//Change mode
    //case 'l':
    //  ;
    //  printw("gpio %d RPM %lf\n",GET_GPIO(PIN_RPM_RR),*rpm_r);
    //  break;
    case '3': case 'e': case 'd': case 'c': case 'w': case 's':
      side = l_str;
      val = l_val;
      break;
    case '4': case 'r': case 'f': case 'v': case 't': case 'g':
      side = r_str;
      val = r_val;
      break;
    default://65-68 up dn right left
      printw("%d %c invalid\n",c,c);
      return 0;
  }
  switch(c) {
    case '3': case '4':
      printw("%c: %s motor max speed\n",c, side);
      set_max(val, max);
      break;
    case 'e': case 'r':
      printw("%c: %s motor increase speed\n",c, side);
      increase_by(val,step, min, max);
      break;
    case 'd': case 'f':
      printw("%c: %s motor decrease speed\n",c, side);
      increase_by(val,-step, min, max);
      break;
    case 'c': case 'v':
      printw("%c: %s motor min speed\n",c, side);
      set_min(val,min);
      break;
    case 'w': case 't':
      printw("%c: %s motor forward\n",c, side);
      *val= fabs(*val);
      break;
    case 's': case 'g':
      printw("%c: %s motor reverse\n",c, side);
      *val= -fabs(*val);
      break;
    default :
      break;
  }
  return 0;
}

int keypress_mode_dynamic (char c, double *l_val, double *r_val,
    double step_small, double step_big, double min, double max)
{
  switch (c) {
    case 65:
      if (!(signbit(*l_val) | signbit(*r_val)))
      {//Both forward - accelerate
        printw("Accelerating L, R\n");
        increase_by(l_val, step_small, min, max);
        increase_by(r_val, step_small, min, max);
      } else {//Both reverse or different dir.
        if ((*l_val != min) | (*r_val != min))
        {//Slow down
          printw("Slowing down L, R\n");
          increase_by(l_val, -step_big, min, max);
          increase_by(r_val, -step_big, min, max);
        } else {//Set direction to forward
          *l_val = fabs(*l_val);
          *r_val = fabs(*r_val);
        }
      }
      break;
    case 66:
      if (signbit(*l_val) & signbit(*r_val))
      {//Both reverse /- accelerate
        printw("Accelerating L, R\n");
        increase_by(l_val, step_small, min, max);
        increase_by(r_val, step_small, min, max);
      } else {//Both forward or different dir
        if ((*l_val != min) | (*r_val != min))
        {//Slow down
          printw("Slowing down L, R\n");
          increase_by(l_val, -step_big, min, max);
          increase_by(r_val, -step_big, min, max);
        } else {//Set direction to reverse
          *l_val= -fabs(*l_val);
          *r_val = -fabs(*r_val);
        }
      }
      break;
      // Accelerate no matter what direction
    case 67:
      printw("Accelerating R\n");
      increase_by(r_val, step_small, min, max);
      break;
    case 68:
      printw("Accelerating L\n");
      increase_by(l_val, step_small, min, max);
      break;
    case 9://Tab
      return 1;
    case ',':
      timeout(-1);
      break;
    case '.':
      timeout(100);
      break;
    case ' ':
      set_min(l_val, min);
      set_min(r_val, min);
      break;
    case '`':
      endwin();
      *running = 0;
      return 0;
    case 27:
    case 91:
      break;
    default:
      printw("Slowing down\n");
      increase_by(l_val, -step_small, min, max);
      increase_by(r_val, -step_small, min, max);
  }
  return 0;
}

void create_thread(pthread_t * thread, void * (*start_routine)(void *), void *arg)
{
  int iret = pthread_create(thread, NULL, start_routine, arg);
  if (iret)
  {
    fprintf(stderr,"Error - pthread_create() return code: %d\n",iret);
    exit(EXIT_FAILURE);
  }
  printf("Created pthread");
}

void set_pin(int g,int state){
  INP_GPIO(g); // must use INP_GPIO before we can use OUT_GPIO
  OUT_GPIO(g);
  if (state){
    GPIO_SET = 1<<g;
  } else {
    GPIO_CLR = 1<<g;
  }
}
//One loop iter approx 200ns
int poll_pin_n_waves(int g, int n, double *return_freq, int max_loop_iters)
{
  long elapsed[n+1];//Will ignore the first one because it is incorrect
  int i = 0;//Recorded rising edges
  int j = 0;//Total loop iterations
  INP_GPIO(g);

  struct timespec ts_last;//Last rising edge
  struct timespec ts_now;
  int state = 1;//Register when state goes from LOW to HIGH
  clock_gettime(CLOCK_MONOTONIC, &ts_last);
  while (i < n + 1)
  {
    if (j > max_loop_iters)
    {
      if (i > 0) return 1;//Timed out, but some waves recorded
      return 2;
    }
    if (GET_GPIO(g))//Current state is HIGH
    {
      if (!state)//And last state was LOW.. Rising edge
      {
        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        timespec_diff(&ts_last, &ts_now, &elapsed[i]);
        ts_last = ts_now;
        i++;
      }
      state = 1;//State is now HIGH
    } else {//State is LOW
      state = 0;
    }
    j++;
  }

  //Calculate mean
  long sum = 0;
  for (i = 1;i<n+1;i++) sum += elapsed[i];

  //Convert ns to s, and invert to Hz
  *return_freq = (double)1000000000.0*n/sum;
  return 0;
}
//Will not be able to measure 1Hz bc gettime loops around 1s
void *poll_pin(void * arguments)
{
  struct poll_arg_struct *args = (struct poll_arg_struct *)arguments;
  int g = args->g;
  double *measured_freq = args->measured_freq;
  long *sleep_duration= args->sleep_duration;
  INP_GPIO(g);

  while (*sleep_duration >= 0)
  {
    //For now, assume not measuring shorter than 100Hz
    //10 waves is 0.1 s => 500 000 loop iters
    int result = poll_pin_n_waves(g, 10, measured_freq, 5000);//500 000
    if (result == 1) *measured_freq = -1.0;
    else if (result == 2)*measured_freq = -2.0;
    nanosleep((const struct timespec[]){{0, *sleep_duration}}, NULL);
  }
  return 0;
}
void* toggle_pins(void *arguments)
{
  struct arg_struct *args = (struct arg_struct *)arguments;
  int *running = args->running;
  int g_A = args->g_A;
  int g_B = args->g_B;
  int g;
  double *duty_cycle= args->duty_cycle;
  double freq = 100;//[Hertz]
  double last_dc;
  long period = (long)(1000000000/freq);
  //printf("%d : Freq: %f\n",g,*freq);
  INP_GPIO(g_A); // must use INP_GPIO before we can use OUT_GPIO
  OUT_GPIO(g_A);
  INP_GPIO(g_B);
  OUT_GPIO(g_B);
  while (*running)
  {
    last_dc = *duty_cycle;//So we only have to do division when freq changes
    if (last_dc == 0)//Motor still, outputs LOW
    {
      GPIO_CLR = 1<<g_A;//Make sure output is always LOW
      GPIO_CLR = 1<<g_B;//Make sure output is always LOW
      nanosleep((const struct timespec[]){{0, 1000000}}, NULL);//Sleep for 1ms before checking again
    } else {
      g = last_dc > 0 ? g_A : g_B;//toggle A if forward
      long time_HIGH = (long)(period*fabs(last_dc));
      long elapsed;
      struct timespec ts_start;
      struct timespec ts_mid;
      struct timespec ts_end;
      clock_gettime(CLOCK_MONOTONIC, &ts_end);
      while (last_dc == *duty_cycle)
      {
        GPIO_SET = 1<<g;
        clock_gettime(CLOCK_MONOTONIC, &ts_start);
        timespec_diff(&ts_end, &ts_start, &elapsed);
	//nanosleep always 60us too long
        nanosleep((const struct timespec[]){{0, time_HIGH-elapsed-NSLEEPDELAY}}, NULL);
        GPIO_CLR = 1<<g;
        clock_gettime(CLOCK_MONOTONIC, &ts_mid);
        timespec_diff(&ts_end, &ts_mid, &elapsed);
        nanosleep((const struct timespec[]){{0, period-elapsed-NSLEEPDELAY}}, NULL);
        clock_gettime(CLOCK_MONOTONIC, &ts_end);
      }
    }
  }
  return 0;
}

void set_max(double *val, double max)
{
  *val = signbit(*val) ? -max : max;
}

void set_min(double *val, double min){
  *val = signbit(*val) ? -min : min;
}

void increase_by(double *val, double inc, double min, double max)
{
  if(fabs(*val) + inc >= max) set_max(val, max);
  else if(fabs(*val) + inc <= min) set_min(val, min);
  else
  {
    double dc_abs = fabs(*val) + inc;
    *val= signbit(*val) ? -dc_abs : dc_abs;
  }
}

void manual_input(double *duty_l, double *duty_r)
{
  printf("Set duty cycle of left motor: ");
  fflush(stdout);
  scanf("%lf",duty_l);

  printf("Set duty cycle of right motor: ");
  fflush(stdout);
  scanf("%lf",duty_r);
}

int format_motor_text(char *text, int length, double *duty_cycle)
{
  if (length < 4) return 1;

  char dir_char = 'F';
  if (signbit(*duty_cycle)) dir_char = 'R';

  strncpy(text,"OFF",length);
  snprintf(text, length-3, "%lf", *duty_cycle);//Leave space at end for at least 3
  snprintf(text + strlen(text) - 1, 3, " %c", dir_char);//-1 to overwrite trailing null
  return 0;
}
