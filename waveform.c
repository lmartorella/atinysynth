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

/* Amplitude scaling when 16 bits are used */
#define VOICE_WF_AMP_SCALE	(8)

/*!
 * Number of fractional bits for `period` and `period_remain`.
 * This allows tuned notes even in lower sampling frequencies.
 * The integer part (12 bits) is wide enough to render a 20Hz 
 * note on the higher 48kHz sampling frequency.
 */
#define PERIOD_FP_SCALE 	(4)

int8_t voice_wf_next() {
#if defined(USE_SAWTOOTH) || defined(USE_TRIANGLE) || defined(USE_DC)
	switch(wf_gen->mode) 
#endif
    {
#ifdef USE_DC
		case VOICE_MODE_DC:
			_DPRINTF("wf=%p mode=DC amp=%d\n",
					wf_gen, wf_gen->amplitude);
			return wf_gen->int_amplitude;
#endif
#if defined(USE_SAWTOOTH) || defined(USE_TRIANGLE) || defined(USE_DC)
		case VOICE_MODE_SQUARE:
#endif
			if (cur_voice->wf.period > 0) {
				if ((cur_voice->wf.period_remain >> PERIOD_FP_SCALE) == 0) {
					/* Swap value */
					cur_voice->wf.int_sample = -cur_voice->wf.int_sample;
					cur_voice->wf.period_remain += cur_voice->wf.period;
				}
				cur_voice->wf.period_remain -= (1 << PERIOD_FP_SCALE);
			}
			_DPRINTF("wf=%p mode=SQUARE amp=%d rem=%d "
					"→ sample=%d\n",
					wf_gen, wf_gen->amplitude,
					wf_gen->period_remain,
					wf_gen->sample);
			return cur_voice->wf.int_sample;
#ifdef USE_SAWTOOTH
		case VOICE_MODE_SAWTOOTH:
			if ((wf_gen->period_remain >> PERIOD_FP_SCALE) == 0) {
				/* Back to -amplitude */
				wf_gen->fp_sample = -wf_gen->fp_amplitude
					<< VOICE_WF_AMP_SCALE;
				wf_gen->period_remain = wf_gen->period;
			} else {
				wf_gen->fp_sample += wf_gen->fp_step;
			}
			wf_gen->period_remain -= (1 << PERIOD_FP_SCALE);
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
			}
			wf_gen->period_remain -= (1 << PERIOD_FP_SCALE);
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

void voice_wf_set(struct seq_frame_t* const frame) {
//static void voice_wf_set_square_p(struct voice_wf_gen_t* const wf_gen, struct voice_wf_def_t* const wf_def) {
#if defined(USE_SAWTOOTH) || defined(USE_TRIANGLE) || defined(USE_DC)
	wf_gen->mode = VOICE_MODE_SQUARE;
#endif
	cur_voice->wf.int_sample = cur_voice->wf.int_amplitude = frame->wf_amplitude;
	cur_voice->wf.period_remain = cur_voice->wf.period = frame->wf_period;
}

void voice_wf_set_square_p(uint16_t period, int8_t amplitude) {
	struct seq_frame_t frame;
	// Half period for square generator
	frame.wf_period = period >> 1;
	frame.wf_amplitude = amplitude;
	voice_wf_set(&frame);
}

void voice_wf_set_square(uint16_t freq, int8_t amplitude) {
	voice_wf_set_square_p(voice_wf_freq_to_period(freq), amplitude);
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
	// Half period for square generator
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

void voice_wf_set_(struct voice_wf_def_t* const wf_def) {
#if defined(USE_SAWTOOTH) || defined(USE_TRIANGLE) || defined(USE_DC)
	switch (wf_def->mode) 
#endif
    {
#ifdef USE_DC
		case VOICE_MODE_DC:
			voice_wf_set_dc(wf_gen, wf_def->amplitude);
			return;
#endif
#if defined(USE_SAWTOOTH) || defined(USE_TRIANGLE) || defined(USE_DC)
		case VOICE_MODE_SQUARE:
#endif
			voice_wf_set_square_p(wf_def->period, wf_def->amplitude);
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
	}
}

int8_t voice_wf_setup_def(struct seq_frame_t* frame, uint16_t frequency, uint8_t amplitude, uint8_t waveform) {
	if (waveform != VOICE_MODE_SQUARE) {
		// Not supported
		return 0;
	}
	uint16_t period = frequency > 0 ? (voice_wf_freq_to_period(frequency) >> 1): 0;
	if (amplitude > 0x7f || period > 0x3fff) {
		// Out of range for 7/14-bit packed data
		return 0;
	}
	frame->wf_amplitude = amplitude;
	frame->wf_period = period;
	return 1;
}
