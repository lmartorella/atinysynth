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

#include "debug.h"
#include "adsr.h"
#include <stdlib.h>

/*!
 * Configure the ADSR.
 */
void adsr_config(struct adsr_env_gen_t* const adsr, struct seq_frame_t* const frame) {
	adsr->def.release_start = frame->adsr_release_start;
	adsr->def.time_scale = frame->adsr_time_scale_1;
	adsr->next_event = adsr->def.time_scale;
	adsr->state_counter = ADSR_STATE_INIT;
	adsr->gain = 8;
}

/*!
 * Compute the ADSR gain
 */
uint8_t adsr_next(struct adsr_env_gen_t* const adsr) {
	if (adsr->next_event) {
		/* Still waiting for next event */
		adsr->next_event--;
		_DPRINTF("adsr=%p gain=%d next_in=%d\n",
				adsr, adsr->gain, adsr->next_event);
		return adsr->gain;
	}
	
	/**
	 * Attack = gain from 7 to 0
	 */
	if (adsr->state_counter < ADSR_STATE_SUSTAIN_START) {
		adsr->gain = 8 - adsr->state_counter;
	} 
	// Then decay to 1
	else if (adsr->state_counter < adsr->def.release_start) {
		adsr->gain = 1;
	}
	else if (adsr->state_counter < adsr->def.release_start + ADSR_STATE_RELEASE_DURATION) {
		// From 2 to 8
		adsr->gain = ((adsr->state_counter - adsr->def.release_start) << 3) + 1;
	}
	// Then remain mute until ADSR_STATE_DONE
	else {
		adsr->gain = 8;
	} 

	if (adsr->state_counter > ADSR_TIME_UNITS) {
		// 0 is the final state (fast to check)
		adsr->state_counter = 0xff;
	}

	adsr->next_event = adsr->def.time_scale;
	adsr->state_counter++;
	return adsr->gain;
}
