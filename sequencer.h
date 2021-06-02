/*!
 * Polyphonic synthesizer for microcontrollers.
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
#ifndef _SEQUENCER_H
#define _SEQUENCER_H

#include <stdint.h>

/*! 
 * Define a single step/frame of the sequencer. It applies to the active channel.
 * Contains the definition of the next waveform and envelope.
 */
struct seq_frame_t {
    /*! ADSR time-scale - 1 (avoid a 16bit decrement at runtime) */
    uint16_t adsr_time_scale_1;
    /*! Waveform full period as `sample_freq / frequency`, or zero for pauses */
    uint16_t wf_period;
    /*! Waveform amplitude */
    uint8_t wf_amplitude;
    /*! When the release period starts, time units over the ADSR_TIME_UNITS scale */
    uint8_t adsr_release_start;
};

/*! 
 * Plays a stream sequence of frames, in the order requested by the synth.
 * The frames must then be sorted in the same fetch order and not in channel order.
 * Frames will be fed using the handler passed by `new_frame_require`.
 */
#ifndef SEQ_CHANNEL_COUNT
void seq_play_stream(uint8_t voices);
#else
void seq_play_stream();
#endif

/*! Requires a new frame. The call never fails. Returns a zero frame at the end of the stream, or if EOF */
extern struct seq_frame_t seq_buf_frame;

/*! Requires a new frame to be written in `seq_buf_frame`. The call never fails. */
void new_frame_require();

/*! Use it when `seq_play_stream` is in use, must be called at every sample */
void seq_feed_synth();

/*! List of frames, used by `seq_frame_map_t` */
struct seq_frame_list_t {
	/*! Frame count */
	int count;
	/*! List of frames */
	struct seq_frame_t* frames;
};

/*! 
 * A frame map is an intermediate data in which frames are organized by channel.
 * Typically requires dynamic memory allocation (heap) to allocate the whole tune
 * upfront.
 */
struct seq_frame_map_t {
	/*! Channel count */
	int channel_count;
	/*! Array of frame lists */
	struct seq_frame_list_t* channels;
}; 

/*! Compile/reorder a frame-map (by channel) to a sequential stream */
void seq_compile(struct seq_frame_map_t* map, struct seq_frame_t** frame_stream, int* frame_count, int* voice_count, int* do_clip_check);

/*! Free the stream allocated by `seq_compile`. */
void seq_free(struct seq_frame_t* seq_frame_stream);

#endif
