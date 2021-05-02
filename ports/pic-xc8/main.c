/*
 * File:   main.c
 * Author: lucia
 *
 * Created on 2 maggio 2021, 12.01
 */
#include <xc.h>
#include <stdlib.h>
#include "synth.h"
#include "sequencer.h"

static struct voice_ch_t poly_voice[1];
static struct poly_synth_t synth;

void main(void) {
    synth.voice = poly_voice;
    struct seq_stream_header_t seq_stream_header;
    seq_play_stream(&seq_stream_header, 4, &synth);

    seq_feed_synth(&synth);

    while (synth.enable) {
        _DPRINTF("enable = 0x%lx\n", synth.enable);
        poly_synth_next(&synth);
        seq_feed_synth(&synth);
    }
}
