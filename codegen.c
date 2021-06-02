/*!
 * Tune compressor and code generator for Polyphonic synthesizer for microcontrollers.
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

#include "codegen.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

struct distribution16_t {
	int map[0x10000];
	int map_of_refs[0x10000];
	int refs_count;
    int* refs;
    int bit_count;
};

static struct distribution16_t dist_adsr_time_scale;
static struct distribution16_t dist_wf_period;
static struct distribution16_t dist_wf_amplitude;
static struct distribution16_t dist_adsr_release_start;

static void distribution_init(struct distribution16_t* dist) {
	memset(dist->map, 0, 0x10000 * sizeof(int));
	memset(dist->map_of_refs, 0xff, 0x10000 * sizeof(int));
	dist->refs_count = 0;
    dist->refs = 0;
}

static void distribution_add(struct distribution16_t* dist, uint16_t value) {
	if (!dist->map[value]) {
		dist->refs_count++;
	}
	dist->map[value]++;
}

static void distribution_calc(struct distribution16_t* dist) {
	dist->bit_count = ceil(log(dist->refs_count) / log(2));
	printf("%d (%d bits)\n", dist->refs_count, dist->bit_count);

    dist->refs = malloc(sizeof(int) * dist->refs_count);
    int j = 0;
    for (int i = 0; i < 0x10000; i++) {
        if (dist->map[i]) {
            dist->refs[j] = i;
            dist->map_of_refs[i] = j;
            j++;
        }
    }
}

static void distribution_codegen(FILE *file, const char* var_name, const char* var_type, struct distribution16_t* dist) {
    fprintf(file, "const %s %s[] = {\n\t", var_type, var_name);
    for (int i = 0; i < dist->refs_count; i++) {
        fprintf(file, "0x%x, ", dist->refs[i]);
    }
    fprintf(file, "\n};\n\n");
}

/*! Analyze the stream to extract the data ref tables */
static void codegen_compress_stream(struct seq_frame_t* frame_stream, int frame_count) {
	distribution_init(&dist_adsr_time_scale);
	distribution_init(&dist_wf_period);
	distribution_init(&dist_wf_amplitude);
	distribution_init(&dist_adsr_release_start);

	for (int i = 0; i < frame_count; i++) {
		struct seq_frame_t* frame = frame_stream + i;
		distribution_add(&dist_adsr_time_scale, frame->adsr_time_scale);
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
}

int codegen_write(const char* tune_name, struct seq_frame_t* frame_stream, int frame_count, int channel_count, int has_clip) {
    codegen_compress_stream(frame_stream, frame_count);

    // Prepare the header for tune_gen.h (with dynamic bit sizes)
	FILE *hSrc = fopen("tune_gen.h", "w");
	if (!hSrc) {
		fprintf(stderr, "Cannot write the tune_gen.h file\n");
		return 1;
	}

	fprintf(hSrc, "#include \"sequencer.h\"\n\n");
	fprintf(hSrc, "// Tune: %s\n\n", tune_name);

    fprintf(hSrc, "struct tune_frame_t {\n");
	if (dist_adsr_time_scale.bit_count) {
    	fprintf(hSrc, "\tuint8_t adsr_time_scale : %d;\n", dist_adsr_time_scale.bit_count);
	}
	if (dist_wf_period.bit_count) {
    	fprintf(hSrc, "\tuint8_t wf_period : %d;\n", dist_wf_period.bit_count);
	}
	if (dist_wf_amplitude.bit_count) {
    	fprintf(hSrc, "\tuint8_t wf_amplitude : %d;\n", dist_wf_amplitude.bit_count);
	}
	if (dist_adsr_release_start.bit_count) {
    	fprintf(hSrc, "\tuint8_t adsr_release_start : %d;\n", dist_adsr_release_start.bit_count);
	}
	fprintf(hSrc, "};\n\n");

    fprintf(hSrc, "extern const uint16_t tune_adsr_time_scale_refs[];\n");
    fprintf(hSrc, "extern const uint16_t tune_wf_period_refs[];\n");
    fprintf(hSrc, "extern const uint8_t tune_wf_amplitude_refs[];\n");
    fprintf(hSrc, "extern const uint8_t tune_adsr_release_start_refs[];\n");
    fprintf(hSrc, "extern const struct tune_frame_t tune_data[];\n\n");

	fprintf(hSrc, "#define TUNE_DATA_COUNT %d\n", frame_count);
	if (!has_clip) {
		fprintf(hSrc, "#define NO_CLIP_CHECK\n");
	}
	fprintf(hSrc, "#define SEQ_CHANNEL_COUNT %d\n\n", channel_count);

	printf("File tune_gen.h written\n");
	fclose(hSrc);

	// Save the compiled output to tune_gen.c (table sources)
	FILE *cSrc = fopen("tune_gen.c", "w");
	if (!cSrc) {
		fprintf(stderr, "Cannot write the tune_gen.c file\n");
		return 1;
	}
	fprintf(cSrc, "#include \"tune_gen.h\"\n\n");
	fprintf(cSrc, "// Tune: %s\n\n", tune_name);

    distribution_codegen(cSrc, "tune_adsr_time_scale_refs", "uint16_t", &dist_adsr_time_scale);
    distribution_codegen(cSrc, "tune_wf_period_refs", "uint16_t", &dist_wf_period);
    distribution_codegen(cSrc, "tune_wf_amplitude_refs", "uint8_t", &dist_wf_amplitude);
    distribution_codegen(cSrc, "tune_adsr_release_start_refs", "uint8_t", &dist_adsr_release_start);

    fprintf(cSrc, "const struct tune_frame_t tune_data[TUNE_DATA_COUNT] = {\n");
	for (int i = 0; i < frame_count; i++) {
		fprintf(cSrc, "\t{ ");
		if (dist_adsr_time_scale.bit_count) {
			fprintf(cSrc, "%d, ", dist_adsr_time_scale.map_of_refs[frame_stream[i].adsr_time_scale]);
		}
		if (dist_wf_period.bit_count) {
            fprintf(cSrc, "%d, ", dist_wf_period.map_of_refs[frame_stream[i].wf_period]);
		}
		if (dist_wf_amplitude.bit_count) {
            fprintf(cSrc, "%d, ", dist_wf_amplitude.map_of_refs[frame_stream[i].wf_amplitude]);
		}
		if (dist_adsr_release_start.bit_count) {
            fprintf(cSrc, "%d, ", dist_adsr_release_start.map_of_refs[frame_stream[i].adsr_release_start]);
		}
		fprintf(cSrc, " },\n");
	}

	fprintf(cSrc, "};\n\n");
	printf("File tune_gen.c written\n");
	fclose(cSrc);

	return 0;
}
