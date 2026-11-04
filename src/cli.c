#include "../include/cli.h"
#include "../include/uart.h"
#include "../include/kernel.h"
#include "../include/helpers.h"
#include <avr/io.h>
#include <stdint.h>
#include <stddef.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

// --- PROGMEM Strings ---
// We store all command names in Flash memory instead of SRAM
// If we kept these as normal strings, they would eat up 
// our precious 2KB of SRAM right at startup 
const char cmd_name_uptime[] PROGMEM = "uptime";
const char cmd_name_ps[] PROGMEM = "ps";
const char cmd_name_memory[] PROGMEM = "memory";
const char cmd_name_led_on[] PROGMEM = "led on";
const char cmd_name_led_off[] PROGMEM = "led off";
const char cmd_name_help[] PROGMEM = "help";
const char cmd_name_clear[] PROGMEM = "clear";
const char cmd_name_reboot[] PROGMEM = "reboot";
const char cmd_name_echo[] PROGMEM = "echo";
const char cmd_name_suspend[] PROGMEM = "suspend";
const char cmd_name_resume[] PROGMEM = "resume";

volatile uint8_t led_is_blinking = 1;

// --- Command Handlers ---

void led_on_command(char* args) {
    led_is_blinking = 1;
    uart_puts("LED is turned on.\r\n");
}

void led_off_command(char* args) {
    led_is_blinking = 0;
    uart_puts("LED is turned off.\r\n");
}

void uptime_command(char* args) {
    // buffers for custom _itoa function to hold string representations of numbers
    char buff_ticks_seconds[3];
    char buff_ticks_minutes[10];

    // system_ticks counts milliseconds. divide by 1000 to get total seconds
    if ((system_ticks / 1000) > 59) {
        _itoa((system_ticks / 1000 / 60), buff_ticks_minutes);
        _itoa((system_ticks / 1000 % 60), buff_ticks_seconds);

        // uart_puts_P prints strings directly from Flash (PSTR macro puts the string in Flash)
        uart_puts_P(PSTR("Uptime: "));
        uart_puts(buff_ticks_minutes);
        uart_puts_P(PSTR(" minutes "));
        uart_puts(buff_ticks_seconds);
        uart_puts_P(PSTR(" seconds\r\n"));
    } else  {
        _itoa(system_ticks / 1000, buff_ticks_seconds);

        uart_puts_P(PSTR("Uptime: "));
        uart_puts(buff_ticks_seconds);
        uart_puts_P(PSTR(" seconds\r\n"));
    }
}

// Lists all currently created tasks (like 'ps' in Linux)
void ps_command(char* args) {
    for (uint8_t i = 0; i < COUNT_TASKS; i++) {
        char task_num[2];
        _itoa(i, task_num);

        uart_puts_P(PSTR("[Task "));
        uart_puts(task_num);
        uart_puts_P(PSTR("]: "));

        uart_puts(tasks[i].name); 

        uart_puts_P(PSTR("\r\n"));
    }
}

// Shows detailed statistics about our SRAM usage and task stacks
void memory_command(char* args) {
    char free_ram_buff[5];
    char free_stack_global_buff[5];
    char heap_global_buff[5];

    _itoa(os_get_free_heap(), free_ram_buff);
    _itoa(os_get_used_stack_heap(), free_stack_global_buff);
    _itoa(HEAP_LENGTH, heap_global_buff);

    uart_puts_P(PSTR("\r\nUnallocated SRAM: "));
    uart_puts(free_ram_buff);
    uart_puts_P(PSTR(" bytes \r\n"));
    uart_puts_P(PSTR("\r\nOS Heap Used: "));
    uart_puts(free_stack_global_buff);
    uart_puts_P(PSTR(" / "));
    uart_puts(heap_global_buff);
    uart_puts_P(PSTR(" bytes used\r\n\n"));

    for (int8_t i = 0; i < COUNT_TASKS; i++) {
        char free_stack_buff[4];
        char i_buff[2];
        char stack_size_buff[4];

        _itoa(os_get_task_used_stack(i), free_stack_buff);
        _itoa(i, i_buff);
        _itoa(os_get_stack_size(i), stack_size_buff);

        uart_puts_P(PSTR("[Task "));
        uart_puts(i_buff);
        uart_puts_P(PSTR("] "));
        uart_puts(tasks[i].name);
        uart_puts_P(PSTR(" : "));
        uart_puts(free_stack_buff);
        uart_puts_P(PSTR(" / "));
        uart_puts(stack_size_buff);
        uart_puts_P(PSTR(" bytes used\r\n"));
    }
}

// Prints all available commands
void help_command(char* args);

// Clears the terminal screen using standard ANSI escape codes
void clear_command(char* args) {
    // \033[2J clears entire screen, \033[H moves cursor to top left
    uart_puts_P(PSTR("\033[2J\033[H"));
}

void reboot_command(char* args) {
    cli();
    asm volatile ("jmp 0");
}

// Prints back whatever text you typed after the command
void echo_command(char* args) {
    if (args != NULL) {
        uart_puts(args);
    }
    uart_puts_P(PSTR("\r\n"));
}

// Freezes a task by its ID
void suspend_command(char* args) {
    if (args == NULL) {
        uart_puts_P(PSTR("Error: Please provide task ID\r\n"));
        return;
    }
    
    // Convert string argument to integer
    uint8_t id = _atoi(args);
    
    if (id < COUNT_TASKS) {
        tasks[id].is_suspended = 1;
        uart_puts_P(PSTR("Task suspended.\r\n"));
    } else {
        uart_puts_P(PSTR("Error: Invalid task ID\r\n"));
    }
}

// Unfreezes a task by its ID
void resume_command(char* args) {
    if (args == NULL) {
        uart_puts_P(PSTR("Error: Please provide task ID\r\n"));
        return;
    }
    
    uint8_t id = _atoi(args);
    
    if (id < COUNT_TASKS) {
        tasks[id].is_suspended = 0;
        uart_puts_P(PSTR("Task resumed.\r\n"));
    } else {
        uart_puts_P(PSTR("Error: Invalid task ID\r\n"));
    }
}

// Array of command structures
// we place this entire array in PROGMEM so the pointers and struct data don't take up SRAM
const command_t cli_commands[] PROGMEM = {
    {cmd_name_led_on, led_on_command},
    {cmd_name_led_off, led_off_command},
    {cmd_name_reboot, reboot_command},
    {cmd_name_uptime, uptime_command},
    {cmd_name_ps, ps_command},
    {cmd_name_memory, memory_command},
    {cmd_name_help, help_command},
    {cmd_name_clear, clear_command},
    {cmd_name_echo, echo_command},
    {cmd_name_suspend, suspend_command},
    {cmd_name_resume, resume_command},
};

// calculate the number of commands automatically based on array size
#define CLI_CMD_COUNT (sizeof(cli_commands) / sizeof(command_t))

// Prints all available commands
void help_command(char* args) {
    uart_puts_P(PSTR("\r\nAvailable commands:\r\n"));
    for (uint8_t i = 0; i < CLI_CMD_COUNT; i++) {
        // Read the name from PROGMEM and print it
        const char* progmem_name_ptr = (const char*) pgm_read_word(&cli_commands[i].name);
        uart_puts_P(PSTR("- "));
        uart_puts_P(progmem_name_ptr);
        uart_puts_P(PSTR("\r\n"));
    }
}

// Parses the user input and tries to find a matching command
uint8_t terminal_process_input(char* input_buffer) {
    uint8_t command_found = 0;

    // Split the input into two parts: the command and its arguments
    // _strtok replaces the first space with '\0' and returns the first word
    char* cmd_name = _strtok(input_buffer, ' ');
    char* cmd_args = _strtok(NULL, '\r'); // Get the rest of the string
    
    if (cmd_name == NULL) return 0; // Empty input

    for (uint8_t i = 0; i < CLI_CMD_COUNT; i++) {
        // Because the cli_commands array is in Flash, we can't just read it like a normal array
        // we have to use pgm_read_word() to fetch the 16-bit pointer to the command name string
        const char* progmem_name_ptr = (const char*) pgm_read_word(&cli_commands[i].name);

        // strcmp_P compares a string in RAM (input_buffer) with a string in Flash (progmem_name_ptr)
        if (strcmp_P(cmd_name, progmem_name_ptr) == 0) {
            // read the function pointer from Flash
            void (*cmd_handler)(char*) = (void (*)(char*)) pgm_read_word(&cli_commands[i].handler);
            // execute the matched command
            cmd_handler(cmd_args);

            command_found = 1;
            break;  // stop searching once we found a match
        }
    }

    // return 1 if command was not recognized
    if (!command_found) {
        return 1;
    }

    return 0;
}

void terminal_task() {
    char strBuffer[32];
    uint8_t indexCounter = 0;

    uart_puts_P(PSTR("> "));
    while (1) {
        int16_t ch = uart_getc_nonblocking();

        if (ch != -1) {
            char c = (char)ch;
            if (c == '\r') {
                strBuffer[indexCounter] = '\0';
                uart_puts_P(PSTR("\r\n"));
                if (terminal_process_input(strBuffer) == 1) {
                    uart_puts_P(PSTR("\r\nCommand not found."));
                }
                indexCounter = 0;
                uart_puts_P(PSTR("\r\n> "));
            } else if (c == '\b' || c == '\x7F') {
                if (indexCounter > 0) {
                    indexCounter--;
                    uart_puts_P(PSTR("\b \b"));
                }
            } else {
                uart_putc(c);
                strBuffer[indexCounter] = c;
                if (indexCounter < (sizeof(strBuffer) - 1)) {
                    indexCounter++;
                }
            }
        } else {
            os_delay(10);
        }
    }
}

