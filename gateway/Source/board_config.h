#ifndef __BOARD_CONFIG_H
#define __BOARD_CONFIG_H

/**
 * @defgroup    bsp_board_config    Board configuration
 * @ingroup     bsp_board
 * @brief       Board specific definitions
 *
 * @{
 * @file
 * @author Alexandre Abadie <alexandre.abadie@inria.fr>
 * @copyright Inria, 2023
 * @}
 */

#include "gpio.h"

/**
 * @name    Debug pins definitions
 * @{
 */
#define DEBUG1_PORT 0
#define DEBUG1_PIN  13
#define DEBUG2_PORT 0
#define DEBUG2_PIN  14
#define DEBUG3_PORT 0
#define DEBUG3_PIN  15
/** @} */

/**
 * @name    LEDs pins definitions
 * @{
 */
#define LED1_PORT DEBUG1_PORT
#define LED1_PIN  DEBUG1_PIN
#define LED2_PORT DEBUG2_PORT
#define LED2_PIN  DEBUG2_PIN
#define LED3_PORT DEBUG3_PORT
#define LED3_PIN  DEBUG3_PIN
/** @} */

/**
 * @name    UART pins definitions
 * @{
 */
#define UART_RX_PORT 0
#define UART_RX_PIN  8
#define UART_TX_PORT 0
#define UART_TX_PIN  6
/** @} */

///! LED1 pin
static const gpio_t db_led1 = { .port = LED1_PORT, .pin = LED1_PIN };

///! LED2 pin
static const gpio_t db_led2 = { .port = LED2_PORT, .pin = LED2_PIN };

///! LED3 pin
static const gpio_t db_led3 = { .port = LED3_PORT, .pin = LED3_PIN };

///! UART RX pin
static const gpio_t db_uart_rx = { .port = UART_RX_PORT, .pin = UART_RX_PIN };

///! UART TX pin
static const gpio_t db_uart_tx = { .port = UART_TX_PORT, .pin = UART_TX_PIN };

#endif
