#include "sequencer.h"

// Tune: resources/tetris.mml

struct tune_frame_t {
	uint8_t adsr_time_scale : 3;
	uint8_t wf_period : 5;
};

extern const uint16_t tune_adsr_time_scale_refs[];
extern const uint16_t tune_wf_period_refs[];
extern const uint8_t tune_wf_amplitude_refs[];
extern const uint8_t tune_adsr_release_start_refs[];
extern const struct tune_frame_t tune_data[];

#define TUNE_DATA_COUNT 658

