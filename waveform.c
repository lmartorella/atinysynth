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
#include "debug.h"
#include <stdlib.h>

/* Amplitude scaling */
#define VOICE_WF_AMP_SCALE	(8)

/*!
 * Number of fractional bits for `period` and `period_remain`.
 * This allows tuned notes even in lower sampling frequencies.
 * The integer part (12 bits) is wide enough to render a 20Hz 
 * note on the higher 48kHz sampling frequency.
 */
#define PERIOD_FP_SCALE 	(4)

int8_t voice_wf_next(struct voice_wf_gen_t* const wf_gen) {
#if defined(USE_SAWTOOTH) || defined(USE_TRIANGLE) || defined(USE_NOISE) || defined(USE_DC)
	switch(wf_gen->mode) 
#endif
    {
#ifdef USE_DC
		case VOICE_MODE_DC:
			_DPRINTF("wf=%p mode=DC amp=%d\n",
					wf_gen, wf_gen->amplitude);
			return wf_gen->int_amplitude;
#endif
#ifdef USE_NOISE
		case VOICE_MODE_NOISE: {
			uint16_t sample = (rand() /
				(RAND_MAX/512)) - 256;
			sample *= wf_gen->int_amplitude;
			_DPRINTF("wf=%p mode=NOISE amp=%d → sample=%d\n",
					wf_gen, wf_gen->amplitude,
					wf_gen->sample);
			return sample >> VOICE_WF_AMP_SCALE;
		}
#endif
#if defined(USE_SAWTOOTH) || defined(USE_TRIANGLE) || defined(USE_NOISE) || defined(USE_DC)
		case VOICE_MODE_SQUARE:
#endif
			if (wf_gen->period > 0) {
				if ((wf_gen->period_remain >> PERIOD_FP_SCALE) == 0) {
					/* Swap value */
					wf_gen->int_sample = -wf_gen->int_sample;
					wf_gen->period_remain += wf_gen->period;
				} else {
					wf_gen->period_remain -= (1 << PERIOD_FP_SCALE);
				}
			}			_DPRINTF("wf=%p mode=SQUARE amp=%d rem=%d "
					"→ sample=%d\n",
					wf_gen, wf_gen->amplitude,
					wf_gen->period_remain,
					wf_gen->sample);
			return wf_gen->int_sample;
#ifdef USE_SAWTOOTH
		case VOICE_MODE_SAWTOOTH:
			if ((wf_gen->period_remain >> PERIOD_FP_SCALE) == 0) {
				/* Back to -amplitude */
				wf_gen->fp_sample = -wf_gen->fp_amplitude
					<< VOICE_WF_AMP_SCALE;
				wf_gen->period_remain = wf_gen->period;
			} else {
				wf_gen->fp_sample += wf_gen->fp_step;
				wf_gen->period_remain -= (1 << PERIOD_FP_SCALE);
			}
			_DPRINTF("wf=%p mode=SAWTOOTH amp=%d rem=%d step=%d "
					"→ sample=%d\n",
					wf_gen, wf_gen->amplitude,
					wf_gen->period_remain, wf_gen->step,
					wf_gen->sample);
			return wf_gen->fp_sample >> VOICE_WF_AMP_SCALE;
#endif
#ifdef USE_TRIANGLE
		case VOICE_MODE_TRIANGLE:
			if ((wf_gen->period_remain >> PERIOD_FP_SCALE) == 0) {
				/* Switch direction */
				if (wf_gen->fp_step > 0)
					wf_gen->fp_sample = wf_gen->fp_amplitude;
				else
					wf_gen->fp_sample = -wf_gen->fp_amplitude;
				wf_gen->fp_step = -wf_gen->fp_step;
				wf_gen->period_remain += wf_gen->period;
			} else {
				wf_gen->fp_sample += wf_gen->fp_step;
				wf_gen->period_remain -= (1 << PERIOD_FP_SCALE);
			}
			_DPRINTF("wf=%p mode=TRIANGLE amp=%d rem=%d step=%d "
					"→ sample=%d\n",
					wf_gen, wf_gen->amplitude,
					wf_gen->period_remain, wf_gen->step,
					wf_gen->sample);
			return wf_gen->fp_sample >> VOICE_WF_AMP_SCALE;
#endif
	}
}

/* Compute frequency period (full wave) */
uint16_t voice_wf_freq_to_period(uint16_t freq) {
	/* Use 16-bit 12.4 fixed point */  
	return (((uint32_t)(synth_freq << PERIOD_FP_SCALE)) / freq);
}

#ifdef USE_DC
void voice_wf_set_dc(struct voice_wf_gen_t* const wf_gen,
		int8_t amplitude) {
	wf_gen->mode = VOICE_MODE_DC;
	wf_gen->int_amplitude = amplitude;
}
#endif

void voice_wf_set(struct voice_wf_gen_t* const wf_gen, struct voice_wf_def_t* const wf_def) {
//static void voice_wf_set_square_p(struct voice_wf_gen_t* const wf_gen, struct voice_wf_def_t* const wf_def) {
#if defined(USE_SAWTOOTH) || defined(USE_TRIANGLE) || defined(USE_NOISE) || defined(USE_DC)
	wf_gen->mode = VOICE_MODE_SQUARE;
#endif
	wf_gen->int_sample = wf_gen->int_amplitude = wf_def->amplitude;
	wf_gen->period_remain = wf_gen->period = wf_def->period >> 1;
	_DPRINTF("wf=%p INIT mode=SQUARE amp=%d per=%d rem=%d "
			"→ sample=%d\n",
			wf_gen, wf_gen->amplitude, wf_gen->period,
			wf_gen->period_remain,
			wf_gen->sample);
}

void voice_wf_set_square_p(struct voice_wf_gen_t* const wf_gen, uint16_t period, int8_t amplitude) {
	struct voice_wf_def_t wf_def;
	wf_def.period = period >> 1;
	wf_def.amplitude = amplitude;
	voice_wf_set(wf_gen, &wf_def);
}

void voice_wf_set_square(struct voice_wf_gen_t* const wf_gen, uint16_t freq, int8_t amplitude) {
	voice_wf_set_square_p(wf_gen, voice_wf_freq_to_period(freq), amplitude);
}

#ifdef USE_SAWTOOTH
static void voice_wf_set_sawtooth_p(struct voice_wf_gen_t* const wf_gen,
		uint16_t period, int8_t amplitude) {
	wf_gen->mode = VOICE_MODE_SAWTOOTH;
	wf_gen->fp_sample = -(int16_t)amplitude << VOICE_WF_AMP_SCALE;
	wf_gen->period = period;
	wf_gen->period_remain = wf_gen->period;
	wf_gen->fp_amplitude = -wf_gen->fp_sample;
	wf_gen->fp_step = (wf_gen->fp_amplitude / (wf_gen->period >> PERIOD_FP_SCALE)) << 1;
	_DPRINTF("wf=%p INIT mode=SAWTOOTH amp=%d per=%d step=%d rem=%d "
			"→ sample=%d\n",
			wf_gen, wf_gen->amplitude, wf_gen->period,
			wf_gen->step, wf_gen->period_remain,
			wf_gen->sample);
}

void voice_wf_set_sawtooth(struct voice_wf_gen_t* const wf_gen,
		uint16_t freq, int8_t amplitude) {
	uint16_t period = voice_wf_freq_to_period(freq);
	voice_wf_set_sawtooth_p(wf_gen, period, amplitude);
}
#endif

#ifdef USE_TRIANGLE
static void voice_wf_set_triangle_p(struct voice_wf_gen_t* const wf_gen,
		uint16_t period, int8_t amplitude) {
	wf_gen->mode = VOICE_MODE_TRIANGLE;
	wf_gen->fp_sample = -(int16_t)amplitude << VOICE_WF_AMP_SCALE;
	wf_gen->period = period >> 1;
	wf_gen->period_remain = wf_gen->period;
	wf_gen->fp_amplitude = -wf_gen->fp_sample;
	wf_gen->fp_step = (wf_gen->fp_amplitude / (wf_gen->period >> PERIOD_FP_SCALE)) << 1;
	_DPRINTF("wf=%p INIT mode=TRIANGLE amp=%d per=%d step=%d rem=%d "
			"→ sample=%d\n",
			wf_gen, wf_gen->amplitude, wf_gen->period,
			wf_gen->step, wf_gen->period_remain,
			wf_gen->sample);
}

void voice_wf_set_triangle(struct voice_wf_gen_t* const wf_gen,
		uint16_t freq, int8_t amplitude) {
	uint16_t period = voice_wf_freq_to_period(freq);
	voice_wf_set_triangle_p(wf_gen, period, amplitude);
}
#endif

#ifdef USE_NOISE
void voice_wf_set_noise(struct voice_wf_gen_t* const wf_gen,
		int8_t amplitude) {
	wf_gen->mode = VOICE_MODE_NOISE;
	wf_gen->int_amplitude = amplitude;
}
#endif

void voice_wf_set_(struct voice_wf_gen_t* const wf_gen, struct voice_wf_def_t* const wf_def) {
#if defined(USE_SAWTOOTH) || defined(USE_TRIANGLE) || defined(USE_NOISE) || defined(USE_DC)
	switch (wf_def->mode) 
#endif
    {
#ifdef USE_DC
		case VOICE_MODE_DC:
			voice_wf_set_dc(wf_gen, wf_def->amplitude);
			return;
#endif
#if defined(USE_SAWTOOTH) || defined(USE_TRIANGLE) || defined(USE_NOISE) || defined(USE_DC)
		case VOICE_MODE_SQUARE:
#endif
			voice_wf_set_square_p(wf_gen, wf_def->period, wf_def->amplitude);
			return;
#ifdef USE_SAWTOOTH
		case VOICE_MODE_SAWTOOTH:
			voice_wf_set_sawtooth_p(wf_gen, wf_def->period, wf_def->amplitude);
			return;
#endif
#ifdef USE_TRIANGLE
		case VOICE_MODE_TRIANGLE:
			voice_wf_set_triangle_p(wf_gen, wf_def->period, wf_def->amplitude);
			return;
#endif
#ifdef USE_NOISE
		case VOICE_MODE_NOISE:
			voice_wf_set_noise(wf_gen, wf_def->amplitude);
			return;
#endif
	}
}

/*
 * vim: set sw=8 ts=8 noet si tw=72
 */
