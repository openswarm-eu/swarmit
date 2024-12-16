#ifndef __CLOCK_H
#define __CLOCK_H

/**
 * @defgroup    bsp_clock   Clock
 * @ingroup     bsp
 * @brief       Functions to initialize low and high frequency clocks
 *
 * @{
 * @file
 * @author Alexandre Abadie <alexandre.abadie@inria.fr>
 * @copyright Inria, 2022
 * @}
 */

/**
 * @brief Initialize and start the High Frequency clock
 */
void hfclk_init(void);

/**
 * @brief Initialize and start the Low Frequency clock
 */
void lfclk_init(void);

#endif
