// Copyright (C) 2014 Diego Herranz
#define NO_BIT_DEFINES
#define NO_LEGACY_DEFINES
#include <pic14regs.h>//<pic16f685.h>
#include <stdint.h>

// Oscillator Selection bits (INTOSCIO oscillator: I/O function on RA6/OSC2/CLKOUT pin, I/O function on RA7/OSC1/CLKIN),
// disable watchdog,
// and disable low voltage programming.
// The rest of fuses are left as default.
__code uint16_t __at (_CONFIG) __configword = _INTRC_OSC_NOCLKOUT & _WDTE_OFF & _MCLRE_OFF;

#define TX PORTCbits.RC2
#define TX_TRIS TRISCbits.TRISC2

#define RX PORTBbits.RB4
#define RX_TRIS TRISBbits.TRISB4

void setup();

// Uncalibrated delay, just waits a number of for-loop iterations
void delay(uint16_t iterations)
{
    uint16_t i;
    for (i = 0; i < iterations; i++) {
        // Prevent this loop from being optimized away.
        __asm__ ("nop") ;
    }
}

volatile uint8_t blink = 1;
struct uart_struct {
    uint8_t rx_buf;
    uint8_t next_rx;//0 means ready, otherwise (1<<bit)
    uint8_t tx_buf;
    uint8_t next_tx;
};
volatile struct uart_struct uart = {0};

void main(void)
{
    uint16_t d,k;
    setup();


    TX_TRIS = 0; // Pin as output
    TX = 0; // LED off

    while (1) {
        //if(!blink) continue;
        //d = RX?10000:30000;
        k = uart.rx_buf;
        d = 10000*(k?k:1);
        //d = 10000*((blink>6)?6:blink+1);
        TX = 1; // LED On
        //delay(30000); // ~500ms @ 4MHz
        delay(d);
        TX = 0; // LED Off
        delay(d);
        //blink--;
    }
}

void UART_start_bit()
{
    uint8_t rx = RX;
    if (!rx) {
        //blink = 2;
        TMR2 = 0;
        T2CONbits.TMR2ON = 1;
        uart.rx_buf = 0;
        uart.next_rx = 1;//Next mask is 0x01
        IOCBbits.IOCB4 = 0;
    }
    INTCONbits.RABIF = 0;//Clear flag
}

void UART_rx()
{
    if(RX) uart.rx_buf |= uart.next_rx;
    uart.next_rx <<= 1;
    blink++;
    if (!uart.next_rx){//Last one
        T2CONbits.TMR2ON = 0;
        IOCBbits.IOCB4 = 1;
    }
    PIR1bits.TMR2IF = 0;
}

void Itr_Routine(void) __interrupt 0
{
    if (INTCONbits.RABIF) UART_start_bit();
    if (PIR1bits.TMR2IF) UART_rx();
}

void setup()
{
    //Disable ADC
    ANSEL = 0;
    ANSELH = 0;
    TRISA = 1;
    TRISB = 1;
    TRISC = 1;

    OPTION_REGbits.NOT_RABPU = 1;//Don't disable pull-ups

    INTCON = 0;//Disable all interrupts and clear all flags
    PIE1 = 0;
    PIE2 = 0;
    PIR1 = 0;
    PIR2 = 0;

    INTCONbits.GIE = 1;//Enable global interrupts
    INTCONbits.PEIE = 1;//Enable peripheral interrupts
    INTCONbits.RABIE = 1;//Enable interrupts on RA and RB
    //Setup IOC
    RX_TRIS = 1;
    blink += 2*RX;
//    WPUBbits.WPUB4 = 1;//Weak pull-up
    IOCBbits.IOCB4 = 1;//Interrupt on RB4 change

    //Setup Timer 2 (baud rate timing)
    T2CON = 0;
    PR2 = 35-1;//114286 baud
    PR2 = 9-1;//111111 baud
    PIE1bits.TMR2IE = 1;
}
