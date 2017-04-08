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

/* Waveform generation modes */
#define VOICE_MODE_DC		(0)
#define	VOICE_MODE_SQUARE	(1)
#define VOICE_MODE_SAWTOOTH	(2)
#define VOICE_MODE_TRIANGLE	(3)
#define VOICE_MODE_NOISE	(4)

/* Amplitude scaling */
#define VOICE_WF_AMP_SCALE	(8)

int8_t voice_wf_next(struct voice_wf_gen_t* const wf_gen) {
	switch(wf_gen->mode) {
		case VOICE_MODE_DC:
			return wf_gen->amplitude;
		case VOICE_MODE_NOISE:
			wf_gen->sample = (rand() /
				(RAND_MAX/512)) - 256;
			wf_gen->sample *= wf_gen->amplitude;
			break;
		case VOICE_MODE_SQUARE:
			if (!wf_gen->period_remain) {
				/* Swap value */
				wf_gen->sample = -wf_gen->sample;
				wf_gen->period_remain = wf_gen->period;
			} else {
				wf_gen->period_remain--;
			}
			break;
		case VOICE_MODE_SAWTOOTH:
			if (!wf_gen->period_remain) {
				/* Back to -amplitude */
				wf_gen->sample = -wf_gen->amplitude
					<< VOICE_WF_AMP_SCALE;
				wf_gen->period_remain = wf_gen->period;
			} else {
				wf_gen->sample += wf_gen->step;
				wf_gen->period_remain--;
			}
			break;
		case VOICE_MODE_TRIANGLE:
			if (!wf_gen->period_remain) {
				/* Switch direction */
				if (wf_gen->step > 0)
					wf_gen->sample = -wf_gen->amplitude;
				else
					wf_gen->sample = wf_gen->amplitude;
				wf_gen->step = -wf_gen->step;
				wf_gen->period_remain = wf_gen->period;
			} else {
				wf_gen->sample += wf_gen->step;
				wf_gen->period_remain--;
			}
			break;
	}

	return wf_gen->sample >> VOICE_WF_AMP_SCALE;
}

/* Compute frequency period (sawtooth wave) */
static uint8_t voice_wf_calc_sawtooth_period(uint16_t freq) {
	return (synth_freq / freq);
}

/* Compute frequency period (square/triangle wave) */
static uint8_t voice_wf_calc_square_period(uint16_t freq) {
	return voice_wf_calc_sawtooth_period(freq) >> 1;
}


void voice_wf_set_dc(struct voice_wf_gen_t* const wf_gen,
		uint8_t amplitude) {
	wf_gen->mode = VOICE_MODE_DC;
	wf_gen->amplitude = amplitude;
}

void voice_wf_set_square(struct voice_wf_gen_t* const wf_gen,
		uint16_t freq, uint8_t amplitude) {
	wf_gen->mode = VOICE_MODE_SQUARE;
	wf_gen->amplitude = amplitude << VOICE_WF_AMP_SCALE;
	wf_gen->period = voice_wf_calc_square_period(freq);
	wf_gen->period_remain = wf_gen->period;
	wf_gen->sample = wf_gen->amplitude;
}

void voice_wf_set_sawtooth(struct voice_wf_gen_t* const wf_gen,
		uint16_t freq, uint8_t amplitude) {
	wf_gen->mode = VOICE_MODE_SAWTOOTH;
	wf_gen->sample = wf_gen->amplitude << VOICE_WF_AMP_SCALE;
	wf_gen->period = voice_wf_calc_sawtooth_period(freq);
	wf_gen->period_remain = wf_gen->period;
	wf_gen->amplitude = -wf_gen->sample;
	wf_gen->step = ((int32_t)(wf_gen->amplitude << 1)) / wf_gen->period;
}

void voice_wf_set_triangle(struct voice_wf_gen_t* const wf_gen,
		uint16_t freq, uint8_t amplitude) {
	wf_gen->mode = VOICE_MODE_TRIANGLE;
	wf_gen->sample = wf_gen->amplitude << VOICE_WF_AMP_SCALE;
	wf_gen->period = voice_wf_calc_square_period(freq);
	wf_gen->period_remain = wf_gen->period;
	wf_gen->amplitude = -wf_gen->sample;
	wf_gen->step = ((int32_t)(wf_gen->amplitude << 1)) / wf_gen->period;
}

void voice_wf_set_noise(struct voice_wf_gen_t* const wf_gen,
		uint8_t amplitude) {
	wf_gen->mode = VOICE_MODE_SQUARE;
	wf_gen->amplitude = amplitude;
}
/*
 * vim: set sw=8 ts=8 noet si tw=72
 */