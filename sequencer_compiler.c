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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int clip_count = 0;

struct compiler_channel_state_t {
	/*! The positions of every channel in the input channel map */
	int position;
};

/*! State of the sequencer compiler */
struct compiler_state_t {
	/*! The input channel map */
	struct seq_frame_map_t* input_map;
	/*! The output frame stream */
	struct seq_frame_t* out_stream;
	/*! The position of writing frame in the output stream */
	int stream_position;
	/*! State of each channel */
	struct compiler_channel_state_t* channels;
};

/*! Feed the first free channel and copy the selected frame in the output stream */
static void seq_feed_channels(struct compiler_state_t* state) {
	intptr_t mask = 1;
	int voice_idx = 0;
	for (int map_idx = 0; map_idx < state->input_map->channel_count; map_idx++) {
		const struct seq_frame_list_t* channel = &state->input_map->channels[map_idx]; 
		// Skip empty channels
		if (channel->count > 0) {
			if (state->channels[voice_idx].position < channel->count && (synth.enable & mask) == 0) {
				// Feed data
				struct seq_frame_t* frame = &channel->frames[state->channels[voice_idx].position++];

				voice_wf_set(&synth.voice[voice_idx].wf, frame);
				adsr_config(&synth.voice[voice_idx].adsr, frame);

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

void seq_compile(struct seq_frame_map_t* map, struct seq_frame_t** frame_stream, int* frame_count, int* voice_count, int* do_clip_check) {
	int total_frame_count = 0;
	// Skip empty channels
	int valid_channel_count = 0;
	for (int i = 0; i < map->channel_count; i++) {
		if (map->channels[i].count > 0) {
			valid_channel_count++;
			total_frame_count += map->channels[i].count;
		}
	}

	// Prepare output buffer, with total frame count
	*frame_count = total_frame_count;
	*voice_count = valid_channel_count;
	*frame_stream = malloc(sizeof(struct seq_frame_t) * (*frame_count));

	// Now play sequencer data, currently by channel, simulating the timing of the synth.
	synth.enable = 0;

	struct compiler_state_t state;
	state.channels = malloc(sizeof(struct compiler_channel_state_t) * valid_channel_count);
	memset(state.channels, 0, sizeof(struct compiler_channel_state_t) * valid_channel_count);
	state.input_map = map;
	state.out_stream = *frame_stream;
	state.stream_position = 0;

	seq_feed_channels(&state);
	while (synth.enable) {
		poly_synth_next();
		seq_feed_channels(&state);
	}

	printf("Compiler stats:\n");
	if (clip_count) {
		printf("\tWARN: clip count: %d (slower)\n", clip_count);
	} else {
		printf("\tno clip (faster)\n");
	}

	free(state.channels);
}

void seq_free(struct seq_frame_t* seq_frame_stream) {
	free(seq_frame_stream);
}

struct distribution16_t {
	// Distribution map
	int map[0x10000];
	// Reverse map, from value to index
	int map_of_refs[0x10000];
	struct ref_map_t refs;
};

static struct distribution16_t dist_adsr_time_scale;
static struct distribution16_t dist_wf_period;
static struct distribution16_t dist_wf_amplitude;
static struct distribution16_t dist_adsr_release_start;

static void distribution_init(struct distribution16_t* dist) {
	memset(dist->map, 0, 0x10000 * sizeof(int));
	memset(dist->map_of_refs, 0xff, 0x10000 * sizeof(int));
	dist->refs.count = 0;
    dist->refs.values = 0;
}

static void distribution_add(struct distribution16_t* dist, uint16_t value) {
	if (!dist->map[value]) {
		dist->refs.count++;
	}
	dist->map[value]++;
}

static void distribution_calc(struct distribution16_t* dist) {
	dist->refs.bit_count = ceil(log(dist->refs.count) / log(2));
	printf("%d (%d bits)\n", dist->refs.count, dist->refs.bit_count);

    dist->refs.values = malloc(sizeof(int) * dist->refs.count);
    int j = 0;
    for (int i = 0; i < 0x10000; i++) {
        if (dist->map[i]) {
            dist->refs.values[j] = i;
            dist->map_of_refs[i] = j;
            j++;
        }
    }
}

struct stream_writer_t {
	uint8_t* buffer;
	int pos;
	int bit_pos;
};

static void write_bits(struct stream_writer_t* writer, uint8_t data, uint8_t bits) {
	if (bits) {
		uint16_t buffer = data << writer->bit_pos;

		writer->buffer[writer->pos] |= (buffer & 0xff);
		writer->buffer[writer->pos + 1] |= (buffer >> 8);

		writer->bit_pos += bits;
		if (writer->bit_pos >= 8) {
			writer->bit_pos -= 8;
			writer->pos++;
		}
	}
}

int stream_compress(struct seq_frame_t* frame_stream, int frame_count, struct bit_stream_t* stream) {
	// Analyze the stream to extract the data ref tables
	distribution_init(&dist_adsr_time_scale);
	distribution_init(&dist_wf_period);
	distribution_init(&dist_wf_amplitude);
	distribution_init(&dist_adsr_release_start);

	for (int i = 0; i < frame_count; i++) {
		struct seq_frame_t* frame = frame_stream + i;
		distribution_add(&dist_adsr_time_scale, frame->adsr_time_scale_1);
		distribution_add(&dist_wf_period, frame->wf_period);
		distribution_add(&dist_wf_amplitude, frame->wf_amplitude);
		distribution_add(&dist_adsr_release_start, frame->adsr_release_start);
	}

	printf("Distribution chart for %d frames:\n", frame_count);
	printf("\tadsr_time_scale: ");
	distribution_calc(&dist_adsr_time_scale);
	printf("\twf_period: ");
	distribution_calc(&dist_wf_period);
	printf("\twf_amplitude: ");
	distribution_calc(&dist_wf_amplitude);
	printf("\tadsr_release_start: ");
	distribution_calc(&dist_adsr_release_start);

	// Copy output ref maps
	stream->refs_adsr_time_scale = dist_adsr_time_scale.refs;
	stream->refs_wf_period = dist_wf_period.refs;
	stream->refs_wf_amplitude = dist_wf_amplitude.refs;
	stream->refs_adsr_release_start = dist_adsr_release_start.refs;

	// Check limitation of uncompress algo
	if (dist_adsr_time_scale.refs.bit_count > 8 || 
		dist_wf_period.refs.bit_count > 8 || 
		dist_wf_amplitude.refs.bit_count > 8 || 
		dist_adsr_release_start.refs.bit_count > 8) {
		fprintf(stderr, "Field ref doesn't fit in 8 bit");
		return 1;
	}

	int bits_per_frame = dist_adsr_time_scale.refs.bit_count + dist_wf_period.refs.bit_count + dist_wf_amplitude.refs.bit_count + dist_adsr_release_start.refs.bit_count;
	// The last frame data will be all 0s
	stream->data_size = (int)ceil((frame_count + 1) * bits_per_frame / 8.0);
	printf("Stream size: %d bytes\n", stream->data_size);

	// +1 again for the write_bits rounding
	stream->data = malloc(stream->data_size + 1);
	memset(stream->data, 0, stream->data_size + 1);

	// Now compile down the bit stream
	struct stream_writer_t stream_writer;
	stream_writer.buffer = stream->data;
	stream_writer.pos = 0;
	stream_writer.bit_pos = 0;
	for (int i = 0; i < frame_count; i++) {
		write_bits(&stream_writer, dist_adsr_time_scale.map_of_refs[frame_stream[i].adsr_time_scale_1], dist_adsr_time_scale.refs.bit_count);
		write_bits(&stream_writer, dist_wf_period.map_of_refs[frame_stream[i].wf_period], dist_wf_period.refs.bit_count);
		write_bits(&stream_writer, dist_wf_amplitude.map_of_refs[frame_stream[i].wf_amplitude], dist_wf_amplitude.refs.bit_count);
		write_bits(&stream_writer, dist_adsr_release_start.map_of_refs[frame_stream[i].adsr_release_start], dist_adsr_release_start.refs.bit_count);
	}

	// EOF. The risk is that a valid note close to the stream end has all refs = 0. However this is only a filler to be discarded when the pointer reaches the end
	write_bits(&stream_writer, 0, dist_adsr_time_scale.refs.bit_count);
	write_bits(&stream_writer, 0, dist_wf_period.refs.bit_count);
	write_bits(&stream_writer, 0, dist_wf_amplitude.refs.bit_count);
	write_bits(&stream_writer, 0, dist_adsr_release_start.refs.bit_count);

	return 0;
}

void stream_free(struct bit_stream_t* stream) {
	free(stream->data);
	free(stream->refs_adsr_time_scale.values);
	free(stream->refs_wf_period.values);
	free(stream->refs_wf_amplitude.values);
	free(stream->refs_adsr_release_start.values);
}

