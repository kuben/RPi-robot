#include "utils.h"

char num2char(uint8_t num)
{
    return 48+num;//ASCII 48 is 0
}

int num2str(uint8_t num, char *str, uint8_t *len)
{
    uint8_t length;
    uint8_t digits[3] = {0};//Most significant first
    digits[0] = num/100;
    digits[1] = (num/10)%10;
    digits[2] = num%10;

    if (num > 99)
    {
        length = 3;
        str[0] = num2char(digits[0]);
        str[1] = num2char(digits[1]);
        str[2] = num2char(digits[2]);
    }
    else if (num > 9)
    {
        length = 3;
        str[0] = num2char(digits[1]);
        str[1] = num2char(digits[2]);
    }
    else
    {
        length = 1;
        str[0] = num2char(digits[2]);
    }
    if(len) *len = length;//If not null pointer
    return 0;
}

void strcpy(uint8_t *dest, const uint8_t *src)
{
     while(*src) {
        *dest = *src;
        src++;
        dest++;
    }
    *dest = *src;
}

// Uncalibrated delay, just waits a number of for-loop iterations
void delay(uint16_t iterations)
{
    uint16_t i;
    for (i = 0; i < iterations; i++) {
        // Prevent this loop from being optimized away.
        __asm__ ("nop") ;
    }
}
