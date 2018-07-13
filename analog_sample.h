#ifndef ANALOG_SAMPLE
#define ANALOG_SAMPLE

#include <pthread.h>
#include <curses.h>
#include <time.h>

struct a_s_arg_struct
{
  float *sample_buffer;
  struct timespec *time_buffer;
  float *sample_freq;
  int buffer_size;
  int *channel;
  int *running;
  long *duration_ms;
};

struct graph_properties
{
  int graph_width;
  int graph_height;

  float step;
  int buf_last;
  float y_max;
  long t_max;

  float y_ticks_max;
  float y_ticks_step;
  float y_ticks_min;

  long t_ticks_step;
  long t_ticks_max;
  char t_unit[3];//us, ms or s
  void (*t_diff)(struct timespec *, struct timespec *, long *);
};

void switch_unit(struct graph_properties props);
void unit_s(struct graph_properties props);
void unit_ms(struct graph_properties props);
void unit_us(struct graph_properties props);

void draw_y_axis(struct graph_properties *props, WINDOW *yax_win);
void draw_t_axis(struct graph_properties *props, WINDOW *bot_win, int *channel, long *duration_ms);

int open_oscilloscope();
void *poll_analog(void *arguments);
#endif
