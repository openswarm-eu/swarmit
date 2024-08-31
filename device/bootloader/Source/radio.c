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

#include "clock.h"
#include "ipc.h"
#include "radio.h"
#include "tz.h"

//=========================== variables ========================================

static radio_cb_t _radio_callback = NULL;

//=========================== public ===========================================

void radio_init(radio_cb_t callback, radio_ble_mode_t mode) {

    hfclk_init();

    // Disable all DCDC regulators (use LDO)
    NRF_REGULATORS_S->VREGRADIO.DCDCEN = (REGULATORS_VREGRADIO_DCDCEN_DCDCEN_Disabled << REGULATORS_VREGRADIO_DCDCEN_DCDCEN_Pos);
    NRF_REGULATORS_S->VREGMAIN.DCDCEN  = (REGULATORS_VREGMAIN_DCDCEN_DCDCEN_Disabled << REGULATORS_VREGMAIN_DCDCEN_DCDCEN_Pos);
    NRF_REGULATORS_S->VREGH.DCDCEN     = (REGULATORS_VREGH_DCDCEN_DCDCEN_Disabled << REGULATORS_VREGH_DCDCEN_DCDCEN_Pos);

    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_MUTEX);

    // Map P0.29 to network core
    NRF_P0_S->PIN_CNF[29] = GPIO_PIN_CNF_MCUSEL_NetworkMCU << GPIO_PIN_CNF_MCUSEL_Pos;

    // Map P1.0-9 to network core
    for (uint8_t pin = 0; pin < 10; pin++) {
        NRF_P1_S->PIN_CNF[pin] = GPIO_PIN_CNF_MCUSEL_NetworkMCU << GPIO_PIN_CNF_MCUSEL_Pos;
    }

    tz_configure_ram_non_secure(3, 1);

    NRF_IPC_S->INTENSET                       = 1 << IPC_CHAN_RADIO_RX;
    NRF_IPC_S->SEND_CNF[IPC_CHAN_REQ]         = 1 << IPC_CHAN_REQ;
    NRF_IPC_S->RECEIVE_CNF[IPC_CHAN_RADIO_RX] = 1 << IPC_CHAN_RADIO_RX;
    NRF_IPC_S->RECEIVE_CNF[IPC_CHAN_STOP]     = 1 << IPC_CHAN_STOP;

    NVIC_EnableIRQ(IPC_IRQn);
    NVIC_ClearPendingIRQ(IPC_IRQn);
    NVIC_SetPriority(IPC_IRQn, IPC_IRQ_PRIORITY);

    // Start the network core
    release_network_core();

    if (callback) {
        _radio_callback = callback;
    }

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

//=========================== interrupt handlers ===============================

void IPC_IRQHandler(void) {

    if (NRF_IPC_S->EVENTS_RECEIVE[IPC_CHAN_RADIO_RX]) {
        NRF_IPC_S->EVENTS_RECEIVE[IPC_CHAN_RADIO_RX] = 0;
        if (_radio_callback) {
            mutex_lock();
            _radio_callback((uint8_t *)ipc_shared_data.radio.rx_pdu.buffer, ipc_shared_data.radio.rx_pdu.length);
            mutex_unlock();
        }
    }
}
