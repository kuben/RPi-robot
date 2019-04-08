// Copyright (C) 2014 Diego Herranz
#define NO_BIT_DEFINES
#include <pic14regs.h>//<pic16f685.h>
#include <stdint.h>

// Oscillator Selection bits (INTOSCIO oscillator: I/O function on RA6/OSC2/CLKOUT pin, I/O function on RA7/OSC1/CLKIN),
// disable watchdog,
// and disable low voltage programming.
// The rest of fuses are left as default.
__code uint16_t __at (_CONFIG) __configword = _INTRC_OSC_NOCLKOUT & _WDTE_OFF & _MCLRE_OFF;

#define LED_PORT PORTAbits.RA0
#define LED_TRIS TRISAbits.TRISA0

// Uncalibrated delay, just waits a number of for-loop iterations
void delay(uint16_t iterations)
{
	uint16_t i;
	for (i = 0; i < iterations; i++) {
		// Prevent this loop from being optimized away.
		__asm nop __endasm;
	}
}

static const int bajs = 69;
void main(void)
{
    TRISA = 0xff;
    TRISB = 0xff;
    TRISC = 0xff;
	LED_TRIS = 0; // Pin as output
	LED_PORT = 0; // LED off


	while (1) {
		LED_PORT = 1; // LED On
//		LED_PORT = PORTBbits.RB6;
		delay(30000); // ~500ms @ 4MHz
//        PORTA = 0;
//        PORTB = 0;
//        PORTC = 0;
		LED_PORT = 0; // LED Off
		delay(30000); // ~500ms @ 4MHz
//        PORTA = -1;
//        PORTB = -1;
//        PORTC = -1;
	}
}

