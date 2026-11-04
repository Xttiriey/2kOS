#ifndef UART_H
#define UART_H

#include <avr/pgmspace.h>
#include "../include/kernel.h"

#define _BAUD_ 4800UL     // Baud rate (bps)

#define _UBRR_ (F_CPU / (16 * _BAUD_) - 1) // Formula for asynchronous normal mode (divisor 16)

void USART_init(unsigned int ubrr);
void uart_putc(char c);
void uart_puts(const char *str);
unsigned char uart_getc();
int16_t uart_getc_nonblocking();
void uart_puts_P(const char *progmem_s);
void uart_puts_sync_P(const char *progmem_s);

#endif
