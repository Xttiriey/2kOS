#ifndef CLI_H
#define CLI_H

#include <stdint.h>

typedef struct  {
    const char *name;
    void (*handler)(char *args);
} command_t;

extern const command_t cli_commands[];
extern uint8_t commands_count;

extern volatile uint8_t led_is_blinking;

void terminal_task();

#endif


