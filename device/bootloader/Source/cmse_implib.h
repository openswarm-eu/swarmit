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

#include "lh2.h"

typedef void (*ipc_isr_cb_t)(const uint8_t *, size_t) __attribute__((cmse_nonsecure_call));
typedef struct {
    int x;
    int y;
} swarmit_lh2_position_t;

__attribute__((cmse_nonsecure_entry, aligned)) void swarmit_keep_alive(void);
__attribute__((cmse_nonsecure_entry, aligned)) void swarmit_send_data_packet(const uint8_t *packet, uint8_t length);
__attribute__((cmse_nonsecure_entry, aligned)) void swarmit_send_raw_data(const uint8_t *packet, uint8_t length);
__attribute__((cmse_nonsecure_entry, aligned)) void swarmit_ipc_isr(ipc_isr_cb_t cb);
__attribute__((cmse_nonsecure_entry, aligned)) void swarmit_init_rng(void);
__attribute__((cmse_nonsecure_entry, aligned)) void swarmit_read_rng(uint8_t *value);
__attribute__((cmse_nonsecure_entry, aligned)) uint64_t swarmit_read_device_id(void);
__attribute__((cmse_nonsecure_entry, aligned)) void swarmit_log_data(uint8_t *data, size_t length);

// Lighthouse 2 functions
__attribute__((cmse_nonsecure_entry, aligned)) void swarmit_lh2_init(db_lh2_t *lh2);
__attribute__((cmse_nonsecure_entry, aligned)) void swarmit_lh2_start(void);
__attribute__((cmse_nonsecure_entry, aligned)) void swarmit_lh2_stop(void);
__attribute__((cmse_nonsecure_entry, aligned)) void swarmit_lh2_process_location(db_lh2_t *lh2);
__attribute__((cmse_nonsecure_entry, aligned)) void swarmit_lh2_position(swarmit_lh2_position_t *position);
__attribute__((cmse_nonsecure_entry, aligned)) void swarmit_lh2_spim_isr(void);

// SAADC functions
__attribute__((cmse_nonsecure_entry, aligned)) void swarmit_saadc_read(uint8_t channel, uint16_t *value);

#endif // __CMSE_IMPLIB_H
