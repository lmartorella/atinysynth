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

#include "sequencer.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>

/*! State used between `seq_play_stream` and `seq_feed_synth` */
static uint8_t voice_count;

/*! State of the sequencer compiler */
struct compiler_state_t {
	/*! The input channel map */
	struct seq_frame_map_t* input_map;
	/*! The output frame stream */
	struct seq_frame_t* out_stream;
	/*! The position of writing frame in the output stream */
	int stream_position;
	/*! The positions of every channel in the input channel map */
	int* channel_positions;
};

/*! Feed the first free channel and copy the selected frame in the output stream */
static void seq_feed_channels(struct compiler_state_t* state) {
	intptr_t mask = 1;
	int voice_idx = 0;
	for (int map_idx = 0; map_idx < state->input_map->channel_count; map_idx++) {
		// Skip empty channels
		const struct seq_frame_list_t* channel = &state->input_map->channels[map_idx]; 
		if (channel->count > 0) {
			if (state->channel_positions[voice_idx] < channel->count && (synth.enable & mask) == 0) {
				// Feed data
				struct seq_frame_t* frame = channel->frames + (state->channel_positions[voice_idx]++);

				voice_wf_set(&synth.voice[voice_idx].wf, &frame->waveform_def);
				adsr_config(&synth.voice[voice_idx].adsr, &frame->adsr_def);

				synth.enable |= mask;

				state->out_stream[state->stream_position++] = *frame;
				// Don't overload the CPU with multiple frames per sample
				// This will create minimum phase errors (of 1 sample period) but will keep the process real-time on slower CPUs
				break;
			}
			mask <<= 1;
			voice_idx++;
		}
	}
}

void seq_compile(struct seq_frame_map_t* map, struct seq_frame_t** frame_stream, int* frame_count, int* voice_count) {
	int total_frame_count = 0;
	// Skip empty channels
	int valid_channel_count = 0;
	for (int i = 0; i < map->channel_count; i++) {
		if (map->channels[i].count > 0) {
			valid_channel_count++;
			total_frame_count += map->channels[i].count;
		}
	}

	// Prepare output buffer, with total frame count, plus ending frame
	*frame_count = total_frame_count + 1;
	*voice_count = valid_channel_count;
	*frame_stream = malloc(sizeof(struct seq_frame_t) * (*frame_count));

	// Now play sequencer data, currently by channel, simulating the timing of the synth.
	synth.enable = 0;

	struct compiler_state_t state;
	state.channel_positions = malloc(sizeof(int) * valid_channel_count);
	memset(state.channel_positions, 0, sizeof(int) * valid_channel_count);
	state.input_map = map;
	state.out_stream = *frame_stream;
	state.stream_position = 0;

	seq_feed_channels(&state);
	while (synth.enable) {
		poly_synth_next(&synth);
		seq_feed_channels(&state);
	}
    
    // Add end frame (zero)
    memset((*frame_stream) + total_frame_count, 0, sizeof(struct seq_frame_t));
    
	free(state.channel_positions);
}

int seq_play_stream(const struct seq_stream_header_t* stream_header) {
	if (stream_header->voices > VOICE_COUNT) {
		_DPRINTF("Not enough voices");
		return 1;
	}
	if (stream_header->synth_frequency != synth_freq) {
		_DPRINTF("Mismatching sampling frequency");
		return 1;
	}
    if (stream_header->frame_size != sizeof(struct seq_frame_t)) {
        _DPRINTF("Mismatching frame size");
        return 1;
    }

	voice_count = stream_header->voices;
	// Disable all channels
	synth.enable = 0;
	return 0;
}

void seq_feed_synth() {
	CHANNEL_MASK_T mask = 1;
    struct voice_ch_t* voice = synth.voice;
	uint8_t i = voice_count;
    do {
        if ((synth.enable & mask) == 0) {
            // Feed data
            struct seq_frame_t frame;
			new_frame_require(&frame);
            if (frame.adsr_def.time_scale == 0) {
                // End-of-stream
                return;
            }

            voice_wf_set(&voice->wf, &frame.waveform_def);
            adsr_config(&voice->adsr, &frame.adsr_def);

            synth.enable |= mask;

			// Don't overload the CPU with multiple frames per sample
			// This will create minimum phase errors (of 1 sample period) but will keep the process real-time on slower CPUs
			break;
		}
		mask <<= 1;
		voice++;
		i--;
	} while (i);
}

void seq_free(struct seq_frame_t* frame_stream) {
	free(frame_stream);
}
