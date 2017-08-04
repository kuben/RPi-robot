//
//  How to access GPIO registers from C-code on the Raspberry-Pi
//  Example program
//  15-January-2012
//  Dom and Gert
//  Revised: 15-Feb-2013


// Access from ARM Running Linux

#define BCM2708_PERI_BASE        0x3F000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <curses.h>

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

int  mem_fd;
void *gpio_map;

// I/O access
volatile unsigned *gpio;


// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH

#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define NSLEEPDELAY 65129
struct arg_struct
{
  double *freq;
  int g;
};

struct poll_arg_struct
{
  int g;
  double *measured_freq;
  long *sleep_duration;
};

void set_max(double *freq);
void set_min(double *freq);
void increase(double *freq);
void decrease(double *freq);
void increase_by(double *freq, double inc);


void setup_io();
void manual_input(double *freq_l, double *freq_r, int *polarity);
void timespec_diff(struct timespec *start, struct timespec *stop, long *result);
void set_pin(int g,int state);
void *toggle_pins(void * arguments);
int poll_pin_n_waves(int g, int n, double *return_freq, int max_loop_iters);
void *poll_pin(void * arguments);

void printButton(int g)
{
  if (GET_GPIO(g)) // !=0 <-> bit is 1 <- port is HIGH=3.3V
    printw("Button pressed!\n");
  else // port is LOW=0V
    printw("Button released!\n");
}

int main(int argc, char **argv)
{
  // Set up gpi pointer for direct register access
  setup_io();

  pthread_t motor_l, motor_r, poll_thread;
  int iret1, iret2, iret3;
  double *freq_l = malloc(sizeof(double));
  double *freq_r = malloc(sizeof(double));
  double *measured_freq = malloc(sizeof(double));
  long *sleep_duration = malloc(sizeof(long));

  *freq_l = (double){atof(argv[1])};
  *freq_r = (double){atof(argv[2])};
  *sleep_duration = 900000000;
  *measured_freq = 0.0;
  int polarity = 0;

  struct arg_struct *args_l = malloc(sizeof(struct arg_struct));
  args_l->freq = freq_l;
  args_l->g = 17;

  struct arg_struct *args_r = malloc(sizeof(struct arg_struct));
  args_r->freq = freq_r;
  args_r->g = 18;

  struct poll_arg_struct *args_poll = malloc(sizeof(struct poll_arg_struct));
  args_poll->g = 4;
  args_poll->measured_freq = measured_freq;
  args_poll->sleep_duration = sleep_duration;

  /* Create independent threads each of which will execute function */
  iret1 = pthread_create( &motor_l, NULL, toggle_pins, (void *)args_l);
  if(iret1)
  {
	  fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
	  exit(EXIT_FAILURE);
  }

  iret2 = pthread_create( &motor_r, NULL, toggle_pins, (void *)args_r);
  if(iret2)
  {
	  fprintf(stderr,"Error - pthread_create() return code: %d\n",iret2);
	  exit(EXIT_FAILURE);
  }

  iret3 = pthread_create( &poll_thread, NULL, poll_pin, (void *)args_poll);
  if(iret3)
  {
	  fprintf(stderr,"Error - pthread_create() return code: %d\n",iret3);
	  exit(EXIT_FAILURE);
  }

  printf("pthread_create() for thread 1 returns: %d\n",iret1);
  printf("pthread_create() for thread 2 returns: %d\n",iret2);
  printf("pthread_create() for thread 3 returns: %d\n",iret3);

  /* Initialize Curses*/
  initscr();
  noecho();
  raw();
  timeout(-1);
  int mode = 0;//0 - Increments, 1 - Hold for speed

  while (*freq_l >= 0 || *freq_r >= 0)
  {
    char l_text[10];
    char r_text[10];
    char status_text[50];
    strncpy(l_text,"OFF",10);
    strncpy(r_text,"OFF",10);
    if (*freq_l >= 0) snprintf(l_text, 10, "%lf", *freq_l);
    if (*freq_r >= 0) snprintf(r_text, 10, "%lf", *freq_r);
    snprintf(status_text, 50, "%lf", *measured_freq);

    mvprintw(0,0,"LEFT %-10s   RIGHT %s  POLARITY %d READ FREQ %s\n",l_text,r_text,polarity,status_text);
    refresh();

    set_pin(22,CHECK_BIT(polarity,0));
    set_pin(27,CHECK_BIT(polarity,1));

    move(1,0);
    int c = getch();
    if (mode == 0)
    {
      switch(c) {
	case '3':
	  printw("%c: Left motor max speed\n",c);
	  set_max(freq_l);
	  break;
	case 'e':
	  printw("%c: Left motor increase speed\n",c);
	  increase(freq_l);
	  break;
	case 'd':
	  printw("%c: Left motor decrease speed\n",c);
	  decrease(freq_l);
	  break;
	case 'c':
	  printw("%c: Left motor min speed\n",c);
	  set_min(freq_l);
	  break;
	case '4':
	  printw("%c: Right motor max speed\n",c);
	  set_max(freq_r);
	  break;
	case 'r':
	  printw("%c: Right motor increase speed\n",c);
	  increase(freq_r);
	  break;
	case 'f':
	  printw("%c: Right motor decrease speed\n",c);
	  decrease(freq_r);
	  break;
	case 'v':
	  printw("%c: Right motor min speed\n",c);
	  set_min(freq_r);
	  break;
	case ' ':
	  endwin();
	  manual_input(freq_l, freq_r, &polarity);
	  initscr();
	  break;
	case '`':
	  endwin();
	  *freq_l = -1;
	  *freq_r = -1;
	  *sleep_duration = -1;
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
	default://65-68 up dn right left
	  printw("%d %c invalid\n",c,c);
    }
    } else {
      switch (c) {
	case 65:
	  printw("Accelerating L, R\n");
	  increase_by(freq_l, 50.0);
	  increase_by(freq_r, 50.0);
	  break;
	case 66:
	  printw("Slowing down L, R\n");
	  increase_by(freq_l, -50.0);
	  increase_by(freq_r, -50.0);
	  break;
	case 67:
	  printw("Accelerating    R\n");
	  increase_by(freq_r, 50.0);
	  break;
	case 68:
	  printw("Accelerating L\n");
	  increase_by(freq_l, 50.0);
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
	  set_min(freq_l);
	  set_min(freq_r);
	  break;
	case '`':
	  endwin();
	  *freq_l = -1;
	  *freq_r = -1;
	  *sleep_duration = -1;
	  return 0;
	case 27:
	case 91:
	  break;
	default:
	  printw("Slowing down\n");
          increase_by(freq_l, -10.0);
	  increase_by(freq_r, -10.0);
      }
    }
  }

  //Unneccessary
  pthread_join( motor_l, NULL);
  pthread_join( motor_r, NULL);

  free(freq_l);
  free(freq_r);
  free(args_l);
  free(args_r);

  free(gpio_map);
  exit(EXIT_SUCCESS);

  return 0;

} // main
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
  int g = args->g;
  double *freq = args->freq;
  double last_freq;
  //printf("%d : Freq: %f\n",g,*freq);
  INP_GPIO(g); // must use INP_GPIO before we can use OUT_GPIO
  OUT_GPIO(g);
  while (*freq >= 0)
  {
    last_freq = *freq;//So we only have to do division when freq changes
    if (last_freq == 0)//Special case, don'w want div by 0
    {
      GPIO_CLR = 1<<g;//Make sure output is always LOW
      nanosleep((const struct timespec[]){{0, 1000000}}, NULL);//Sleep for 1ms before checking again
    } else {
      long halfperiod = (long)(500000000/ last_freq);
      long period = 2*halfperiod;
      printf("%d : Halfperiod: %ldns\n",g,halfperiod);

      long elapsed;
      struct timespec ts_start;
      struct timespec ts_mid;
      struct timespec ts_end;
      clock_gettime(CLOCK_MONOTONIC, &ts_end);
      while (last_freq == *freq)
      {
        GPIO_SET = 1<<g;
        clock_gettime(CLOCK_MONOTONIC, &ts_start);
        timespec_diff(&ts_end, &ts_start, &elapsed);
	//nanosleep always 60us too long
        nanosleep((const struct timespec[]){{0, halfperiod-elapsed-NSLEEPDELAY}}, NULL);
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

//
// Set up a memory regions to access GPIO
//
void setup_io()
{
   /* open /dev/mem */
   if ((mem_fd = open("/dev/gpiomem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/gpiomem \n");
      exit(-1);
   }

   /* mmap GPIO */
   gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      BLOCK_SIZE,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      GPIO_BASE         //Offset to GPIO peripheral
   );

   close(mem_fd); //No need to keep mem_fd open after mmap

   if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      exit(-1);
   }

   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;


} // setup_io
void set_max(double *freq)
{
  *freq = 2500;
}

void set_min(double *freq){
  *freq = 0;
}

void increase(double *freq)
{
  if(*freq > 2400) set_max(freq);
  else *freq += 100;
}
void decrease(double *freq)
{
  if(*freq < 100) set_min(freq);
  else *freq -= 100;
}
void increase_by(double *freq, double inc)
{
  if(*freq + inc > 2500) set_max(freq);
  else if(*freq + inc < 0) set_min(freq);
  else *freq += inc;
}

void manual_input(double *freq_l, double *freq_r, int *polarity)
{
  printf("Set freqency of left motor: ");
  fflush(stdout);
  scanf("%lf",freq_l);

  printf("Set freqency of right motor: ");
  fflush(stdout);
  scanf("%lf",freq_r);
  printf("Set polarity: ");
  scanf("%d", polarity);


}
void timespec_diff(struct timespec *start, struct timespec *stop, long *nsec)
{
  if ((stop->tv_nsec - start->tv_nsec) < 0) {
    *nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
  } else {
    *nsec = stop->tv_nsec - start->tv_nsec;
  }

  return;
}
