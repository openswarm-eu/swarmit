/**
 * @file
 * @author Alexandre Abadie <alexandre.abadie@inria.fr>
 * @brief Device bootloader application
 *
 * @copyright Inria, 2024
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arm_cmse.h>
#include <nrf.h>

#include "radio.h"
#include "tz.h"

__attribute__((cmse_nonsecure_entry)) void reload_wdt0(void);

__attribute__((cmse_nonsecure_entry)) void reload_wdt0(void) {
    NRF_WDT0_S->RR[0] = WDT_RR_RR_Reload << WDT_RR_RR_Pos;
}

typedef void (*reset_handler_t)(void) __attribute__((cmse_nonsecure_call));

typedef struct {
    uint32_t msp;                  ///< Main stack pointer
    reset_handler_t reset_handler; ///< Reset handler
} vector_table_t;

static vector_table_t *table = (vector_table_t *)0x00004000; // Image should start with vector table

static void setup_watchdog0(void) {

    // Configuration: keep running while sleeping + pause when halted by debugger
    NRF_WDT0_S->CONFIG = (WDT_CONFIG_SLEEP_Run << WDT_CONFIG_SLEEP_Pos |
                         WDT_CONFIG_HALT_Pause << WDT_CONFIG_HALT_Pos);

    // Enable reload register 0
    NRF_WDT0_S->RREN = WDT_RREN_RR0_Enabled << WDT_RREN_RR0_Pos;

    // Configure timeout and callback
    NRF_WDT0_S->CRV = 32768 - 1;
    NRF_WDT0_S->TASKS_START = WDT_TASKS_START_TASKS_START_Trigger << WDT_TASKS_START_TASKS_START_Pos;
}

static void setup_watchdog1(void) {

    // Configuration: keep running while sleeping + pause when halted by debugger
    NRF_WDT1_S->CONFIG = (WDT_CONFIG_SLEEP_Run << WDT_CONFIG_SLEEP_Pos);

    // Enable reload register 0
    NRF_WDT1_S->RREN = WDT_RREN_RR0_Enabled << WDT_RREN_RR0_Pos;

    // Configure timeout and callback
    NRF_WDT1_S->CRV = 32768 - 1;
}

static void setup_ns_user(void) {

    // Prioritize Secure exceptions over Non-Secure
    // Set non-banked exceptions to target Non-Secure
    uint32_t aircr = SCB->AIRCR & (~(SCB_AIRCR_VECTKEY_Msk));
    aircr |= SCB_AIRCR_PRIS_Msk | SCB_AIRCR_BFHFNMINS_Msk;
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
    tz_configure_ram_secure(0, 3);
    // Configure non secure RAM
    tz_configure_ram_non_secure(4, 48);

    // First flash region (16kiB) is secure and contains the bootloader
    tz_configure_flash_secure(0, 1);
    // Configure non secure flash address space
    tz_configure_flash_non_secure(1, 63);

    // Configure Non Secure Callable subregion
    NRF_SPU_S->FLASHNSC[0].REGION = 0;
    NRF_SPU_S->FLASHNSC[0].SIZE = 8;

    // Configure access to allows peripherals from non secure world
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_I2S0);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_MUTEX);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_OSCILLATORS_REGULATORS);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_P0_P1);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_PDM0);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_EGU0);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_EGU1);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_EGU2);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_EGU3);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_EGU4);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_EGU5);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_PWM0);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_PWM1);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_PWM2);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_PWM3);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_QDEC0);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_QDEC1);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_QSPI);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_RTC0);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_RTC1);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_SAADC);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_SPIM0_SPIS0_TWIM0_TWIS0_UARTE0);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_SPIM1_SPIS1_TWIM1_TWIS1_UARTE1);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_SPIM2_SPIS2_TWIM2_TWIS2_UARTE2);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_SPIM3_SPIS3_TWIM3_TWIS3_UARTE3);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_SPIM4);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_TIMER0);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_TIMER1);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_TIMER2);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_USBD);
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_USBREGULATOR);

    // Set interrupt state as non secure for non secure peripherals
    NVIC_SetTargetState(I2S0_IRQn);
    NVIC_SetTargetState(PDM0_IRQn);
    NVIC_SetTargetState(EGU0_IRQn);
    NVIC_SetTargetState(EGU1_IRQn);
    NVIC_SetTargetState(EGU2_IRQn);
    NVIC_SetTargetState(EGU3_IRQn);
    NVIC_SetTargetState(EGU4_IRQn);
    NVIC_SetTargetState(EGU5_IRQn);
    NVIC_SetTargetState(PWM0_IRQn);
    NVIC_SetTargetState(PWM1_IRQn);
    NVIC_SetTargetState(PWM2_IRQn);
    NVIC_SetTargetState(PWM3_IRQn);
    NVIC_SetTargetState(QDEC0_IRQn);
    NVIC_SetTargetState(QDEC1_IRQn);
    NVIC_SetTargetState(QSPI_IRQn);
    NVIC_SetTargetState(RTC0_IRQn);
    NVIC_SetTargetState(RTC1_IRQn);
    NVIC_SetTargetState(SAADC_IRQn);
    NVIC_SetTargetState(SPIM0_SPIS0_TWIM0_TWIS0_UARTE0_IRQn);
    NVIC_SetTargetState(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn);
    NVIC_SetTargetState(SPIM2_SPIS2_TWIM2_TWIS2_UARTE2_IRQn);
    NVIC_SetTargetState(SPIM3_SPIS3_TWIM3_TWIS3_UARTE3_IRQn);
    NVIC_SetTargetState(SPIM4_IRQn);
    NVIC_SetTargetState(TIMER0_IRQn);
    NVIC_SetTargetState(TIMER1_IRQn);
    NVIC_SetTargetState(TIMER2_IRQn);
    NVIC_SetTargetState(USBD_IRQn);
    NVIC_SetTargetState(USBREGULATOR_IRQn);
    NVIC_SetTargetState(GPIOTE1_IRQn);

    // All GPIOs are non secure
    NRF_SPU_S->GPIOPORT[0].PERM = 0;
    NRF_SPU_S->GPIOPORT[1].PERM = 0;

    __DSB(); // Force memory writes before continuing
    __ISB(); // Flush and refill pipeline with updated permissions
}

static const uint8_t swrmt_preamble[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
};

static void _radio_callback(uint8_t *packet, uint8_t length) {
    (void)length;
    if (memcmp((void *)packet, swrmt_preamble, sizeof(swrmt_preamble)/sizeof(uint8_t)) == 0) {
        // System reset will switch to the user non secure partition
        NVIC_SystemReset();
    }
}

int main(void) {

    setup_watchdog1();

    // PPI connection: IPC_RECEIVE -> WDT_START
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_DPPIC);
    NRF_SPU_S->DPPI[0].PERM &= ~(SPU_DPPI_PERM_CHANNEL0_Msk);
    NRF_SPU_S->DPPI[0].LOCK |= SPU_DPPI_LOCK_LOCK_Locked << SPU_DPPI_LOCK_LOCK_Pos;
    NRF_IPC_S->PUBLISH_RECEIVE[2] = IPC_PUBLISH_RECEIVE_EN_Enabled << IPC_PUBLISH_RECEIVE_EN_Pos;
    NRF_WDT1_S->SUBSCRIBE_START = WDT_SUBSCRIBE_START_EN_Enabled << WDT_SUBSCRIBE_START_EN_Pos;;
    NRF_DPPIC_NS->CHENSET = (DPPIC_CHENSET_CH0_Enabled << DPPIC_CHENSET_CH0_Pos);
    NRF_DPPIC_S->CHENSET = (DPPIC_CHENSET_CH0_Enabled << DPPIC_CHENSET_CH0_Pos);

    // Network core must remain on
    radio_init(&_radio_callback, RADIO_BLE_1MBit);
    radio_set_frequency(8);
    radio_rx();

    // Check reset reason and switch to user image if reset was not triggered by any wdt timeout
    uint32_t resetreas = NRF_RESET_S->RESETREAS;
    NRF_RESET_S->RESETREAS = NRF_RESET_S->RESETREAS;
    if (!(
        (resetreas & RESET_RESETREAS_DOG0_Detected << RESET_RESETREAS_DOG0_Pos) ||
        (resetreas & RESET_RESETREAS_DOG1_Detected << RESET_RESETREAS_DOG1_Pos)
    )) {
        // Initialize watchdog and non secure access
        setup_ns_user();
        setup_watchdog0();

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

    // Management code
    NRF_P0_S->DIRSET = (1 << 31);
    while (1) {
        __WFE();
    }
}
