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

#define PIN_1 PORTAbits.RA5
#define PIN_2 PORTAbits.RA4
#define PIN_3 PORTAbits.RA0
#define PIN_4 PORTCbits.RC1
#define PIN_5 PORTCbits.RC5
#define PIN_6 PORTAbits.RA2
#define PIN_7 PORTCbits.RC2
#define PIN_8 PORTCbits.RC4
#define PIN_9 PORTBbits.RB5
#define PIN_10 PORTBbits.RB7

void setup();
void setup_tmr_indicator();
void setup_tmr_adc();
void rx_done(uint8_t word);
void spi_msg(char *msg);
void led_boot_sequence();
inline void toggle_leds();
inline void leds_high();
inline void leds_low();

volatile uint8_t indicator_progress = 0;//Updated at 15.25Hz
volatile uint8_t max_ind_prog = (uint8_t)(15.25*1);

volatile uint8_t message[6];
volatile uint8_t msg_idx = 0;

struct ADC_buffer {
    volatile uint8_t voltage_high;
    volatile uint8_t voltage_low;
    volatile uint8_t current_high;
    volatile uint8_t current_low;
};
struct ADC_buffer adc_buf = {0xff};

#define indicator(IND, PREV, CUR) while(indicator_progress != IND);\
    CUR = 0; PREV = 1

void main(void)
{
    setup();
    leds_high();

    message[0] = 0xff;
    message[1] = 0xff;
    volatile uint8_t i = 0;
    uint8_t p = 0; //Non-volatile dummy variable
    while (1) {
        indicator(0, p    , PIN_1);
        indicator(1, PIN_1, PIN_2);
        indicator(2, PIN_2, PIN_3);
        indicator(3, PIN_3, PIN_4);
        indicator(4, PIN_4, PIN_5);
        indicator(5, PIN_5, PIN_6);
        indicator(6, PIN_6, PIN_7);
        indicator(7, PIN_7, PIN_8);
        indicator(8, PIN_8, PIN_9);
        indicator(9, PIN_9, PIN_10);
        indicator(10, PIN_10, p);

        ADCON0bits.GO = 1;
    }
}

inline void toggle_leds()
{
    PORTA = PORTA^A_MASK;
    PORTB = PORTB^B_MASK;
    PORTC = PORTC^C_MASK;
}

inline void leds_high()
{
    PORTA = 1;
    PORTB = 1;
    PORTC = 1;
}

inline void leds_low()
{
    PORTA = 0;
    PORTB = 0;
    PORTC = 0;
}

void spi_msg(char *msg)
{
    strcpy(message, msg);
    //TX_PENDING = 1;
    msg_idx = 0;
}

void Itr_Routine(void) __interrupt 0
{
    // This interrupt should be fired at 15.25Hz
    // Increase counter to measure progress in blinking
    if (INTCONbits.T0IF)
    {
        indicator_progress++;
        if(indicator_progress > max_ind_prog) indicator_progress = 0;
        INTCONbits.T0IF = 0;
    }

    // SPI interrupt
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

    // ADC conversion complete
    if(PIR1bits.ADIF)
    {
        if (ADCON0bits.CHS == 4){ // Channel 4 is voltage sensor
            adc_buf.voltage_high = ADRESH;
            adc_buf.voltage_low = ADRESL;
            ADCON0bits.CHS = 7; // Select AN ch 7
        } else {
            adc_buf.current_high = ADRESH;
            adc_buf.current_low = ADRESL;
            ADCON0bits.CHS = 4; // Select AN ch 4
        }
        ADCON0bits.ADFM = 1;//Right justify data
        /*
        val = ((uint16_t)ADRESH << 8) + ADRESL;
        num2str10bit(val, adc_str);
        */
        PIR1bits.ADIF = 0;
    }
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
    ADCON0bits.CHS = 4;
    ADCON0bits.ADFM = 1;//Right justify data
    ADCON0bits.ADON = 1;
    PIR1bits.ADIF = 0;
    PIE1bits.ADIE = 1;//Enable interrupt

    //Determine what kind of reboot we experienced
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

    //setup_tmr_adc();

    led_boot_sequence();

    setup_tmr_indicator(); // After boot sequence, starts interrupting
}

void setup_tmr_indicator()
{
    OPTION_REGbits.T0CS = 0;    //TMR0 source internal clock
    OPTION_REGbits.PSA = 0;     //Pre-scaler assigned to TMR0
    OPTION_REGbits.PS = 0x7;  //TMR0 1:256 pre-scaler
    TMR0 = 0;
    INTCONbits.T0IE = 1; //Enable interrupt
}

void setup_tmr_adc()
{
    T2CON = 0;
    TMR2 = 0;
    T2CONbits.T2CKPS = 2;
    T2CONbits.TOUTPS = 0;
    PR2 = 30;
    PIE1bits.TMR2IE = 0;
    T2CONbits.TMR2ON = 1;
}

#define boot_I(PREV, CUR) PREV = 0; CUR = 1; delay(1200)
void led_boot_sequence()
{
    uint8_t p = 0;//Dummy variable, may be optimized away by compiler
    //Boot sequence I
    boot_I(p, PIN_1);
    boot_I(PIN_1, PIN_2);
    boot_I(PIN_2, PIN_3);
    boot_I(PIN_3, PIN_4);
    boot_I(PIN_4, PIN_5);
    boot_I(PIN_5, PIN_6);
    boot_I(PIN_6, PIN_7);
    boot_I(PIN_7, PIN_8);
    boot_I(PIN_8, PIN_9);
    boot_I(PIN_9, PIN_10);
    delay(2000);
    boot_I(PIN_10, PIN_9);
    boot_I(PIN_9, PIN_8);
    boot_I(PIN_8, PIN_7);
    boot_I(PIN_7, PIN_6);
    boot_I(PIN_6, PIN_5);
    boot_I(PIN_5, PIN_4);
    boot_I(PIN_4, PIN_3);
    boot_I(PIN_3, PIN_2);
    boot_I(PIN_2, PIN_1);
    boot_I(PIN_1, p);
    delay(15000);

    //Boot sequence II
    boot_I(p    , PIN_1);
    boot_I(p    , PIN_2);
    boot_I(p    , PIN_3);
    boot_I(PIN_1, PIN_4);
    boot_I(PIN_2, PIN_5);
    boot_I(PIN_3, PIN_6);
    boot_I(PIN_4, PIN_7);
    boot_I(PIN_5, PIN_8);
    boot_I(PIN_6, PIN_9);
    boot_I(PIN_7, PIN_10);
    boot_I(PIN_8, p);
    boot_I(PIN_9, p);
    delay(2000);
    boot_I(p    , PIN_9);
    boot_I(p    , PIN_8);
    boot_I(PIN_10, PIN_7);
    boot_I(PIN_9, PIN_6);
    boot_I(PIN_8, PIN_5);
    boot_I(PIN_7, PIN_4);
    boot_I(PIN_6, PIN_3);
    boot_I(PIN_5, PIN_2);
    boot_I(PIN_4, PIN_1);
    boot_I(PIN_3, p);
    boot_I(PIN_2, p);
    boot_I(PIN_1, p);
    delay(10000);

    //Boot sequence III
    boot_I(p, PIN_1);
    boot_I(p, PIN_2);
    boot_I(p, PIN_3);
    boot_I(p, PIN_4);
    boot_I(p, PIN_5);
    boot_I(p, PIN_6);
    boot_I(p, PIN_7);
    boot_I(p, PIN_8);
    boot_I(p, PIN_9);
    boot_I(p, PIN_10);
    delay(20000);
}
