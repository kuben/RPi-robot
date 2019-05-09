#ifndef UTILS
#define UTILS

#include <time.h>
#include <pthread.h>

#define BCM2708_PERI_BASE        0x3F000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

struct timespec starting_time;

void init_time();
long running_time();

int  mem_fd;
void *gpio_map;

// I/O access
volatile unsigned *gpio;

struct text_bar
{
    char text[50];
    int x;//Position in curses window
    int y;
};
extern struct text_bar debug_bar;
extern struct text_bar messages_bar;
extern struct text_bar peripherals_bar;

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH

#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock

void setup_io();
void free_io();
void printButton(int g);

void create_thread(pthread_t * thread, void * (*start_routine)(void *), void *arg);

int delay(int nsec);
void pin_up(int pin);
void pin_down(int pin);
void pin_set(int pin, int status);
void timespec_diff_s(struct timespec *start, struct timespec *stop, long *nsec);
void timespec_diff_ms(struct timespec *start, struct timespec *stop, long *msec);
void timespec_diff(struct timespec *start, struct timespec *stop, long *nsec);
int shift_into_array(long *array, int size, long element_in);

#endif
