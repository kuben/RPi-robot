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
#include "analog_sample.h"

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define NSLEEPDELAY 65129
#define PIN_PROX 3

struct query_arg_struct
{
  float *battery_voltage;
  double *rpm_r;
  double *rpm_l;
  int *running;
};

void set_max(double *val, double max);
void set_min(double *val, double min);
void increase_by(double *val, double inc, double min, double max);

void manual_input(double *duty_l, double *duty_r);
int format_motor_text(char *text, int length, double *duty_cycle);

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
  double *rpm_l = malloc(sizeof(double))
  float *battery_voltage = malloc(sizeof(float))

  *duty_l = 0.0;//(double){atof(argv[1])};
  *duty_r = 0.0;//(double){atof(argv[2])};
  *rpm_r = 0.0;
  *rpm_l = 0.0;
  *battery_voltage = 0.0;

  //double temp;

  pthread_t query_thread;//Only one thread PIC for information

  struct rpm_arg_struct *args_query = malloc(sizeof(struct query_arg_struct));
  *args_query = (struct query_arg_struct){.battery_voltage = battery_voltage,
        .rpm_r = rpm_r, .rpm_l = rpm_l, .running = running};

  create_thread(&query_thread, query_adc, (void *)args_query);

  /* Initialize Curses*/
  initscr();
  noecho();
  raw();
  timeout(100);//Refresh every 0.1s
  int mode = 0;//0 - Increments, 1 - Hold for speed
  INP_GPIO(PIN_PROX);

  while (*running)
  {
    char l_text[10];
    char r_text[10];
    char status_text[50];

    format_motor_text(l_text,sizeof(l_text),duty_l);
    format_motor_text(r_text,sizeof(r_text),duty_r);
    snprintf(status_text, 50, "%lf", *batter_voltage);
    mvprintw(0,0,"LEFT %-10s   RIGHT %s  READ FREQ %s  (mode %d)\n",l_text,r_text,status_text,mode);

    snprintf(l_text, 10, "%lf", *rpm_r);
    snprintf(r_text, 10, "%lf", *rpm_r);

    mvprintw(1,0,"     %-10s         %s\n",l_text,r_text);

    //Read sensors

    //Channel 0 - Temperature
    //read_mcp3008(0, sensor_voltage);
    //temp = *sensor_voltage/0.11;//In celcius
    //mvprintw(2,0,"Temp %-4.1lf°C (%-4lfV)", temp, *sensor_voltage);

    //Channel 1 - Proximity
    //read_mcp3008(1, sensor_voltage);
    //mvprintw(4,0,"In proximity? (%-4lfV) ", *sensor_voltage);

    //Debugging string
    mvprintw(3,0,"%-50s\n",debug_text);
    refresh();

    move(3,0);
    int c = getch();
    if (c==-1) continue;//No input
    if (c=='l') open_oscilloscope();
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

  endwin();
  //Unneccessary
  pthread_join( query_thread, NULL);

  free(running);
  free(duty_l);
  free(duty_r);
  free(rpm_l);
  free(rpm_r);
  free(battery_voltage);
  free(args_query);

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
