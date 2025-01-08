/**
 * @file
 * @ingroup drv_protocol
 *
 * @brief  nRF52833-specific definition of the "protocol" driver module.
 *
 * @author Alexandre Abadie <alexandre.abadie@inria.fr>
 *
 * @copyright Inria, 2022
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "device.h"
#include "protocol.h"
#include "rng.h"

//=========================== public ===========================================

void protocol_header_to_buffer(uint8_t *buffer, uint64_t dst,
                                  application_type_t application, command_type_t command_type) {
    uint8_t  rnd[4] = { 0 };
    for (uint8_t i = 0; i < 4; i++) {
        rng_read(&rnd[i]);
    }

    uint32_t msg_id = 0;
    memcpy(&msg_id, rnd, sizeof(uint32_t));

    __attribute__((aligned(4))) protocol_header_t header = {
        .dst         = dst,
        .src         = db_device_id(),
        .swarm_id    = SWARM_ID,
        .application = application,
        .version     = FIRMWARE_VERSION,
        .msg_id      = msg_id,
        .type        = command_type,
    };
    memcpy(buffer, &header, sizeof(protocol_header_t));
}
