/**
 * @file
 * @author Alexandre Abadie <alexandre.abadie@inria.fr>
 * @brief Sample non secure application
 *
 * @copyright Inria, 2024
 *
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <nrf.h>

void reload_wdt0(void);
static bool _timer_running = false;

static void delay_ms(uint32_t ms) {
    NRF_TIMER0_NS->TASKS_CAPTURE[0] = 1;
    NRF_TIMER0_NS->CC[0] += ms * 1000;
    _timer_running = true;
    while (_timer_running) {
        __WFE();
    }
}

int main(void) {
    puts("Hello Non Secure World!");
    NRF_P0_NS->DIRSET = (1 << 30);

    NRF_TIMER0_NS->TASKS_CLEAR = 1;
    NRF_TIMER0_NS->PRESCALER   = 4;  // Run TIMER at 1MHz
    NRF_TIMER0_NS->BITMODE     = (TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos);
    NRF_TIMER0_NS->INTEN       = (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos);
    NVIC_EnableIRQ(TIMER0_IRQn);
    NRF_TIMER0_NS->TASKS_START = 1;

    NRF_DPPIC_NS->CHENSET = (DPPIC_CHENSET_CH0_Disabled << DPPIC_CHENSET_CH0_Pos);

    while (1) {
        delay_ms(200);
        reload_wdt0();
        NRF_P0_NS->OUT ^= (1 << 30);
    };
}

void TIMER0_IRQHandler(void) {

    if (NRF_TIMER0_NS->EVENTS_COMPARE[0] == 1) {
        NRF_TIMER0_NS->EVENTS_COMPARE[0] = 0;
        _timer_running = false;
    }
}
