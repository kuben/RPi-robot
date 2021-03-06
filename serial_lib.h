#ifndef SERIAL_LIB
#define SERIAL_LIB

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
#ifdef __arm__
#include <linux/spi/spidev.h>
#endif
#include <termios.h>

#include "utils.h"

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
#ifdef __arm__
int open_spi_power();
int open_spi_driver();
int close_spi_power();
int close_spi_driver();
int read_power_mcu(float *batt_voltage, float *current);
int read_mcp3008(int channel, float *voltage);
#endif

int open_uart();
void close_uart();
int tx_uart(uint8_t word);
int rx_uart_word(uint8_t *word);
int rx_uart_message(char *message, size_t size);
int format_serial_text(struct text_bar *bar);

#endif
