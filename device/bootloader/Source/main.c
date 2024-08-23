/*********************************************************************
*                    SEGGER Microcontroller GmbH                     *
*                        The Embedded Experts                        *
**********************************************************************

-------------------------- END-OF-HEADER -----------------------------

File    : main.c
Purpose : Generic application start

*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <arm_cmse.h>
#include <nrf.h>

#define LED1_PIN 28
#define LED2_PIN 29
#define LED3_PIN 30
#define LED4_PIN 31

__attribute__((cmse_nonsecure_entry)) void increment_counter(void);
__attribute__((cmse_nonsecure_entry)) void get_counter(uint32_t *counter);
__attribute__((cmse_nonsecure_entry)) void initialize_leds(void);
__attribute__((cmse_nonsecure_entry)) void toggle_led1(void);
__attribute__((cmse_nonsecure_entry)) void toggle_led2(void);
__attribute__((cmse_nonsecure_entry)) void reload_wdt(void);

static uint32_t _counter;

__attribute__((cmse_nonsecure_entry)) void increment_counter(void) {
    _counter++;
}

__attribute__((cmse_nonsecure_entry)) void get_counter(uint32_t *counter) {
    *counter = _counter;
}

__attribute__((cmse_nonsecure_entry)) void initialize_leds(void) {
    NRF_P0_S->DIRSET = (1 << LED1_PIN);
    NRF_P0_S->OUTSET = (1 << LED1_PIN);
    NRF_P0_S->DIRSET = (1 << LED2_PIN);
    NRF_P0_S->OUTSET = (1 << LED2_PIN);
}

__attribute__((cmse_nonsecure_entry)) void toggle_led1(void) {
    NRF_P0_S->OUT ^= (1 << LED1_PIN);
}

__attribute__((cmse_nonsecure_entry)) void toggle_led2(void) {
    NRF_P0_S->OUT ^= (1 << LED2_PIN);
}

__attribute__((cmse_nonsecure_entry)) void reload_wdt(void) {
    NRF_WDT0_S->RR[0] = WDT_RR_RR_Reload << WDT_RR_RR_Pos;
}

typedef void (*reset_handler_t)(void) __attribute__((cmse_nonsecure_call));

typedef struct {
    uint32_t msp;                  ///< Main stack pointer
    reset_handler_t reset_handler; ///< Reset handler
} vector_table_t;

static vector_table_t *table = (vector_table_t *)0x00020000; // Image should start with vector table

static void initialize_watchdog(void) {

    // Configuration: keep running while sleeping + pause when halted by debugger
    NRF_WDT0_S->CONFIG = (WDT_CONFIG_SLEEP_Run << WDT_CONFIG_SLEEP_Pos |
                         WDT_CONFIG_HALT_Pause << WDT_CONFIG_HALT_Pos);

    // Enable reload register 0
    NRF_WDT0_S->RREN = WDT_RREN_RR0_Enabled << WDT_RREN_RR0_Pos;

    // Configure timeout and callback
    NRF_WDT0_S->CRV = (5 * 32768) - 1;
    NRF_WDT0_S->TASKS_START = WDT_TASKS_START_TASKS_START_Trigger << WDT_TASKS_START_TASKS_START_Pos;
}

static void initialize_ns(void) {

    // Prioritize Secure exceptions over Non-Secure
    // Set non-banked exceptions to target Non-Secure
    // Don't allow Non-Secure firmware to issue System resets
    uint32_t aircr = SCB->AIRCR & (~(SCB_AIRCR_VECTKEY_Msk));
    aircr |= SCB_AIRCR_PRIS_Msk | SCB_AIRCR_BFHFNMINS_Msk | SCB_AIRCR_SYSRESETREQS_Msk;
    SCB->AIRCR = ((0x05FAUL << SCB_AIRCR_VECTKEY_Pos) & SCB_AIRCR_VECTKEY_Msk) | aircr;

    // Allow FPU in non secure
    SCB->NSACR |= (1UL << SCB_NSACR_CP10_Pos) | (1UL << SCB_NSACR_CP11_Pos);

    // Enable secure fault handling
    SCB->SHCSR |= SCB_SHCSR_SECUREFAULTENA_Msk;

    // Enable div by zero usage fault
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;

    // Enable not aligned access fault
    SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk;

    // Disable SAU in order to use SPU instead
    SAU->CTRL = 0;;
    SAU->CTRL |= 1 << 1;  // Make all memory non secure

    // Configure secure RAM. One RAM region takes 8KiB so secure RAM is 128KiB.
    for (uint8_t region = 0; region < 16; region++) {
        NRF_SPU_S->RAMREGION[region].PERM = (SPU_RAMREGION_PERM_READ_Enable << SPU_RAMREGION_PERM_READ_Pos |
                                             SPU_RAMREGION_PERM_WRITE_Enable << SPU_RAMREGION_PERM_WRITE_Pos |
                                             SPU_RAMREGION_PERM_EXECUTE_Enable << SPU_RAMREGION_PERM_EXECUTE_Pos |
                                             SPU_RAMREGION_PERM_SECATTR_Secure << SPU_RAMREGION_PERM_SECATTR_Pos);
    }
    // Configure non secure RAM
    for (uint8_t region = 16; region < 64; region++) {
        NRF_SPU_S->RAMREGION[region].PERM = (SPU_RAMREGION_PERM_READ_Enable << SPU_RAMREGION_PERM_READ_Pos |
                                             SPU_RAMREGION_PERM_WRITE_Enable << SPU_RAMREGION_PERM_WRITE_Pos |
                                             SPU_RAMREGION_PERM_EXECUTE_Enable << SPU_RAMREGION_PERM_EXECUTE_Pos |
                                             SPU_RAMREGION_PERM_SECATTR_Non_Secure << SPU_RAMREGION_PERM_SECATTR_Pos);
    }

    // First flash regions are secure. One flash region takes 16KiB so secure flash is 128 KiB.
    for (uint8_t region = 0; region < 8; region++) {
        NRF_SPU_S->FLASHREGION[region].PERM = (SPU_FLASHREGION_PERM_READ_Enable << SPU_FLASHREGION_PERM_READ_Pos |
                                               SPU_FLASHREGION_PERM_WRITE_Enable << SPU_FLASHREGION_PERM_WRITE_Pos |
                                               SPU_FLASHREGION_PERM_EXECUTE_Enable << SPU_FLASHREGION_PERM_EXECUTE_Pos |
                                               SPU_FLASHREGION_PERM_SECATTR_Secure << SPU_FLASHREGION_PERM_SECATTR_Pos);
    }
    // Configure non secure flash address space
    for (uint8_t region = 8; region < 64; region++) {
        NRF_SPU_S->FLASHREGION[region].PERM = (SPU_FLASHREGION_PERM_READ_Enable << SPU_FLASHREGION_PERM_READ_Pos |
                                               SPU_FLASHREGION_PERM_WRITE_Enable << SPU_FLASHREGION_PERM_WRITE_Pos |
                                               SPU_FLASHREGION_PERM_EXECUTE_Enable << SPU_FLASHREGION_PERM_EXECUTE_Pos |
                                               SPU_FLASHREGION_PERM_SECATTR_Non_Secure << SPU_FLASHREGION_PERM_SECATTR_Pos);
    }

    // Configure Non Secure Callable subregion
    NRF_SPU_S->FLASHNSC[0].REGION = 7;
    NRF_SPU_S->FLASHNSC[0].SIZE = 8;

    // Set clock NS as non secure
    NRF_SPU_S->PERIPHID[5].PERM = SPU_PERIPHID_PERM_SECATTR_NonSecure << SPU_PERIPHID_PERM_SECATTR_Pos;

    __DSB(); // Force memory writes before continuing
    __ISB(); // Flush and refill pipeline with updated permissions
}

int main(void) {

    // Check reset reason and remain in loop if reset was triggered by wdt timeout
    uint32_t resetreas = NRF_RESET_S->RESETREAS;
    NRF_RESET_S->RESETREAS = NRF_RESET_S->RESETREAS;
    if (resetreas & RESET_RESETREAS_DOG0_Detected << RESET_RESETREAS_DOG0_Pos) {
        NRF_P0_S->DIRSET = (1 << LED4_PIN);
        NRF_P0_S->OUTSET = (1 << LED4_PIN);
        while (1) {
            NRF_P0_S->OUT ^= (1 << LED4_PIN);
            volatile uint16_t delay = 0xffff;
            while (delay--) {}
        }
    }

    // Initialize watchdog and non secure access
    initialize_watchdog();
    initialize_ns();

    // Set the vector table address prior to jumping to image
    SCB_NS->VTOR = (uint32_t)table;
    __TZ_set_MSP_NS(table->msp);
    __TZ_set_CONTROL_NS(0);

    // Flush and refill pipeline
    __ISB();

    // Jump to non secure image
    reset_handler_t reset_handler_ns = (reset_handler_t)(cmse_nsfptr_create(table->reset_handler));
    reset_handler_ns();

    while (1) {}
}
