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

struct arg_struct
{
  double *freq;
  int g;
};

void setup_io();
void timespec_diff(struct timespec *start, struct timespec *stop, long *result);
void set_pin(int g,int state);
void *toggle_pins(void * arguments);

void printButton(int g)
{
  if (GET_GPIO(g)) // !=0 <-> bit is 1 <- port is HIGH=3.3V
    printf("Button pressed!\n");
  else // port is LOW=0V
    printf("Button released!\n");
}

int main(int argc, char **argv)
{
  // Set up gpi pointer for direct register access
  setup_io();

  pthread_t motor_l, motor_r;
  int iret1, iret2;
  double *freq_l = malloc(sizeof(double));
  double *freq_r = malloc(sizeof(double));

  *freq_l = (double){atof(argv[1])};
  *freq_r = (double){atof(argv[2])};

  struct arg_struct *args_l = malloc(sizeof(struct arg_struct));
  args_l->freq = freq_l;
  args_l->g = 17;

  struct arg_struct *args_r = malloc(sizeof(struct arg_struct));
  args_r->freq = freq_r;
  args_r->g = 18;

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

  printf("pthread_create() for thread 1 returns: %d\n",iret1);
  printf("pthread_create() for thread 2 returns: %d\n",iret2);

  char l_text[10];
  char r_text[10];
  int polarity;
  while (*freq_l >= 0 || *freq_r >= 0)
  {/*
    strncpy(l_text,"OFF",10);
    strncpy(r_text,"OFF",10);
    if (*freq_l >= 0) snprintf(l_text, 10, "%lf", *freq_l);
    if (*freq_r >= 0) snprintf(r_text, 10, "%lf", *freq_r);
    
    printf("LEFT %-10s   RIGHT %s\n",l_text,r_text);
    char c = getchar();
    printf("Char: %c", c);*/

    /*
    printf("Set freqency of left motor: ");
    fflush(stdout);
    scanf("%lf",freq_l);

    printf("Set freqency of right motor: ");
    fflush(stdout);
    scanf("%lf",freq_r);*/
    printf("Set polarity \n");
    scanf("%d", &polarity);

    set_pin(22,CHECK_BIT(polarity,0));
    set_pin(27,CHECK_BIT(polarity,1));
    
  }

  //Unneccessary
  pthread_join( motor_l, NULL);
  pthread_join( motor_r, NULL);

  free(freq_l);
  free(freq_r);
  free(args_l);
  free(args_r);

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
void* toggle_pins(void *arguments)
{
  struct arg_struct *args = (struct arg_struct *)arguments;
  int rep;
  int g = args->g;
  double *freq = args->freq;
  double last_freq;
  //printf("%d : Freq: %f\n",g,*freq);
  // Set GPIO pins 11 to output
  INP_GPIO(g); // must use INP_GPIO before we can use OUT_GPIO
  OUT_GPIO(g);

  while (*freq >= 0)
  {
    last_freq = *freq;
    long halfperiod = (long)(500000000/ *freq);
    long period = 2*halfperiod;
    //printf("%d : Halfperiod: %dns\n",g,halfperiod);

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
      nanosleep((const struct timespec[]){{0, halfperiod-elapsed}}, NULL);
      GPIO_CLR = 1<<g;
      clock_gettime(CLOCK_MONOTONIC, &ts_mid);
      timespec_diff(&ts_end, &ts_mid, &elapsed);
      nanosleep((const struct timespec[]){{0, period-elapsed}}, NULL);
      clock_gettime(CLOCK_MONOTONIC, &ts_end);
    }

  }

}

//
// Set up a memory regions to access GPIO
//
void setup_io()
{
   /* open /dev/mem */
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
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

void timespec_diff(struct timespec *start, struct timespec *stop, long *nsec)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        *nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        *nsec = stop->tv_nsec - start->tv_nsec;
    }

    return;
}
