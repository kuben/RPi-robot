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

#include "spi_lib.h"

static void pabort(const char *s)
{
	perror(s);
	abort();
}

static const char *device = "/dev/spidev0.0";
static uint32_t mode;
static uint8_t bits = 8;
static uint32_t speed = 500000;
static uint16_t delay;

static void transfer(int fd, uint8_t const *tx, uint8_t const *rx, size_t len)
{
	int ret;
	int out_fd;
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = len,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	if (mode & SPI_TX_QUAD)
		tr.tx_nbits = 4;
	else if (mode & SPI_TX_DUAL)
		tr.tx_nbits = 2;
	if (mode & SPI_RX_QUAD)
		tr.rx_nbits = 4;
	else if (mode & SPI_RX_DUAL)
		tr.rx_nbits = 2;
	if (!(mode & SPI_LOOP)) {
		if (mode & (SPI_TX_QUAD | SPI_TX_DUAL))
			tr.rx_buf = 0;
		else if (mode & (SPI_RX_QUAD | SPI_RX_DUAL))
			tr.tx_buf = 0;
	}

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");
}


int8_t bit_flip(uint8_t word)
{
  return ((word * 0x0802LU & 0x22110LU) | (word * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
}

int read_mcp3008(int channel, uint16_t *left_response, uint16_t *right_response)
{
	int ret = 0;
	int fd;

	if (channel < 0 || channel > 7)
    {
        char *str = (char*)malloc(30 * sizeof(char));
        sprintf(str, "channel %d invalid", channel);
        pabort(str);
    }

	fd = open(device, O_RDWR);
	if (fd < 0)
		pabort("can't open device");

	/*
	 * spi mode
	 */
	ret = ioctl(fd, SPI_IOC_WR_MODE32, &mode);
	if (ret == -1)
		pabort("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE32, &mode);
	if (ret == -1)
		pabort("can't get spi mode");

	/*
	 * bits per word
	 */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");

	/*
	 * max speed hz
	 */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");

	//printf("spi mode: 0x%x\n", mode);
	//printf("bits per word: %d\n", bits);
	//printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);

    char tx_pattern[] = { 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78 };
    uint8_t tx[] = { tx_pattern[channel], 0x00, 0x00, 0x00 };
    uint8_t rx[ARRAY_SIZE(tx)] = {0, };
	transfer(fd, tx, rx, sizeof(tx));

    *left_response = ((uint16_t)rx[1] << 2) + (rx[2] >> 6);
    *right_response = ((uint16_t)bit_flip(rx[3])<<7) + (bit_flip(rx[2])>>1);
    //unsigned short guaranteed at least 16 bits, printf doesn't have uint16 type
    //printf("Left: %hu Right: %hu\n", (unsigned short) left_response, (unsigned short) right_response);

	close(fd);

	return ret;
}
