#include "sequencer.h"

// Tune: resources/scale.mml

#define BITS_ADSR_TIME_SCALE 2
#define BITS_WF_PERIOD 4
#define BITS_WF_AMPLITUDE 1
#define BITS_ADSR_RELEASE_START 2

#define TUNE_DATA_SIZE 49
#define NO_CLIP_CHECK
#define SEQ_CHANNEL_COUNT 1

extern const uint16_t tune_adsr_time_scale_refs[];
extern const uint16_t tune_wf_period_refs[];
extern const uint8_t tune_wf_amplitude_refs[];
extern const uint8_t tune_adsr_release_start_refs[];
extern const uint8_t tune_data[TUNE_DATA_SIZE];

