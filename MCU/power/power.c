// Copyright (C) 2014 Diego Herranz
#define NO_BIT_DEFINES
#define NO_LEGACY_DEFINES
#include <pic16f690.h>
#include <stdint.h>
#include "../utils.h"

// Oscillator Selection bits (INTOSCIO oscillator: I/O function on RA6/OSC2/CLKOUT pin, I/O function on RA7/OSC1/CLKIN),
// disable watchdog,
// and disable low voltage programming.
// The rest of fuses are left as default.
__code uint16_t __at (_CONFIG) __configword = _INTRC_OSC_NOCLKOUT & _WDTE_OFF & _MCLRE_OFF
        & _PWRTE_ON & _BOR_ON
        ;

#define A_MASK 0x37
#define B_MASK 0xa0
#define C_MASK 0x36

#define NOP __asm__ ("nop")
#define TNOP NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP
#define NNOP NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP

void setup();
void setup_tmr_pwm();
void rx_done(uint8_t word);
void spi_msg(char *msg);
void led_boot_sequence();
inline void toggle_leds();

volatile uint8_t message[6];
volatile uint8_t msg_idx = 0;

struct ADC_buffer {
    volatile uint8_t voltage_high;
    volatile uint8_t voltage_low;
    volatile uint8_t current_high;
    volatile uint8_t current_low;
};
struct ADC_buffer adc_buf = {0xff};

//uint16_t val = 0;
void main(void)
{
    setup();
    led_boot_sequence();
    if(!PCONbits.NOT_BOR){
        PCONbits.NOT_BOR = 1;//Reset bit for future resets
        //spi_msg("Boot (BOR)\n");
    } else if(!PCONbits.NOT_POR){
        PCONbits.NOT_BOR = 1;//Reset bit for future resets
        PCONbits.NOT_POR = 1;//Reset bit for future resets
        //spi_msg("Boot (POR)\n");
    } else {
        //spi_msg("Boot (MCLR)\n");
    }

    //spi_msg("An. read \"    \" and \"    \".");
    message[0] = 0xff;
    message[1] = 0xff;
    volatile uint8_t i = 0;
    while (1) {
        toggle_leds();
        delay(10000);
        ADCON0bits.GO = 1;
        while (ADCON0bits.GO); //Wait until finished
        /*
        val = ((uint16_t)ADRESH << 8) + ADRESL;
        num2str10bit(val, adc_str);
        */
        adc_buf.voltage_high = ADRESH;
        adc_buf.voltage_low= ADRESL;
        ADCON0bits.CHS = 4; // Select AN ch 4
        ADCON0bits.ADFM = 1;//Right justify data

        toggle_leds();
        delay(10000);
        ADCON0bits.GO = 1;
        while (ADCON0bits.GO); //Wait until finished
       /* val = ((uint16_t)ADRESH << 8) + ADRESL;
        num2str10bit(val, adc_str2);
        */
        adc_buf.current_high = ADRESH;
        adc_buf.current_low= ADRESL;
        ADCON0bits.CHS = 4; // Select AN ch 4
        ADCON0bits.CHS = 7; // Select AN ch 7
        ADCON0bits.ADFM = 1;//Right justify data
    }
}

inline void toggle_leds()
{
    PORTA = PORTA^A_MASK;
    PORTB = PORTB^B_MASK;
    PORTC = PORTC^C_MASK;
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

void spi_msg(char *msg)
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
        if (read == 'R') msg_idx = 0; // Reset

        if (msg_idx == 0){
            message[2] = adc_buf.voltage_high;
            message[3] = adc_buf.voltage_low;
            message[4] = adc_buf.current_high;
            message[5] = adc_buf.current_low;
        }
        SSPBUF = message[msg_idx];
        msg_idx++;
        if (msg_idx == sizeof(message)){
            msg_idx = 0;
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
    TRISA = 0xff^A_MASK;
    TRISB = 0xff^B_MASK;
    TRISC = 0xff^C_MASK;

    PORTA = 0x00;
    PORTB = 0x00;
    PORTC = 0x00;

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
        toggle_leds();
            delay(30000);
        toggle_leds();
        delay(5000);
    }
}
