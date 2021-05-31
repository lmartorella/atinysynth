ADSR-based Polyphonic Sequencer
=================================

This project is intended to be a polyphonic sequencer for use in
embedded 8-bit microcontrollers.  It features multi-voice synthesis for
multiple channels.

This is a fork of the [sjlongland/atinysynth](https://github.com/sjlongland/atinysynth/) project, optimized for the tiny PIC12F683 Microchip microcontroller.

The goal is to implement a polyphonic tune player in only 2K instructions (containing the tune data) and 128 bytes of RAM.

Yes. 

ADSR: principle of operation
----------------------

The ADSR is a cut version of the main [sjlongland/atinysynth](https://github.com/sjlongland/atinysynth/) project.

Instead of allowing variable ADSR parameters, this fork implement a quasi-fixed envelope function, using only bit shift operators.


Waveform: principle of operation
----------------------

The only waveform implemented in this fork is the square wave.

Refer the main [sjlongland/atinysynth](https://github.com/sjlongland/atinysynth/) project for principle of operation.


# Sequencer

Since the synthesizer state machine is effective in defining when a "note" envelope is terminated, it is then possible to store all the subsequent "notes" in a stream of consecutive *steps*. Each step contains a pair of waveform settings and ADSR settings. 

This allow polyphonic tunes to be "pre-compiled" and stored in small binary files, or microcontroller EEPROM, and to be accessed in serial fashion.

Each tune are stored in a way that each frame in the stream should feed the next available channel with the `enable` flag of the `struct poly_synth_t` structure reset.

In order to arrange the steps of all the channels in the correct sequence, a *sequencer compiler* has to be run on all the channel steps, and sort it correctly using an instance of the synth configured in the exact way of the target system (e.g. same sampling rate, same number of voices, etc...).

This compiler is not optimized to run on a microcontroller (it requires dynamic memory allocation), but to be run on a PC in order to obtain compact binary files to be played by the sequencer on the host MCU.

To save memory for the tiniest 8-bit microcontrollers, the sequencer stream header and the steps are defined in a compact 8-bit binary format:

```
// A frame
struct seq_frame_t {
    /*! Envelope definition */
    struct adsr_env_def_t adsr_def;
    /*! Waveform definition */
    struct voice_wf_def_t waveform_def;
};
```

where `adsr_env_def_t` is the argument for the `adsr_config`, and `voice_wf_def_t` is the minimum set of arguments to initialize a waveform.

In order to save computational-demanding 16-bit division operations on 8-bit targets, the waveform frequency in the definition is expressed as waveform period instead of frequency in Hz, to allow faster play at runtime.

This requires the sequencer compiler to known in advance the target sampling rate.

For this reason, a stream header contains the information to avoid issues during reproduction:

```
struct seq_stream_header_t {
    /*! Sampling frequency required for correct timing */
    uint16_t synth_frequency;
    /*! Size of a single frame in bytes */
    uint8_t frame_size;
    /*! Number of voices */
    uint8_t voices;
    /*! Total frame count */
    uint16_t frames;
    /*! Follow frames data, as stream of seq_frame_t */
};
```

The `frame_size` field is useful when the code in the target microcontroller is compiled with different setting (e.g. different time scale, or different set of features that requires less data, like no Attack/Decay, etc...).

### Typical usage

The sequencer can be fed via a callback, in order to support serial read for example from serial EEPROM or streams.

```c
/*! Requires a new frame. The handler must return 1 if a new frame was acquired, or zero if EOF */
void seq_set_stream_require_handler(uint8_t (*handler)(struct seq_frame_t* frame));

/*! 
 * Plays a stream sequence of frames, in the order requested by the synth.
 * The frames must then be sorted in the same fetch order and not in channel order.
 */
int seq_play_stream(const struct seq_stream_header_t* stream_header, uint8_t voice_count, struct poly_synth_t* synth);

/*! Use it when `seq_play_stream` is in use, one call per sample */
void seq_feed_synth(struct poly_synth_t* synth);
```

## MML compiler

A very common language to define tunes in a quasi-human-readable fashion is the [Music Macro Language](https://en.wikipedia.org/wiki/Music_Macro_Language) (MML).

The project contains an implementation of a MML parser that creates a sequencer stream. In that way, it is possible to 'compile' tunes into binary streams, embed it in the microcontroller and play it from the sequencer stream with the least as computational power as possible.

The MML dialect implemented supports multi-voice: each voice can be specified on a different line, prefixed with the voice number (from *A* to *Z*).

| command       | meaning  |
| ------------- |-------------|
| `cdefgab` | The letters `a` to `g` correspond to the musical pitches and cause the corresponding note to be played. Sharp notes are produced by appending a `+` or `#`, and flat notes by appending a `-`. The length of a note can be specified by appending a number representing its length (see `l` command). One or more dots `.` can be added to increase the length of 3/2. |
| `p` or `r` | A pause or rest. Like the notes, it is possible to specify the length appending a number and/or dots. | 
| `n`\<n> | Plays a *note code*, between 0 and 84. `0` is the C at octave 0, `33` is A at octave 2 (440Hz), etc... | 
| `o`\<n\> | Specify the octave the instrument will play in (from 0 to 6). The default octave is 2 (corresponding to the fourth-octave in scientific pitch).
| `<`, `>` | Used to step up or down one octave.
| `l`\<n\> | Specify the default length used by notes or rests which do not explicitly define one. `4` means 1/4, `16` means 1/16 etc... One or more dots `.` can be added to increase the length of 3/2.
| `v`\<n\> | Sets the volume of the instruments. It will set the current waveform amplitude (127 being the maximum modulation).
| `t`\<n\> | Sets the tempo in beats per minute.
| `mn`, `ml`, `ms` | Sets the articulation for the current instrument. Stands for *music normal* (note plays for 7/8 of the length), *music legato* (note plays full length) and *music staccato* (note plays 3/4 of length). This is implemented using the *decay* of ADSR modulation.
| `ws`, `ww`, `wt` (*) | Sets the square waveform, sawtooth waveform or triangle waveform for the current instrument.
| `\|` | The pipe character, used in music sheet notation to help aligning different channel, is ignored.
| `#`, `;` | Characters to denote comment lines: it will skip the rest of the line.
| `A-Z` (*) | Sets the active voice for the current MML line. Multiple characters can be specified: in that case all the selected voices will receive the MML commands until the end of the line.

(*) custom MML dialect.

The MML compiler is not optimized to run on a microcontroller (it requires dynamic memory allocation), but to be run on a PC in order to obtain the data to create a binary stream for the sequencer. The typical usage is a compiler for PC.

### Typical usage

The MML file should be loaded entirely in memory to be compiled. 

```c
// Set the error handler in order to show errors and line/col counts
mml_set_error_handler(stderr_err_handler);
struct seq_frame_map_t map;
// Parse the MML file and produce sequencer frames as stream.
if (mml_compile(mml_content, &map)) {
    // Error
}
// Compile the channel data map in a stream
struct seq_frame_t* frame_stream;
int frame_count;
int voice_count;
seq_compile(&map, &frame_stream, &frame_count, &voice_count);

// Save the frame stream...

// Free memory
mml_free(map);
seq_free(frame_stream);
```

Compiling
-----

### PIC12F683 port

You need the latest version of [MPLAB IDE](https://www.microchip.com/en-us/development-tools-tools-and-software/mplab-x-ide) to build for the Microchip MCU.

The project is optimized to be built even with the __free version__ of the [XC8 compiler](https://www.microchip.com/en-us/development-tools-tools-and-software/mplab-xc-compilers). Obviously using the PRO version you will be able to pack more tune data in the EEPROM.

The tune is stored in the rest of the program memory left, packed in the 14-bit wide words in order to squeeze the MCU to the max.

### PC port (`pc`)

This uses `libao` and a command line interface to simulate the output of
the synthesizer and to output a `.wav` file.  It was used to debug the synthesizer.

The PC port can be used to compile MML tunes to the sequencer binary format:

* `compile-mml FILE.mml` compiles the .mml file and produces a `sequencer.bin` output

and to play sequencer files as well:

* `sequencer FILE.bin` loads and plays the sequencer binary file passed as input.


