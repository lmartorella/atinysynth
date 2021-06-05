/*!
 * Polyphonic synthesizer for microcontrollers.
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
#ifndef _SYNTH_H
#define _SYNTH_H
#include "voice.h"
#include "debug.h"

#ifndef SYNTH_FREQ
/*!
 * Sample rate for the synthesizer: this needs to be declared in the
 * application.
 */
extern const uint16_t __attribute__((weak)) synth_freq;
#else
#define synth_freq		SYNTH_FREQ
#endif

/*!
 * Polyphonic synthesizer structure
 */
struct poly_synth_t {
	/*! Pointer to voices.  There may be up to 16 voices referenced. */
	struct voice_ch_t voice[VOICE_COUNT];
};

extern struct poly_synth_t synth;

#endif
