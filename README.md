ADSR-based Polyphonic Sequencer
=================================

This project is intended to be a polyphonic sequencer for use in
embedded 8-bit microcontrollers.  It features multi-voice synthesis for
multiple channels.

This is a fork of the [sjlongland/atinysynth](https://github.com/sjlongland/atinysynth/) project for AT AVR MCUs, but oriented and optimized for the tiny PIC12F683 Microchip microcontroller.

The goal is to implement a polyphonic tune player in only 2K instructions (containing the code *and* the tune data) and to stay in 128 bytes of RAM.

Yes. 

ADSR: principle of operation
----------------------

The ADSR is a cut version of the main [sjlongland/atinysynth](https://github.com/sjlongland/atinysynth/) project.

Instead of allowing variable ADSR parameters, this fork implement a quasi-fixed envelope function, using only bit shift operators.


Waveform: principle of operation
----------------------

The only waveform implemented in this fork is the square wave.

Refer the main [sjlongland/atinysynth](https://github.com/sjlongland/atinysynth/) project for principle of operation.

## Sequencer

Since the synthesizer state machine is effective in defining when a "note" envelope is terminated, it is then possible to store all the subsequent "notes" in a stream of consecutive *steps*. Each step contains a pair of waveform settings and ADSR settings. 

This allow polyphonic tunes to be "pre-compiled" and stored in small binary files, or microcontroller EEPROM, and to be accessed in serial fashion.

Each tune are stored in a way that each frame in the stream should feed the next available channel with the `enable` flag of the `struct poly_synth_t` structure reset.

In order to arrange the steps of all the channels in the correct sequence, a *sequencer compiler* has to be run on all the channel steps, and sort it correctly using an instance of the synth configured in the exact way of the target system (e.g. same sampling rate, same number of voices, etc...).

This compiler is not optimized to run on a microcontroller (it requires dynamic memory allocation), but to be run on a PC in order to obtain compact binary files to be played by the sequencer on the host MCU.

To save memory for the tiniest 8-bit microcontrollers, the sequencer stream header and the steps are defined in a compact binary format:

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

## Banning *mul*s

The free version of the XC8 compiler (like any other non-optimized compiler for CPUs without native addressing with displacement) would produce the `_bmul` software implementation when a for-loop is written in this manner:

```c
uint8_t i = 0;
for (; i < MAX; i++) { 
    // ....
    consume(synth.voice[i].adsr);
}
```

The multiplication is used to compute the displacement of the target voice, and it is especially slow if the size of the `struct voice_ch_t` is not a power of 2.

The same for-loop above written as:

```c
uint8_t i = 0;
struct voice_ch_t* ptr = &synth.voice[0];
for (; i < MAX; i++, ptr++) { 
    // ....
    consume(ptr->adsr);
}
```
is insanely faster (since it is implemented with simply additions and a temporary 8-bit pointer variable), and will save code space not producing the `_bmul` implementation.

Now the remaining integer multiplication is the original ADSR modulation:

```c
uint8_t amplitude = adsr_next(&(voice->adsr));
// Upscale...
int16_t value = voice_wf_next(&(voice->wf));
// 16-bit integer mul
value *= amplitude;
// and then division by 256
value >>= 8;
```

This above happens for each voice for each sample.

A clever way to optimize this is to follow the natural logarithmic scale of the human hearing, and use exponential gain instead of linear ones. With this approximation, the gain can be seen as a number of bit shifts:

```c
uint8_t gain = adsr_next(&(voice->adsr));
int8_t value = voice_wf_next(&(voice->wf));
// 8-bit domain again!
value >>= gain;
```

<img src="./doc/wave.png" alt="waveform" width="300px">

The approximation, so evident and ugly in the linear scale, is not actually so bad once played through a speaker.

## Bit compressor

After the sequencer produced the stream of frames, now we need to squeeze it even more to fit in the code EEPROM (2K words of 14 bits).

Unfortunately the base level PIC12 doesn't expose any instructions to access to code memory at runtime. So packing the stream in 14-bits words is not an option.

However, the XC8 compiler is great in implementing constant array of values in code memory, using the 'Computed GOTO' technique as described in the Microchip datasheet and hardcoded `RETLW` instructions.

With that, it is possible to put constant data packed as bytes in the code memory.

So the goal is to reduce the frame packet in - say - 2 bytes each, adding only a little bit more of computational required at runtime.

Analyizing the frame structure of a typical tune, we can say that the total _number_ of different notes and lengths is very less than the length of the whole tune.

So the compressor stage is simply mapping all the possible notes, lengths, and other frame definition values in tables that will be used to deferencing the real value.

The result is then produced _as direct C source for XC8_. Here an example:

```c
// Tune: resources/scale.mml

struct tune_frame_t {
	uint8_t adsr_time_scale : 2;
	uint8_t wf_period : 4;
	uint8_t wf_amplitude : 1;
	uint8_t adsr_release_start : 2;
};

const uint16_t tune_adsr_time_scale_refs[] = {
	0x0, 0x7d, 0x1f4, 
};

const uint16_t tune_wf_period_refs[] = {
	0x0, 0x1e9, 0x207, 0x225, 0x245, 0x268, 0x28e, 0x2b5, 0x2dd, 0x30a, 0x337, 0x369, 0x39c, 0x3d4, 
};

const uint8_t tune_wf_amplitude_refs[] = {
	0x0, 0x78, 
};

const uint8_t tune_adsr_release_start_refs[] = {
	0x0, 0x2f, 0x37, 0x3f, 
};

const struct tune_frame_t tune_data[] = {
	{ 1, 13, 1, 2 },
	{ 1, 12, 1, 2 },
	{ 1, 11, 1, 2 },
	{ 1, 10, 1, 2 },
	{ 1, 9, 1, 2 },
	{ 1, 8, 1, 2 },
	{ 1, 7, 1, 2 },
	{ 1, 6, 1, 2 },
	{ 1, 5, 1, 2 },
	{ 1, 4, 1, 2 },
	{ 1, 3, 1, 2 },
	{ 1, 2, 1, 2 },
	[...]
};
```

So the `scale.mml` example only uses two different amplitudes, plus the 0x0 used by pauses, so it requires 2 bits per frames.

The note periods require 4 bits, the ADSR envelope only 2 (only 3 different envelope used), etc..

The total size of a packet dropped now to 9 bits. Without any other complex bit-level stream manipulation, the XC8 compiler will layout the necessary code to push the tune data in EEPROM (alongside with the ref tables), requiring only 2 bytes per notes.

# Appendixes

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


