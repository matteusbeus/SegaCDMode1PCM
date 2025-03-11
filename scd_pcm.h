#include <genesis.h>

#ifdef USE_SCD_IN_RAM
#define SCD_CODE_ATTR __attribute__((section(".data"), aligned(16)))
#else
#define SCD_CODE_ATTR
#endif

// scd_init_pcm initializes the PCM driver
void scd_init_pcm(void);

// scd_upload_buf copies data to word RAM and sends a request to the SegaCD
// to copy it to an internal buffer in program RAM
//
// value range for buf_id: [1, 256]
// the sample must be under 128KiB due to word RAM limitations in 1M mode
// the passed data can be a WAV file, for which unsigned 8-bit PCM, IMA ADPCM (codec id: 0x11) 
// or SB4 ADPCM (codec id: 0x0200) formats are supported, otherwise raw unsigned 8-bit PCM 
// data is assumed
//
// replacing data in a previously initialized buffer of sufficient size is supported
// otherwise a new memory block will be allocated from the available memory pool
// once the driver runs out of memory, no further allocations will be possible and
// the driver will have to be re-initialized by calling scd_init_pcm 
void scd_upload_buf(u16 buf_id, const u8 *data, u32 data_len) SCD_CODE_ATTR;

// scd_upload_buf starts playback on source from the start of the buffer
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
u8 scd_play_src(u8 src_id, u16 buf_id, u16 freq, u8 pan, u8 vol, u8 autoloop) SCD_CODE_ATTR;

// scd_punpause_src pauses or unpauses the source
//
// value range for src_id: [1, 8]
u8 scd_punpause_src(u8 src_id, u8 paused) SCD_CODE_ATTR;

// scd_update_src updates the frequency, panning, volume and autoloop property for the source
//
// value range for src_id: [1, 8] and a special value of 255, which allocates a new free source id
// values for freq: [0, 32767] value of 0 means "use frequency derived from the WAVE file"
// values for pan: [0, 255] value of 255 disables panning, 0 is full left, 128 is center, and 254 is full right
// values for vol: [0, 255]
// values for autoloop: [0, 255], a boolean: the source will automatically loopf from the start after
// reaching the end of the playback buffer
void scd_update_src(u8 src_id, u16 freq, u8 pan, u8 vol, u8 autoloop) SCD_CODE_ATTR;

// scd_stop_src stops playback on the given source
//
// value range for src_id: [1, 8]
void scd_stop_src(u8 src_id) SCD_CODE_ATTR;

// scd_rewind_src sets position for the given source to the start of the playback buffer
//
// value range for src_id: [1, 8]
void scd_rewind_src(u8 src_id) SCD_CODE_ATTR;

// scd_getpos_for_src returns playback position for the given source
//
// value range for src_id: [1, 8]
//
// returned value: current read position in PCM memory of the ricoh chip for the first channel of the source
u16 scd_getpos_for_src(u8 src_id) SCD_CODE_ATTR;

// scd_clear_pcm stops playback on all channels
void scd_clear_pcm(void) SCD_CODE_ATTR;

// returns playback status mask for all sources
// if a source is active, it will have its bit set to 1 in the mask:
// bit 0 for source id 1, bit 1 for source id 2, etc
int scd_get_playback_status(void) SCD_CODE_ATTR;

// queues a scd_play_src call, always returns 0
u8 scd_queue_play_src(u8 src_id, u16 buf_id, u16 freq, u8 pan, u8 vol, u8 autoloop) SCD_CODE_ATTR;

// queues a scd_update_src call
void scd_queue_update_src(u8 src_id, u16 freq, u8 pan, u8 vol, u8 autoloop) SCD_CODE_ATTR;

// queues a scd_stop_src call
void scd_queue_stop_src(u8 src_id) SCD_CODE_ATTR;

// queues a scd_clear_pcm call
void scd_queue_clear_pcm(void) SCD_CODE_ATTR;

// flushes the command queue
int scd_flush_cmd_queue(void) SCD_CODE_ATTR;
