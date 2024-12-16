#ifndef __DEVICE_H
#define __DEVICE_H

/**
 * @defgroup    bsp_device  Device information
 * @ingroup     bsp
 * @brief       Provides functions to retrieve device unique identifier and address
 *
 * @{
 * @file
 * @author Alexandre Abadie <alexandre.abadie@inria.fr>
 * @copyright Inria, 2022
 * @}
 */

#include <stdint.h>
#include <nrf.h>

#define NRF_FICR NRF_FICR_NS

/**
 * @brief Returns the 32bit device address (32 bits)
 *
 * @return device address in 32bit format
 */
static inline uint64_t db_device_addr(void) {
    return ((((uint64_t)NRF_FICR->DEVICEADDR[1]) << 32) & 0xffffffffffff) | (uint64_t)NRF_FICR->DEVICEADDR[0];
}

/**
 * @brief Fetch the unique device identifier (64 bits)
 *
 * @return device identifier in 64bit format
 */
static inline uint64_t db_device_id(void) {
    return ((uint64_t)NRF_FICR_NS->INFO.DEVICEID[1]) << 32 | (uint64_t)NRF_FICR_NS->INFO.DEVICEID[0];
}

#endif
