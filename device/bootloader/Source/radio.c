/**
 * @file
 * @ingroup bsp_radio
 *
 * @brief  nrf5340-app-specific definition of the "radio" bsp module.
 *
 * @author Said Alvarado-Marin <said-alexander.alvarado-marin@inria.fr>
 *
 * @copyright Inria, 2022
 */
#include <nrf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "ipc.h"
#include "radio.h"
#include "tz.h"

//=========================== variables ========================================

extern ipc_shared_data_t ipc_shared_data;

//=========================== public ===========================================

void radio_init(radio_ble_mode_t mode) {

    ipc_shared_data.radio.mode = mode;
    ipc_network_call(IPC_RADIO_INIT_REQ);
}

void radio_set_frequency(uint8_t freq) {
    ipc_shared_data.radio.frequency = freq;
    ipc_network_call(IPC_RADIO_FREQ_REQ);
}

void radio_set_channel(uint8_t channel) {
    ipc_shared_data.radio.channel = channel;
    ipc_network_call(IPC_RADIO_CHAN_REQ);
}

void radio_set_network_address(uint32_t addr) {
    ipc_shared_data.radio.addr = addr;
    ipc_network_call(IPC_RADIO_ADDR_REQ);
}

void radio_tx(const uint8_t *tx_buffer, uint8_t length) {
    ipc_shared_data.radio.tx_pdu.length = length;
    memcpy((void *)ipc_shared_data.radio.tx_pdu.buffer, tx_buffer, length);
    ipc_network_call(IPC_RADIO_TX_REQ);
}

void radio_rx(void) {
    ipc_network_call(IPC_RADIO_RX_REQ);
}

int8_t radio_rssi(void) {
    ipc_network_call(IPC_RADIO_RSSI_REQ);
    return ipc_shared_data.radio.rssi;
}

void radio_disable(void) {
    ipc_network_call(IPC_RADIO_DIS_REQ);
}
