#ifndef __MIRA_H
#define __MIRA_H

/**
 * @defgroup    drv_tdma_client      TDMA client radio driver
 * @ingroup     drv
 * @brief       Driver for Time-Division-Multiple-Access fot the DotBot radio
 *
 * @{
 * @file
 * @author Said Alvarado-Marin <said-alexander.alvarado-marin@inria.fr>
 * @copyright Inria, 2024-now
 * @}
 */

#include <stdint.h>
#include <nrf.h>

//=========================== prototypes =======================================

/**
 * @brief Initializes mira
 */
void mira_init(void);

/**
 * @brief Queues a single node packet to send through mira
 *
 * @param[in] packet pointer to the array of data to send over the radio
 * @param[in] length Number of bytes to send
 *
 */
void mira_node_tx(const uint8_t *packet, uint8_t length);

#endif
