#ifndef __PROTOCOL_H
#define __PROTOCOL_H

/**
 * @defgroup    drv_protocol    DotBot protocol implementation
 * @ingroup     drv
 * @brief       Definitions and implementations of the DotBot protocol
 *
 * @{
 * @file
 * @author Alexandre Abadie <alexandre.abadie@inria.fr>
 * @copyright Inria, 2022
 * @}
 */

#include <stdlib.h>
#include <stdint.h>

//=========================== defines ==========================================

#define FIRMWARE_VERSION  (9)                   ///< Version of the firmware
#define BROADCAST_ADDRESS 0xffffffffffffffffUL  ///< Broadcast address
#define GATEWAY_ADDRESS   0x0000000000000000UL  ///< Gateway address

/// Protocol packet type
typedef enum {
    PACKET_BEACON            = 1,  ///< Beacon packet
    PACKET_JOIN_REQUEST      = 2,  ///< Join request packet
    PACKET_JOIN_RESPONSE     = 3,  ///< Join response packet
    PACKET_LEAVE             = 4,  ///< Leave packet
    PACKET_DATA              = 5,  ///< Data packet
    PACKET_TDMA_UPDATE_TABLE = 6,  ///< TDMA table update packet
    PACKET_TDMA_SYNC_FRAME   = 7,  ///< TDMA sync frame packet
    PACKET_TDMA_KEEP_ALIVE   = 8,  ///< TDMA keep alive packet
} packet_type_t;

/// DotBot protocol header
typedef struct __attribute__((packed)) {
    uint8_t       version;      ///< Version of the firmware
    packet_type_t packet_type;  ///< Type of packet
    uint64_t      dst;          ///< Destination address of this packet
    uint64_t      src;          ///< Source address of this packet
} protocol_header_t;

///< DotBot protocol TDMA table update [all units are in microseconds]
typedef struct __attribute__((packed)) {
    uint32_t frame_period;       ///< duration of a full TDMA frame
    uint32_t rx_start;           ///< start to listen for packets
    uint16_t rx_duration;        ///< duration of the RX period
    uint32_t tx_start;           ///< start of slot for transmission
    uint16_t tx_duration;        ///< duration of the TX period
    uint32_t next_period_start;  ///< time until the start of the next TDMA frame
} protocol_tdma_table_t;

///< DotBot protocol sync messages marks the start of a TDMA frame [all units are in microseconds]
typedef struct __attribute__((packed)) {
    uint32_t frame_period;  ///< duration of a full TDMA frame
} protocol_sync_frame_t;

//=========================== public ===========================================

/**
 * @brief   Write the protocol header in a buffer
 *
 * @param[out]  buffer      Bytes array to write to
 * @param[in]   dst         Destination address written in the header
 *
 * @return                  Number of bytes written in the buffer
 */
size_t protocol_header_to_buffer(uint8_t *buffer, uint64_t dst);

/**
 * @brief   Write a TDMA keep alive packet in a buffer
 *
 * @param[out]  buffer      Bytes array to write to
 * @param[in]   dst         Destination address written in the header
 *
 * @return                  Number of bytes written in the buffer
 */
size_t protocol_tdma_keep_alive_to_buffer(uint8_t *buffer, uint64_t dst);

/**
 * @brief   Write a TDMA table update in a buffer
 *
 * @param[out]  buffer      Bytes array to write to
 * @param[in]   dst         Destination address written in the header
 * @param[in]   tdma_table  Pointer to the TDMA table
 *
 * @return                  Number of bytes written in the buffer
 */
size_t protocol_tdma_table_update_to_buffer(uint8_t *buffer, uint64_t dst, protocol_tdma_table_t *tdma_table);

/**
 * @brief   Write a TDMA sync frame in a buffer
 *
 * @param[out]  buffer      Bytes array to write to
 * @param[in]   dst         Destination address written in the header
 * @param[in]   sync_frame  Pointer to the sync frame
 *
 * @return                  Number of bytes written in the buffer
 */
size_t protocol_tdma_sync_frame_to_buffer(uint8_t *buffer, uint64_t dst, protocol_sync_frame_t *sync_frame);

#endif
