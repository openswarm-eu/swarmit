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
#include "protocol.h"
#include "radio.h"

#define SWRMT_USER_IMAGE_BASE_ADDRESS       (0x00004000)
#define GPIO_CHANNELS_COUNT                 (5U)

//=========================== variables =========================================

typedef struct {
    bool        req_received;
    uint8_t     req_buffer[255];
    uint8_t     notification_buffer[255];
    ipc_req_t   ipc_req;
    bool        ipc_log_received;
    uint8_t     gpio_event_idx;
    uint8_t     hash[SWRMT_OTA_SHA256_LENGTH];
    uint64_t    device_id;
    bool        timer_running;
} swrmt_app_data_t;

static swrmt_app_data_t _app_vars = { 0 };

//static const uint8_t _pin_mapping[] = {
//    [0] = 4,    // P0.04 (app) => P1.00 (net)
//    [1] = 5,    // P0.05 (app) => P1.01 (net)
//    [2] = 6,    // P0.06 (app) => P1.02 (net)
//    [3] = 7,    // P0.07 (app) => P1.03 (net)
//    [4] = 25,   // P0.25 (app) => P1.04 (net)
//};

//=========================== functions =========================================

static void _radio_callback(uint8_t *packet, uint8_t length) {

    if (memcmp(packet, swrmt_preamble, SWRMT_PREAMBLE_LENGTH) != 0) {
        // Ignore non swarmit packets
        return;
    }

    uint8_t *ptr = packet + SWRMT_PREAMBLE_LENGTH;

    uint64_t target_device_id;
    memcpy(&target_device_id, ptr, sizeof(uint64_t));
    if (target_device_id != _app_vars.device_id && target_device_id != 0) {
        // Ignore packet not targetting this device
        return;
    }

    ptr += sizeof(uint64_t);
    memcpy(_app_vars.req_buffer, ptr, length - SWRMT_PREAMBLE_LENGTH - sizeof(uint64_t));
    _app_vars.req_received = true;
}

uint32_t _timestamp(void) {
    NRF_TIMER0_NS->TASKS_CAPTURE[0] = 1;
    return NRF_TIMER0_NS->CC[0];
}

uint64_t _deviceid(void) {
    return ((uint64_t)NRF_FICR_NS->INFO.DEVICEID[1]) << 32 | (uint64_t)NRF_FICR_NS->INFO.DEVICEID[0];
}

static void delay_ms(uint32_t ms) {
    NRF_TIMER0_NS->TASKS_CAPTURE[0] = 1;
    NRF_TIMER0_NS->CC[0] += ms * 1000;
    _app_vars.timer_running = true;
    while (_app_vars.timer_running) {
        __WFE();
    }
}

//void _gpio_init(uint8_t pin) {
//    NRF_P1_NS->PIN_CNF[pin] = 0;
//    NRF_P1_NS->PIN_CNF[pin] &= ~(1UL << GPIO_PIN_CNF_INPUT_Pos);
//    NRF_P1_NS->PIN_CNF[pin] |= (GPIO_PIN_CNF_PULL_Pulldown << GPIO_PIN_CNF_PULL_Pos);

//    NRF_GPIOTE_NS->CONFIG[pin] = (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos) |
//                                    (pin << GPIOTE_CONFIG_PSEL_Pos) |
//                                    (1 << GPIOTE_CONFIG_PORT_Pos) |
//                                    (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos);
//    NRF_GPIOTE_NS->INTENSET |= (1 << pin);
//}

//uint8_t _gpio_read(uint8_t pin) {
//    return (NRF_P1_NS->IN & (1 << pin)) ? 1 : 0;
//}

//=========================== main ==============================================

int main(void) {

    _app_vars.device_id = _deviceid();
    // Configure constant latency mode for better performances
    NRF_POWER_NS->TASKS_CONSTLAT = 1;

    NRF_IPC_NS->INTENSET                            = (1 << IPC_CHAN_REQ) | (1 << IPC_CHAN_LOG_EVENT);
    NRF_IPC_NS->SEND_CNF[IPC_CHAN_EXPERIMENT_START] = 1 << IPC_CHAN_EXPERIMENT_START;
    NRF_IPC_NS->SEND_CNF[IPC_CHAN_EXPERIMENT_STOP]  = 1 << IPC_CHAN_EXPERIMENT_STOP;
    NRF_IPC_NS->SEND_CNF[IPC_CHAN_OTA_START]        = 1 << IPC_CHAN_OTA_START;
    NRF_IPC_NS->SEND_CNF[IPC_CHAN_OTA_CHUNK]        = 1 << IPC_CHAN_OTA_CHUNK;
    NRF_IPC_NS->RECEIVE_CNF[IPC_CHAN_REQ]           = 1 << IPC_CHAN_REQ;
    NRF_IPC_NS->RECEIVE_CNF[IPC_CHAN_LOG_EVENT]     = 1 << IPC_CHAN_LOG_EVENT;

    NVIC_EnableIRQ(IPC_IRQn);
    NVIC_ClearPendingIRQ(IPC_IRQn);
    NVIC_SetPriority(IPC_IRQn, 1);

    //NRF_P1_NS->DIRSET = (1 << 4);
    //NRF_P1_NS->OUTSET = (1 << 4);

    // Configure timer used for timestamping events
    NRF_TIMER0_NS->TASKS_CLEAR = 1;
    NRF_TIMER0_NS->PRESCALER   = 4;
    NRF_TIMER0_NS->BITMODE     = (TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos);
    NRF_TIMER0_NS->INTENSET    = (1 << TIMER_INTENSET_COMPARE0_Pos);
    NVIC_EnableIRQ(TIMER0_IRQn);
    NRF_TIMER0_NS->TASKS_START = 1;

    // Initialize monitoring gpios
    //NVIC_EnableIRQ(GPIOTE_IRQn);
    //for (uint8_t pin = 0; pin < 5; pin++) {
    //    _gpio_init(pin);
    //}

    ipc_shared_data.net_ready = true;

    while (1) {
        __WFE();

        if (_app_vars.req_received) {
            _app_vars.req_received = false;
            swrmt_request_t *req = (swrmt_request_t *)_app_vars.req_buffer;
            switch (req->type) {
                case SWRMT_REQ_EXPERIMENT_START:
                    NRF_IPC_NS->TASKS_SEND[IPC_CHAN_EXPERIMENT_START] = 1;
                    break;
                case SWRMT_REQ_EXPERIMENT_STOP:
                    NRF_IPC_NS->TASKS_SEND[IPC_CHAN_EXPERIMENT_STOP] = 1;
                    break;
                case SWRMT_REQ_OTA_START:
                {
                    const swrmt_ota_start_pkt_t *pkt = (const swrmt_ota_start_pkt_t *)req->data;
                    // Copy expected hash
                    memcpy(_app_vars.hash, pkt->hash, SWRMT_OTA_SHA256_LENGTH);

                    // Erase the corresponding flash pages.
                    mutex_lock();
                    ipc_shared_data.ota.image_size = pkt->image_size;
                    mutex_unlock();
                    NRF_IPC_NS->TASKS_SEND[IPC_CHAN_OTA_START] = 1;

                    NRF_P0_NS->OUT ^= (1 << 29);
                } break;
                case SWRMT_REQ_OTA_CHUNK:
                {
                    const swrmt_ota_chunk_pkt_t *pkt = (const swrmt_ota_chunk_pkt_t *)req->data;
                    mutex_lock();
                    ipc_shared_data.ota.chunk_index = pkt->index;
                    ipc_shared_data.ota.chunk_size = pkt->chunk_size;
                    memcpy((uint8_t *)ipc_shared_data.ota.chunk, pkt->chunk, pkt->chunk_size);
                    mutex_unlock();
                    NRF_IPC_NS->TASKS_SEND[IPC_CHAN_OTA_CHUNK] = 1;
                    NRF_P0_NS->OUT ^= (1 << 29);
                } break;
                default:
                    break;
            }
        }

        if (_app_vars.ipc_req != IPC_REQ_NONE) {
            ipc_shared_data.net_ack = false;
            switch (_app_vars.ipc_req) {
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
                {
                    radio_tx((uint8_t *)ipc_shared_data.radio.tx_pdu.buffer, ipc_shared_data.radio.tx_pdu.length);
                    delay_ms(10);
                 }   break;
                case IPC_RADIO_RSSI_REQ:
                    ipc_shared_data.radio.rssi = radio_rssi();
                    break;
                default:
                    break;
            }
            ipc_shared_data.net_ack = true;
            _app_vars.ipc_req      = IPC_REQ_NONE;
        }

        if (_app_vars.ipc_log_received) {
            _app_vars.ipc_log_received = false;
            // Notify log data
            swrmt_notification_t notification = {
                .device_id = _deviceid(),
                .type = SWRMT_NOTIFICATION_LOG_EVENT,
            };
            memcpy(_app_vars.notification_buffer, &notification, sizeof(swrmt_notification_t));
            uint32_t timestamp = _timestamp();
            memcpy(_app_vars.notification_buffer + sizeof(swrmt_notification_t), &timestamp, sizeof(uint32_t));
            memcpy(_app_vars.notification_buffer + sizeof(swrmt_notification_t) + sizeof(uint32_t), (void *)&ipc_shared_data.log, sizeof(ipc_log_data_t));
            radio_disable();
            radio_tx(_app_vars.notification_buffer, sizeof(swrmt_notification_t) + sizeof(uint32_t) + sizeof(ipc_log_data_t));
        }

        //if (_app_vars.gpio_event_idx != 0xff) {
        //    _app_vars.gpio_event_idx = 0xff;
        //    // Notify gpio event
        //    swrmt_notification_t notification = {
        //        .device_id = _deviceid(),
        //        .type = SWRMT_NOTIFICATION_GPIO_EVENT,
        //    };

        //    swrmt_gpio_event_t event = {
        //        .timestamp = _timestamp(),
        //        .data = {
        //            .port = 0,
        //            .pin = _pin_mapping[_app_vars.gpio_event_idx],
        //            .value = _gpio_read(_app_vars.gpio_event_idx),
        //        }
        //    };
        //    _app_vars.gpio_event_idx = 0xff;
        //    memcpy(_app_vars.notification_buffer, &notification, sizeof(swrmt_notification_t));
        //    memcpy(_app_vars.notification_buffer + sizeof(swrmt_notification_t), &event, sizeof(swrmt_gpio_event_t));
        //    radio_disable();
        //    radio_tx(_app_vars.notification_buffer, sizeof(swrmt_notification_t) + sizeof(swrmt_gpio_event_t));
        //}
    };
}

void IPC_IRQHandler(void) {
    if (NRF_IPC_NS->EVENTS_RECEIVE[IPC_CHAN_REQ]) {
        NRF_IPC_NS->EVENTS_RECEIVE[IPC_CHAN_REQ] = 0;
        _app_vars.ipc_req                        = ipc_shared_data.req;
    }

    if (NRF_IPC_NS->EVENTS_RECEIVE[IPC_CHAN_LOG_EVENT]) {
        NRF_IPC_NS->EVENTS_RECEIVE[IPC_CHAN_LOG_EVENT] = 0;
        _app_vars.ipc_log_received                     = true;
    }
}

//void GPIOTE_IRQHandler(void) {
//    for (uint8_t i = 0; i < GPIO_CHANNELS_COUNT; ++i) {
//        if (NRF_GPIOTE_NS->EVENTS_IN[i] == 1) {
//            NRF_GPIOTE_NS->EVENTS_IN[i] = 0;
//            _app_vars.gpio_event_idx    = i;
//            break;
//        }
//    }
//}

void TIMER0_IRQHandler(void) {

    if (NRF_TIMER0_NS->EVENTS_COMPARE[0] == 1) {
        NRF_TIMER0_NS->EVENTS_COMPARE[0] = 0;
        _app_vars.timer_running = false;
    }
}
