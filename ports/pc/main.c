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
#include <stdio.h>
#include <string.h>
#include <ao/ao.h>

struct poly_synth_t synth;

static int16_t samples[8192];
static uint16_t samples_sz = 0;
static void (*feed_channels)(struct poly_synth_t* synth) = NULL;
static struct seq_frame_t* seq_frame_stream;
static int current_frame;

/* Read and play a MML file */
static void mml_error(const char* err, int line, int column) {
	fprintf(stderr, "Error reading MML file: %s at line %d, pos %d\n", err, line, column);
}

static int write_source(const char* tune_name, int frame_count) {
	// Save the compiled output to tune_gen.s (ASM data)
	FILE *cSrc = fopen("tune_gen.s", "w");
	if (!cSrc) {
		fprintf(stderr, "Cannot write the tune_gen.s file\n");
		return 1;
	}
	fprintf(cSrc, "#include <xc.inc>\n\n");
	fprintf(cSrc, "// Tune: %s\n\n", tune_name);
	fprintf(cSrc, "psect\ttune, local, class=CODE, merge=1, delta=2\n");
	fprintf(cSrc, "global\t_tune_data\n\n");
	fprintf(cSrc, "_tune_data:\n");
	for (int i = 0; i < frame_count; i++) {
		fprintf(cSrc, "\tDW\t0x%x, 0x%x, 0x%x\n", 
			seq_frame_stream[i].wf_period, 
			seq_frame_stream[i].adsr_time_scale,
			(seq_frame_stream[i].wf_amplitude << 7) + seq_frame_stream[i].adsr_release_start);
	}
	fprintf(cSrc, "\n");
	printf("File tune_gen.s written\n");
	fclose(cSrc);
	return 0;
}

static int open_mml(const char* name, int* voice_count) {
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

	// Sort frames in stream
	int frame_count;
	seq_compile(&map, &seq_frame_stream, &frame_count, voice_count);
	mml_free(&map);
	
	return write_source(name, frame_count);
}

void new_frame_require(struct seq_frame_t* frame) {
	*frame = seq_frame_stream[current_frame++];
}

int main(int argc, char** argv) {
	int voice = 0;

	synth.enable = 0;

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
			if (open_mml(name, &voice_count)) {
				return 1;
			}

			_DPRINTF("playing sequencer stream\n");
			current_frame = 0;

			seq_play_stream(voice_count);
			feed_channels = seq_feed_synth;
		}
		argv++;
		argc--;

		/* Play out any remaining samples */
		if (feed_channels) {
			feed_channels(&synth);
		}

		if (synth.enable)
			_DPRINTF("----- Start playback (0x%lx) -----\n",
					synth.enable);

		while (synth.enable) {
			int16_t* sample_ptr = samples;
			uint16_t samples_remain = sizeof(samples)
						/ sizeof(uint16_t);

			/* Fill the buffer as much as we can */
			while (synth.enable && samples_remain) {
				_DPRINTF("enable = 0x%lx\n", synth.enable);
				int16_t s = poly_synth_next(&synth);
				*sample_ptr = s << 8;
				sample_ptr++;
				samples_sz++;
				samples_remain--;
				if (feed_channels) {
					feed_channels(&synth);
				}
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
