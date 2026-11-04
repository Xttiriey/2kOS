#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "../include/uart.h"

// Ring buffer for receiving data (RX). 16 bytes is enough for typing commands
#define RX_BUFFER_SIZE 16

volatile char rx_buffer[RX_BUFFER_SIZE];
volatile uint8_t rx_head = 0;   // Where we write the next incoming byte
volatile uint8_t rx_tail = 0;   // Where we read the next byte from

// Ring buffer for transmitting data (TX)
//  32 bytes allows us to send short strings without blocking the CPU
#define TX_BUFFER_SIZE 32

volatile char tx_buffer[TX_BUFFER_SIZE];
volatile uint8_t tx_head = 0;
volatile uint8_t tx_tail = 0;

void USART_init(unsigned int ubrr) {
    // Set the baud rate prescale (calculated in uart.h)
    UBRR0 = ubrr;

    // Enable receiver (RXEN0), transmitter (TXEN0), and RX Complete Interrupt (RXCIE0)
    UCSR0B |= (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);

    // Set 8-bit data frame format (standard 8N1 configuration (see in datasheet))
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// Hardware interrupt: Fires automatically when a new byte arrives on the RX pin
ISR(USART_RX_vect) {
    // reading UDR0 register automatically clears the hardware interrupt flag
    char data = UDR0;

    // calculate next head position in the ring buffer
    uint8_t next_head = (rx_head + 1) % RX_BUFFER_SIZE;
    
    // if the buffer is not full, store the byte. If it's full, we just drop it 
    // to prevent memory corruption (head shouldn't overwrite the tail)
    if (next_head != rx_tail) {
        rx_buffer[rx_head] = data;
        rx_head = next_head;
    }
}

void uart_putc(char c) {
    /* 
        Old blocking method:
        Waiting for empty transmit buffer
        while (!(UCSR0A & (1 << UDRE0)));
        UDR0 = c; 
     */

    uint8_t next_head = (tx_head + 1) % TX_BUFFER_SIZE;

    // if the buffer is completely full, we have to wait here until the
    // interrupt (USART_UDRE_vect) sends a byte and frees up some space
    while (next_head == tx_tail) {
        // just spin and wait
    }


    // Put data into our buffer and move the head
    tx_buffer[tx_head] = c;
    tx_head = next_head;

    // Turn on the "Data Register Empty" interrupt
    //
    UCSR0B |= (1 << UDRIE0);
}

void uart_putc_sync(char c) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

void uart_puts_sync_P(const char *progmem_s) {
    while (1) {
        char c = pgm_read_byte(progmem_s);
        if (c == '\0') break;
        uart_putc_sync(c);
        progmem_s++;
    }
}

// Hardware interrupt: Fires whenever the hardware UDR0 register is empty and ready for a new byte
ISR(USART_UDRE_vect) {
    // check if we still have bytes waiting in our buffer
    if (tx_head != tx_tail) {
        // throw the next byte into the register 
        // this clears the UDRE flag temporarily until the byte is physically sent
        UDR0 = tx_buffer[tx_tail];
        tx_tail = (tx_tail + 1) % TX_BUFFER_SIZE;
    } else {
        // Buffer is completely empty
        // We MUST disable this interrupt, otherwise it will fire infinitely 
        // millions of times and crash OS
        UCSR0B &= ~(1 << UDRIE0);
    }
}

// Helper function to send a normal RAM string
void uart_puts(const char* str) {
    while (*str != '\0') {
        uart_putc(*str);
        str++;
    }
}

// Old blocking read (not used much since we have an OS now lol)
unsigned char uart_getc() {
    // Waiting for data to be receiver
    while (!(UCSR0A & (1 << RXC0)));

    // Get and return received data from buffer
    return UDR0;
}

// Pulls a character from our RX ring buffer without blocking the CPU
int16_t uart_getc_nonblocking() {
    // if head == tail, the buffer is empty
    if (rx_head == rx_tail) {
        return -1;  // return -1 to indicate no data
    }

    // Grab the data and move the tail forward
    char data = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;

    return (uint8_t)data;
}


// Special function to print strings that are stored in Flash memory (PROGMEM) instead of RAM
void uart_puts_P(const char *progmem_s) {
    while (1) {
        char c = pgm_read_byte(progmem_s);
        if (c == '\0')
            break;
        else {
            uart_putc(c);
            progmem_s++;
        }
    }
}
