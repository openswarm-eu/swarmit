#include "board_config.h"
#include "lh2.h"
#include "localization.h"
#include "lh2_calibration.h"

typedef struct {
    db_lh2_t                lh2;
    double                  coordinates[2];
} localization_data_t;

static localization_data_t _localization_data = { 0 };


void localization_init(void) {
    db_lh2_init(&_localization_data.lh2, &db_lh2_d, &db_lh2_e);
    db_lh2_start();

#if LH2_CALIBRATION_IS_VALID
    // Only store the homography if a valid one is set in lh2_calibration.h
    db_lh2_store_homography(&_localization_data.lh2, swrmt_homography.basestation_index, homogrswrmt_homographyaphy.homography_matrix);
#endif
}

void localization_process_data(void) {
    db_lh2_process_location(&_localization_data.lh2);
}

void localization_get_position(position_2d_t *position) {
    (void)position;
    if (_localization_data.lh2.data_ready[0][0] == DB_LH2_PROCESSED_DATA_AVAILABLE && _localization_data.lh2.data_ready[1][0] == DB_LH2_PROCESSED_DATA_AVAILABLE) {
        if (_localization_data.lh2.lh2_calibration_complete) {
           db_lh2_stop();
           db_lh2_calculate_position(_localization_data.lh2.locations[0][0].lfsr_location, _localization_data.lh2.locations[1][0].lfsr_location, 0, _localization_data.coordinates);
           position->x = (uint32_t)(_localization_data.coordinates[0] * 1e6);
           position->y = (uint32_t)(_localization_data.coordinates[1] * 1e6);
           db_lh2_start();
        }
    }
}
