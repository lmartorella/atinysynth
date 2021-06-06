/*!
 * Bit-stream C code generator for Polyphonic synthesizer for microcontrollers.
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

static void distribution_codegen(FILE *file, const char* var_name, const char* var_type, struct ref_map_t* refs) {
    fprintf(file, "const %s %s[] = {\n\t", var_type, var_name);
    for (int i = 0; i < refs->count; i++) {
        fprintf(file, "0x%x, ", refs->values[i]);
    }
    fprintf(file, "\n};\n\n");
}

int codegen_write(const char* tune_name, struct bit_stream_t* stream, int channel_count, int has_clip) {
    // Prepare the header for tune_gen.h (with dynamic bit sizes)
	FILE *hSrc = fopen("tune_gen.h", "w");
	if (!hSrc) {
		fprintf(stderr, "Cannot write the tune_gen.h file\n");
		return 1;
	}

	fprintf(hSrc, "#include \"sequencer.h\"\n\n");

	fprintf(hSrc, "// Auto-generated code. Don't modify\n");
	fprintf(hSrc, "// Tune: %s\n\n", tune_name);

	fprintf(hSrc, "#define BITS_ADSR_TIME_SCALE %d\n", stream->refs_adsr_time_scale.bit_count);
	fprintf(hSrc, "#define BITS_WF_PERIOD %d\n", stream->refs_wf_period.bit_count);
	fprintf(hSrc, "#define BITS_WF_AMPLITUDE %d\n", stream->refs_wf_amplitude.bit_count);
	fprintf(hSrc, "#define BITS_ADSR_RELEASE_START %d\n\n", stream->refs_adsr_release_start.bit_count);

	fprintf(hSrc, "#define TUNE_DATA_SIZE %d\n", stream->data_size);
	if (!has_clip) {
		fprintf(hSrc, "#define NO_CLIP_CHECK\n");
	}
	fprintf(hSrc, "#define SEQ_CHANNEL_COUNT %d\n\n", channel_count);

    fprintf(hSrc, "extern const uint16_t tune_adsr_time_scale_refs[];\n");
    fprintf(hSrc, "extern const uint16_t tune_wf_period_refs[];\n");
    fprintf(hSrc, "extern const int8_t tune_wf_amplitude_refs[];\n");
    fprintf(hSrc, "extern const uint8_t tune_adsr_release_start_refs[];\n");
    fprintf(hSrc, "extern const uint8_t tune_data[TUNE_DATA_SIZE];\n\n");

	printf("File tune_gen.h written\n");
	fclose(hSrc);

	// Save the compiled output to tune_gen.c (table sources)
	FILE *cSrc = fopen("tune_gen.c", "w");
	if (!cSrc) {
		fprintf(stderr, "Cannot write the tune_gen.c file\n");
		return 1;
	}
	fprintf(cSrc, "#include \"tune_gen.h\"\n\n");

	fprintf(hSrc, "// Auto-generated code. Don't modify\n");
	fprintf(cSrc, "// Tune: %s\n\n", tune_name);

    distribution_codegen(cSrc, "tune_adsr_time_scale_refs", "uint16_t", &stream->refs_adsr_time_scale);
    distribution_codegen(cSrc, "tune_wf_period_refs", "uint16_t", &stream->refs_wf_period);
    distribution_codegen(cSrc, "tune_wf_amplitude_refs", "int8_t", &stream->refs_wf_amplitude);
    distribution_codegen(cSrc, "tune_adsr_release_start_refs", "uint8_t", &stream->refs_adsr_release_start);

    fprintf(cSrc, "const uint8_t tune_data[TUNE_DATA_SIZE] = {\n\t");
	for (int i = 0; i < stream->data_size; i++) {
		fprintf(cSrc, "0x%x, ", stream->data[i]);
		if ((i % 16) == 15) {
			fprintf(cSrc, "\n\t");
		}
	}

	fprintf(cSrc, "\n};\n\n");
	printf("File tune_gen.c written\n");
	fclose(cSrc);

	return 0;
}
