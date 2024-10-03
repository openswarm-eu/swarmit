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
#define DB_DEBUG1_PORT 0
#define DB_DEBUG1_PIN  13
#define DB_DEBUG2_PORT 0
#define DB_DEBUG2_PIN  14
#define DB_DEBUG3_PORT 0
#define DB_DEBUG3_PIN  15
/** @} */

/**
 * @name    LEDs pins definitions
 * @{
 */
#define DB_LED1_PORT DB_DEBUG1_PORT
#define DB_LED1_PIN  DB_DEBUG1_PIN
#define DB_LED2_PORT DB_DEBUG2_PORT
#define DB_LED2_PIN  DB_DEBUG2_PIN
#define DB_LED3_PORT DB_DEBUG3_PORT
#define DB_LED3_PIN  DB_DEBUG3_PIN
/** @} */

/**
 * @name    UART pins definitions
 * @{
 */
#define DB_UART_RX_PORT 0
#define DB_UART_RX_PIN  8
#define DB_UART_TX_PORT 0
#define DB_UART_TX_PIN  6
/** @} */

///! LED1 pin
static const gpio_t db_led1 = { .port = DB_LED1_PORT, .pin = DB_LED1_PIN };

///! LED2 pin
static const gpio_t db_led2 = { .port = DB_LED2_PORT, .pin = DB_LED2_PIN };

///! LED3 pin
static const gpio_t db_led3 = { .port = DB_LED3_PORT, .pin = DB_LED3_PIN };

///! UART RX pin
static const gpio_t db_uart_rx = { .port = DB_UART_RX_PORT, .pin = DB_UART_RX_PIN };

///! UART TX pin
static const gpio_t db_uart_tx = { .port = DB_UART_TX_PORT, .pin = DB_UART_TX_PIN };

#endif
