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
#ifndef _ADSR_H
#define _ADSR_H

#include "debug.h"
#include <stdint.h>

/* ADSR states, in time units. MAX_TIME_UNIT is fixed, and terminates the envelope */
#define ADSR_STATE_INIT     		0x00
#define ADSR_STATE_DECAY_START		0x10
#define ADSR_STATE_SUSTAIN_START	0x20
// Release start is dynamic
#define ADSR_STATE_RELEASE_DURATION 0x10
#define ADSR_STATE_DONE				0x7f

#define ADSR_TIME_UNITS (ADSR_STATE_DONE + 1)

#include "poly_cfg.h"

/*!
 * ADSR Envelope Generator definition.
 */
struct adsr_env_def_t {
	/*! Time scale, samples per unit. */
	TIME_SCALE_T time_scale;
	/*! When the release period starts, time units */
	uint8_t release_start;
#ifndef ADSR_FIXED_PEAK_AMP
	/*! Attack peak amplitude */
	uint8_t peak_amp;
#endif
#ifndef ADSR_FIXED_SUSTAIN_AMP
	/*! Sustain amplitude */
	uint8_t sustain_amp;
#endif
};

/*!
 * ADSR Envelope Generator data. 
 */
struct adsr_env_gen_t {
	/*! Definition */
	struct adsr_env_def_t def;
	/*! Time to next event in samples.*/
	TIME_SCALE_T next_event;
	/*! Current ADSR state / counter */
	uint8_t state_counter;
	/*! Present amplitude */
	uint8_t amplitude;
};

/*!
 * Reset the ADSR state ready for the next note.
 */
static inline void adsr_reset(struct adsr_env_gen_t* const adsr) {
	adsr->next_event = 0;
	adsr->state_counter = ADSR_STATE_INIT;
	_DPRINTF("adsr=%p INIT time_scale=%d "
			"delay_time=%d "
			"attack_time=%d "
			"decay_time=%d "
			"sustain_time=%d "
			"release_time=%d "
			"peak_amp=%d "
			"sustain_amp=%d\n",
			adsr, adsr->time_scale,
			adsr->delay_time,
			adsr->attack_time,
			adsr->decay_time,
			adsr->sustain_time,
			adsr->release_time,
			adsr->peak_amp,
			adsr->sustain_amp);
}

/*!
 * Configure the ADSR.
 */
void adsr_config(struct adsr_env_gen_t* const adsr, struct adsr_env_def_t* const def);

/*!
 * Compute the ADSR amplitude
 */
uint8_t adsr_next(struct adsr_env_gen_t* const adsr);

/*!
 * Test to see if the ADSR is done.
 */
static inline uint8_t adsr_is_done(struct adsr_env_gen_t* const adsr) {
	return (adsr->state_counter == ADSR_STATE_DONE);
}

/*!
 * Test to see if the ADSR is awaiting a trigger.
 */
// static inline uint8_t adsr_is_waiting(struct adsr_env_gen_t* const adsr) {
// 	return ((adsr->next_event == TIME_SCALE_MAX)
// 			&& ((adsr->state == ADSR_STATE_DELAY_EXPIRE)
// 				|| (adsr->state == ADSR_STATE_SUSTAIN_EXPIRE)));
// }

// /*!
//  * Test to see if the ADSR is idle.
//  */
// static inline uint8_t adsr_is_idle(struct adsr_env_gen_t* const adsr) {
// 	return (adsr->state == ADSR_STATE_IDLE);
// }

/*!
 * Tell the ADSR to move onto the next state.
 */
static inline void adsr_continue(struct adsr_env_gen_t* const adsr) {
	adsr->next_event = 0;
}

#endif
/*
 * vim: set sw=8 ts=8 noet si tw=72
 */
