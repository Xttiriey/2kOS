#include "../include/idle_task.h"
#include <avr/sleep.h>

void idle_task() {
    set_sleep_mode(SLEEP_MODE_IDLE);

    while(1) {
        sleep_enable();
        
        sleep_cpu();

        sleep_disable();
    }
}
