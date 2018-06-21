#include "analog_sample.h"
#include "utils.h"
#include "spi_lib.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <curses.h>

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
  int *running = malloc(sizeof(int));
  *running = 1;
  *channel = 0;

  pthread_t a_s_thread;
  struct a_s_arg_struct *args_a_s = malloc(sizeof(*args_a_s));
  *args_a_s = (struct a_s_arg_struct){.sample_buffer = sample_buffer,
        .time_buffer = time_buffer, .buffer_size = buffer_size,
        .sample_freq = &freq, .channel = channel, .running = running };

  create_thread(&a_s_thread, poll_analog, (void *)args_a_s);

  //Graph parameters
  int graph_width = COLS - 5;
  float step = 1.0;//step*graph_width has to be <= than buffer_size. Allow float steps
  int last = (int)((graph_width-1)*step);
  float t_max = last/freq;
  int graph_height = LINES - 5;
  float max = 3.3;//Top of the graph, bottom is zero for now
  char *points = malloc((graph_width+1)*graph_height);//+1 in graph_width for /n; Last char will be \x00

  timeout(-1);
  clear();
  refresh();
  WINDOW *top_win = newwin(1, COLS, 0, 0);
  WINDOW *yax_win = newwin(graph_height, 5, 1, 0);//y-axis
  WINDOW *graph_win = newwin(graph_height, graph_width, 1, 5);
  WINDOW *bot_win = newwin(4, COLS, graph_height + 1, 0);

  wtimeout(graph_win, 100);

  mvwprintw(yax_win,0,0,"[V] ^");
  for (int i = 1; i < graph_height; i++)
  {
    if(i%10)//Not dividible by 10
    {
      mvwprintw(yax_win,i,0,"    |");
    } else {
      mvwprintw(yax_win,i,0,"%.2f+", max-(max*i)/graph_height);
    }
  }
  wrefresh(yax_win);
  mvwprintw(bot_win,0,0,"    +");
  //mvwprintw(bot_win,0,0,"    └");
  whline(bot_win,'-',graph_width);//Draw line (- character) starting from cursor pos
  //whline(bot_win,'─',graph_width);//Draw line ( character) starting from cursor pos
  mvwprintw(bot_win,2,0,"ch %d  step %-5lf  last %-8d  t_max %-8lf",*channel,step,last,t_max);
  wrefresh(bot_win);
  mvwprintw(top_win,0,0,"q quit  c change channel  s set step  l set last  t set t_max");
  wrefresh(top_win);
  while(*running)
  {
    //Clear graph
    memset(points,' ',(graph_width+1)*graph_height);
    for (int i = 0;i < graph_width;i++)
    {
      //Place point x=i/step at appropriate y position
      int y = (int)((1-sample_buffer[(int)(i*step)]/max)*graph_height);//y axis downwards
      if (y < 0) y=0;
      if (y >= graph_height) y = graph_height-1;
      points[y*(graph_width+1) + i] = '*';
    }
    for (int i = 1;i <= graph_height; i++)
    {
      points[i*(graph_width+1) - 1] = '\n';
    }
    points[(graph_width+1)*graph_height - 1] = 0;

    int c = wgetch(graph_win);//Refreshes graph_win
    switch (c) {
      case -1://No keypress, only update graph
        mvwprintw(graph_win,0,0,"%s",points);
        continue;
      case 'q':
        *running = 0;
        break;
      case 'c':
        *channel = (*channel + 1) % 8;
        mvwprintw(bot_win,3,0,"Changed channel to %d.",*channel);
        break;
      case 's':
        wscanw(graph_win, "%f", &step);
        last = (int)((graph_width-1)*step);
        t_max = last/freq;
        mvwprintw(bot_win,3,0,"Changed step to %lf.",step);
        break;
      case 'l':
        wscanw(graph_win, "%d", &last);
        step = last/(graph_width - 1);
        t_max = last/freq;
        mvwprintw(bot_win,3,0,"Changed last to %d.",last);
        break;
      case 't':
        wscanw(graph_win, "%f", &t_max);
        last = (int)(t_max*freq);
        step = last/(graph_width - 1);
        mvwprintw(bot_win,3,0,"Changed t_max to %lf.",t_max);
        break;
      default:
        mvwprintw(bot_win,3,0,"%c invalid character", &c);
    }
    mvwprintw(bot_win,2,0,"ch %d  step %-5lf  last %-8d  t_max %-8lf",*channel,step,last,t_max);
    wrefresh(bot_win);
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
      n = 0;
    }

    read_mcp3008(*channel, sample_buffer + n);
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
