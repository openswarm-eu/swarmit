/**
 * @file
 * @defgroup project_nrf5340_net_core   nRF5340 network core
 * @ingroup projects
 * @brief This application is used to control the radio and rng peripherals and to interact with the application core
 *
 * @author Alexandre Abadie <alexandre.abadie@inria.fr>
 * @copyright Inria, 2023
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <nrf.h>
// Include BSP headers
#include "ipc.h"
#include "radio.h"

//=========================== variables =========================================

static bool      _data_received = false;
static ipc_req_t _req_received  = IPC_REQ_NONE;

//=========================== functions =========================================

static void _radio_callback(uint8_t *packet, uint8_t length) {
    NRF_P0_NS->OUT ^= (1 << 29);
    if (memcmp(packet, "STOP", length) == 0) {
        NRF_IPC_NS->TASKS_SEND[IPC_CHAN_STOP] = 1;
    } else if (memcmp(packet, "START", length) == 0) {
        mutex_lock();
        ipc_shared_data.radio.rx_pdu.length = length;
        memcpy((void *)ipc_shared_data.radio.rx_pdu.buffer, packet, length);
        mutex_unlock();
        _data_received = true;
    }
}

//=========================== main ==============================================

int main(void) {

    // Configure constant latency mode for better performances
    NRF_POWER_NS->TASKS_CONSTLAT = 1;

    NRF_IPC_NS->INTENSET                    = 1 << IPC_CHAN_REQ;
    NRF_IPC_NS->SEND_CNF[IPC_CHAN_RADIO_RX] = 1 << IPC_CHAN_RADIO_RX;
    NRF_IPC_NS->SEND_CNF[IPC_CHAN_STOP]     = 1 << IPC_CHAN_STOP;
    NRF_IPC_NS->RECEIVE_CNF[IPC_CHAN_REQ]   = 1 << IPC_CHAN_REQ;

    NVIC_EnableIRQ(IPC_IRQn);
    NVIC_ClearPendingIRQ(IPC_IRQn);
    NVIC_SetPriority(IPC_IRQn, 1);

    NRF_P0_NS->DIRSET = (1 << 29);
    ipc_shared_data.net_ready = true;

    while (1) {
        __WFE();
        if (_data_received) {
            _data_received                            = false;
            NRF_IPC_NS->TASKS_SEND[IPC_CHAN_RADIO_RX] = 1;
        }
        if (_req_received != IPC_REQ_NONE) {
            ipc_shared_data.net_ack = false;
            switch (_req_received) {
                case IPC_RADIO_INIT_REQ:
                    radio_init(&_radio_callback, ipc_shared_data.radio.mode);
                    break;
                case IPC_RADIO_FREQ_REQ:
                    radio_set_frequency(ipc_shared_data.radio.frequency);
                    break;
                case IPC_RADIO_CHAN_REQ:
                    radio_set_channel(ipc_shared_data.radio.channel);
                    break;
                case IPC_RADIO_ADDR_REQ:
                    radio_set_network_address(ipc_shared_data.radio.addr);
                    break;
                case IPC_RADIO_RX_REQ:
                    radio_rx();
                    break;
                case IPC_RADIO_DIS_REQ:
                    radio_disable();
                    break;
                case IPC_RADIO_TX_REQ:
                    radio_tx((uint8_t *)ipc_shared_data.radio.tx_pdu.buffer, ipc_shared_data.radio.tx_pdu.length);
                    break;
                case IPC_RADIO_RSSI_REQ:
                    ipc_shared_data.radio.rssi = radio_rssi();
                    break;
                default:
                    break;
            }
            ipc_shared_data.net_ack = true;
            _req_received           = IPC_REQ_NONE;
        }
    };
}

void IPC_IRQHandler(void) {
    if (NRF_IPC_NS->EVENTS_RECEIVE[IPC_CHAN_REQ]) {
        NRF_IPC_NS->EVENTS_RECEIVE[IPC_CHAN_REQ] = 0;
        _req_received                            = ipc_shared_data.req;
    }
}
