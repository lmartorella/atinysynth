/*!
 * Polyphonic synthesizer for microcontrollers.  PC port.
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

#include "synth.h"
#include "debug.h"
#include "sequencer.h"
#include "mml.h"
#include "codegen.h"
#include <stdio.h>
#include <string.h>
#include <ao/ao.h>

struct poly_synth_t synth;

static int16_t samples[8192];
static uint16_t samples_sz = 0;
static struct bit_stream_t bit_stream;
static int stream_pos;
static int stream_pos_bit;

/* Read and play a MML file */
static void mml_error(const char* err, int line, int column) {
	fprintf(stderr, "Error reading MML file: %s at line %d, pos %d\n", err, line, column);
}

static int process_mml(const char* name, int* voice_count) {
	FILE *fp = fopen(name, "r");
	if (!fp) {
		fprintf(stderr, "Error reading MML file: %s", name);
		return 1;
	}
	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
   	char* content = malloc(size + 1);
	fseek(fp, 0, SEEK_SET);
	fread(content, 1, size, fp);
	content[size] = 0;

	mml_set_error_handler(mml_error);
	int err;

	struct seq_frame_map_t map;
	err = mml_compile(content, &map);
	if (err) {
		return err;
	}
	free(content);
	int channel_count = map.channel_count;

	// Sort frames in stream
	int do_clip_check;
	struct seq_frame_t* seq_frame_stream;
	int current_frame;
	int frame_count;
	seq_compile(&map, &seq_frame_stream, &frame_count, voice_count, &do_clip_check);
	mml_free(&map);

	// Compress stream
	if (stream_compress(seq_frame_stream, frame_count, &bit_stream)) {
		return 1;
	}
	
	return codegen_write(name, &bit_stream, channel_count, do_clip_check);
}

static uint8_t read_bits(uint8_t bits) {
	if (bits) {
		uint16_t buffer = bit_stream.data[stream_pos] + (bit_stream.data[stream_pos + 1] << 8);
		buffer >>= stream_pos_bit;
		uint8_t ret = buffer & ((1 << bits) - 1);

		stream_pos_bit += bits;
		if (stream_pos_bit >= 8) {
			stream_pos_bit -= 8;
			stream_pos++;
		}

		return ret;
	} else {
		return 0;
	}
}

void new_frame_require() {
	uint8_t ref_adsr_time_scale = read_bits(bit_stream.refs_adsr_time_scale.bit_count);
	uint8_t ref_wf_period = read_bits(bit_stream.refs_wf_period.bit_count);
	uint8_t ref_wf_amplitude = read_bits(bit_stream.refs_wf_amplitude.bit_count);
	uint8_t ref_adsr_release_start = read_bits(bit_stream.refs_adsr_release_start.bit_count);

	if (stream_pos >= (bit_stream.data_size - 1) && !ref_adsr_time_scale && !ref_wf_period && !ref_wf_amplitude && !ref_adsr_release_start) {
		seq_buf_frame.adsr_time_scale_1 = 0;
	} else {
		seq_buf_frame.adsr_time_scale_1 = bit_stream.refs_adsr_time_scale.values[ref_adsr_time_scale];
		seq_buf_frame.wf_period = bit_stream.refs_wf_period.values[ref_wf_period];
		seq_buf_frame.wf_amplitude = bit_stream.refs_wf_amplitude.values[ref_wf_amplitude];
		seq_buf_frame.adsr_release_start = bit_stream.refs_adsr_release_start.values[ref_adsr_release_start];
	}
}

int main(int argc, char** argv) {
	int voice = 0;

	memset(synth.voice, 0, sizeof(synth.voice));

	ao_sample_format format;
	memset(&format, 0, sizeof(format));
	format.bits = 16;
	format.channels = 1;
	format.rate = synth_freq;
	format.byte_format = AO_FMT_NATIVE;

	ao_initialize();
	int wav_driver = ao_driver_id("wav");
	ao_device* wav_device = ao_open_file(
		wav_driver, "out.wav", 1, &format, NULL
	);

	if (!wav_device) {
		fprintf(stderr, "Failed to open WAV device\n");
		return 1;
	}

	ao_device* live_device = NULL;
	int live_driver = ao_default_driver_id();
	live_device = ao_open_live(live_driver, &format, NULL);
	if (!live_device) {
		printf("Live driver not available\n");
	}

	argc--;
	argv++;
	while (argc > 0) {
		/* Check for MML compilation only */
		if (!strcmp(argv[0], "compile-mml")) {
			const char* name = argv[1];
			argv++;
			argc--;
			_DPRINTF("compiling MML %s\n", name);
			
			int voice_count;
			if (process_mml(name, &voice_count)) {
				return 1;
			}

			_DPRINTF("playing sequencer stream\n");
			stream_pos = 0;
			stream_pos_bit = 0;

			seq_play_stream(voice_count);
		}
		argv++;
		argc--;

		/* Play out any remaining samples */
		while (!seq_end) {
			int16_t* sample_ptr = samples;
			uint16_t samples_remain = sizeof(samples) / sizeof(uint16_t);

			/* Fill the buffer as much as we can */
			while (!seq_end && samples_remain) {
				int16_t s = seq_feed_synth();
				*sample_ptr = s << 8;
				sample_ptr++;
				samples_sz++;
				samples_remain--;
			}
			ao_play(wav_device, (char*)samples, 2*samples_sz);

			if (live_device) {
				ao_play(
					live_device,
					(char*)samples, 2*samples_sz
				);
			}
			samples_sz = 0;
		}
	}

	ao_close(wav_device);
	if (live_device) {
		ao_close(live_device);
	}
	ao_shutdown();
	return 0;
}
