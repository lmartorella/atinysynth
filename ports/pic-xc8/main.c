/*
 * File:   main.c
 * Author: lucia
 *
 * Created on 2 maggio 2021, 12.01
 */
#include <xc.h>
#include <stdlib.h>
#include <string.h>
#include "synth.h"
#include "sequencer.h"

static struct poly_synth_t synth;

static const struct seq_stream_header_t seq_stream_header = { SYNTH_FREQ, sizeof(struct seq_frame_t), 2, 100 };
static const struct seq_frame_t seq_stream_data[] = {
    { { 100, 1, 1}, { 12, 100 } },
    { { 200, 1, 1}, { 12, 100 } },
    { { 300, 1, 1}, { 12, 100 } }
};

static uint16_t i = 0;

uint8_t new_frame_require(struct seq_frame_t* frame) {
    memcpy(frame, seq_stream_data + i, sizeof(struct seq_frame_t));
    return ++i < 4;
}

void main(void) {
    seq_play_stream(&seq_stream_header, VOICE_COUNT, &synth);
    
    seq_feed_synth(&synth);

    while (synth.enable) {
        _DPRINTF("enable = 0x%lx\n", synth.enable);
        poly_synth_next(&synth);
        seq_feed_synth(&synth);
    }
}
