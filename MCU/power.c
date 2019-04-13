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

#define LED_PORT PORTAbits.RA0
#define LED_TRIS TRISAbits.TRISA0

#define NOP __asm__ ("nop")
#define TNOP NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP
#define NNOP NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP

void setup();
void setup_tmr_pwm();
void rx_done(uint8_t word);
void uart_msg(char *msg);
void led_boot_sequence();

volatile uint8_t l_speed = 7;
volatile uint8_t write = 0;

volatile uint8_t message[30];
volatile uint8_t msg_idx = 0;
char *adc_str = message + 10;
char *adc_str2 = message + 21;

uint16_t val = 0;
void main(void)
{
    setup();
    led_boot_sequence();
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

    uart_msg("An. read \"    \" and \"    \".");
    volatile uint8_t i = 0;
    while (1) {
        LED_PORT = !LED_PORT;
        delay(10000);
        ADCON0bits.GO = 1;
        while (ADCON0bits.GO); //Wait until finished
        val = ((uint16_t)ADRESH << 8) + ADRESL;
        num2str10bitalt(val, adc_str);
        ADCON0bits.CHS = 4; // Select AN ch 4
    ADCON0bits.ADFM = 1;//Right justify data

        LED_PORT = !LED_PORT;
        delay(10000);
        ADCON0bits.GO = 1;
        while (ADCON0bits.GO); //Wait until finished
        val = ((uint16_t)ADRESH << 8) + ADRESL;
        num2str10bitalt(val, adc_str2);
        ADCON0bits.CHS = 7; // Select AN ch 7
    ADCON0bits.ADFM = 1;//Right justify data
    }
//        adc_str[0] = ADRESH;
//        adc_str[1] = ADRESL;
}

/*
void receive(uint8_t word)
{
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
    */

void uart_msg(char *msg)
{
    strcpy(message, msg);
    //TX_PENDING = 1;
    msg_idx = 0;
}

void Itr_Routine(void) __interrupt 0
{
    if (PIR1bits.SSPIF & SSPSTATbits.BF)
    {
        volatile uint8_t read = SSPBUF;
        if (read == 'S') msg_idx = 0; // Start
        //if (SSPCONbits.WCOL) SSPBUF = 0x80;
        //else if (SSPCONbits.WCOL) SSPBUF = 0x40;
        //else SSPBUF = write;
        //write++;
        //SSPBUF = read;

        if (msg_idx == 0xff)
        {
            SSPBUF = 0x00;
            PIR1bits.SSPIF = 0;
            return;
        }
        SSPBUF = message[msg_idx];
        // Send 0 as last char so receiver knows when to stop reading
        if (!message[msg_idx] || msg_idx == sizeof(message)){
            msg_idx = 0xff;
            //msg_idx = 0;
            //message[0] = 0; // Clear message
        } else {
            msg_idx++;
        }

        PIR1bits.SSPIF = 0;
    }
    /*
    if(PIR1bits.ADIF)
    {
        PIR1bits.ADIF = 0;
    }
    */
}

void setup()
{
    //All pins inputs
    TRISA = 0xff;
    TRISB = 0xff;
    TRISC = 0xff;

    ANSEL = 0x90;//AN7 (RC3) and AN4 (RC0)
    ANSELH = 0;

    OPTION_REGbits.NOT_RABPU = 1;//Don't disable pull-ups

    INTCON = 0;//Disable all interrupts and clear all flags
    PIE1 = 0;
    PIE2 = 0;
    PIR1 = 0;
    PIR2 = 0;

    INTCONbits.GIE = 1;//Enable global interrupts
    INTCONbits.PEIE = 1;//Enable peripheral interrupts
    PIE1bits.SSPIE = 1;//Enable SPI receive interrupts

    //Setup SPI
    TRISBbits.TRISB4 = 1; //SDI input
    TRISBbits.TRISB6 = 1; //SCK input
    TRISCbits.TRISC6 = 1; //SS  input
    TRISCbits.TRISC7 = 0; //SDO output
    SSPCON = 0;
    SSPSTATbits.CKE = 0;
    SSPCONbits.CKP = 0;
    SSPCONbits.SSPM = 0x04;//0x05; //SPI Slave, SS ctrl disabled
    SSPCONbits.SSPEN = 1;
    SSPBUF = 0x00;

    //Setup ADC
    ADCON1 = 0x50;//T_AD = 4us at 4MHz osc
    ADCON0bits.CHS = 0x7;
    ADCON0bits.ADFM = 1;//Right justify data
    ADCON0bits.ADON = 1;
    //PIR1bits.ADIF = 0;
    //PIE1bits.ADIE = 1;//Enable interrupt

    //Setup outputs
    LED_TRIS = 0;

    LED_PORT = 0;

    //Setup and start timer 2 for pwm
    //setup_tmr_pwm();
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

void led_boot_sequence()
{
    for (int i=0; i < 3; i++){
        LED_PORT = 1;
        delay(10000);
        LED_PORT = 0;
        delay(5000);
    }
}
