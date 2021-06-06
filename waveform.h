/*!
 * Polyphonic synthesizer for microcontrollers.  Voice waveform generator.
 * (C) 2017 Stuart Longland
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */
#ifndef _WAVEFORM_H
#define _WAVEFORM_H
#include <stdint.h>

#include "sequencer.h"

/*!
 * Waveform generator state.  12 bytes.
 */
struct voice_wf_gen_t {
	union {
		// For square gen, 8-bit sample is enough
		struct {
			/*! Waveform output sample */
			int8_t int_sample;
			/*! Amplitude sample */
			int8_t int_amplitude;
		};
	};
	/*! Samples to next waveform period (12.4 fixed point) */
	uint16_t period_remain;
	/*!
	 * Period duration in samples (12.4 fixed point).
	 * (Half period for SQUARE and TRIANGLE)
	 */
	uint16_t period;
};

/**
 * The Waveform definition. 4 bytes.
 */ 
struct voice_wf_def_t {
	/*! Waveform amplitude (7-bit) */
	int8_t amplitude;
	/*! Waveform full period as `sample_freq / frequency`, or zero for pauses */
	uint16_t period;
};

/* Compute frequency period of a generic wave */
uint16_t voice_wf_freq_to_period(uint16_t frequency);

/*!
 * Configure the generator using waveform type and common parameters
 */
void voice_wf_set(struct seq_frame_t* const frame);

/*!
 * Retrieve the next sample from the generator.
 */
int8_t voice_wf_next();

/*! Setup def */
int8_t voice_wf_setup_def(struct seq_frame_t* frame, uint16_t frequency, int8_t amplitude);

#endif
