/*
 * SPI testing utility (using spidev driver)
 *
 * Copyright (c) 2007  MontaVista Software, Inc.
 * Copyright (c) 2007  Anton Vorontsov <avorontsov@ru.mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * Cross-compile with cross-gcc -I/path/to/cross-kernel/include
 */

#include "serial_lib.h"
#include "utils.h"

static void pabort(const char *s)
{
    perror(s);
    abort();
}

#ifdef __arm__
static const float V_ref = 3.3;
static const char *spi_device_power = "/dev/spidev0.0";
static const char *spi_device_driver = "/dev/spidev1.0";
int fd_power = -1;
int fd_driver = -1;

static uint32_t spi_mode = 0x3;
static uint8_t spi_bits = 8;
static uint32_t spi_speed = 1550;
static uint16_t delay_usecs = 0;

static void spi_transfer(int fd, uint8_t const *tx, uint8_t *rx, size_t len)
{
    int ret;
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = len,
        .delay_usecs = delay_usecs,
        .speed_hz = spi_speed,
        .bits_per_word = spi_bits,
    };

    if (spi_mode & SPI_TX_QUAD)
        tr.tx_nbits = 4;
    else if (spi_mode & SPI_TX_DUAL)
        tr.tx_nbits = 2;
    if (spi_mode & SPI_RX_QUAD)
        tr.rx_nbits = 4;
    else if (spi_mode & SPI_RX_DUAL)
        tr.rx_nbits = 2;
    if (!(spi_mode & SPI_LOOP)) {
        if (spi_mode & (SPI_TX_QUAD | SPI_TX_DUAL))
            tr.rx_buf = 0;
        else if (spi_mode & (SPI_RX_QUAD | SPI_RX_DUAL))
            tr.tx_buf = 0;
    }

    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1)
        pabort("can't send spi message");
}


uint8_t bit_flip(uint8_t word)
{
    return ((word * 0x0802LU & 0x22110LU) | (word * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
}

float adc_response_to_float(uint8_t *response_arr)
{
    uint16_t response = ((uint16_t)(response_arr[0] & 3) << 8) + response_arr[1];
    return V_ref*response/1024;
}

const float cur_sens = 0.185;// Sensitivity of current sensor [V/A]
int open_spi_power()
{
    if(fd_power >= 0) pabort("Power file descriptor already existing");
    fd_power = open(spi_device_power, O_RDWR);
    if (fd_power < 0) pabort("can't open spi device");
    return 0;
}

int open_spi_driver()
{
    if(fd_driver >= 0) pabort("Driver file descriptor already existing");
    fd_power = open(spi_device_driver, O_RDWR);
    if (fd_driver < 0) pabort("can't open spi device");
    return 0;
}

int close_spi_power()
{
    return close(fd_power);
}

int close_spi_driver()
{
    return close(fd_driver);
}


const float cur_sens = 0.185;// Sensitivity of current sensor [V/A]
int read_power_mcu(float *batt_voltage, float *current)
{
    const uint8_t tx[] = { 0, 0, 0, 0, 0, 'R'};
    uint8_t rx[sizeof(tx)] = {0, };
    spi_transfer(fd, tx, rx, sizeof(tx));

    // For debugging
    write_to_bar(debug_bar, "DBG: SPI read %.2x %.2x %.2x %.2x %.2x %.2x"
            , rx[0], rx[1], rx[2], rx[3], rx[4], rx[5]);

    // Check validity
    if (rx[0] != 0xff || rx[1] != 0xff) return 1;
    if (rx[2] & 0xfc) return 1;
    if (rx[4] & 0xfc) return 1;

    *batt_voltage = adc_response_to_float(rx+2);
    *current = adc_response_to_float(rx+4);
    *current = *current/cur_sens;
    return 0;
}

int read_mcp3008(int channel, float *voltage)
{
    int ret = 0;
    int fd;

    if (channel < 0 || channel > 7)
    {
        char *str = (char*)malloc(30 * sizeof(char));
        sprintf(str, "channel %d invalid", channel);
        pabort(str);
    }

    fd = open(spi_device, O_RDWR);
    if (fd < 0)
        pabort("can't open device");

    /*
     * spi mode
     */
    ret = ioctl(fd, SPI_IOC_WR_MODE32, &spi_mode);
    if (ret == -1)
        pabort("can't set spi mode");

    ret = ioctl(fd, SPI_IOC_RD_MODE32, &spi_mode);
    if (ret == -1)
        pabort("can't get spi mode");

    /*
     * bits per word
     */
    ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spi_bits);
    if (ret == -1)
        pabort("can't set bits per word");

    ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &spi_bits);
    if (ret == -1)
        pabort("can't get bits per word");

    /*
     * max speed hz
     */
    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed);
    if (ret == -1)
        pabort("can't set max speed hz");

    ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &spi_speed);
    if (ret == -1)
        pabort("can't get max speed hz");

    //printf("spi mode: 0x%x\n", mode);
    //printf("bits per word: %d\n", bits);
    //printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);

    char tx_pattern[] = { 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0 };
    uint8_t tx[] = { 0x01, tx_pattern[channel], 0x00 };
    uint8_t rx[ARRAY_SIZE(tx)] = {0, };
    spi_transfer(fd, tx, rx, sizeof(tx));

    uint16_t left_response = ((uint16_t)(rx[1] & 3) << 8) + rx[2];
    *voltage = V_ref*left_response/1024;
    //unsigned short guaranteed at least 16 bits, printf doesn't have uint16 type
    //printf("Left: %hu Right: %hu\n", (unsigned short) left_response, (unsigned short) right_response);

    close(fd);

    return ret;
}
#endif

int uart0_filestream = -1;
#ifdef __arm__
///static const char *uart_device = "/dev/ttyAMA0";
static const char *uart_device = "/dev/ttyS1";
#else
static const char *uart_device = "/dev/ttyUSB0";
#endif


int open_uart()
{
    //At bootup, pins 8 and 10 are already set to UART0_TXD, UART0_RXD (ie the alt0 function) respectively
    uart0_filestream = -1;

    //	O_NDELAY / O_NONBLOCK (same function) - Enables nonblocking mode. When set
    //	read requests on the file can return immediately with a failure status
    //	if there is no input immediately available (instead of blocking). Likewise,
    //	write requests can also return immediately with a failure status if the
    //	output can't be written immediately.
    //
    //	O_NOCTTY - When set and path identifies a terminal device, open() shall not cause the terminal device to become the controlling terminal for the process.
    uart0_filestream = open(uart_device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (uart0_filestream == -1)
    {
        //ERROR - CAN'T OPEN SERIAL PORT
        snprintf(debug_bar.text,sizeof(debug_bar.text)
                ,"Error - Unable to open UART.  Ensure it is not in use by another application");
        return 1;
    }

    //CONFIGURE THE UART
    //The flags (defined in /usr/include/termios.h - see http://pubs.opengroup.org/onlinepubs/007908799/xsh/termios.h.html):
    //	Baud rate:- B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, B230400, B460800, B500000, B576000, B921600, B1000000, B1152000, B1500000, B2000000, B2500000, B3000000, B3500000, B4000000
    //	CSIZE:- CS5, CS6, CS7, CS8
    //	CLOCAL - Ignore modem status lines
    //	CREAD - Enable receiver
    //	IGNPAR = Ignore characters with parity errors
    //	ICRNL - Map CR to NL on input (Use for ASCII comms where you want to auto correct end of line characters - don't use for bianry comms!)
    //	PARENB - Parity enable
    //	PARODD - Odd parity (else even)
    struct termios options;
    tcgetattr(uart0_filestream, &options);
    options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;		//<Set baud rate
    options.c_iflag = IGNPAR;// | IGNBRK;
    options.c_oflag = 0;
    options.c_lflag = 0;
    tcflush(uart0_filestream, TCIFLUSH);
    tcsetattr(uart0_filestream, TCSANOW, &options);
    snprintf(peripherals_bar.text,sizeof(peripherals_bar.text),"UART open");

    return 0;
}

void close_uart(){
    close(uart0_filestream);
    snprintf(peripherals_bar.text,sizeof(peripherals_bar.text),"UART closed");
}

int tx_uart(uint8_t word){
    if (uart0_filestream == -1)
    {
        snprintf(debug_bar.text,sizeof(debug_bar.text),"UART TX error");
        return 1;
    }
    write(uart0_filestream, &word, 1);
    return 0;
}

int rx_uart_word(uint8_t *word)
{
    if (uart0_filestream == -1)
    {
        snprintf(debug_bar.text,sizeof(debug_bar.text),"UART RX error");
        return 1;
    }
    read(uart0_filestream, (void*)word, 1);
    return 0;
}

int rx_uart_message(char *message, size_t size)
{
    if (uart0_filestream == -1)
    {
        snprintf(debug_bar.text,sizeof(debug_bar.text),"UART RX error");
        return 1;
    }
    read(uart0_filestream, (void*)message, size);
    return 0;
}

int format_serial_text(struct text_bar *bar)
{
    const char* str_open = "open";
    const char* str_closed= "closed";
#ifdef __arm__
    write_to_bar(bar, "Power SPI %s (%s)\t Driver SPI %s (%s)\t UART %s (%s)"
            , (fd_power >= 0)?str_open:str_closed, spi_device_power
            , (fd_driver>= 0)?str_open:str_closed, spi_device_driver
            , (uart0_filestream >= 0)?"open":"closed", uart_device);
#else
    write_to_bar(bar, "UART %s (%s)"
            , (uart0_filestream >= 0)?str_open:str_closed, uart_device);
#endif
    return 0;
}
