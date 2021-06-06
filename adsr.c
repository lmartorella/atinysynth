/*!
 * Polyphonic synthesizer for microcontrollers.  ADSR Envelope generator.
 * (C) 2017 Stuart Longland - Luciano Martorella
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
#include "voice.h"
#include <stdlib.h>

/*!
 * Configure the ADSR.
 */
void adsr_config(struct seq_frame_t* const frame) {
	cur_voice->adsr.def.release_start = frame->adsr_release_start;
	cur_voice->adsr.next_event = cur_voice->adsr.def.time_scale = frame->adsr_time_scale_1;
	cur_voice->adsr.state_counter = ADSR_STATE_INIT; // 1
	// Start from mute
	cur_voice->adsr.gain = 6;
}

/*!
 * Compute the ADSR gain
 */
void adsr_next() {
	if (cur_voice->adsr.next_event) {
		/* Still waiting for next event */
		cur_voice->adsr.next_event--;
	} else {
		if (!cur_voice->adsr.state_counter) {
			// Abort
			cur_voice->adsr.gain = 6;
			return;
		}
		if (cur_voice->adsr.state_counter < ADSR_STATE_SUSTAIN_START) {
			// Counter from 1 to 6: 5 steps.
			// From 6 to 0
			cur_voice->adsr.gain--;
		} 
		else if (cur_voice->adsr.state_counter < ADSR_STATE_DECAY_START) {
			// Remain to zero
		}
		else if (cur_voice->adsr.state_counter < cur_voice->adsr.def.release_start) {
			// Then decay to 1 and stay
			cur_voice->adsr.gain = 1;
		}
		else {
			// Decrease from 2 to 8 every 8 counters
			if (!(cur_voice->adsr.state_counter & 0x7)) {
				cur_voice->adsr.gain++;
			}
		} 

		if (cur_voice->adsr.state_counter > ADSR_TIME_UNITS) {
			// 0 is the final state (fast to check)
			cur_voice->adsr.state_counter = ADSR_STATE_END;
		} else {
			cur_voice->adsr.next_event = cur_voice->adsr.def.time_scale;
			cur_voice->adsr.state_counter++;
		}
	}
}
