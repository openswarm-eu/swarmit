/**
 * @file
 * @author Alexandre Abadie <alexandre.abadie@inria.fr>
 * @brief Startup code and vectors definition for nRF53 network core
 *
 * @copyright Inria, 2024
 *
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <nrf.h>
#include "fault_handlers.h"

extern __NO_RETURN int main(void);
extern void system_init(void);
__attribute__ ((weak, alias("Dummy_Handler"))) void exit(int status);

// Import symbols defined by the SEGGER linker
extern void __SEGGER_RTL_init_heap(void *ptr, size_t len);

extern uint32_t __data_load_start__;
extern uint32_t __data_start__;
extern uint32_t __data_end__;
extern uint32_t __text_load_start__;
extern uint32_t __text_start__;
extern uint32_t __text_end__;
extern uint32_t __fast_load_start__;
extern uint32_t __fast_start__;
extern uint32_t __fast_end__;
extern uint32_t __ctors_load_start__;
extern uint32_t __ctors_start__;
extern uint32_t __ctors_end__;
extern uint32_t __dtors_load_start__;
extern uint32_t __dtors_start__;
extern uint32_t __dtors_end__;
extern uint32_t __rodata_load_start__;
extern uint32_t __rodata_start__;
extern uint32_t __rodata_end__;
extern uint32_t __tdata_load_start__;
extern uint32_t __tdata_start__;
extern uint32_t __tdata_end__;

extern uint32_t __bss_start__;
extern uint32_t __bss_end__;
extern uint32_t __tbss_start__;
extern uint32_t __tbss_end__;

extern uint32_t __heap_start__;
extern uint32_t __heap_end__;

extern uint32_t __stack_start__;
extern uint32_t __stack_end__;
extern uint32_t __stack_process_start__;
extern uint32_t __stack_process_end__;
extern uint32_t __HEAPSIZE__;
extern uint32_t __STACKSIZE__;
extern uint32_t __STACKSIZE_PROCESS__;

__NO_RETURN extern void Reset_Handler(void);
__NO_RETURN void Dummy_Handler(void);

// Exceptions handlers
__attribute__ ((weak, alias("Dummy_Handler"))) void NMI_Handler(void);
__attribute__ ((weak, alias("Dummy_Handler"))) void MemManage_Handler(void);
__attribute__ ((weak, alias("Dummy_Handler"))) void BusFault_Handler(void);
__attribute__ ((weak, alias("Dummy_Handler"))) void UsageFault_Handler(void);
__attribute__ ((weak, alias("Dummy_Handler"))) void SVC_Handler(void);
__attribute__ ((weak, alias("Dummy_Handler"))) void DebugMon_Handler(void);
__attribute__ ((weak, alias("Dummy_Handler"))) void PendSV_Handler(void);
__attribute__ ((weak, alias("Dummy_Handler"))) void SysTick_Handler(void);

void HardFault_Handler(void);

// External interrupts handlers
 __attribute__ ((weak, alias("Dummy_Handler"))) void CLOCK_POWER_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void RADIO_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void RNG_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void GPIOTE_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void WDT_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void TIMER0_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void ECB_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void AAR_CCM_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void TEMP_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void RTC0_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void IPC_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void SERIAL0_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void EGU0_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void RTC1_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void TIMER1_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void TIMER2_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void SWI0_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void SWI1_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void SWI2_IRQHandler(void);
 __attribute__ ((weak, alias("Dummy_Handler"))) void SWI3_IRQHandler(void);

// Vector table
typedef void(*vector_table_t)(void);
extern const vector_table_t _vectors[496];
const vector_table_t _vectors[496] __attribute__((used, section(".vectors"))) = {
    (vector_table_t)&__stack_end__,     //     Initial Stack Pointer
    Reset_Handler,                      //     Reset Handler
    NMI_Handler,                        // -14 NMI Handler
    HardFault_Handler,                  // -13 Hard Fault HandleR
    MemManage_Handler,                  // -12 MPU Fault Handler
    BusFault_Handler,                   // -11 Bus Fault Handler
    UsageFault_Handler,                 // -10 Usage Fault Handler
    0,                                  //     Reserved
    0,                                  //     Reserved
    0,                                  //     Reserved
    0,                                  //     Reserved
    SVC_Handler,                        //  -5 SVCall Handler
    DebugMon_Handler,                   //  -4 Debug Monitor Handler
    0,                                  //     Reserved
    PendSV_Handler,                     //  -2 PendSV Handler
    SysTick_Handler,                    //  -1 SysTick Handler

    // External Interrupts
    0,
    0,
    0,
    0,
    0,
    CLOCK_POWER_IRQHandler,
    0,
    0,
    RADIO_IRQHandler,
    RNG_IRQHandler,
    GPIOTE_IRQHandler,
    WDT_IRQHandler,
    TIMER0_IRQHandler,
    ECB_IRQHandler,
    AAR_CCM_IRQHandler,
    0,
    TEMP_IRQHandler,
    RTC0_IRQHandler,
    IPC_IRQHandler,
    SERIAL0_IRQHandler,
    EGU0_IRQHandler,
    0,
    RTC1_IRQHandler,
    0,
    TIMER1_IRQHandler,
    TIMER2_IRQHandler,
    SWI0_IRQHandler,
    SWI1_IRQHandler,
    SWI2_IRQHandler,
    SWI3_IRQHandler,
};

// Entry point
void Reset_Handler(void) {
    SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  /* set CP10 and CP11 Full Access */

    uint32_t *src = &__data_load_start__;
    uint32_t *dst = &__data_start__;
    while(dst < &__data_end__) {
        *dst++ = *src++;
    }
#if defined(DEBUG)
    src = &__text_load_start__;
    dst = &__text_start__;
    while(dst < &__text_end__) {
        if (dst == src) {
            break;
        }
        *dst++ = *src++;
    }
#endif
    src = &__fast_load_start__;
    dst = &__fast_start__;
    while(dst < &__fast_end__) {
        *dst++ = *src++;
    }
    src = &__ctors_load_start__;
    dst = &__ctors_start__;
    while(dst < &__ctors_end__) {
        *dst++ = *src++;
    }
    src = &__dtors_load_start__;
    dst = &__dtors_start__;
    while(dst < &__dtors_end__) {
        *dst++ = *src++;
    }
#if defined(DEBUG)
    src = &__rodata_load_start__;
    dst = &__rodata_start__;
    while(dst < &__rodata_end__) {
        if (dst == src) {
            break;
        }
        *dst++ = *src++;
    }
#endif
    src = &__tdata_load_start__;
    dst = &__tdata_start__;
    while(dst < &__tdata_end__) {
        if (dst == src) {
            *dst++ = *src++;
        }
    }

    // Zeroing bss data
    src = &__bss_start__;
    while(src < &__bss_end__) {
        *src++ = 0;
    }
    src = &__tbss_start__;
    while(src < &__tbss_end__) {
        *src++ = 0;
    }

    // Calling constructors
    typedef void (*ctor_func_t)(void);
    ctor_func_t func = (ctor_func_t)&__ctors_start__;
    while(&func < (ctor_func_t *)&__ctors_end__) {
        func++();
    }

    system_init();
    main();
}

// Exception handlers
void HardFault_Handler(void) {
    __ASM(
         "tst    LR, #4             ;"  // Check EXC_RETURN in Link register bit 2.
         "ite    EQ                 ;"
         "mrseq  R0, MSP            ;"  // Stacking was using MSP.
         "mrsne  R0, PSP            ;"  // Stacking was using PSP.
         "b      HardFaultHandler   ;"  // Stack pointer passed through R0.
    );
}

void Dummy_Handler(void) {
   while(1) {
       __NOP();
   }
}
