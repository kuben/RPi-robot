//
//  How to access GPIO registers from C-code on the Raspberry-Pi
//  Example program
//  15-January-2012
//  Dom and Gert
//  Revised: 15-Feb-2013


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <curses.h>
#include <math.h>

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

void set_max(double *duty_cycle);
void set_min(double *duty_cycle);
void increase_by(double *duty_cycle, double inc);

void create_thread(pthread_t * thread, void * (*start_routine)(void *), void *arg);

void manual_input(double *duty_l, double *duty_r);
int format_motor_text(char *text, int length, double *duty_cycle);
void set_pin(int g,int state);
void *toggle_pins(void * arguments);
int poll_pin_n_waves(int g, int n, double *return_freq, int max_loop_iters);
void *poll_pin(void * arguments);

int main(int argc, char **argv)
{
  // Set up gpi pointer for direct register access
  setup_io();

  int *running = malloc(sizeof(int));
  *running = 1;

  double *duty_l = malloc(sizeof(double));
  double *duty_r = malloc(sizeof(double));
  double *rpm_r = malloc(sizeof(double));
  double *measured_freq = malloc(sizeof(double));
  long *sleep_duration = malloc(sizeof(long));
  long *rpm_sleep_duration = malloc(sizeof(long));//Will need one more

  *duty_l = 0.0;//(double){atof(argv[1])};
  *duty_r = 0.0;//(double){atof(argv[2])};
  *rpm_r = 0.0;
  *sleep_duration = 900000000;
  *rpm_sleep_duration = 1000000;//1ms
  *measured_freq = 0.0;
  
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
	  .pin_write = PIN_RPM_RW, .sleep_duration = rpm_sleep_duration};

  /* Create independent threads each of which will execute function */
  create_thread(&motor_l, toggle_pins, (void *)args_l);
  create_thread(&motor_r, toggle_pins, (void *)args_r);
  //create_thread(&poll_thread, poll_pin, (void *)args_poll);
  create_thread(&rpm_thread, read_rpm, (void *)args_rpm);

  /* Initialize Curses*/
  initscr();
  noecho();
  raw();
  timeout(-1);
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
  
    snprintf(l_text, 10, "%lf", *rpm_r);
    snprintf(r_text, 10, "%lf", *rpm_r);

    mvprintw(0,0,"LEFT %-10s   RIGHT %s  READ FREQ %s\n",l_text,r_text,status_text);
    mvprintw(1,0,"     %-10s         %s\n",l_text,r_text);
    refresh();

    move(2,0);
    int c = getch();
    if (mode == 0)
    {
      switch(c) {
        case '3':
          printw("%c: Left motor max speed\n",c);
          set_max(duty_l);
          break;
        case 'e':
          printw("%c: Left motor increase speed\n",c);
          increase_by(duty_l,0.1);
          break;
        case 'd':
          printw("%c: Left motor decrease speed\n",c);
          increase_by(duty_l,-0.1);
          break;
        case 'c':
          printw("%c: Left motor min speed\n",c);
          set_min(duty_l);
          break;
        case '4':
          printw("%c: Right motor max speed\n",c);
          set_max(duty_r);
          break;
        case 'r':
          printw("%c: Right motor increase speed\n",c);
          increase_by(duty_r,0.1);
          break;
        case 'f':
          printw("%c: Right motor decrease speed\n",c);
          increase_by(duty_r,-0.1);
          break;
        case 'v':
          printw("%c: Right motor min speed\n",c);
          set_min(duty_r);
          break;
        case 'w':
          printw("%c: Left motor forward\n",c);
	  *duty_l = fabs(*duty_l);
          break;
        case 's':
          printw("%c: Left motor reverse\n",c);
	  *duty_l = -fabs(*duty_l);
          break;
        case 't':
          printw("%c: Right motor forward\n",c);
	  *duty_r = fabs(*duty_r);
          break;
        case 'g':
          printw("%c: Right motor reverse\n",c);
	  *duty_r = -fabs(*duty_r);
          break;
        case ' ':
          endwin();
          manual_input(duty_l, duty_r);
          initscr();
          break;
        case '`':
          endwin();
	  *running = 0;
          *sleep_duration = -1;
          *rpm_sleep_duration = -1;
          return 0;
        case 'p':
          /*struct timespec ts_start;
            struct timespec ts_test;
            struct timespec ts_end;
            long elapsed;
            int k;
          //500ns just empty code
          //Av. 500ns - 700ns GET_GPIO(4)
          //Av. 1200ns measure time; GET_GPIO; measure time
          clock_gettime(CLOCK_MONOTONIC, &ts_start);

          clock_gettime(CLOCK_MONOTONIC, &ts_test);
          i = GET_GPIO(4);
          clock_gettime(CLOCK_MONOTONIC, &ts_test);

          clock_gettime(CLOCK_MONOTONIC, &ts_end);
          timespec_diff(&ts_start, &ts_end, &elapsed);
          if (i) printf("True Time elapsed: %ldns, state: %ld",elapsed,i);
          else printf("FalseTime elapsed: %ldns, state: %ld",elapsed,i);*/
          //for(int i = 0;i < 10000;i++)
          break;
        case 9://Tab
          mode = 1;
          timeout(100);
          break;
        case 'l':
          ;
          printw("gpio %d RPM %lf\n",GET_GPIO(PIN_RPM_RR),*rpm_r);
          break;
        default://65-68 up dn right left
          printw("%d %c invalid\n",c,c);
       }
    } else {
      switch (c) {
        case 65:
          if (!(signbit(*duty_r) | signbit(*duty_l)))
          {//Both forward - accelerate
            printw("Accelerating L, R\n");
            increase_by(duty_l, 0.05);
            increase_by(duty_r, 0.05);
          } else {//Both reverse or different dir.
            if ((*duty_l != 0.0) | (*duty_r != 0.0))
            {//Slow down
              printw("Slowing down L, R\n");
              increase_by(duty_l, -0.2);
              increase_by(duty_r, -0.2);
            } else {//Set direction to forward
	      *duty_l = fabs(*duty_l);
	      *duty_r = fabs(*duty_r);
            }
          }
          break;
        case 66:
          if (signbit(*duty_r) & signbit(*duty_l))
          {//Both reverse /- accelerate
            printw("Accelerating L, R\n");
            increase_by(duty_l, 0.05);
            increase_by(duty_r, 0.05);
          } else {//Both forward or different dir
            if ((*duty_l != 0.0) | (*duty_r != 0.0))
            {//Slow down
              printw("Slowing down L, R\n");
              increase_by(duty_l, -0.2);
              increase_by(duty_r, -0.2);
            } else {//Set direction to reverse
	      *duty_l = -fabs(*duty_l);
	      *duty_r = -fabs(*duty_r);
            }
	  }
          break;
        // Accelerate no matter what direction
        case 67:
          printw("Accelerating    R\n");
          increase_by(duty_r, 0.05);
          break;
        case 68:
          printw("Accelerating L\n");
          increase_by(duty_l, 0.05);
          break;
        case 9://Tab
          mode = 0;
          timeout(-1);
          break;
        case ',':
          timeout(-1);
          break;
        case '.':
          timeout(100);
          break;
        case ' ':
          set_min(duty_l);
          set_min(duty_r);
          break;
        case '`':
          endwin();
          *running = -1;
          *sleep_duration = -1;
          *rpm_sleep_duration = -1;
          return 0;
        case 27:
        case 91:
          break;
        default:
          printw("Slowing down\n");
          increase_by(duty_l, -0.05);
          increase_by(duty_r, -0.05);
      }
    }
  }

  //Unneccessary
  pthread_join( motor_l, NULL);
  pthread_join( motor_r, NULL);

  free(duty_l);
  free(duty_r);
  free(args_l);
  free(args_r);


  GPIO_CLR = 1<<PIN_FREQ_POLL;
  GPIO_CLR = 1<<PIN_PROX;
  GPIO_CLR = 1<<PIN_L_A;
  GPIO_CLR = 1<<PIN_L_B;
  GPIO_CLR = 1<<PIN_R_A;
  GPIO_CLR = 1<<PIN_R_B;

  free(gpio_map);
  exit(EXIT_SUCCESS);

  return 0;

} // main

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

void set_max(double *duty_cycle)
{
  *duty_cycle = signbit(*duty_cycle) ? -1.0 : 1.0;
}

void set_min(double *duty_cycle){
  *duty_cycle = signbit(*duty_cycle) ? -0.0 : 0.0;
}

void increase_by(double *duty_cycle, double inc)
{
  if(fabs(*duty_cycle) + inc >= 1.0) set_max(duty_cycle);
  else if(fabs(*duty_cycle) + inc <= 0.0) set_min(duty_cycle);
  else
  {
    double dc_abs = fabs(*duty_cycle) + inc;
    *duty_cycle = signbit(*duty_cycle) ? -dc_abs : dc_abs;
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
