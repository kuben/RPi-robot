// Copyright (C) 2014 Diego Herranz
#define NO_BIT_DEFINES
#define NO_LEGACY_DEFINES
#include <pic16f690.h>
#include <stdint.h>
#include "utils.h"

// Oscillator Selection bits (INTOSCIO oscillator: I/O function on RA6/OSC2/CLKOUT pin, I/O function on RA7/OSC1/CLKIN),
// disable watchdog,
// and disable low voltage programming.
// The rest of fuses are left as default.
__code uint16_t __at (_CONFIG) __configword = _INTRC_OSC_NOCLKOUT & _WDTE_OFF & _MCLRE_OFF
        & _PWRTE_ON & _BOR_ON
        ;

#define REN PORTCbits.RC2
#define RR PORTBbits.RB6
#define RF PORTBbits.RB4
#define LEN PORTCbits.RC4
#define LR PORTCbits.RC3
#define LF PORTCbits.RC5

#define REN_TRIS TRISCbits.TRISC2
#define RR_TRIS TRISBbits.TRISB6
#define RF_TRIS TRISBbits.TRISB4
#define LEN_TRIS TRISCbits.TRISC4
#define LR_TRIS TRISCbits.TRISC3
#define LF_TRIS TRISCbits.TRISC5

#define TX_PENDING PIE1bits.TXIE

#define NOP __asm__ ("nop")
#define TNOP NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP
#define NNOP NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP

void setup();
void setup_tmr_pwm();
void rx_done(uint8_t word);
void uart_msg(char *msg);

volatile uint8_t l_speed = 15, r_speed = 15;

volatile uint8_t message[20];
volatile uint8_t msg_idx = 0;

void main(void)
{
    setup();
    if(!PCONbits.NOT_BOR){
        PCONbits.NOT_BOR = 1;//Reset bit for future resets
        uart_msg("Boot (BOR)\n");
    } else if(!PCONbits.NOT_POR){
        PCONbits.NOT_BOR = 1;//Reset bit for future resets
        PCONbits.NOT_POR = 1;//Reset bit for future resets
        uart_msg("Boot (POR)\n");
    } else {
        uart_msg("Boot (MCLR)\n");
    }

    while (1) {
        if (TMR2 < l_speed) LEN = 1;
        else LEN = 0;
        if (TMR2 < r_speed) REN = 1;
        else REN = 0;
    }
}

/*
 * Receives 8-bit commands
 * 00sxxxxx for brake:
 * 10sxxxxx for forward:
 * 01sxxxxx for reverse:
 *  s = 0 for left, 1 for right
 *  xxxxx is 5 bit speed. 0 for duty cycle 0%, 31 for duty cycle 100%
 * 11xxxxxx for request:
 *   xxxxxx = 000001 for some command...
 */
void receive(uint8_t word)
{
    if ((word & 0xc0) == 0xc0){ //Request
        switch (word & 0x3f){
            case 0x01://AD channel 4
                ADCON0 = 0x81 | (4<<2);
                TNOP;
                ADCON0bits.GO = 1;
                break;
            case 0x02://AD channel 5
                ADCON0 = 0x81 | (5<<2);
                TNOP;
                ADCON0bits.GO = 1;
                break;
            default:
                uart_msg("Request\n");
        }
        return;
    } else if (word & 0x20){//Set right speed
        switch (word & 0xc0){
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
    } else {//Set left speed
        switch (word & 0xc0){
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
    strcpy(message, "  [   ]      !\n");
    if(word < 32 || word == 127 || word > 127) message[0] = ' ';//Unprintable/DEL
    else message[0] = word;
    num2str(word,message+3,0);
    num2str(l_speed,message+8,0);
    num2str(r_speed,message+11,0);
    msg_idx = 0;
    TX_PENDING = 1;
    return;
}

void uart_msg(char *msg)
{
    strcpy(message, msg);
    TX_PENDING = 1;
    msg_idx = 0;
}

void Itr_Routine(void) __interrupt 0
{
    if(TX_PENDING & PIR1bits.TXIF)
    {
        //Load next char to uart tx
        TXREG = message[msg_idx];
        msg_idx++;
        if (!message[msg_idx] || msg_idx == sizeof(message)){
            msg_idx = 0;
            TX_PENDING = 0;//Nothing more to send
        }
    }

    if(PIR1bits.RCIF)
    {
        if(RCSTAbits.OERR)
        {
            RCSTAbits.CREN = 0;//Toggle CREN to clear overflow error
            RCSTAbits.CREN = 1;
        }
        if(RCSTAbits.OERR);//Nothing on framing error
        receive(RCREG);
    }

    if(PIR1bits.ADIF)
    {
        uint16_t val = ((uint16_t)ADRESH << 8) + ADRESL;
        char str[12];
        strcpy(str,"Conv.     !\n");
        str[9] = num2char((uint8_t)val%10);
        str[8] = num2char((uint8_t)(val/10)%10);
        str[7] = num2char((uint8_t)(val/100)%10);
        str[6] = num2char((uint8_t)(val/1000)%10);
        uart_msg(str);
        PIR1bits.ADIF = 0;
    }
}

void setup()
{
    //All pins inputs
    TRISA = 0xff;
    TRISB = 0xff;
    TRISC = 0xff;

    ANSEL = 0x30;//AN4 and 5 analog (RC1 and 0)
    ANSELH = 0;

    OPTION_REGbits.NOT_RABPU = 1;//Don't disable pull-ups

    INTCON = 0;//Disable all interrupts and clear all flags
    PIE1 = 0;
    PIE2 = 0;
    PIR1 = 0;
    PIR2 = 0;

    INTCONbits.GIE = 1;//Enable global interrupts
    INTCONbits.PEIE = 1;//Enable peripheral interrupts
    PIE1bits.RCIE = 1;//Enable receive interrupts

    //Setup UART
    BAUDCTLbits.BRG16 = 1;//16 bit baud rate generator
    TXSTAbits.BRGH = 1;//High speed baud rate select
    SPBRG = 8;//111 111 baud

    TXSTAbits.SYNC = 0;//Asynchronous operation
    RCSTAbits.SPEN = 1;//Enables uart, configures pins
    TXSTAbits.TXEN = 1;//Enable transmitter

    RCSTAbits.CREN = 1;//Enable receiver

    //Setup ADC
    ADCON1 = 0x50;//T_AD = 4us at 4MHz osc
    ADCON0bits.ADFM = 1;//Right justify data
    PIR1bits.ADIF = 0;
    PIE1bits.ADIE = 1;//Enable interrupt

    //Setup Right,Left Forward, Reverse and Enable as ouputs
    RF_TRIS = 0;
    RR_TRIS = 0;
    REN_TRIS = 0;
    LF_TRIS = 0;
    LR_TRIS = 0;
    LEN_TRIS = 0;

    RF = 1;//Forward
    RR = 0;
    REN = 0;
    LF = 1;
    LR = 0;
    LEN = 0;

    //Setup and start timer 2 for pwm
    setup_tmr_pwm();
}

void setup_tmr_pwm()
{
    T2CON = 0;
    TMR2 = 0;
    T2CONbits.T2CKPS = 2;
    T2CONbits.TOUTPS = 0;
    PR2 = 30;
    PIE1bits.TMR2IE = 0;
    T2CONbits.TMR2ON = 1;
}
