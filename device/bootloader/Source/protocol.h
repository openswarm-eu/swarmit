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

#include <stdint.h>

//=========================== defines ==========================================

#define FIRMWARE_VERSION  (9)                   ///< Version of the firmware
#define SWARM_ID          (0x0000)              ///< Default swarm ID
#define BROADCAST_ADDRESS 0xffffffffffffffffUL  ///< Broadcast address
#define GATEWAY_ADDRESS   0x0000000000000000UL  ///< Gateway address

#define SWRMT_PREAMBLE_LENGTH       (8U)
#define SWRMT_OTA_CHUNK_SIZE        (128U)

static const uint8_t swrmt_preamble[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
};

typedef struct __attribute__((packed)) {
    uint32_t index;                             ///< Index of the chunk
    uint8_t  chunk_size;                        ///< Size of the chunk
    uint8_t  chunk[SWRMT_OTA_CHUNK_SIZE];       ///< Bytes array of the firmware chunk
} swrmt_ota_chunk_pkt_t;

typedef enum {
    SWRMT_EXPERIMENT_READY,
    SWRMT_EXPERIMENT_RUNNING,
} swrmt_experiment_status_t;

typedef enum {
    SWRMT_NOTIFICATION_STATUS,
    SWRMT_NOTIFICATION_OTA_START_ACK,
    SWRMT_NOTIFICATION_OTA_CHUNK_ACK,
    SWRMT_NOTIFICATION_GPIO_EVENT,
    SWRMT_NOTIFICATION_LOG_EVENT,
} swrmt_notification_type_t;

typedef struct __attribute__((packed)) {
    uint64_t                    device_id;
    swrmt_notification_type_t   type;
} swrmt_notification_t;

/// Command type
typedef enum {
    PROTOCOL_CMD_MOVE_RAW       = 0,   ///< Move raw command type
    PROTOCOL_CMD_RGB_LED        = 1,   ///< RGB LED command type
    PROTOCOL_LH2_RAW_DATA       = 2,   ///< Lighthouse 2 raw data
    PROTOCOL_LH2_LOCATION       = 3,   ///< Lighthouse processed locations
    PROTOCOL_ADVERTISEMENT      = 4,   ///< DotBot advertisements
    PROTOCOL_GPS_LOCATION       = 5,   ///< GPS data from SailBot
    PROTOCOL_DOTBOT_DATA        = 6,   ///< DotBot specific data (for now location and direction)
    PROTOCOL_CONTROL_MODE       = 7,   ///< Robot remote control mode (automatic or manual)
    PROTOCOL_LH2_WAYPOINTS      = 8,   ///< List of LH2 waypoints to follow
    PROTOCOL_GPS_WAYPOINTS      = 9,   ///< List of GPS waypoints to follow
    PROTOCOL_SAILBOT_DATA       = 10,  ///< SailBot specific data (for now GPS and direction)
    PROTOCOL_CMD_XGO_ACTION     = 11,  ///< XGO action command
    PROTOCOL_LH2_PROCESSED_DATA = 12,  ///< Lighthouse 2 data processed at the DotBot
    PROTOCOL_TDMA_UPDATE_TABLE  = 13,  ///< Receive new timings for the TDMA table
    PROTOCOL_TDMA_SYNC_FRAME    = 14,  ///< Sent by the gateway at the beginning of a TDMA frame.
    PROTOCOL_TDMA_KEEP_ALIVE    = 15,  ///< Sent by the client if there is nothing else to send.
    PROTOCOL_SWARMIT_PACKET     = 16,  ///< Swarmit packet type
} command_type_t;

/// Application type
typedef enum {
    DotBot        = 0,  ///< DotBot application
    SailBot       = 1,  ///< SailBot application
    FreeBot       = 2,  ///< FreeBot application
    XGO           = 3,  ///< XGO application
    LH2_mini_mote = 4,  ///< LH2 mini mote application
} application_type_t;

/// DotBot protocol header
typedef struct __attribute__((packed)) {
    uint64_t           dst;          ///< Destination address of this packet
    uint64_t           src;          ///< Source address of this packet
    uint16_t           swarm_id;     ///< Swarm ID
    application_type_t application;  ///< Application type
    uint8_t            version;      ///< Version of the firmware
    uint32_t           msg_id;       ///< Message ID
    command_type_t     type;         ///< Type of command following this header
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

/**
 * @brief   Write the protocol header in a buffer
 *
 * @param[out]  buffer          Bytes array to write to
 * @param[in]   dst             Destination address written in the header
 * @param[in]   application     Application type that relates to this header
 * @param[in]   command_type    Command type that follows this header
 */
void protocol_header_to_buffer(uint8_t *buffer, uint64_t dst, application_type_t application, command_type_t command_type);

#endif
