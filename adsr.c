/*!
 * Polyphonic synthesizer for microcontrollers.  ADSR Envelope generator.
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

#include "adsr.h"
#include <stdlib.h>
#ifdef __AVR_ARCH__
#include <avr/pgmspace.h>
#endif

/* ADSR attack/decay adjustments */
#define ADSR_EXP_AMP_FACTOR	(6)
#define ADSR_EXP_SHIFT		(-8)
#define ADSR_EXP_TIME_FACTOR	(1)
#define ADSR_LIN_AMP_FACTOR	(6)
#define ADSR_DECAY_OFFSET	(16)

/*!
 * ADSR Attack amplitude exponential shift.
 */
static uint8_t adsr_attack_shift(uint8_t sample) {
	return (sample + ADSR_EXP_SHIFT)/ADSR_EXP_TIME_FACTOR
		- ADSR_EXP_AMP_FACTOR;
}

/*!
 * ADSR Release amplitude exponential shift.
 */
static uint8_t adsr_release_shift(uint8_t sample) {
	return adsr_attack_shift(ADSR_DECAY_OFFSET - sample);
}

/*!
 * Reset the ADSR state ready for the next note
 */
void adsr_reset(struct adsr_env_gen_t* const adsr) {
	adsr->next_event = 0;
	adsr->state = ADSR_STATE_IDLE;
}

/*!
 * Compute the ADSR amplitude
 */
uint8_t adsr_next(struct adsr_env_gen_t* const adsr) {
	if (adsr->next_event) {
		/* Still waiting for next event */
		adsr->next_event--;
		return adsr->amplitude;
	}

	/*
	 * We use if statements here since we might want to jump
	 * between states.  This lets us do that more easily.
	 */

	if (adsr->state == ADSR_STATE_IDLE) {
		/* Are registers set up? */
		if (!adsr->time_scale)
			return 0;
		if (!(adsr->delay_time || adsr->attack_time
				|| adsr->decay_time || adsr->release_time))
			return 0;
		if (!(adsr->peak_amp || adsr->sustain_amp))
			return 0;

		/* All good */
		if (adsr->delay_time)
			adsr->state = ADSR_STATE_DELAY_INIT;
		else
			adsr->state = ADSR_STATE_DELAY_EXPIRE;
	}

	if (adsr->state == ADSR_STATE_DELAY_INIT) {
		/* Setting up a delay */
		adsr->amplitude = 0;
		adsr->next_event = adsr->time_scale
			* adsr->delay_time;
		adsr->state = ADSR_STATE_DELAY_EXPIRE;
		/* Wait for delay */
		return adsr->amplitude;
	}

	if (adsr->state == ADSR_STATE_DELAY_EXPIRE) {
		/* Delay has expired */
		if (adsr->attack_time)
			adsr->state = ADSR_STATE_ATTACK_INIT;
		else
			adsr->state = ADSR_STATE_ATTACK_EXPIRE;
	}

	if (adsr->state == ADSR_STATE_ATTACK_INIT) {
		/* Attack is divided into 16 segments */
		adsr->time_step = (uint16_t)((adsr->attack_time
				* adsr->time_scale) >> 4);
		adsr->counter = 16;
		adsr->next_event = adsr->time_step;
		adsr->state = ADSR_STATE_ATTACK;
	}

	if (adsr->state == ADSR_STATE_ATTACK) {
		if (adsr->counter) {
			/* Change of amplitude */
			uint16_t lin_amp = (16-adsr->counter)
				* adsr->peak_amp;
			adsr->amplitude = lin_amp
				>> ADSR_LIN_AMP_FACTOR;
			adsr->amplitude += adsr->peak_amp >> 
				adsr_attack_shift(adsr->counter);
			/* Go around again */
			adsr->counter--;
			adsr->next_event = adsr->time_step;
			return adsr->amplitude;
		} else {
			adsr->state = ADSR_STATE_ATTACK_EXPIRE;
		}
	}

	if (adsr->state == ADSR_STATE_ATTACK_EXPIRE) {
		if (adsr->decay_time)
			adsr->state = ADSR_STATE_DECAY_INIT;
		else
			adsr->state = ADSR_STATE_DECAY_EXPIRE;
	}

	if (adsr->state == ADSR_STATE_DECAY_INIT) {
		/* We should be at full amplitude */
		adsr->amplitude = adsr->peak_amp;

		adsr->time_step = (uint16_t)((adsr->decay_time
					* adsr->time_scale) >> 4);
		adsr->counter = 16;
		adsr->next_event = adsr->time_step;
		adsr->state = ADSR_STATE_DECAY;
	}

	if (adsr->state == ADSR_STATE_DECAY) {
		if (adsr->counter) {
			/* Linear decrease in amplitude */
			uint16_t delta = adsr->peak_amp
				- adsr->sustain_amp;
			delta *= adsr->counter;
			delta >>= 4;

			adsr->amplitude = adsr->sustain_amp + delta;
			adsr->next_event = adsr->time_step;
			adsr->counter--;
		} else {
			adsr->state = ADSR_STATE_DECAY_EXPIRE;
		}
	}

	if (adsr->state == ADSR_STATE_DECAY_EXPIRE) {
		if (adsr->sustain_time)
			adsr->state = ADSR_STATE_SUSTAIN_INIT;
		else
			adsr->state = ADSR_STATE_SUSTAIN_EXPIRE;
	}

	if (adsr->state == ADSR_STATE_SUSTAIN_INIT) {
		adsr->amplitude = adsr->sustain_amp;
		adsr->next_event = adsr->time_scale
			* adsr->sustain_time;
		adsr->state = ADSR_STATE_SUSTAIN_EXPIRE;
		/* Wait for delay */
		return adsr->amplitude;
	}

	if (adsr->state == ADSR_STATE_SUSTAIN_EXPIRE) {
		if (adsr->release_time)
			adsr->state = ADSR_STATE_RELEASE_INIT;
		else
			adsr->state = ADSR_STATE_RELEASE_EXPIRE;
	}

	if (adsr->state == ADSR_STATE_RELEASE_INIT) {
		adsr->time_step = (uint16_t)((adsr->release_time
					* adsr->time_scale) >> 4);
		adsr->counter = 16;
		adsr->next_event = adsr->time_step;
		adsr->state = ADSR_STATE_RELEASE;
	}

	if (adsr->state == ADSR_STATE_RELEASE) {
		if (adsr->counter) {
			/* Change of amplitude */
			uint16_t lin_amp = adsr->counter
				* adsr->peak_amp;
			adsr->amplitude = lin_amp
				>> ADSR_LIN_AMP_FACTOR;
			adsr->amplitude += adsr->peak_amp >> 
				adsr_release_shift(adsr->counter);
			/* Go around again */
			adsr->counter--;
			adsr->next_event = adsr->time_step;
		} else {
			adsr->state = ADSR_STATE_RELEASE_EXPIRE;
		}
	}

	if (adsr->state == ADSR_STATE_RELEASE_EXPIRE) {
		/* Reset the state */
		adsr->state = ADSR_STATE_IDLE;
		adsr->amplitude = 0;
	}

	return adsr->amplitude;
}

/*
 * vim: set sw=8 ts=8 noet si tw=72
 */