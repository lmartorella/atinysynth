#include "sequencer.h"

// Tune: resources/tetris.mml

struct tune_frame_t {
	uint8_t adsr_time_scale : 5;
	uint8_t wf_period : 5;
	uint8_t wf_amplitude : 1;
	uint8_t adsr_release_start : 1;
};

extern const uint16_t tune_adsr_time_scale_refs[];
extern const uint16_t tune_wf_period_refs[];
extern const uint8_t tune_wf_amplitude_refs[];
extern const uint8_t tune_adsr_release_start_refs[];
extern const struct tune_frame_t tune_data[];

#define TUNE_DATA_COUNT 703
#define NO_CLIP_CHECK
#define SEQ_CHANNEL_COUNT 3

