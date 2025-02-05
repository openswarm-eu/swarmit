/**
 * @file
 * @ingroup drv_tdma_client
 *
 * @brief  nrf5340-app-specific definition of the "tdma_client" drv module.
 *
 * @author Said Alvarado-Marin <said-alexander.alvarado-marin@inria.fr>
 *
 * @copyright Inria, 2024
 */
#include <nrf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "ipc.h"
#include "radio.h"
#include "tz.h"
#include "tdma_client.h"

//=========================== variables ========================================

extern volatile __attribute__((section(".shared_data"))) ipc_shared_data_t ipc_shared_data;

//=========================== public ===========================================

void tdma_client_init(radio_mode_t radio_mode, uint8_t radio_freq) {

    // APPMUTEX (address at 0x41030000 => periph ID is 48)
    tz_configure_periph_non_secure(NRF_APPLICATION_PERIPH_ID_MUTEX);

    // Store information in the shared data before sending it to the net-core
    ipc_shared_data.tdma_client.mode              = radio_mode;
    ipc_shared_data.tdma_client.frequency         = radio_freq;

    // Initialize TDMA client drv in the net-core
    ipc_network_call(IPC_TDMA_CLIENT_INIT_REQ);
}

void tdma_client_set_table(const tdma_client_table_t *table) {

    // Copy the set table to the IPC shared data
    ipc_shared_data.tdma_client.table_set.frame_duration = table->frame_duration;
    ipc_shared_data.tdma_client.table_set.rx_start       = table->rx_start;
    ipc_shared_data.tdma_client.table_set.rx_duration    = table->rx_duration;
    ipc_shared_data.tdma_client.table_set.tx_start       = table->tx_start;
    ipc_shared_data.tdma_client.table_set.tx_duration    = table->tx_duration;
    // Request the network core to copy the data.
    ipc_network_call(IPC_TDMA_CLIENT_SET_TABLE_REQ);
}

void tdma_client_get_table(tdma_client_table_t *table) {

    // Request the network core to copy the table's data.
    ipc_network_call(IPC_TDMA_CLIENT_GET_TABLE_REQ);
    // Copy the get table from the IPC shared data
    table->frame_duration = ipc_shared_data.tdma_client.table_get.frame_duration;
    table->rx_start       = ipc_shared_data.tdma_client.table_get.rx_start;
    table->rx_duration    = ipc_shared_data.tdma_client.table_get.rx_duration;
    table->tx_start       = ipc_shared_data.tdma_client.table_get.tx_start;
    table->tx_duration    = ipc_shared_data.tdma_client.table_get.tx_duration;
}

void tdma_client_tx(const uint8_t *packet, uint8_t length) {
    ipc_shared_data.tdma_client.tx_pdu.length = length;
    memcpy((void *)ipc_shared_data.tdma_client.tx_pdu.buffer, packet, length);
    ipc_network_call(IPC_TDMA_CLIENT_TX_REQ);
}

void tdma_client_flush(void) {
    ipc_network_call(IPC_TDMA_CLIENT_FLUSH_REQ);
}

void tdma_client_empty(void) {
    ipc_network_call(IPC_TDMA_CLIENT_EMPTY_REQ);
}

tdma_registration_state_t tdma_client_get_status(void) {
    ipc_network_call(IPC_TDMA_CLIENT_STATUS_REQ);
    return ipc_shared_data.tdma_client.registration_state;
}
