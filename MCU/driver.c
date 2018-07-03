// Copyright (C) 2014 Diego Herranz
#define NO_BIT_DEFINES
#define NO_LEGACY_DEFINES
#include <pic14regs.h>//<pic16f685.h>
#include <stdint.h>
#include "utils.h"

// Oscillator Selection bits (INTOSCIO oscillator: I/O function on RA6/OSC2/CLKOUT pin, I/O function on RA7/OSC1/CLKIN),
// disable watchdog,
// and disable low voltage programming.
// The rest of fuses are left as default.
__code uint16_t __at (_CONFIG) __configword = _INTRC_OSC_NOCLKOUT & _WDTE_OFF & _MCLRE_OFF;

#define TX PORTCbits.RC2
#define TX_TRIS TRISCbits.TRISC2

#define RX PORTBbits.RB4
#define RX_TRIS TRISBbits.TRISB4

#define REN PORTBbits.RB7
#define RF PORTBbits.RB6
#define RR PORTBbits.RB5
#define LEN PORTCbits.RC4
#define LF PORTAbits.RA4
#define LR PORTCbits.RC5

#define REN_TRIS TRISBbits.TRISB7
#define RF_TRIS TRISBbits.TRISB6
#define RR_TRIS TRISBbits.TRISB5
#define LEN_TRIS TRISCbits.TRISC4
#define LF_TRIS TRISAbits.TRISA4
#define LR_TRIS TRISCbits.TRISC5

#define NOP __asm__ ("nop")
#define TNOP NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP
#define NNOP NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP

void setup();
void rx_done(uint8_t word);
int transmit(uint8_t word);
void UART_tx();
void UART_start_bit();

volatile uint8_t l_speed = 0, r_speed = 0;

volatile uint8_t blink = 1;
volatile uint8_t message[20];

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
    setup();

    //Both outputs forward
    LF = 1;
    RF = 1;

    strcpy(message,"\n\nBooted!\n");
    while (1) {//Continuosly send message
        if(message[0] == 0) continue;//Don't print empty message
        if(transmit(message[i])) continue;
        i++;
        if (!message[i] || i == sizeof(message)){
            //message[0] = 0;
            i = 0;
        }
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
        if(RX) rx_done(uart.rx_buf);
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

/*
 * Receives 8-bit commands
 * 00sxxxxx for brake:
 * 10sxxxxx for forward:
 * 01sxxxxx for reverse:
 *  s = 0 for left, 1 for right
 *  xxxxx is 5 bit speed. 0 for duty cycle 0%, 31 for duty cycle 100%
 * 11xxxxxx for request:
 *   xxxxxx = 000000 for some command...
 */
void rx_done(uint8_t word)
{
    strcpy(message, "Received:   [    ]!\n");
    message[10] = word;
    num2str(word,message+13,0);
    message[13] = num2char(word/100);
    message[14] = num2char((word%100)/10);
    message[15] = num2char(word%10);

    return;
    if ((word & 0xc0) == 0xc0){
        //Request
        return;
    }
    //If not request then set speed
    if (word & 0x20){//Right
        switch (word & 0xe0){
            case 0x00://Brake
                RF = 0;
                RR = 0;
                break;
            case 0x80://Forward
                RF = 1;
                RR = 0;
                break;
            case 0x40://Reverse
                RF = 0;
                RR = 1;
                break;
        }
        r_speed = word & 0x1f;
    } else {//Left
        switch (word & 0xe0){
            case 0x00://Brake
                LF = 0;
                LR = 0;
                break;
            case 0x80://Forward
                LF = 1;
                LR = 0;
                break;
            case 0x40://Reverse
                LF = 0;
                LR = 1;
                break;
        }
        l_speed = word & 0x1f;
    }
}

int transmit(uint8_t word)
{
    //Ensure no pending receive or transmit operation
    if(uart.next_tx || uart.next_rx) return 1;

    IOCBbits.IOCB4 = 0;//Disable IOC interrupt during tx
    TX = 0;//Start bit
    uart.next_tx = 0x01;
    uart.tx_send = word;
    TMR2 = 0;
    blink = 0;
    T2CONbits.TMR2ON = 1;
    return 0;
}

void UART_tx()
{
    uint8_t nexttx;
    if (uart.next_tx == 0xfe){//Waiting to send stop bit
        TX = 1;//Stop bit high
        uart.next_tx = 0xff;
    } else if (uart.next_tx == 0xff){//Wait before ready
        uart.next_tx = 0;//Ready
        T2CONbits.TMR2ON = 0;
        IOCBbits.IOCB4 = 1;//Reenable IOC for rx
    } else {//Waiting to send data bit
        //uart.next_tx = 1<<blink;
        nexttx = (1<<blink);
        TX = (uart.tx_send & (1<<blink)) > 0;//Send data bit n
        //TX = (uart.tx_send & uart.next_tx) > 0;//Send data bit n
        //TX = (uart.tx_send & nexttx) > 0;//Send data bit n
        //if(uart.next_tx < 0x80) uart.next_tx = uart.next_tx<<1;//Wait for next bit
        if(blink < 7) uart.next_tx = uart.next_tx<<1;//Wait for next bit
        else uart.next_tx = 0xfe;//Now wait for stop bit
        blink++;
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
    IOCBbits.IOCB4 = 1;//Interrupt on RB4 change

    TX_TRIS = 0; // Pin as output
    TX = 1;//tx idles high

    //Setup Timer 2 (baud rate timing)
    T2CON = 0;
    T2CONbits.T2CKPS = 1; //1200 baud
    PR2 = 67;
    PIE1bits.TMR2IE = 1;

    //Setup Right,Left Forward, Reverse and Enable as ouputs
    RF_TRIS = 0;
    RR_TRIS = 0;
    REN_TRIS = 0;
    LF_TRIS = 0;
    LR_TRIS = 0;
    LEN_TRIS = 0;

    RF = 0;
    RR = 0;
    REN = 0;
    LF = 0;
    LR = 0;
    LEN = 0;
}
