#include "../include/kernel.h"
#include "../include/uart.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>

/*
    Custom Preemptive RTOS Kernel for AVR (ATmega328P)

    How it works under the hood:
    Instead of standard sequential execution, we use a hardware timer (TIMER0_COMPA_vect) 
    to periodically interrupt the CPU. When the timer fires, we save all 32 general-purpose 
    CPU registers and the Status Register (SREG) onto the current task's stack. 
    Then our round-robin scheduler picks the next active task, swaps the hardware Stack Pointer (SP), 
    and restores its registers.

    To make a completely new task runnable for the very first time, we have to "fake" its history.
    We manually build a stack frame in os_task_create() that looks EXACTLY like the task was 
    already running and got interrupted by the OS. We push the task's starting function address 
    (Program Counter), enable global interrupts in a fake SREG, and clear out R0-R31.
*/

// Global variables for our memory and scheduler
uint8_t os_heap[HEAP_LENGTH];
TCB_t tasks[COUNT_TASKS];
uint16_t heap_allocated;
volatile uint8_t current_task = 0;
volatile uint32_t system_ticks = 0;

void kernel_panic(const char* reason_P) {
    cli();

    uart_puts_sync_P(PSTR("\r\n================================\r\n"));
    uart_puts_sync_P(PSTR("     !!! KERNEL PANIC !!!\r\n"));
    uart_puts_sync_P(PSTR("================================\r\n\n"));

    uart_puts_sync_P(PSTR("Reason: "));
    uart_puts_sync_P(reason_P);

    uart_puts_sync_P(PSTR("\r\nSystem halted.\r\n"));

    for (volatile uint32_t i = 0; i < 150000; i++) {

    }

    asm volatile ("jmp 0");
}

void os_task_create(uint8_t task_id,
                    void (*task_handler)(void), 
                    uint16_t stack_size, 
                    const char *name) 
{
    if ((heap_allocated + stack_size) > HEAP_LENGTH) {
        kernel_panic(PSTR("Out of OS Heap!"));
    }

    if (task_id >= COUNT_TASKS) {
        kernel_panic(PSTR("Invalid Task ID! If you want to add more tasks,\r\nincrease the value of COUNT_TASKS in kernel.h\r\nand also make sure you have enough memory"));
    }
    // assign a piece of our global heap to this task's stack
    tasks[task_id].stack_base = &os_heap[heap_allocated];
    tasks[task_id].stack_size = stack_size;

    // fill the stack with a known pattern (0xAA). 
    // later we can count how many 0xAA are left to calculate maximum stack usage
    for (uint16_t i = 0; i < stack_size; i++) {
        tasks[task_id].stack_base[i] = 0xAA;
    }

    // magic word at the very end of the stack to detect stack overflow
    *((uint32_t*)tasks[task_id].stack_base) = STACK_MAGIC_WORD;

    // Program Counter (2 bytes), where the CPU should jump
    // stack is LIFO, the lowest byte of the address goes to the very bottom
    tasks[task_id].stack_base[stack_size - 1] = (uint16_t)task_handler & 0xFF;
    tasks[task_id].stack_base[stack_size - 2] = ((uint16_t)task_handler >> 8) & 0xFF;

    // r0 register (needs to be saved/restored)
    tasks[task_id].stack_base[stack_size - 3] = 0;

    // Status Register - SREG (1 byte) 0x80 (0b10000000) 
    // sets the Global Interrupt Enable (I-bit)
    // if not set, the OS timer tick will never fire again after this task starts
    tasks[task_id].stack_base[stack_size - 4] = 0x80;

    // General Purpose Registers R1-R31 (32 bytes)
    // initialized to 0, we must provide dummy data to prevent stack pointer misalignment
    for (int i = stack_size - 5; i > (stack_size - 36); i--) {
        tasks[task_id].stack_base[i] = 0;
    }

    // Save the final Stack Pointer into the TCB
    // the pointer points to the next free byte after our 35 bytes of "fake" history
    tasks[task_id].stack_pointer = &tasks[task_id].stack_base[stack_size - 36];

    // initialize task state
    tasks[task_id].sleep_ticks = 0;
    tasks[task_id].task_id = task_id;
    tasks[task_id].name = name;

    // all tasks start running by default
    tasks[task_id].is_suspended = 0;

    tasks[task_id].is_allocated = 1;

    // move the heap pointer for the next task
    heap_allocated += stack_size;
}

// "naked" attribute tells GCC compiler NOT to generate standard prologue/epilogue
// we are doing context switching, so we must control exactly what goes on the stack
void __attribute__((naked)) os_yield() {

    // SAVE CONTEXT OF CURRENT TASK

    // When calling PUSH - stack pointer is decremented by 1
    // and data is pushed onto the stack

    // Register SREG is located in I/O Memory Space
    // so we use instruction IN to load a value from SREG
    // into r0 and then PUSH it onto the stack

    asm volatile (
        "push r0 \n\t"              // save r0 first to use it for SREG
        "in r0, __SREG__ \n\t"      // read SREG (it contains current interrupt state flags)
        "cli \n\t"                  // disable interrupts immediately so context switch isn't broken
        "push r0 \n\t"              // Save SREG to stack
        // push all remaining general-purpose registers (r1-r31)
        "push r1 \n\t"  "push r2 \n\t"  "push r3 \n\t"  "push r4 \n\t"
        "push r5 \n\t"  "push r6 \n\t"  "push r7 \n\t"  "push r8 \n\t"
        "push r9 \n\t"  "push r10 \n\t" "push r11 \n\t" "push r12 \n\t"
        "push r13 \n\t" "push r14 \n\t" "push r15 \n\t" "push r16 \n\t"
        "push r17 \n\t" "push r18 \n\t" "push r19 \n\t" "push r20 \n\t"
        "push r21 \n\t" "push r22 \n\t" "push r23 \n\t" "push r24 \n\t"
        "push r25 \n\t" "push r26 \n\t" "push r27 \n\t" "push r28 \n\t"
        "push r29 \n\t" "push r30 \n\t" "push r31 \n\t"
        "clr r1 \n\t"               // GCC expects r1 to be always zero in C code
        ::: "memory"
    );

    // Save the hardware stack pointer into current task's structure
    tasks[current_task].stack_pointer = (uint8_t*) SP;

    // stack overflow check
    uint32_t magic = *((uint32_t*)tasks[current_task].stack_base);

    if (magic != STACK_MAGIC_WORD) {
        kernel_panic(PSTR("Stack Overflow detected"));
    }

    // Scheduler logic: simple Round-Robin.
    // Find the next task in the array that is not sleeping
    uint8_t checked_tasks = 0;
    do {
        current_task = (current_task + 1) % COUNT_TASKS;
        checked_tasks++;

        if (checked_tasks > COUNT_TASKS) {
            kernel_panic(PSTR("All tasks blocked!"));
        }
    } while (tasks[current_task].sleep_ticks > 0 || 
            tasks[current_task].is_suspended == 1 || 
            tasks[current_task].is_allocated == 0);

    // Setting a stack pointer of the new chosen task
    SP = (uint16_t) tasks[current_task].stack_pointer;

    // RESTORE CONTEXT OF NEW TASK (in exact reverse order of pushing)

    // When calling POP - stack pointer is incremented by 1
    // and data is popped from the stack

    asm volatile ( \
        "pop r31 \n\t" "pop r30 \n\t" "pop r29 \n\t" "pop r28 \n\t" \
        "pop r27 \n\t" "pop r26 \n\t" "pop r25 \n\t" "pop r24 \n\t" \
        "pop r23 \n\t" "pop r22 \n\t" "pop r21 \n\t" "pop r20 \n\t" \
        "pop r19 \n\t" "pop r18 \n\t" "pop r17 \n\t" "pop r16 \n\t" \
        "pop r15 \n\t" "pop r14 \n\t" "pop r13 \n\t" "pop r12 \n\t" \
        "pop r11 \n\t" "pop r10 \n\t" "pop r9 \n\t"  "pop r8 \n\t" \
        "pop r7 \n\t"  "pop r6 \n\t"  "pop r5 \n\t"  "pop r4 \n\t" \
        "pop r3 \n\t"  "pop r2 \n\t"  "pop r1 \n\t" \
        "pop r0 \n\t" \
        "out __SREG__, r0 \n\t" \
        "pop r0 \n\t" \
        ::: "memory" \
    );

    // instruction RETI - pops the 16-bit PC from the stack, 
    // enables global interrupts, and jumps to the task,
    asm volatile ("reti");
}

// Hardware Timer Interrupt (fires every tick, e.g. 1ms)
ISR(TIMER0_COMPA_vect, ISR_NAKED) {

    // Exact same context saving as in os_yield()

    asm volatile ( \
        "push r0 \n\t" \
        "in r0, __SREG__ \n\t" \
        "push r0 \n\t" \
        "push r1 \n\t"  "push r2 \n\t"  "push r3 \n\t"  "push r4 \n\t" \
        "push r5 \n\t"  "push r6 \n\t"  "push r7 \n\t"  "push r8 \n\t" \
        "push r9 \n\t"  "push r10 \n\t" "push r11 \n\t" "push r12 \n\t" \
        "push r13 \n\t" "push r14 \n\t" "push r15 \n\t" "push r16 \n\t" \
        "push r17 \n\t" "push r18 \n\t" "push r19 \n\t" "push r20 \n\t" \
        "push r21 \n\t" "push r22 \n\t" "push r23 \n\t" "push r24 \n\t" \
        "push r25 \n\t" "push r26 \n\t" "push r27 \n\t" "push r28 \n\t" \
        "push r29 \n\t" "push r30 \n\t" "push r31 \n\t" "clr r1 \n\t" ::: "memory"\
    );

    // Save stack pointer
    tasks[current_task].stack_pointer = (uint8_t*) SP;

    // stack overflow check
    uint32_t magic = *((uint32_t*)tasks[current_task].stack_base);

    if (magic != STACK_MAGIC_WORD) {
        kernel_panic(PSTR("Stack Overflow detected"));
    }

    system_ticks++;

    // update sleep timers for all tasks, if a task is sleeping = subtract 1 tick
    for (int8_t i = 0; i < COUNT_TASKS; i++) {
        if (tasks[i].sleep_ticks > 0)
            tasks[i].sleep_ticks--;
    }

    // Round-Robin scheduler logic
    uint8_t checked_tasks = 0;
    do {
        current_task = (current_task + 1) % COUNT_TASKS;
        checked_tasks++;

        if (checked_tasks > COUNT_TASKS) {
            kernel_panic(PSTR("All tasks blocked!"));
        }
    } while (tasks[current_task].sleep_ticks > 0 ||
            tasks[current_task].is_suspended == 1 ||
            tasks[current_task].is_allocated == 0);

    SP = (uint16_t) tasks[current_task].stack_pointer;

    // When calling POP - stack pointer is incremented by 1
    // and data is popped from the stack

    // Exact same context restoring as in os_yield()

    asm volatile ( \
        "pop r31 \n\t" "pop r30 \n\t" "pop r29 \n\t" "pop r28 \n\t" \
        "pop r27 \n\t" "pop r26 \n\t" "pop r25 \n\t" "pop r24 \n\t" \
        "pop r23 \n\t" "pop r22 \n\t" "pop r21 \n\t" "pop r20 \n\t" \
        "pop r19 \n\t" "pop r18 \n\t" "pop r17 \n\t" "pop r16 \n\t" \
        "pop r15 \n\t" "pop r14 \n\t" "pop r13 \n\t" "pop r12 \n\t" \
        "pop r11 \n\t" "pop r10 \n\t" "pop r9 \n\t"  "pop r8 \n\t" \
        "pop r7 \n\t"  "pop r6 \n\t"  "pop r5 \n\t"  "pop r4 \n\t" \
        "pop r3 \n\t"  "pop r2 \n\t"  "pop r1 \n\t" \
        "pop r0 \n\t" \
        "out __SREG__, r0 \n\t" \
        "pop r0 \n\t" \
        ::: "memory" \
    );

    asm volatile ("reti");
}

// Jumpstarts the OS by loading the first task's context
void os_start_application() {
    SP = (uint16_t) tasks[0].stack_pointer;

    // Pop all initial fake registers we created in os_task_create
    asm volatile ( \
        "pop r31 \n\t" "pop r30 \n\t" "pop r29 \n\t" "pop r28 \n\t" \
        "pop r27 \n\t" "pop r26 \n\t" "pop r25 \n\t" "pop r24 \n\t" \
        "pop r23 \n\t" "pop r22 \n\t" "pop r21 \n\t" "pop r20 \n\t" \
        "pop r19 \n\t" "pop r18 \n\t" "pop r17 \n\t" "pop r16 \n\t" \
        "pop r15 \n\t" "pop r14 \n\t" "pop r13 \n\t" "pop r12 \n\t" \
        "pop r11 \n\t" "pop r10 \n\t" "pop r9 \n\t"  "pop r8 \n\t" \
        "pop r7 \n\t"  "pop r6 \n\t"  "pop r5 \n\t"  "pop r4 \n\t" \
        "pop r3 \n\t"  "pop r2 \n\t"  "pop r1 \n\t" \
        "pop r0 \n\t" \
        "out __SREG__, r0 \n\t" \
        "pop r0 \n\t" \
    );

    // This 'reti' jumps to the first task and turns on interrupts globally
    asm volatile ("reti");
}

// Puts the current task to sleep for a specific amount of timer ticks
void os_delay(uint16_t ticks) {
    if (ticks == 0) return;
    
    // 16-bit variables on an 8-bit AVR processor take two instructions to write,
    // we must temporarily disable interrupts, otherwise the timer might fire in the middle 
    // of writing and read a corrupted value

    uint8_t sreg = SREG;    // save current interrupt state
    cli();                  // disable interrupts

    tasks[current_task].sleep_ticks = ticks;

    SREG = sreg;            // restore interrupt state

    // Instantly give up the CPU so other tasks can run while we wait
    os_yield();
}

void os_mutex_init(mutex_t *m) {
    m->is_locked = 0;
    m->owner_id = 0;
}

// Spinlock mutex - locks a resource for the current task
void os_mutex_lock(mutex_t *m) {
    while (1) {
        // atomic check
        cli();

        if (m->is_locked == 0) {
            m->is_locked = 1;
            m->owner_id = current_task;
            sei();
            // we got the lock, exit the loop
            break;
        } else {
            // didn't get it, turn interrupts back on and try again
            sei();
        }
    }
}

// Unlocks the resource but only if the current task is the actual owner
void os_mutex_unlock(mutex_t *m) {
    cli();
    if (m->owner_id == current_task) {
        m->is_locked = 0;
        m->owner_id = 0;
    }
    sei();
}

// Just returns the amount of free general SRAM (not part of the task stacks)
uint16_t os_get_free_heap() {
    return RAMEND - (uint16_t)&__bss_end;
}

// Calculates the maximum amount of stack space this task has ever used
// we count the untouched 0xAA bytes at the bottom and subtract that from the total size 
// to see exactly how many bytes were overwritten during execution
uint16_t os_get_task_used_stack(uint8_t task_id) {
    int16_t i;

    // start from index 4 because the very bottom contains our MAGIC WORD (4 bytes)
    for (i = 4; i < tasks[task_id].stack_size; i++) {
        if (tasks[task_id].stack_base[i] != 0xAA)
            break;  // found modified memory, stack reached here at some point
    }

    // Total size - untouched bytes = maximum used bytes
    return (tasks[task_id].stack_size - i);
}

uint16_t os_get_stack_size(uint8_t task_id) {
    return tasks[task_id].stack_size;
}

uint16_t os_get_used_stack_heap() {
    return heap_allocated;
}
