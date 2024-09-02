#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>

#define SWRMT_PREAMBLE_LENGTH       (8U)
#define SWRMT_OTA_CHUNK_SIZE        (128U)
#define SWRMT_OTA_SHA256_LENGTH     (32U)

static const uint8_t swrmt_preamble[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
};

#define SWRMT_REQ_START         0x01        ///< Start the experiment
#define SWRMT_REQ_STOP          0x02        ///< Stop the experiment
#define SWRMT_REQ_OTA_START     0x03        ///< Start OTA
#define SWRMT_REQ_OTA_CHUNK     0x04        ///< OTA chunk

typedef struct __attribute__((packed)) {
    uint8_t         type;                       ///< Request type
    uint8_t         data[255 - SWRMT_PREAMBLE_LENGTH -1];                  ///< Data associated with the request
} swrmt_request_t;

typedef struct __attribute__((packed)) {
    uint8_t         preamble[SWRMT_PREAMBLE_LENGTH];    ///< Preamble bytes
    swrmt_request_t request;                            ///< Request
} swrmt_packet_t;

typedef struct __attribute__((packed)) { 
    uint32_t image_size;                        ///< User image size in bytes
    uint8_t hash[SWRMT_OTA_SHA256_LENGTH];      ///< SHA256 hash of the firmware
} swrmt_ota_start_pkt_t;

typedef struct __attribute__((packed)) { 
    uint32_t index;                             ///< Index of the chunk
    uint8_t  chunk[SWRMT_OTA_CHUNK_SIZE];       ///< Bytes array of the firmware chunk
    uint8_t  chunk_size;                        ///< Size of the chunk
} swrmt_ota_chunk_pkt_t;

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

typedef struct {
    uint8_t port;  ///< Port number of the GPIO
    uint8_t pin;   ///< Pin number of the GPIO
    uint8_t value;
} gpio_data_t;

typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    gpio_data_t data;
} swrmt_gpio_event_t;

#endif  // __PROTOCOL_H
