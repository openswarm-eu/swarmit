#include <nrf.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "board_config.h"
#include "clock.h"
#include "gpio.h"
#include "hdlc.h"
#include "timer.h"
#include "uart.h"
#include "tdma_server.h"

//=========================== defines ==========================================
#define DOTBOT_GW_RADIO_MODE RADIO_BLE_2MBit
#define TIMER_DEV           (1)
#define BUFFER_MAX_BYTES (255U)       ///< Max bytes in UART receive buffer
#define UART_BAUDRATE    (1000000UL)  ///< UART baudrate used by the gateway
#define UART_INDEX       (0)  ///< Index of UART peripheral to use
#define RADIO_QUEUE_SIZE (8U)                             ///< Size of the radio queue (must by a power of 2)
#define RADIO_FREQ       (8U)                             //< Set the frequency to 2408 MHz
#define UART_QUEUE_SIZE  ((BUFFER_MAX_BYTES + 1) * 2)  ///< Size of the UART queue size (must by a power of 2)
#define RADIO_APP           (DotBot)                         // DotBot Radio App

typedef struct {
    uint8_t length;                       ///< Length of the radio packet
    uint8_t buffer[BUFFER_MAX_BYTES];  ///< Buffer containing the radio packet
} gateway_radio_packet_t;

typedef struct {
    uint8_t                current;                       ///< Current position in the queue
    uint8_t                last;                          ///< Position of the last item added in the queue
    gateway_radio_packet_t packets[RADIO_QUEUE_SIZE];  ///< Buffer containing the received bytes
} gateway_radio_packet_queue_t;

typedef struct {
    uint16_t current;                     ///< Current position in the queue
    uint16_t last;                        ///< Position of the last item added in the queue
    uint8_t  buffer[UART_QUEUE_SIZE];  ///< Buffer containing the received bytes
} gateway_uart_queue_t;

typedef struct {
    uint8_t                      hdlc_rx_buffer[BUFFER_MAX_BYTES * 2];  ///< Buffer where message received on UART is stored
    uint8_t                      hdlc_tx_buffer[BUFFER_MAX_BYTES * 2];  ///< Internal buffer used for sending serial HDLC frames
    uint32_t                     buttons;                                  ///< Buttons state (one byte per button)
    uint8_t                      radio_tx_buffer[BUFFER_MAX_BYTES];     ///< Internal buffer that contains the command to send (from buttons)
    gateway_radio_packet_queue_t radio_queue;                              ///< Queue used to process received radio packets outside of interrupt
    gateway_uart_queue_t         uart_queue;                               ///< Queue used to process received UART bytes outside of interrupt
    bool                         led1_blink;                               ///< Whether the status LED should blink
} gateway_vars_t;

//=========================== variables ========================================

static gateway_vars_t _gw_vars;

//=========================== callbacks ========================================

static void _uart_callback(uint8_t data) {
    _gw_vars.uart_queue.buffer[_gw_vars.uart_queue.last] = data;
    _gw_vars.uart_queue.last                             = (_gw_vars.uart_queue.last + 1) & (UART_QUEUE_SIZE - 1);
}

static void _radio_callback(uint8_t *packet, uint8_t length) {
    memcpy(_gw_vars.radio_queue.packets[_gw_vars.radio_queue.last].buffer, packet, length);
    _gw_vars.radio_queue.packets[_gw_vars.radio_queue.last].length = length;
    _gw_vars.radio_queue.last                                      = (_gw_vars.radio_queue.last + 1) & (RADIO_QUEUE_SIZE - 1);
}

static void _led1_blink_fast(void) {
    if (_gw_vars.led1_blink) {
        db_gpio_toggle(&db_led1);
    }
}

static void _led2_shutdown(void) {
    db_gpio_set(&db_led2);
}

static void _led3_shutdown(void) {
    db_gpio_set(&db_led3);
}

//=========================== main =============================================

int main(void) {
    hfclk_init();
    _gw_vars.led1_blink = true;
    // Initialize user feedback LEDs
    db_gpio_init(&db_led1, GPIO_OUT);  // Global status
    db_gpio_set(&db_led1);
    timer_init(TIMER_DEV);
    timer_set_periodic_ms(TIMER_DEV, 0, 50, _led1_blink_fast);
    timer_set_periodic_ms(TIMER_DEV, 1, 20, _led2_shutdown);
    timer_set_periodic_ms(TIMER_DEV, 2, 20, _led3_shutdown);
    db_gpio_init(&db_led2, GPIO_OUT);  // Packet received from Radio (e.g from a DotBot)
    db_gpio_set(&db_led2);
    db_gpio_init(&db_led3, GPIO_OUT);  // Packet received from UART (e.g from the computer)
    db_gpio_set(&db_led3);

    // Configure Radio as transmitter
    tdma_server_init(&_radio_callback, DOTBOT_GW_RADIO_MODE, RADIO_FREQ, RADIO_APP);

    // Initialize the gateway context
    _gw_vars.buttons             = 0x0000;
    _gw_vars.radio_queue.current = 0;
    _gw_vars.radio_queue.last    = 0;
    db_uart_init(UART_INDEX, &db_uart_rx, &db_uart_tx, UART_BAUDRATE, &_uart_callback);

    // Initialization done, wait a bit and shutdown status LED
    timer_delay_s(TIMER_DEV, 1);
    db_gpio_set(&db_led1);
    _gw_vars.led1_blink = false;

    while (1) {

        while (_gw_vars.radio_queue.current != _gw_vars.radio_queue.last) {
            db_gpio_clear(&db_led2);
            size_t frame_len = db_hdlc_encode(_gw_vars.radio_queue.packets[_gw_vars.radio_queue.current].buffer, _gw_vars.radio_queue.packets[_gw_vars.radio_queue.current].length, _gw_vars.hdlc_tx_buffer);
            db_uart_write(UART_INDEX, _gw_vars.hdlc_tx_buffer, frame_len);
            _gw_vars.radio_queue.current = (_gw_vars.radio_queue.current + 1) & (RADIO_QUEUE_SIZE - 1);
        }

        while (_gw_vars.uart_queue.current != _gw_vars.uart_queue.last) {
            db_gpio_clear(&db_led3);
            db_hdlc_state_t hdlc_state = db_hdlc_rx_byte(_gw_vars.uart_queue.buffer[_gw_vars.uart_queue.current]);
            switch ((uint8_t)hdlc_state) {
                case HDLC_STATE_IDLE:
                case HDLC_STATE_RECEIVING:
                case HDLC_STATE_ERROR:
                    break;
                case HDLC_STATE_READY:
                {
                    size_t msg_len = db_hdlc_decode(&_gw_vars.hdlc_rx_buffer[0]);
                    if (msg_len) {
                        tdma_server_tx(&_gw_vars.hdlc_rx_buffer[0], msg_len);
                    }
                } break;
                default:
                    break;
            }
            _gw_vars.uart_queue.current = (_gw_vars.uart_queue.current + 1) & (UART_QUEUE_SIZE - 1);
        }
    }
}
