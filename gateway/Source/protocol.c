/**
 * @file
 * @ingroup drv_protocol
 *
 * @brief  nRF52833-specific definition of the "protocol" driver module.
 *
 * @author Alexandre Abadie <alexandre.abadie@inria.fr>
 *
 * @copyright Inria, 2022
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "device.h"
#include "protocol.h"

//=========================== public ===========================================

static size_t _protocol_header_to_buffer(uint8_t *buffer, uint64_t dst, packet_type_t packet_type) {
    uint64_t src = db_device_id();

    protocol_header_t header = {
        .version     = FIRMWARE_VERSION,
        .packet_type = packet_type,
        .dst         = dst,
        .src         = src,
    };
    memcpy(buffer, &header, sizeof(protocol_header_t));
    return sizeof(protocol_header_t);
}

size_t protocol_header_to_buffer(uint8_t *buffer, uint64_t dst) {
    return _protocol_header_to_buffer(buffer, dst, PACKET_DATA);
}

size_t protocol_tdma_keep_alive_to_buffer(uint8_t *buffer, uint64_t dst) {
    return _protocol_header_to_buffer(buffer, dst, PACKET_TDMA_KEEP_ALIVE);
}

size_t protocol_tdma_table_update_to_buffer(uint8_t *buffer, uint64_t dst, protocol_tdma_table_t *tdma_table) {
    size_t header_length = _protocol_header_to_buffer(buffer, dst, PACKET_TDMA_UPDATE_TABLE);
    memcpy(buffer + sizeof(protocol_header_t), tdma_table, sizeof(protocol_tdma_table_t));
    return header_length + sizeof(protocol_tdma_table_t);
}

size_t protocol_tdma_sync_frame_to_buffer(uint8_t *buffer, uint64_t dst, protocol_sync_frame_t *sync_frame) {
    size_t header_length = _protocol_header_to_buffer(buffer, dst, PACKET_TDMA_SYNC_FRAME);
    memcpy(buffer + sizeof(protocol_header_t), sync_frame, sizeof(protocol_sync_frame_t));
    return header_length + sizeof(protocol_sync_frame_t);
}
