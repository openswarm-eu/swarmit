#include "battery.h"

#include "saadc.h"

void battery_level_init(void) {
    db_saadc_init(DB_SAADC_RESOLUTION_12BIT);
}

uint8_t battery_level_read(void) {
    uint16_t value_12b = 0;
    db_saadc_read(ROBOT_BATTERY_LEVEL_PIN, &value_12b);
    uint32_t max_val = 4095 * 3000 / 3600;
    return (uint8_t)((float)value_12b / max_val * 100);
}
