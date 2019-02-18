#include "analog_sample.h"
#include "utils.h"
#include "serial_lib.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAX_TRIES 10
#define DIFF_LENGTH 100

int open_oscilloscope()
{
  float freq = 1000;//In Hz, set to Inf for no wait
  int buffer_size = 2000;
  int *channel = malloc(sizeof(int));
  //float i 4 bytes, timespec is maybe 12
  //2000 buffer points is 32kB
  float *sample_buffer = malloc(buffer_size*sizeof(float));
  struct timespec *time_buffer = malloc(buffer_size*sizeof(struct timespec));
  long *duration_ms = malloc(sizeof(long));
  int *running = malloc(sizeof(int));
  *running = 1;
  *channel = 0;
  *duration_ms = 1000000000;

  pthread_t a_s_thread;
  struct a_s_arg_struct *args_a_s = malloc(sizeof(*args_a_s));
  *args_a_s = (struct a_s_arg_struct){.sample_buffer = sample_buffer,
        .time_buffer = time_buffer, .buffer_size = buffer_size,
        .sample_freq = &freq, .channel = channel, .running = running,
        .duration_ms = duration_ms};

  create_thread(&a_s_thread, poll_analog, (void *)args_a_s);

  //Graph parameters
  struct graph_properties props = {
        .graph_width = COLS - 5,
        .graph_height = LINES - 5,
        .step = 1.0,//step*graph_width has to be <= than buffer_size. Allow float steps
        .y_max = 3.3,//Top of the graph, bottom is zero for now
        .y_ticks_max = 3.0,
        .y_ticks_step = 0.5,
        .y_ticks_min = 0.5,
        .t_ticks_step = 5,//In units
        .t_unit = "ms",
        .t_diff = timespec_diff_ms
  };
  props.buf_last = (int)((props.graph_width-1)*props.step);
  props.t_max = *duration_ms;//props.buf_last/freq;

  char *points = malloc(props.graph_width*props.graph_height);//Last char will be \x00

  timeout(-1);
  clear();
  refresh();
  WINDOW *top_win = newwin(1, COLS, 0, 0);
  WINDOW *yax_win = newwin(props.graph_height, 5, 1, 0);//y-axis
  WINDOW *graph_win = newwin(props.graph_height, props.graph_width, 1, 5);
  WINDOW *bot_win = newwin(4, COLS, props.graph_height + 1, 0);

  wtimeout(graph_win, 100);

  draw_y_axis(&props, yax_win);
  draw_t_axis(&props, bot_win, channel, *duration_ms);

  mvwprintw(top_win,0,0,"q quit  c change channel  s set step  l set last  t set t_max");
  wrefresh(top_win);
  while(*running)
  {
    //Clear graph
    memset(points,' ',props.graph_width*props.graph_height);
    for (int i = 0;i < props.graph_width;i++)
    {
      //Place point x=i/step at appropriate y position
      int buf_i = (int)(i*props.step);
      int y = (int)((1-sample_buffer[buf_i]/props.y_max)*props.graph_height);//y axis downwards
      if (y < 0) y=0;
      if (y >= props.graph_height) y = props.graph_height-1;
      points[y*props.graph_width + i] = '*';

      mvwprintw(graph_win,49,50,"%ld",*duration_ms);
    }
    points[props.graph_width*props.graph_height - 1] = 0;

    int c = wgetch(graph_win);//Refreshes graph_win
    switch (c) {
      case -1://No keypress, only update graph
        mvwprintw(graph_win,0,0,"%s",points);
        mvwprintw(graph_win,50,50,"buffer[0]='%f', buffer[0]/max='%ld'",
                sample_buffer[0],points);
        continue;
      case 'q':
        *running = 0;
        break;
      case 'c':
        *channel = (*channel + 1) % 8;
        mvwprintw(bot_win,3,0,"Changed channel to %d.",*channel);
        break;
      case 's':
        echo();
        wscanw(graph_win, "%f", &props.step);
        noecho();
        props.buf_last = (int)(props.graph_width*props.step);
        //props.t_max = props.buf_last/freq;
        mvwprintw(bot_win,3,0,"Changed step to %lf.",props.step);
        break;
      case 'l':
        echo();
        wscanw(graph_win, "%d", &props.buf_last);
        noecho();
        props.step = props.buf_last/(props.graph_width - 1);
        //props.t_max = last/freq;
        mvwprintw(bot_win,3,0,"Changed last to %d.",props.buf_last);
        break;
      case 't':
        echo();
        wscanw(graph_win, "%d", &props.t_max);
        noecho();
        //last = (int)(t_max*freq);
        //step = last/(graph_width - 1);
        mvwprintw(bot_win,3,0,"Changed t_max to %ld.",props.t_max);
        break;
      default:
        mvwprintw(bot_win,3,0,"%c invalid character", &c);
    }
    draw_t_axis(&props, bot_win, channel, duration_ms);
  }
  //Do something

  delwin(top_win);
  delwin(graph_win);
  delwin(bot_win);
  delwin(yax_win);
  timeout(100);
  clear();
  refresh();

  pthread_join(a_s_thread, NULL);
  free(points);
  free(args_a_s);
  free(channel);
  free(sample_buffer);
  free(time_buffer);
  free(running);
  return 0;

}

void draw_y_axis(struct graph_properties *props, WINDOW *yax_win)
{
  mvwprintw(yax_win,0,0,"[V] ^");
  mvwvline(yax_win,1,4,'|',props->graph_height-1);//Draw line (- character) starting from cursor pos
  for (float f = props->y_ticks_min; f <= props->y_ticks_max; f += props->y_ticks_step)
  {
    int y = (int)((1-f/props->y_max)*props->graph_height);//y axis downwards
    if (y < 0) y=0;
    if (y >= props->graph_height) y = props->graph_height-1;
    mvwprintw(yax_win,y,0,"%.2f+", f);
  }
  wrefresh(yax_win);
}

void draw_t_axis(struct graph_properties *props, WINDOW *bot_win, int *channel, long *duration_ms)
{
  mvwprintw(bot_win,0,0,"    +");
  //mvwprintw(bot_win,0,0,"    └");
  whline(bot_win,'-',props->graph_width);//Draw line (- character) starting from cursor pos
  //whline(bot_win,'─',graph_width);//Draw line ( character) starting from cursor pos
  for (long l = 0; l <= props->t_ticks_max; l += props->t_ticks_step)
  {
    int i = (props->graph_width - 1)*l/ *duration_ms;
    mvwprintw(bot_win,0,5+i,"+");
    mvwprintw(bot_win,1,5+i,"%ld",l);
  }
  mvwprintw(bot_win,2,0,"ch %d  step %-5lf  last %-8d  t_max %-8ld"
          ,*channel,props->step,props->buf_last,props->t_max);
  wrefresh(bot_win);
}

void* poll_analog(void *arguments)
{
  struct a_s_arg_struct *args = (struct a_s_arg_struct *)arguments;
  float *sample_buffer = args->sample_buffer;
  struct timespec *time_buffer = args->time_buffer;
  float *sample_freq = args->sample_freq;
  int *channel = args->channel;
  int buffer_size = args->buffer_size;
  int *running = args->running;

  float last_freq = *sample_freq;
  long period = (long)(1000000000/last_freq);
  struct timespec ts_last;
  struct timespec ts_now;
  clock_gettime(CLOCK_MONOTONIC, &ts_last);
  clock_gettime(CLOCK_MONOTONIC, &ts_now);

  int n = 0;
  while (*running)
  {
    if (n >= buffer_size)
    {
      timespec_diff_ms(time_buffer + buffer_size,time_buffer, args->duration_ms);
      n = 0;
    }

    //read_mcp3008(*channel, sample_buffer + n);
    clock_gettime(CLOCK_MONOTONIC, &ts_now);
    time_buffer[n] = ts_now;
    if (last_freq != *sample_freq)
    {
      last_freq = *sample_freq;
      period = (long)(1000000000/last_freq);
    }
    if (!isinf(last_freq))
    {
      //TODO implement wait
    }
    n++;
  }
  return 0;
}
