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
#include <stdarg.h>

#include "serial_lib.h"
#include "utils.h"
#ifdef __arm__
#include "analog_sample.h"

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define PIN_PROX 3
#endif
#define NSLEEPDELAY 65129

#ifdef __arm__
struct query_arg_struct
{
  double *rpm_r;
  double *rpm_l;
  int *running;
};
void *query_adc(void *arguments);
#endif

struct motor
{
  uint8_t speed;//Max is 31
  uint8_t mode;//0x00: brake, 0x80 forward, 0x40 reverse.
};

void set_max(struct motor *motor_struct);
void set_min(struct motor *motor_struct, uint8_t min);
void increase_by(struct motor *motor_struct, signed char inc, uint8_t min);
void set_forward(struct motor *motor_struct);
void set_reverse(struct motor *motor_struct);
void set_brake(struct motor *motor_struct);

void manual_input(struct motor *left, struct motor *right);
void send_over_uart();
int format_motor_text(struct text_bar *bar, struct motor *left, struct motor *right);

int keypress_mode_stepwise(char c, struct motor *left, struct motor *right);
int keypress_mode_dynamic (char c, struct motor *left, struct motor *right
        , uint8_t step_small, uint8_t step_big, uint8_t min);

void draw_text_bar(struct text_bar *bar)
{
    mvprintw(bar->y,bar->x,"%s",bar->text);
}

struct text_bar debug_bar = {0};
struct text_bar messages_bar = {0};
struct text_bar peripherals_bar = {0};
int *running;

void write_to_bar(struct text_bar *bar, const char *format, ...)
{
    va_list valist;
    va_start(valist, format);
    vsnprintf(bar->text,sizeof(bar->text), format, valist);
}

int main(int argc, char **argv)
{
  running = malloc(sizeof(int));
  *running = 1;

  struct motor left_motor = {0};
  struct motor right_motor = {0};
  right_motor.mode = 0x20;//Right bit set
  double *rpm_r = malloc(sizeof(double));
  double *rpm_l = malloc(sizeof(double));
  float batt_voltage = 0.0;
  float current_consumption= 0.0;
  *rpm_r = 0.0;
  *rpm_l = 0.0;

#ifdef __arm__
  // Set up gpi pointer for direct register access
  setup_io();

  INP_GPIO(PIN_PROX);
  //double temp;

  pthread_t query_thread;//Only one thread PIC for information

  struct query_arg_struct *args_query = malloc(sizeof(struct query_arg_struct));
  *args_query = (struct query_arg_struct){
        .rpm_r = rpm_r, .rpm_l = rpm_l, .running = running};

  create_thread(&query_thread, query_adc, (void *)args_query);
#endif
  //open_uart();

  /* Initialize Curses*/
  initscr();
  noecho();
  raw();
  timeout(100);//Refresh every 0.1s
  int mode = 0;//0 - Increments, 1 - Hold for speed
  struct text_bar motor_bar = {.x = 0, .y = 0};
  struct text_bar status_bar = {.x = 41, .y = 0};
  debug_bar.y = LINES - 3;
  messages_bar.y = LINES - 2;
  peripherals_bar.y = LINES - 1;
  write_to_bar(&peripherals_bar, "UART closed");
  write_to_bar(&messages_bar, "UART reply: ");

  char debug_str[50] = {0};
  while (*running)
  {
    //rx_uart_message(messages_bar.text+11, sizeof(messages_bar.text)-11);
    int ret = read_power_mcu(&batt_voltage, &current_consumption, debug_str);
    if (ret == 1){
//        write_to_bar(&status_bar, "Error in response over SPI");
        write_to_bar(&status_bar, debug_str);
    } else {
        //write_to_bar(&status_bar, debug_str);
        write_to_bar(&status_bar, "Read SPI: Batt = %.3fV  Current %.3fV  (mode %d)"
                , batt_voltage, current_consumption, mode);
    }
        
    format_motor_text(&motor_bar, &left_motor, &right_motor);

    draw_text_bar(&motor_bar);
    draw_text_bar(&status_bar);
    draw_text_bar(&debug_bar);
    draw_text_bar(&messages_bar);
    draw_text_bar(&peripherals_bar);

    //mvprintw(1,0,"%lf",*rpm_l);
    //mvprintw(1,10,"%lf\n",*rpm_r);

    //Read sensors

    //Channel 0 - Temperature
    //read_mcp3008(0, sensor_voltage);
    //temp = *sensor_voltage/0.11;//In celcius
    //mvprintw(2,0,"Temp %-4.1lf°C (%-4lfV)", temp, *sensor_voltage);

    //Channel 1 - Proximity
    //read_mcp3008(1, sensor_voltage);
    //mvprintw(4,0,"In proximity? (%-4lfV) ", *sensor_voltage);

    refresh();
    int c = getch();
    erase();
    move(3,0);
    if (c==-1) continue;//No input
#ifdef __arm__
    else if (c=='l') open_oscilloscope();
#endif
    else if (c=='n') tx_uart(0xc1);
    else if (c=='m') tx_uart(0xc2);
    if (mode == 0)
    {
      int res = keypress_mode_stepwise(c, &left_motor, &right_motor);
      if (res == 1)
      {
        mode = 1;
        timeout(100);
      }
    }/* else {
      int res = keypress_mode_dynamic(c, &speed_struct, 1, 3, 0);
      if (res == 1)
      {
        mode = 0;
        timeout(100);//Refresh every 0.1s
      }
    }*/
  }

  endwin();
  //close_uart();
#ifdef __arm__
  free_io();
  //Unneccessary
  pthread_join( query_thread, NULL);
  free(args_query);
#endif

  free(running);
  free(rpm_l);
  free(rpm_r);

  exit(EXIT_SUCCESS);

  return 0;

} // main

/*
 * Choose appropriate action depending on mode and key pressed
 * Duty cycle or setpoint depending on arguments
 * Return 1 when changing mode
 */
int keypress_mode_stepwise(char c, struct motor *left, struct motor *right)
{
  const char l_str[6] = "Left";
  const char r_str[6] = "Right";
  const char *side_text;//Pointer to "Left" or "Right"
  struct motor *side;
  switch(c) {
    case ' ':
      endwin();
      manual_input(left, right);
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
    case '3': case 'e': case 'd': case 'c': case 'w': case 's': case 'x':
      side_text = l_str;
      side = left;
      break;
    case '4': case 'r': case 'f': case 'v': case 't': case 'g': case 'b':
      side_text = r_str;
      side = right;
      break;
    default://65-68 up dn right left
      printw("%d %c invalid\n",c,c);
      return 0;
  }
  switch(c) {
    case '3': case '4':
      printw("%c: %s motor max speed\n",c, side_text);
      set_max(side);
      break;
    case 'e': case 'r':
      printw("%c: %s motor increase speed\n",c, side_text);
      increase_by(side, 1, 0);
      break;
    case 'd': case 'f':
      printw("%c: %s motor decrease speed\n",c, side_text);
      increase_by(side,-1, 0);
      break;
    case 'c': case 'v':
      printw("%c: %s motor min speed\n",c, side_text);
      set_min(side,0);
      break;
    case 'w': case 't':
      printw("%c: %s motor forward\n",c, side_text);
      set_forward(side);
      break;
    case 's': case 'g':
      printw("%c: %s motor reverse\n",c, side_text);
      set_reverse(side);
      break;
    case 'x': case 'b':
      printw("%c: %s motor brake\n",c, side_text);
      set_brake(side);
      break;
    default :
      break;
  }
  return 0;
}
/*
int keypress_mode_dynamic (char c, struct motor *speed_struct,
    uint8_t step_small, uint8_t step_big, uint8_t min)
{
  switch (c) {
    case 65:
            //TODO
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
*/
#ifdef __arm__
void *query_adc(void *arguments)
{

}
#endif

void transfer_uart(struct motor *motor_struct)
{
  uint8_t word = motor_struct->mode | motor_struct->speed;
  tx_uart(word);
  sprintf(debug_bar.text,"Transferring %#.2x (%c)", word, word);
}
//Hard-code max as 31
void set_max(struct motor *motor_struct)
{
  motor_struct->speed = 31;
  transfer_uart(motor_struct);
}

void set_min(struct motor *motor_struct, uint8_t min){
  motor_struct->speed = min;
  transfer_uart(motor_struct);
}

void increase_by(struct motor *motor_struct, signed char inc, uint8_t min)
{
  if((signed char)motor_struct->speed + inc >= 31)
    set_max(motor_struct);
  else if((signed char)motor_struct->speed + inc <= (signed char)min)
      set_min(motor_struct, min);
  else motor_struct->speed  = (uint8_t) ((signed char) motor_struct->speed + inc);
  transfer_uart(motor_struct);
}

void set_forward(struct motor *motor_struct)
{
  uint8_t side_bit = 0x20 & motor_struct->mode;
  motor_struct->mode = 0x80 | side_bit;//Leave side bit unchanged
  transfer_uart(motor_struct);
}
void set_reverse(struct motor *motor_struct)
{
  uint8_t side_bit = 0x20 & motor_struct->mode;
  motor_struct->mode = 0x40 | side_bit;//Leave side bit unchanged
  transfer_uart(motor_struct);
}
void set_brake(struct motor *motor_struct)
{
  uint8_t side_bit = 0x20 & motor_struct->mode;
  motor_struct->mode = 0x00 | side_bit;//Leave side bit unchanged
  transfer_uart(motor_struct);
}
//TODO replace scanf or entire function
void manual_input(struct motor *left, struct motor *right)
{
  printf("Set speed of left motor: ");
  fflush(stdout);
  scanf("%hhu",&left->speed);
  left->speed = 0x1f;
  transfer_uart(left);

  printf("Set speed of right motor: ");
  fflush(stdout);
  scanf("%hhu",&right->speed);
  right->speed &= 0x1f;
  transfer_uart(right);
}

void send_over_uart()
{
  char c;
  timeout(-1);
  endwin();
  open_uart();
  printf("Send char: ");
  fflush(stdout);
  c = getch();
  tx_uart(c);
  close_uart();

  initscr();
  timeout(100);
}

//Prints for example
//  L: 81% fwd [25/31]  R: 84% fwd [26/31]
int format_motor_text(struct text_bar *bar, struct motor *left, struct motor *right)
{
  //if (length < 41) return 1;
  const char *fwd = "fwd";
  const char *rev = "rev";
  const char *brk = "brk";
  const char *mode_str_l;
  const char *mode_str_r;
  if (left->mode == 0x80) mode_str_l = fwd;
  else if (left->mode == 0x40) mode_str_l = rev;
  else mode_str_l = brk;
  if (right->mode == 0xa0) mode_str_r = fwd;
  else if (right->mode == 0x60) mode_str_r = rev;
  else mode_str_r = brk;

  write_to_bar(bar, "L: %.3d%% %s [%.2d/31]  R: %.3d%% %s [%.2d/31]"
          ,left->speed*100/31,mode_str_l,left->speed
          ,right->speed*100/31,mode_str_r,right->speed);
  return 0;
}
