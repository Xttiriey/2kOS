#include <avr/io.h>
#include "../include/timer.h"

// --- Automatic timer calculation based on CPU frequency ---
// We want an interrupt exactly every 1 millisecond (1000 Hz tick rate)
// Formula: OCR0A = (F_CPU / (Prescaler * 1000)) - 1

#if F_CPU == 1000000UL
    #define TMR_PRESCALER 8
    #define TMR_CS_BITS (1 << CS01)

#elif F_CPU == 8000000UL
    #define TMR_PRESCALER 64
    #define TMR_CS_BITS ((1 << CS01) | (1 << CS00))

#elif F_CPU == 16000000UL
    #define TMR_PRESCALER 64
    #define TMR_CS_BITS ((1 << CS01) | (1 << CS00))

#else
    // If an unsupported frequency is provided in the Makefile
    // the compiler will throw this error before building the kernel
    #error "Unsupported F_CPU! 2kOS kernel timer supports only 1MHz, 8MHz, or 16MHz."
#endif

#define TMR_TOP_VALUE ((F_CPU / (TMR_PRESCALER * 1000)) - 1)

// Hardware limitation check: OCR0A is an 8-bit register. 
// If the resulting TOP value is > 255, the timer will overflow and fail
#if TMR_TOP_VALUE > 255
    #error "Timer0 TOP value exceeds 255! Math is wrong for this F_CPU."
#endif

void timer0_init() {
    // Enable CTC mode (Clear Timer on Compare Match)
    TCCR0A = (1 << WGM01);
    
    // Set our automatically calculated TOP value (e.g., 124 for 1/8MHz, 249 for 16MHz)
    OCR0A = TMR_TOP_VALUE;

    // Enable Timer/Counter0 Compare Match A interrupt
    TIMSK0 = (1 << OCIE0A);

    // Start the timer with the automatically selected prescaler bits
    TCCR0B = TMR_CS_BITS;
}
