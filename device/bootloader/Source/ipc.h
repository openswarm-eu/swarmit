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
#include <stdlib.h>
#include <string.h>
#include "radio.h"

#define IPC_IRQ_PRIORITY (1)

__attribute__((cmse_nonsecure_entry)) void log_data(uint8_t *data, size_t length);

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
volatile __attribute__((section(".shared_data"))) ipc_shared_data_t ipc_shared_data;

/**
 * @brief Lock the mutex, blocks until the mutex is locked
 */
static inline void mutex_lock(void) {
    while (NRF_MUTEX_NS->MUTEX[0]) {}
}

/**
 * @brief Unlock the mutex, has no effect if the mutex is already unlocked
 */
static inline void mutex_unlock(void) {
    NRF_MUTEX_NS->MUTEX[0] = 0;
}

static inline void ipc_network_call(ipc_req_t req) {
    if (req != IPC_REQ_NONE) {
        ipc_shared_data.req                 = req;
        NRF_IPC_S->TASKS_SEND[IPC_CHAN_REQ] = 1;
    }
    while (!ipc_shared_data.net_ack) {}
    ipc_shared_data.net_ack = false;
};

static inline void release_network_core(void) {
    // Do nothing if network core is already started and ready
    if (!NRF_RESET_S->NETWORK.FORCEOFF && ipc_shared_data.net_ready) {
        return;
    } else if (!NRF_RESET_S->NETWORK.FORCEOFF) {
        ipc_shared_data.net_ready = false;
    }

    NRF_RESET_S->NETWORK.FORCEOFF = (RESET_NETWORK_FORCEOFF_FORCEOFF_Release << RESET_NETWORK_FORCEOFF_FORCEOFF_Pos);

    while (!ipc_shared_data.net_ready) {}
}

__attribute__((cmse_nonsecure_entry)) void log_data(uint8_t *data, size_t length) {
    ipc_shared_data.log.length = length;
    memcpy((void *)ipc_shared_data.log.data, data, length);
    NRF_IPC_S->TASKS_SEND[IPC_CHAN_LOG] = 1;
}

#endif
