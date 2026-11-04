#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

/* 
    OS Memory Architecture & Constraints:
    The ATmega328P has only 2KB (2048 bytes) of SRAM. 
    Consequently, it is obvious that we strictly avoid dynamic memory allocation
    in order to prevent memory fragmentation and unpredictable hard faults.

    Instead, we use a static memory pool approach. All task stacks are allocated directly 
    from a single global array (os_heap) during task creation. 
    The number of tasks (COUNT_TASKS) and their stack sizes must be carefully balanced 
    to leave enough free SRAM for the OS kernel, global variables, and UART buffers.

    Stack monitoring:
    The stack grows downwards. A specific "Magic Word" (0x600DCAFE) is placed at the very bottom 
    of each task's allocated memory. This allows us to easily detect stack overflows. 
    The remaining empty space is filled with 0xAA to help us calculate the High Water Mark 
    (the maximum amount of stack space a task has ever used).
 */

// Number of tasks running in our OS. Keep it small, SRAM is limited!
#define COUNT_TASKS 3

// Total memory reserved for all task stacks combined
#define HEAP_LENGTH 1024

// Used for stack overflow detection (Twin Peaks reference (that's a damn fine cup of coffee))
#define STACK_MAGIC_WORD 0x600DCAFE

// Task Control Block (TCB). Think of it as a passport for each task
// It holds everything the OS needs to know about a task when it's not running
typedef struct {
    uint8_t* stack_base;        // Pointer to the very bottom of the task's stack memory
    uint16_t stack_size;        // Total allocated stack size for this task
    uint8_t task_id;            // Unique ID of the task
    uint8_t* stack_pointer;     // SP is saved here during a context switch
    const char *name;           // Human-readable name (useful for 'ps' command or debugging)
    volatile uint16_t sleep_ticks;  // How many ticks left to sleep. 'volatile' because timer interrupt modifies it!
    volatile uint8_t is_suspended; // 1 = frozen, 0 = running
    uint8_t is_allocated;
} TCB_t;


// A simple spinlock mutex
typedef struct {
    volatile uint8_t is_locked;    // 1 if locked, 0 if free
    uint8_t owner_id;              // ID of the task that currently holds the lock
} mutex_t;



// --- Global variables shared across the OS ---



extern TCB_t tasks[COUNT_TASKS];            // Array holding all task passports
extern uint8_t os_heap[HEAP_LENGTH];        // The big chunk of SRAM we slice for task stacks

extern volatile uint8_t current_task;       // ID of the currently running task
extern volatile uint32_t system_ticks;      // Uptime counter (increments every timer tick)

// Variable provided by the GCC linker
// It points to the end of statically allocated variables in SRAM 
// We use it to calculate how much totally free RAM is left
extern char __bss_end;

extern uint16_t heap_allocated;             // Keeps track of how much of os_heap is already used



// --- OS Functions ---



void kernel_panic(const char* reason_P);

// Allocates stack from the heap and sets up the initial fake context for a new task
void os_task_create(uint8_t task_id,
                    void (*task_handler)(void), 
                    uint16_t stack_size, 
                    const char *name);

// Forces an immediate context switch
// 'naked' means the compiler won't generate prologue/epilogue, we handle the stack manually
void os_yield() __attribute__((naked));

// Kicks off the first task (Task 0) and turns on global interrupts to start the timer
void os_start_application();

// Puts the current task to sleep for a given number of timer ticks
void os_delay(uint16_t ticks);

// Returns remaining general SRAM (outside of our OS heap and global variables)
uint16_t os_get_free_heap();

// Uses the 0xAA watermark to see how much stack space the task NEVER touched
uint16_t os_get_task_used_stack(uint8_t task_id);

// Returns the total allocated size of a task's stack
uint16_t os_get_stack_size(uint8_t task_id);

// Returns how much space is left in our os_heap array for new tasks
uint16_t os_get_used_stack_heap();

void os_mutex_init(mutex_t *m);

void os_mutex_lock(mutex_t *m);

void os_mutex_unlock(mutex_t *m);

#endif
