#include "utils.h"

inline char num2char(uint8_t num)
{
    return 48+num;//ASCII 48 is 0
}

void num2str10bit(uint16_t num, char *str)
{
    // Assume that number is max 1024
    uint8_t digit;
    digit = (num >= 1000)?1:0;
    str[0] = num2char(digit);
    num -= 1000*digit;
    digit = num/100;
    str[1] = num2char(digit);
    uint8_t small_num = (uint8_t)(num - 100*digit);
    digit = small_num/10;
    str[2] = num2char(digit);
    small_num -= 10*digit;
    str[3] = num2char(small_num);
}

/*
*/
void num2str10bitalt(uint16_t num, char *str)
{
    uint8_t snum;
    // Assume that number is max 1024
    if (num >= 1000){
        str[0] = '1';
        str[1] = '0';
        snum = (uint8_t)(num - 1000);
        goto two_digits;
    } else {
        str[0] = '0';
    }

    if(num >= 500){
        if(num >= 800){
            if(num >= 900){
                str[1] = '9';
                snum = (uint8_t)(num - 900);
            } else {
                str[1] = '8';
                snum = (uint8_t)(num - 800);
            }
        } else {
            if(num >= 700){
                str[1] = '7';
                snum = (uint8_t)(num - 700);
            } else if(num >= 600){
                str[1] = '6';
                snum = (uint8_t)(num - 600);
            } else {
                str[1] = '5';
                snum = (uint8_t)(num - 500);
            }
        }
    } else {
        if(num >= 300){
            if(num >= 400){
                str[1] = '4';
                snum = (uint8_t)(num - 400);
            } else {
                str[1] = '3';
                snum = (uint8_t)(num - 300);
            }
        } else {
            if(num >= 200){
                str[1] = '2';
                snum = (uint8_t)(num - 200);
            } else if(num >= 100){
                str[1] = '1';
                snum = (uint8_t)(num - 100);
            } else {
                str[1] = '0';
                snum = (uint8_t)num;
            }
        }
    }

    two_digits:
    if(snum >= 50){
        if(snum >= 80){
            if(snum >= 90){
                str[2] = '9';
                snum -= 90;
            } else {
                str[2] = '8';
                snum -= 80;
            }
        } else {
            if(snum >= 70){
                str[2] = '7';
                snum -= 70;
            } else if(snum >= 60){
                str[2] = '6';
                snum -= 60;
            } else {
                str[2] = '5';
                snum -= 50;
            }
        }
    } else {
        if(snum >= 30){
            if(snum >= 40){
                str[2] = '4';
                snum -= 40;
            } else {
                str[2] = '3';
                snum -= 30;
            }
        } else {
            if(snum >= 20){
                str[2] = '2';
                snum -= 20;
            } else if(snum >= 10){
                str[2] = '1';
                snum -= 10;
            } else {
                str[2] = '0';
            }
        }
    }
    str[3] = num2char(snum);
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
