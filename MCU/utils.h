#ifndef UTILS
#define UTILS

#include <stdint.h>

#define NOP __asm__ ("nop")
#define TNOP NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP
#define NNOP NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP

void delay(uint16_t iterations);
char num2char(uint8_t num);
int num2str(uint8_t num, char *str, uint8_t *len);
void strcpy(uint8_t *dest, const uint8_t *src);
void num2str10bit(uint16_t num, char *str);
void num2str10bitalt(uint16_t num, char *str);

#endif
