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
void adsr_config(struct adsr_env_gen_t* const adsr, struct adsr_env_def_t* const def) {
	adsr->def = *def;
	adsr_reset(adsr);
}

/*!
 * Compute the ADSR gain
 */
uint8_t adsr_next(struct adsr_env_gen_t* const adsr) {
	if (adsr->next_event) {
		/* Still waiting for next event */
		//if (adsr->next_event != TIME_SCALE_MAX)
			adsr->next_event--;
		_DPRINTF("adsr=%p gain=%d next_in=%d\n",
				adsr, adsr->gain, adsr->next_event);
		return adsr->gain;
	}
	
	/*
	 * We use if statements here since we might want to jump
	 * between states.  This lets us do that more easily.
	 */
	if (adsr->state_counter < ADSR_STATE_DECAY_START) {
		adsr->gain = 7 - (adsr->state_counter >> 1);
	} 
	
	else if (adsr->state_counter < ADSR_STATE_SUSTAIN_START) {
		// from gain 0 to 1 (sustain)
		adsr->gain = (adsr->state_counter - ADSR_STATE_DECAY_START) >> 3;
	} 
	
	else if (adsr->state_counter < adsr->def.release_start) {
		adsr->gain = 2;
	}

	else if (adsr->state_counter < adsr->def.release_start + ADSR_STATE_RELEASE_DURATION) {
		// From 2 to 5
		adsr->gain = ((adsr->state_counter - adsr->def.release_start) >> 2) + 2;
	}

	else {
		_DPRINTF("adsr=%p RELEASE EXPIRE\n", adsr);

		/* Mute channel */
		adsr->gain = 8;
	}

	adsr->next_event = adsr->def.time_scale;
	adsr->state_counter++;
	return adsr->gain;
}
