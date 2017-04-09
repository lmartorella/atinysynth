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

/* ADSR states */
#define ADSR_STATE_IDLE			(0x00)
#define ADSR_STATE_DELAY_INIT		(0x10)
#define ADSR_STATE_DELAY_EXPIRE		(0x1f)
#define ADSR_STATE_ATTACK_INIT		(0x20)
#define ADSR_STATE_ATTACK		(0x21)
#define ADSR_STATE_ATTACK_EXPIRE	(0x2f)
#define ADSR_STATE_DECAY_INIT		(0x30)
#define ADSR_STATE_DECAY		(0x31)
#define ADSR_STATE_DECAY_EXPIRE		(0x3f)
#define ADSR_STATE_SUSTAIN_INIT		(0x40)
#define ADSR_STATE_SUSTAIN_EXPIRE	(0x4f)
#define ADSR_STATE_RELEASE_INIT		(0x50)
#define ADSR_STATE_RELEASE		(0x51)
#define ADSR_STATE_RELEASE_EXPIRE	(0x5f)
#define ADSR_STATE_DONE			(0xff)

/*!
 * ADSR Envelope Generator data.  20 bytes.
 */
struct adsr_env_gen_t {
	/*! Time to next event, samples */
	uint32_t next_event;
	/*! Time step, samples */
	uint16_t time_step;
	/*! Time scale, samples per unit */
	uint32_t time_scale;
	/*! Delay period, time units */
	uint8_t delay_time;
	/*! Attack period, time units */
	uint8_t attack_time;
	/*! Decay period, time units */
	uint8_t decay_time;
	/*! Sustain period, time units */
	uint8_t sustain_time;
	/*! Release period, time units */
	uint8_t release_time;
	/*! Attack peak amplitude */
	uint8_t peak_amp;
	/*! Sustain amplitude */
	uint8_t sustain_amp;
	/*! Present amplitude */
	uint8_t amplitude;
	/*! ADSR state */
	uint8_t state;
	/*! ADSR counter */
	uint8_t counter;
};

/*!
 * Reset the ADSR state ready for the next note.
 */
static inline void adsr_reset(struct adsr_env_gen_t* const adsr) {
	adsr->next_event = 0;
	adsr->state = ADSR_STATE_IDLE;
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
static inline void adsr_config(struct adsr_env_gen_t* const adsr,
		uint32_t time_scale, uint8_t delay_time,
		uint8_t attack_time, uint8_t decay_time,
		uint8_t sustain_time, uint8_t release_time,
		uint8_t peak_amp, uint8_t sustain_amp) {

	adsr->time_scale = time_scale;
	adsr->delay_time = delay_time;
	adsr->attack_time = attack_time;
	adsr->decay_time = decay_time;
	adsr->sustain_time = sustain_time;
	adsr->release_time = release_time;
	adsr->peak_amp = peak_amp;
	adsr->sustain_amp = sustain_amp;

	adsr_reset(adsr);
}

/*!
 * Compute the ADSR amplitude
 */
uint8_t adsr_next(struct adsr_env_gen_t* const adsr);

/*!
 * Test to see if the ADSR is done.
 */
static inline uint8_t adsr_is_done(struct adsr_env_gen_t* const adsr) {
	return (adsr->state == ADSR_STATE_DONE);
}

#endif
/*
 * vim: set sw=8 ts=8 noet si tw=72
 */
