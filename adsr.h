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
#include "sequencer.h"
#include <stdint.h>

#define ADSR_TIME_UNITS 0x40

/* ADSR states, in time units. MAX_TIME_UNIT is fixed, and terminates the envelope */
#define ADSR_STATE_INIT     		(0x00 + 1)
#define ADSR_STATE_SUSTAIN_START	(0x08 + 1)
// Release start is dynamic
#define ADSR_STATE_RELEASE_DURATION (6*8)

#include "poly_cfg.h"

/*!
 * ADSR Envelope Generator definition.
 */
struct adsr_env_def_t {
	/*! Time scale, samples per unit. */
	TIME_SCALE_T time_scale;
	/*! When the release period starts, time units over the ADSR_TIME_UNITS scale */
	uint8_t release_start;
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
	/*! Present gain (0 is max, 1 is half, etc...) */
	uint8_t gain;
};

/*!
 * Configure the ADSR.
 */
void adsr_config(struct adsr_env_gen_t* const adsr, struct seq_frame_t* const frame);

/*!
 * Compute the ADSR gain as bit shift count (0 is full amplitude, 1 is half, etc...)
 */
uint8_t adsr_next(struct adsr_env_gen_t* const adsr);

#endif
