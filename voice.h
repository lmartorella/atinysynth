/*!
 * Polyphonic synthesizer for microcontrollers.  Voice generation.
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
#ifndef _VOICE_H
#define _VOICE_H

#include "waveform.h"
#include "adsr.h"
#include "debug.h"

/*!
 * Voice channel state.  30 bytes.
 */
struct voice_ch_t {
	/*!
	 * ADSR Envelope generator state.
	 */
	struct adsr_env_gen_t adsr;
	/*!
	 * Waveform generator state.
	 */
	struct voice_wf_gen_t wf;
};

/*!
 * Compute the next voice channel sample.
 */
inline static int8_t voice_ch_next(struct voice_ch_t* const voice) {
	uint8_t gain = adsr_next(&(voice->adsr));
	_DPRINTF("ch=%p gain=%d\n", voice, -gain);
	if (gain == 8) {
		return 0;
	}

	int8_t value = voice_wf_next(&(voice->wf));
	_DPRINTF("ch=%p value=%d\n", voice, value);
	value >>= gain;

	_DPRINTF("ch=%p out=%d\n", voice, value);
	return value;
}

#endif