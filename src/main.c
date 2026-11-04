#include "../include/uart.h"
#include "../include/timer.h"
#include "../include/kernel.h"
#include "../include/cli.h"
#include "../include/led_task.h"
#include "../include/idle_task.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>

const char boot_logo[] PROGMEM = 
"\r\n"
"  ____  _     ___  ____  \r\n"
" |___ \\| | __/ _ \\/ ___| \r\n"
"   __) | |/ / | | \\___ \\ \r\n"
"  / __/|   <| |_| |___) |\r\n"
" |_____|_|\\_\\\\___/|____/ \r\n"
"                         \r\n"
" 2kOS v1.0 - Static RTOS \r\n"
" ========================\r\n";


int main() {
    DDRB |= (1 << PB0); // Configure LED pin as output

    USART_init(_UBRR_);
    uart_puts_sync_P(boot_logo);
    uart_puts_sync_P(PSTR("[ OK ] UART driver initialized\r\n"));

    timer0_init();
    uart_puts_sync_P(PSTR("[ OK ] Kernel timer (Timer0) started\r\n"));

    // uart_puts_P(boot_logo);
    // uart_puts(boot_logo);
    // uart_puts("\r\n\nKernel is loading...\r\n\n");

    uart_puts_sync_P(PSTR("[....] Allocating task stacks...\r\n"));


    os_task_create(0, led_task, 64, "LED_task");
    os_task_create(1, terminal_task, 256, "Terminal");
    os_task_create(2, idle_task, 64, "Idle");

    uint8_t allocated = 0;
    for (uint8_t i = 0; i < COUNT_TASKS; i++) {
        if (tasks[i].is_allocated) {
            allocated++;
        }
    }

    if (allocated != COUNT_TASKS) {
        kernel_panic(PSTR("Unallocated Task in TCB pool!\r\nThe value of COUNT_TASKS is incorrect."));
    }

    uart_puts_sync_P(PSTR("[ OK ] TCBs and stacks created\r\n"));
    uart_puts_sync_P(PSTR("Starting 2kOS Scheduler...\r\n\r\n"));

    os_start_application();

    return 0;
}
