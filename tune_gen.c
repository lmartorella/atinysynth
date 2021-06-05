#include "tune_gen.h"

// Tune: resources/scale.mml

const uint16_t tune_adsr_time_scale_refs[] = {
	0x12, 0x13, 0x4b, 0x4c, 
};

const uint16_t tune_wf_period_refs[] = {
	0x0, 0x4a, 0x4f, 0x53, 0x58, 0x5e, 0x63, 0x69, 0x6f, 0x76, 0x7d, 0x85, 0x8d, 0x95, 
};

const uint8_t tune_wf_amplitude_refs[] = {
	0x0, 0x78, 
};

const uint8_t tune_adsr_release_start_refs[] = {
	0x27, 0x37, 0x3f, 
};

const uint8_t tune_data[TUNE_DATA_SIZE] = {
	0xf4, 0xe0, 0xb1, 0x43, 0x47, 0xe, 0x5c, 0x37, 0x6c, 0xd4, 0xa0, 0x31, 0x43, 0x46, 0x4c, 0x10, 
	0x1d, 0x38, 0x6c, 0xd2, 0x90, 0x1, 0xc3, 0x5, 0xb, 0x15, 0x28, 0x4c, 0x90, 0x10, 0x19, 0x40, 
	0x17, 0x2e, 0x5b, 0xb4, 0x64, 0xc1, 0x72, 0xc5, 0x4a, 0x15, 0x2a, 0x53, 0xa4, 0x44, 0x7, 0x2, 
	0x0, 
};

