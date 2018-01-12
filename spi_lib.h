#ifndef SPI_LIB
#define SPI_LIB

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/*
struct sensor_arg_struct
{
  int pin_read;
  int pin_write;
  double *rpm;
  long *sleep_duration;
  int *running;
};

void *read_sensors(void *arguments);
*/
int read_mcp3008(int channel, uint16_t *left_response, uint16_t *right_response);
#endif
