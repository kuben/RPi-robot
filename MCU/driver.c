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
void rx_done();
int transmit(uint8_t word);

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

/*
 * uart: low start bit - eight data bits - high stop bit
 * Receive:
 *   If ready and rx changes to low then start timer
 *     On timer 1-8: read rx and store corresponding bit (lsb first)
 *     On timer 9: If high then transmission is deemed succesful
 * Transmit:
 *   Begin transmission by setting tx low. Start timer
 *    On timer 1-8: Set tx to corresponding bit
 *    On timer 9: Set tx high
 * rx will fail if tx ongoing and vice versa
 */
struct uart_struct {
    uint8_t rx_buf;
    uint8_t next_rx;//0: ready, 1,2,4,8,..: waiting on bit n, -1: waiting on stop bit
    uint8_t tx_send;
    uint8_t next_tx;//0: ready, 1,2,4,8,..: waiting on bit n, -1: waiting on stop bit

};
volatile struct uart_struct uart = {0};

void main(void)
{
    uint8_t i = 0;
    const char *message = "Hello World!\n";
    setup();


    TX_TRIS = 0; // Pin as output
    TX = 1;//tx idles high

    while (1) {//Continuosly send message
        if(transmit(message[i])) continue;
        i++;
        if (i == sizeof(message)) i = 0;
    }
}

void UART_start_bit()
{
    //Interrupt disabled when ongoing tx, thus we know that no
    //tx is in progress.
    uint8_t rx = RX;
    if (!rx) {
        //blink = 2;
        uart.rx_buf = 0;//Clear buffer
        uart.next_rx = 1;//Waiting for bit 1
        TMR2 = 0;
        T2CONbits.TMR2ON = 1;
        IOCBbits.IOCB4 = 0;
    }
    INTCONbits.RABIF = 0;//Clear flag
}

void UART_rx()
{
    if (uart.next_rx == 0xff){//Waiting on stop bit
        //Transmission only succesful if stop bit high. IOC reenabled
        //in either case
        if(RX) rx_done();
        uart.next_rx = 0;//Ready
        T2CONbits.TMR2ON = 0;
        IOCBbits.IOCB4 = 1;
    } else {//Waiting on data bit
        if(RX) uart.rx_buf |= uart.next_rx;//Receive data bit n
        uart.next_rx <<= 1;//Wait for next bit
        //blink++;
        if (!uart.next_rx) uart.next_rx = 0xff;//Now wait for stop bit
    }
    PIR1bits.TMR2IF = 0;
}

void rx_done()
{
    ;
}

int transmit(uint8_t word)
{
    //Ensure no pending receive or transmit operation
    if(uart.next_rx || uart.next_rx) return 1;

    IOCBbits.IOCB4 = 0;//Disable IOC interrupt during tx
    TX = 0;//Start bit
    uart.next_tx = 1;
    uart.tx_send = word;
    TMR2 = 0;
    T2CONbits.TMR2ON = 1;
    return 0;
}

void UART_tx()
{
    if (uart.next_tx == 0xff){//Waiting to send stop bit
        TX = 1;//Stop bit high
        uart.next_tx = 0;//Ready
        T2CONbits.TMR2ON = 0;
        IOCBbits.IOCB4 = 1;//Reenable IOC for rx
    } else {//Waiting to send data bit
        TX = uart.tx_send & uart.next_tx;//Send data bit n
        uart.next_tx <<= 1;//Wait for next bit
        if (!uart.next_tx) uart.next_tx = 0xff;//Now wait for stop bit
    }
    PIR1bits.TMR2IF = 0;
}

void Itr_Routine(void) __interrupt 0
{
    if (INTCONbits.RABIF) UART_start_bit();
    if (PIR1bits.TMR2IF) {
        if(uart.next_rx) UART_rx();
        else UART_tx();//Else assume tx
    }
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
