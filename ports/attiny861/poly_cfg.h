#ifndef _POLY_CFG_H
#define _POLY_CFG_H

/*!
 * Polyphonic synthesizer for microcontrollers.
 * (C) 2016 Stuart Longland
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

#define SYNTH_FREQ		(8000)

/*! Type for time scale, samples per unit. 
 * 16 bits would allow 2^24 samples of maximum note duration and a total duration of 255 time unit.
 * This means ~2000 seconds on 8Khz.
 */
#define TIME_SCALE_T    uint16_t

#endif
