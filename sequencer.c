/*!
 * Polyphonic synthesizer for microcontrollers.  Sequencer code.
 * (C) 2021 Luciano Martorella
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
#include "sequencer.h"
#include "synth.h"

/*! State used between `seq_play_stream` and `seq_feed_synth` */
#ifndef SEQ_CHANNEL_COUNT
static uint8_t seq_voice_count;
#else
#define seq_voice_count SEQ_CHANNEL_COUNT
#endif

uint8_t seq_end = 0;
struct seq_frame_t seq_buf_frame;

void seq_play_stream(uint8_t voices) {
#ifndef SEQ_CHANNEL_COUNT
	seq_voice_count = voices;
#endif

	// Disable all channels
    seq_end = 0;
}

int8_t seq_feed_synth() {
#ifndef NO_CLIP_CHECK
	int16_t sample = 0;
#else
	int8_t sample = 0;
#endif

    struct voice_ch_t* voice = &synth.voice[0];
    uint8_t fed = 0;
    uint8_t i = seq_voice_count;
	do {
		sample += voice_ch_next(voice);
        if (!fed && voice->adsr.state_counter == ADSR_STATE_END) {
            // Feed data
			new_frame_require();
            if (seq_buf_frame.adsr_time_scale_1 == 0) {
                // End-of-stream
				seq_end = 1;
                continue;
            }

            voice_wf_set(&voice->wf, &seq_buf_frame);
            adsr_config(&voice->adsr, &seq_buf_frame);

			// Don't overload the CPU with multiple frames per sample
			// This will create minimum phase errors (of 1 sample period) but will keep the process real-time on slower CPUs
            fed = 1;
		}
        i--;
        voice++;
	} while (i);

	/* Handle clipping */
#ifndef NO_CLIP_CHECK
	if (sample > INT8_MAX) {
		sample = INT8_MAX;
#ifdef CHECK_CLIPPING
		clip_count++;
#endif
	} else if (sample < INT8_MIN) {
		sample = INT8_MIN;
#ifdef CHECK_CLIPPING
		clip_count++;
#endif
	}
#endif
	return sample;
}
