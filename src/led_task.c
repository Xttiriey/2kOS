#include "../include/led_task.h"
#include "../include/cli.h"
#include "../include/kernel.h"
#include <avr/io.h>

void led_task() {
    while (1) {
        if (led_is_blinking) {
            PORTB ^= (1 << PB0);
            os_delay(500); 
        } else {
            PORTB &= ~(1 << PB0);
            os_delay(500);
        }
    }
}
