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

#include "waveform.h"
#include "synth.h"
#include <stdlib.h>

/*!
 * Number of fractional bits for `period` and `period_remain`.
 * This allows tuned notes even in lower sampling frequencies.
 * The integer part (12 bits) is wide enough to render a 20Hz 
 * note on the higher 48kHz sampling frequency.
 */
#define PERIOD_FP_SCALE 	(4)

int8_t voice_wf_next() {
	if (cur_voice->wf.period > 0) {
		if ((cur_voice->wf.period_remain >> PERIOD_FP_SCALE) == 0) {
			/* Swap value */
			cur_voice->wf.int_sample = -cur_voice->wf.int_sample;
			cur_voice->wf.period_remain += cur_voice->wf.period;
		}
		cur_voice->wf.period_remain -= (1 << PERIOD_FP_SCALE);
	}
	return cur_voice->wf.int_sample;
}

/* Compute frequency period (full wave) */
uint16_t voice_wf_freq_to_period(uint16_t freq) {
	/* Use 16-bit 12.4 fixed point */  
	return (uint16_t)(((uint32_t)synth_freq << PERIOD_FP_SCALE) / freq);
}

void voice_wf_set(struct seq_frame_t* const frame) {
	cur_voice->wf.int_sample = cur_voice->wf.int_amplitude = frame->wf_amplitude;
	cur_voice->wf.period_remain = cur_voice->wf.period = frame->wf_period;
}

int8_t voice_wf_setup_def(struct seq_frame_t* frame, uint16_t frequency, int8_t amplitude) {
	uint16_t period = frequency > 0 ? (voice_wf_freq_to_period(frequency) >> 1): 0;
	frame->wf_amplitude = amplitude;
	frame->wf_period = period;
	return 1;
}
