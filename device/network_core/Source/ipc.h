#ifndef __IPC_H
#define __IPC_H

/**
 * @defgroup    bsp_ipc Inter-Processor Communication
 * @ingroup     bsp
 * @brief       Control the IPC peripheral (nRF53 only)
 *
 * @{
 * @file
 * @author Alexandre Abadie <alexandre.abadie@inria.fr>
 * @copyright Inria, 2023
 * @}
 */

#include <nrf.h>
#include <stdbool.h>
#include <stdint.h>
#include "radio.h"

#define IPC_IRQ_PRIORITY (1)

#define IPC_LOG_SIZE     (128)

typedef enum {
    IPC_REQ_NONE,        ///< Sorry, but nothing
    IPC_RADIO_INIT_REQ,  ///< Request for radio initialization
    IPC_RADIO_FREQ_REQ,  ///< Request for radio set frequency
    IPC_RADIO_CHAN_REQ,  ///< Request for radio set channel
    IPC_RADIO_ADDR_REQ,  ///< Request for radio set network address
    IPC_RADIO_RX_REQ,    ///< Request for radio rx
    IPC_RADIO_DIS_REQ,   ///< Request for radio disable
    IPC_RADIO_TX_REQ,    ///< Request for radio tx
    IPC_RADIO_RSSI_REQ,  ///< Request for RSSI
} ipc_req_t;

typedef enum {
    IPC_CHAN_REQ      = 0,  ///< Channel used for request events
    IPC_CHAN_RADIO_RX = 1,  ///< Channel used for radio RX events
    IPC_CHAN_STOP     = 2,  ///< Channel used for stopping the experiment
    IPC_CHAN_LOG      = 3,  ///< Channel used for logging events
} ipc_channels_t;

typedef struct __attribute__((packed)) {
    uint8_t length;             ///< Length of the pdu in bytes
    uint8_t buffer[UINT8_MAX];  ///< Buffer containing the pdu data
} ipc_radio_pdu_t;

typedef struct __attribute__((packed)) {
    radio_ble_mode_t    mode;       ///< db_radio_init function parameters
    uint8_t             frequency;  ///< db_set_frequency function parameters
    uint8_t             channel;    ///< db_set_channel function parameters
    uint32_t            addr;       ///< db_set_network_address function parameters
    ipc_radio_pdu_t     tx_pdu;     ///< PDU to send
    ipc_radio_pdu_t     rx_pdu;     ///< Received pdu
    int8_t              rssi;       ///< RSSI value
} ipc_radio_data_t;

typedef struct __attribute__((packed)) {
    uint8_t length;
    uint8_t data[127];
} ipc_log_data_t;

typedef struct __attribute__((packed)) {
    bool             net_ready;  ///< Network core is ready
    bool             net_ack;    ///< Network core acked the latest request
    ipc_req_t        req;        ///< IPC network request
    ipc_log_data_t   log;        ///< Log data
    ipc_radio_data_t radio;      ///< Radio shared data
} ipc_shared_data_t;

/**
 * @brief Variable in RAM containing the shared data structure
 */
volatile __attribute__((used, section(".shared_data"))) ipc_shared_data_t ipc_shared_data;

/**
 * @brief Lock the mutex, blocks until the mutex is locked
 */
static inline void mutex_lock(void) {
    while (NRF_APPMUTEX_NS->MUTEX[0]) {}
}

/**
 * @brief Unlock the mutex, has no effect if the mutex is already unlocked
 */
static inline void mutex_unlock(void) {
    NRF_APPMUTEX_NS->MUTEX[0] = 0;
}

#endif
