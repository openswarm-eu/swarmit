#ifndef __CMSE_IMPLIB_H
#define __CMSE_IMPLIB_H

/**
 * @defgroup    bsp_cmse_implib  CMSE secure gateway functions
 * @ingroup     bsp
 * @brief       Secure gateway functions for Non-Secure Callable functions
 *
 * @{
 * @file
 * @author Alexandre Abadie <alexandre.abadie@inria.fr>
 * @copyright Inria, 2024
 * @}
 */

#include <stdint.h>
#include <stdlib.h>

typedef void (*ipc_isr_cb_t)(const uint8_t *, size_t) __attribute__((cmse_nonsecure_call));

__attribute__((cmse_nonsecure_entry)) void swarmit_reload_wdt0(void);
__attribute__((cmse_nonsecure_entry)) void swarmit_send_packet(const uint8_t *packet, uint8_t length);
__attribute__((cmse_nonsecure_entry)) void swarmit_send_raw_data(const uint8_t *packet, uint8_t length);
__attribute__((cmse_nonsecure_entry)) void swarmit_ipc_isr(ipc_isr_cb_t cb);
__attribute__((cmse_nonsecure_entry)) void swarmit_init_rng(void);
__attribute__((cmse_nonsecure_entry)) void swarmit_read_rng(uint8_t *value);
__attribute__((cmse_nonsecure_entry)) uint64_t swarmit_read_device_id(void);
__attribute__((cmse_nonsecure_entry)) void swarmit_log_data(uint8_t *data, size_t length);

#endif // __CMSE_IMPLIB_H
