# Fusion - A Sega CD Mode 1 PCM driver - SGDK implementation

## Description

This is a branch of the driver created by Vic and Chilly Willy but implemented to work with the SGDK.

The driver supports simultaneous playback of up to 8 mono PCM streams using the Ricoh RF5C164 chip on the Sega CD. The samples can be uploaded from cartridge ROM or CD to SegaCD program RAM and playback can be controlled by the main Sega Genesis/MegaDrive CPU.

The supported formats for sound samples are: WAV IMA ADPCM, SPCM and raw 8-bit PCM.

The demo project that comes with the driver showcases an example of how the driver can be used to start and control playback of multiple PCM streams. The code is based on the SEGA CD Mode 1 CD Player by Chilly Willy.

A couple of important notes:
* None ADPCM/Raw 8-bit PCM samples must be under 128KiB 
* The total amount of memory reserved for sound samples is around 460KiB
* For ADPCM, only mono samples supported
* Stereo samples require 2 hardware channels
* IMA ADPCM decoding is taxing on the Sub-CPU, so realistically up to 7 IMA ADPCM streams can be played back simultaneously without degradation
* The driver also includes CDDA music support

## Notes on IMA ADPCM
You can create IMA ADPCM files using adpcm-xq: https://github.com/dbry/adpcm-xq

1. Install Requirements
Ensure you have Visual Studio Community and the C++ Compile tools installed.

2. Clone the Repository:
`git clone https://github.com/dbry/adpcm-xq.git`

3. Compile adpcm-xq
Open a Visual Studio Command Prompt, navigate to the cloned directory, and run:
`cl /O2 adpcm-xq.c adpcm-lib.c adpcm-dns.c`

4. Convert WAV to IMA ADPCM
Use the newly compiled adpcm-xq.exe to encode a 16-bit WAV file:
`ADPCM-XQ -16 -e input.wav output.wav`

## Notes on SPCM
You can create SPCM files using SoX and Vic's raw2spcm tool: https://github.com/viciious/d32xr/tree/experiment/raw2spcm

1. Convert WAV to RAW
Use SoX to convert your .wav file into a raw format:
`sox file_name.wav -r 27500 -c 2 -e unsigned -b 8 file_name.raw`

2. Convert RAW to SPCM
Next, use raw2spcm to generate the final SPCM file:
`raw2spcm file_name.raw file_name.pcm 27500 2`

## SGDK API for the Driver

```
// scd_init_pcm initializes the PCM driver
void scd_init_pcm(void);

/* SPCM Functions */
// scd_spcm_play_track starts spcm playback
void scd_spcm_play_track(const char *name, int repeat) SCD_CODE_ATTR;

// scd_spcm_stop_track stops spcm playback
void scd_spcm_stop_track() SCD_CODE_ATTR;

// scd_spcm_resume_track resumes spcm playback
void scd_spcm_resume_track(void) SCD_CODE_ATTR;

// scd_spcm_get_playback_status returns playback status mask for spcm
// if a source is active, it will have its bit set to 1 in the mask:
// bit 0 for source id 1, bit 1 for source id 2, etc
int scd_spcm_get_playback_status(void) SCD_CODE_ATTR;

/* CDDA Functions */
// scd_get_disc_info
long long int scd_get_disc_info(void);

// scd_cdda_get_track_info 
long long int scd_cdda_get_track_info(u16 track) SCD_CODE_ATTR;

// scd_cdda_set_volume
void scd_cdda_set_volume(u16 volume) SCD_CODE_ATTR;

// scd_cdda_play_track starts a cdda track
void scd_cdda_play_track(u16 track, u16 repeat) SCD_CODE_ATTR;

// scd_cdda_stop_track stops cdda playback
void scd_cdda_stop_track(void)SCD_CODE_ATTR;

// scd_cdda_toggle_pause toggles pause on and off during cdda playback
void scd_cdda_toggle_pause(void) SCD_CODE_ATTR;

/* WAV Functions */
// scd_src_load_file load wav file from CD into word RAM with an allocated sfx id
void scd_src_load_file(const char *filename, int sfx_id) SCD_CODE_ATTR;

// scd_src_play starts playback on source from the start of the buffer
// source is a virtual playback channel, one or two hardware channels can be mapped
// to a single source
//
// value range for src_id: [1, 8] and a special value of 255, which allocates a new free source id
// value range for buf_id: [1, 256]
// values for freq: [0, 32767] value of 0 means "use frequency derived from the WAVE file"
// values for pan: [0, 255] value of 255 disables panning, 0 is full left, 128 is center, and 254 is full right
// values for vol: [0, 255]
// values for autoloop: [0, 255], a boolean: the source will automatically loopf from the start after
// reaching the end of the playback buffer
//
// returned value: if the passed src_id is 255, then thew newly allocated source id,
// if the allocation has failed, a value of 0 is returned
// otherwise the originally passed value of src_id is returned
u8 scd_src_play(u8 src_id, u16 buf_id, u16 freq, u8 pan, u8 vol, u8 autoloop) SCD_CODE_ATTR;

// scd_src_stop stops playback on the given source
//
// value range for src_id: [1, 8]
void scd_src_stop(u8 src_id) SCD_CODE_ATTR;

// scd_src_toggle_pause pauses or unpauses the source
//
// value range for src_id: [1, 8]
u8 scd_src_toggle_pause(u8 src_id, u8 paused) SCD_CODE_ATTR;

// scd_src_rewind sets position for the given source to the start of the playback buffer
//
// value range for src_id: [1, 8]
void scd_src_rewind(u8 src_id) SCD_CODE_ATTR;

// scd_src_update updates the frequency, panning, volume and autoloop property for the source
//
// value range for src_id: [1, 8] and a special value of 255, which allocates a new free source id
// values for freq: [0, 32767] value of 0 means "use frequency derived from the WAVE file"
// values for pan: [0, 255] value of 255 disables panning, 0 is full left, 128 is center, and 254 is full right
// values for vol: [0, 255]
// values for autoloop: [0, 255], a boolean: the source will automatically loopf from the start after
// reaching the end of the playback buffer
void scd_src_update(u8 src_id, u16 freq, u8 pan, u8 vol, u8 autoloop) SCD_CODE_ATTR;

// scd_src_get_pos returns playback position for the given source
//
// value range for src_id: [1, 8]
//
// returned value: current read position in PCM memory of the ricoh chip for the first channel of the source
u16 scd_src_get_pos(u8 src_id) SCD_CODE_ATTR;

```

## SGDK Adaption
* Programming : Matt Bennion & Victor Luchits

## Orginal Credits
* Programming : Victor Luchits
* Testing: Barone & Chilly Willy

## "SEGA CD Mode 1 CD Player" Credits
* Programming : Chilly Willy

## Original IMA ADPCM 68000 assembler
* Programming : Mikael Kalms

## Links
https://github.com/Kalmalyzer/adpcm-68k

## License
All original code is available under the MIT license.
