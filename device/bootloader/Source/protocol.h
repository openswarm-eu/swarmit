#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>

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

#endif  // __PROTOCOL_H
