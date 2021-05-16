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
static FILE* seq_stream;
static struct seq_stream_header_t seq_stream_header;

/*! Read a script instead of command-line tokens */
static int read_script(const char* name, int* argc, char*** argv) {
	FILE *fp = fopen(name, "r");
	if (!fp) {
		fprintf(stderr, "Failed to open script file: %s\n", name);
		return 0;
	}
	char token[64];
	int n = 0;
	int size = 16;
	char** list = malloc(size * sizeof(char*));
	while (fscanf(fp, "%63s", token) == 1) {
		list[n++] = strdup(token);
		if (n >= size) {
			size += 16;
			list = realloc(list, size * sizeof(char*));
		}
	}

	*argv = list;
	*argc = n;
	return 1;
}

/* Read and play a MML file */
static void mml_error(const char* err, int line, int column) {
	fprintf(stderr, "Error reading MML file: %s at line %d, pos %d\n", err, line, column);
}

static int open_mml(const char* name) {
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
	struct seq_frame_t* frame_stream;
	int frame_count;
	int voice_count;
	seq_compile(&map, &frame_stream, &frame_count, &voice_count);
	mml_free(&map);

	// Save the compiled output to sequencer.bin (binary compact format)
	FILE *bin = fopen("sequencer.bin", "wb");
	if (!bin) {
		fprintf(stderr, "Cannot write the sequencer.bin file\n");
		return 1;
	}
	seq_stream_header.frame_size = sizeof(struct seq_frame_t);
	seq_stream_header.synth_frequency = synth_freq;
	seq_stream_header.voices = voice_count;
	fwrite(&seq_stream_header, 1, sizeof(struct seq_stream_header_t), bin);
	fwrite(frame_stream, frame_count, sizeof(struct seq_frame_t), bin);
	_DPRINTF("File sequencer.bin written\n");
	fclose(bin);

	// Save the compiled output to sequencer_inc.c (C source)
	FILE *cSrc = fopen("sequencer_in.c", "w");
	if (!cSrc) {
		fprintf(stderr, "Cannot write the sequencer_in.c file\n");
		return 1;
	}
	fprintf(cSrc, "// %s\n", name);
	fprintf(cSrc, "const struct seq_stream_header_t tune_header = { %d, %d, %d };\n", synth_freq, sizeof(struct seq_frame_t), voice_count);
	fprintf(cSrc, "const struct seq_frame_t tune_data[] = {\n");
	for (int i = 0; i < frame_count; i++) {
		fprintf(cSrc, "\t{ { %d, %d }, { %d, %d }},\n", 
			frame_stream[i].adsr_def.time_scale, 
			frame_stream[i].adsr_def.release_start,
			frame_stream[i].waveform_def.amplitude,
			frame_stream[i].waveform_def.period);
	}
	fprintf(cSrc, "};\n");
	_DPRINTF("File sequencer_in.c written\n");
	fclose(cSrc);

	seq_free(frame_stream);
	return err;
}

void new_frame_require(struct seq_frame_t* frame) {
	if (fread(frame, 1, sizeof(struct seq_frame_t), seq_stream) != sizeof(struct seq_frame_t)) {
		memset(frame, 0, sizeof(struct seq_frame_t));
	}
}

static int open_seq(const char* name) {
	seq_stream = fopen(name, "rb");
	if (!seq_stream) {
		fprintf(stderr, "Error reading sequencer file: %s", name);
		return 1;
	}

	fread(&seq_stream_header, 1, sizeof(struct seq_stream_header_t), seq_stream);

	int err = seq_play_stream(&seq_stream_header);
	feed_channels = seq_feed_synth;
	return err;
}

int main(int argc, char** argv) {
	int voice = 0;

	synth.enable = 0;
#ifdef SUPPORT_MUTE
	synth.mute = 0;
#endif

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
		if (!strcmp(argv[0], "end"))
			break;

		/* Check for external script */
		if (!strcmp(argv[0], "--")) {
			const char* name = argv[1];
			_DPRINTF("reading script %s\n", name);
			if (!read_script(name, &argc, &argv)) {
				return 1;
			}
			// Fake item will be skipped at the end of the if
			argc++;
			argv--;

		/* Check for MML compilation only */
		} else if (!strcmp(argv[0], "compile-mml")) {
			const char* name = argv[1];
			_DPRINTF("compiling MML %s\n", name);
			
			return open_mml(name);

		/* Check for sequencer file play */
		} else if (!strcmp(argv[0], "sequencer")) {
			const char* name = argv[1];
			_DPRINTF("playing sequencer file %s\n", name);
			
			int err = open_seq(name);
			if (err) {
				return err;
			}

			argv++;
			argc--;

		/* Voice selection */
		} else if (!strcmp(argv[0], "voice")) {
			voice = atoi(argv[1]);
			_DPRINTF("select voice %d\n", voice);
			argv++;
			argc--;

#ifdef SUPPORT_MUTE
		/* Voice channel muting */
		} else if (!strcmp(argv[0], "mute")) {
			int mute = atoi(argv[1]);
			_DPRINTF("mute mask 0x%02x\n", mute);
			synth.mute = mute;
			argv++;
			argc--;
#endif

		/* Voice channel enable */
		} else if (!strcmp(argv[0], "en")) {
			int en = atoi(argv[1]);
			_DPRINTF("enable mask 0x%02x\n", en);
			synth.enable = en;
			argv++;
			argc--;

#ifdef USE_DC
		/* Voice waveform mode selection */
		} else if (!strcmp(argv[0], "dc")) {
			int amp = atoi(argv[1]);
			_DPRINTF("channel %d mode DC amp=%d\n",
					voice, amp);
			voice_wf_set_dc(&poly_voice[voice].wf, amp);
			argv++;
			argc--;
#endif
#ifdef USE_NOISE
		} else if (!strcmp(argv[0], "noise")) {
			int amp = atoi(argv[1]);
			_DPRINTF("channel %d mode NOISE amp=%d\n",
					voice, amp);
			voice_wf_set_noise(&poly_voice[voice].wf, amp);
			argv++;
			argc--;
#endif
		} else if (!strcmp(argv[0], "square")) {
			int freq = atoi(argv[1]);
			int amp = atoi(argv[2]);
			_DPRINTF("channel %d mode SQUARE freq=%d amp=%d\n",
					voice, freq, amp);
			voice_wf_set_square(&synth.voice[voice].wf,
					freq, amp);
			argv += 2;
			argc -= 2;
#ifdef USE_SAWTOOTH
		} else if (!strcmp(argv[0], "sawtooth")) {
			int freq = atoi(argv[1]);
			int amp = atoi(argv[2]);
			_DPRINTF("channel %d mode SAWTOOTH freq=%d amp=%d\n",
					voice, freq, amp);
			voice_wf_set_sawtooth(&poly_voice[voice].wf,
					freq, amp);
			argv += 2;
			argc -= 2;
#endif
#ifdef USE_TRIANGLE
		} else if (!strcmp(argv[0], "triangle")) {
			int freq = atoi(argv[1]);
			int amp = atoi(argv[2]);
			_DPRINTF("channel %d mode TRIANGLE freq=%d amp=%d\n",
					voice, freq, amp);
			voice_wf_set_triangle(&poly_voice[voice].wf,
					freq, amp);
			argv += 2;
			argc -= 2;
#endif

		/* ADSR options */
		} else if (!strcmp(argv[0], "scale")) {
			int scale = atoi(argv[1]);
			_DPRINTF("channel %d ADSR scale %d samples\n",
					voice, scale);
			synth.voice[voice].adsr.def.time_scale = scale;
			argv++;
			argc--;
// #ifndef ADSR_FIXED_DELAY
// 		} else if (!strcmp(argv[0], "delay")) {
// 			int time = atoi(argv[1]);
// 			_DPRINTF("channel %d ADSR delay %d units\n",
// 					voice, time);
// 			poly_voice[voice].adsr.def.delay_time = time;
// 			argv++;
// 			argc--;
// #endif
// #ifndef ADSR_FIXED_ATTACK
// 		} else if (!strcmp(argv[0], "attack")) {
// 			int time = atoi(argv[1]);
// 			_DPRINTF("channel %d ADSR attack %d units\n",
// 					voice, time);
// 			poly_voice[voice].adsr.def.attack_time = time;
// 			argv++;
// 			argc--;
// #endif
// #ifndef ADSR_FIXED_DECAY
// 		} else if (!strcmp(argv[0], "decay")) {
// 			int time = atoi(argv[1]);
// 			_DPRINTF("channel %d ADSR decay %d units\n",
// 					voice, time);
// 			poly_voice[voice].adsr.def.decay_time = time;
// 			argv++;
// 			argc--;
// #endif
		} else if (!strcmp(argv[0], "sustain")) {
			int time = atoi(argv[1]);
			_DPRINTF("channel %d ADSR sustain %d units\n",
					voice, time);
			synth.voice[voice].adsr.def.release_start = ADSR_STATE_SUSTAIN_START + time;
			argv++;
			argc--;
		// } else if (!strcmp(argv[0], "release")) {
		// 	int time = atoi(argv[1]);
		// 	_DPRINTF("channel %d ADSR release %d units\n",
		// 			voice, time);
		// 	synth.voice[voice].adsr.def.release_time = time;
		// 	argv++;
		// 	argc--;
#ifndef ADSR_FIXED_PEAK_AMP
		} else if (!strcmp(argv[0], "peak")) {
			int amp = atoi(argv[1]);
			_DPRINTF("channel %d ADSR peak amplitude %d\n",
					voice, amp);
			poly_voice[voice].adsr.def.peak_amp = amp;
			argv++;
			argc--;
#endif
#ifndef ADSR_FIXED_SUSTAIN_AMP
		} else if (!strcmp(argv[0], "samp")) {
			int amp = atoi(argv[1]);
			_DPRINTF("channel %d ADSR sustain amplitude %d\n",
					voice, amp);
			poly_voice[voice].adsr.def.sustain_amp = amp;
			argv++;
			argc--;
#endif
		} else if (!strcmp(argv[0], "reset")) {
			_DPRINTF("channel %d reset\n", voice);
			adsr_reset(&synth.voice[voice].adsr);
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
