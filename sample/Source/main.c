/**
 * @file
 * @author Alexandre Abadie <alexandre.abadie@inria.fr>
 * @brief Sample non secure application
 *
 * @copyright Inria, 2024
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <nrf.h>

void increment_counter(void);
void get_counter(uint32_t *counter);
void initialize_leds(void);
void toggle_led1(void);
void toggle_led2(void);
void reload_wdt(void);

int main(void) {
    puts("Hello Non Secure World!");
    initialize_leds();

    while (1) {
        increment_counter();

        uint32_t counter;
        get_counter(&counter);
        if ((counter % 10) == 0) {
            toggle_led1();
        }
        printf("Non secure counter: %ld\n", counter);
        if ((counter % 20) == 0) {
            toggle_led2();
        }

        reload_wdt();
        volatile uint32_t delay = 0xffff;
        while (delay--) {}
    };
}
