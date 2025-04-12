#include "scd_pcm.h"

typedef struct
{
    u32 cmd;
    u16 arg[6];
} scd_cmd_t;

#define MAX_SCD_CMDS    16

extern void write_byte(unsigned int dst, unsigned char val);
extern void write_word(unsigned int dst, unsigned short val);
extern void write_long(unsigned int dst, unsigned int val);
extern unsigned char read_byte(unsigned int src);
extern unsigned short read_word(unsigned int src);
extern unsigned int read_long(unsigned int src);
extern void *custom_memcpy(void *dest, const void *src, u32 n);

int mystrlen(const char* string);

static scd_cmd_t scd_cmds[MAX_SCD_CMDS];
static s16 num_scd_cmds;

static void scd_delay(void) SCD_CODE_ATTR;
static char wait_cmd_ack(void) SCD_CODE_ATTR;
static void wait_do_cmd(char cmd) SCD_CODE_ATTR;

/* Initialize Function */
void scd_init_pcm(void)
{
    /*
    * Initialize the PCM driver
    */
    wait_do_cmd('I');
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

/* Core SCD Functions */
static void scd_delay(void)
{
    int cnt = 5;
    do {
        asm __volatile("nop");
    } while (--cnt);
}

static char wait_cmd_ack(void)
{
    char ack = 0;

    do {
        scd_delay();
        ack = read_byte(0xA1200F); // wait for acknowledge byte in sub comm port
    } while (!ack);

    return ack;
}

static void wait_do_cmd(char cmd)
{
    while (read_byte(0xA1200F)) {
        scd_delay(); // wait until Sub-CPU is ready to receive command
    }
    write_byte(0xA1200E, cmd); // set main comm port to command
}

int mystrlen(const char* string)
{
	volatile int rc = 0;

	while (*(string++))
		rc++;

	return rc;
}

/* SPCM Functions */
void scd_spcm_play_track(const char *name, int repeat)
{
    char *scdWordRam = (char *)0x600000; /* word ram on MD side (in 1M mode) */
    custom_memcpy(scdWordRam, name, mystrlen(name)+1);
    write_long(0xA12010, 0x0C0000); /* word ram on CD side (in 1M mode) */
    write_long(0xA12014, repeat);
    wait_do_cmd('Q'); // PlaySPCMTrack command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_spcm_stop_track()
{
    wait_do_cmd('R'); // StopSPCMTrack command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_spcm_resume_track(void)
{
    wait_do_cmd('X'); // ResumeSPCMTrack command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

int scd_spcm_get_playback_status(void)
{
    return read_byte(0xA1202E);
}

/* CDDA Functions */
long long int scd_get_disc_info(void)
{
    union {
        s16 lo[4];
        long long int value;
    } res;

    wait_do_cmd('D'); // GetDiscInfo command
    wait_cmd_ack();
    res.lo[0] = read_word(0xA12020); // status
    res.lo[1] = read_word(0xA12022); // first and last song
    res.lo[2] = read_word(0xA12024); // drive version, flag 
    res.lo[3] = 0;
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result

    return res.value;
}

long long int scd_cdda_get_track_info(u16 track)
{
    union {
        s32 lo[2];
        long long int value;
    } res;

    write_word(0xA12010, track);
    wait_do_cmd('T'); // GetTrackInfo command
    wait_cmd_ack();
    res.lo[0] = read_long(0xA12020); // MMSSFFTN minutes|seconds|frames|track number|
    res.lo[1] = read_byte(0xA12024) & 0xff; // track type - DATA or CDDA (byte)
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result

    return res.value;
}

void scd_cdda_play_track(u16 track, u16 repeat)
{
    write_word(0xA12010, track);
    write_byte(0xA12012, repeat);
    wait_do_cmd('P'); // PlayTrack command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_cdda_stop_track(void)
{
    wait_do_cmd('S'); // StopPlaying command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_cdda_toggle_pause(void)
{
    wait_do_cmd('Z'); // PauseResume command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_cdda_set_volume(u16 volume)
{
    write_word(0xA12010, volume);
    wait_do_cmd('V'); // StopSPCMTrack command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

/* Other Functions */
long long int scd_open_file(const char *name)
{
    int i;
    char *scdfn = (char *)0x600000; /* word ram on MD side (in 1M mode) */
    union {
        s32 lo[2];
        long long int value;
    } handle;

    for (i = 0; name[i]; i++)
        *scdfn++ = name[i];
    *scdfn = 0;

    write_long(0xA12010, 0x0C0000); /* word ram on CD side (in 1M mode) */
    wait_do_cmd('F');
    wait_cmd_ack();
    handle.lo[0] = read_long(0xA12020);
    handle.lo[1] = read_long(0xA12024);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
    return handle.value;
}

void scd_upload_buf(u16 buf_id, const u8 *data, u32 data_len)
{
    u8 *scdWordRam = (u8 *)0x600000;
    custom_memcpy(scdWordRam, data, data_len);
    write_word(0xA12010, buf_id); /* buf_id */
    write_long(0xA12014, 0x0C0000); /* word ram on CD side (in 1M mode) */
    write_long(0xA12018, data_len); /* sample length */
    wait_do_cmd('B'); // SfxCopyBuffer command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_upload_buf_fileofs(u16 buf_id, int numsfx, const u8 *data)
{
    int filelen;
    char *scdWordRam = (char *)0x600000;

    // copy filename
    filelen = mystrlen((char *)data);
    custom_memcpy(scdWordRam, data, filelen+1);

    // copy offsets and lengths
    scdWordRam = (void *)(((u32)scdWordRam + filelen + 1 + 3) & ~3);
    data = (void *)(((u32)data + filelen + 1 + 3) & ~3);
    custom_memcpy(scdWordRam, data, numsfx*2*sizeof(int32_t));

    write_word(0xA12010, buf_id); /* buf_id */
    write_word(0xA12012, numsfx); /* num samples */
    write_long(0xA12014, 0x0C0000); /* word ram on CD side (in 1M mode) */
    wait_do_cmd('K'); // SfxCopyBuffer command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_src_load_file(const char *filename, int sfx_id)
{
    int l = scd_open_file(filename) >> 32;
    if (l < 0) {
        return;
    }

    char buf[1000], *ptr;
    int offsetlen[2];

    offsetlen[0] = 0;
    offsetlen[1] = l;

    custom_memcpy(buf, filename, strlen(filename)+1);
    ptr = (void*)(((u32)buf + strlen(filename) + 1 + 3) & ~3);
    custom_memcpy(ptr, offsetlen, sizeof(offsetlen));

    scd_upload_buf_fileofs(sfx_id, 1, (const u8 *)buf);
}

u8 scd_src_play(u8 src_id, u16 buf_id, u16 freq, u8 pan, u8 vol, u8 autoloop)
{
    write_long(0xA12010, ((unsigned)src_id<<16)|buf_id); /* src|buf_id */
    write_long(0xA12014, ((unsigned)freq<<16)|pan); /* freq|pan */
    write_long(0xA12018, ((unsigned)vol<<16)|autoloop); /* vol|autoloop */
    wait_do_cmd('A'); // SfxPlaySource command
    wait_cmd_ack();
    src_id = read_byte(0xA12020);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
    return src_id;
}

u8 scd_src_toggle_pause(u8 src_id, u8 paused)
{
    write_long(0xA12010, ((unsigned)src_id<<16)|paused); /* src|paused */
    wait_do_cmd('N'); // SfxPUnPSource command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
    return src_id;
}

void scd_src_update(u8 src_id, u16 freq, u8 pan, u8 vol, u8 autoloop)
{
    write_long(0xA12010, ((unsigned)src_id<<16)); /* src|0 */
    write_long(0xA12014, ((unsigned)freq<<16)|pan); /* freq|pan */
    write_long(0xA12018, ((unsigned)vol<<16)|autoloop); /* vol|autoloop */
    wait_do_cmd('U'); // SfxUpdateSource command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

u16 scd_src_get_pos(u8 src_id)
{
    u16 pos;
    write_long(0xA12010, src_id<<16);
    wait_do_cmd('G'); // SfxGetSourcePosition command
    wait_cmd_ack();
    pos = read_word(0xA12020);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
    return pos;
}

void scd_src_stop(u8 src_id)
{
    write_long(0xA12010, ((unsigned)src_id<<16)); /* src|0 */
    wait_do_cmd('O'); // SfxStopSource command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_src_rewind(u8 src_id)
{
    write_long(0xA12010, ((unsigned)src_id<<16)); /* src|0 */
    wait_do_cmd('W'); // SfxRewindSource command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_clear_pcm(void)
{
    wait_do_cmd('L'); // SfxClear command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

int scd_get_playback_status(void)
{
    return read_byte(0xA1202F);
}

/* Queue Functions */
u8 scd_queue_play_src(u8 src_id, u16 buf_id, u16 freq, u8 pan, u8 vol, u8 autoloop)
{
    scd_cmd_t *cmd = scd_cmds + num_scd_cmds;
    if (num_scd_cmds >= MAX_SCD_CMDS)
        return 0;
    cmd->cmd = 'A';
    cmd->arg[0] = src_id;
    cmd->arg[1] = buf_id;
    cmd->arg[2] = freq;
    cmd->arg[3] = pan;
    cmd->arg[4] = vol;
    cmd->arg[5] = autoloop;
    num_scd_cmds++;
    return 0;
}

void scd_queue_update_src(u8 src_id, u16 freq, u8 pan, u8 vol, u8 autoloop)
{
    scd_cmd_t *cmd = scd_cmds + num_scd_cmds;
    if (num_scd_cmds >= MAX_SCD_CMDS)
        return;
    cmd->cmd = 'U';
    cmd->arg[0] = src_id;
    cmd->arg[1] = freq;
    cmd->arg[2] = pan;
    cmd->arg[3] = vol;
    cmd->arg[4] = autoloop;
    num_scd_cmds++;
}

void scd_queue_stop_src(u8 src_id)
{
    scd_cmd_t *cmd = scd_cmds + num_scd_cmds;
    if (num_scd_cmds >= MAX_SCD_CMDS)
        return;
    cmd->cmd = 'S';
    cmd->arg[0] = src_id;
    num_scd_cmds++;
}

void scd_queue_clear_pcm(void)
{
    scd_cmd_t *cmd = scd_cmds + num_scd_cmds;
    if (num_scd_cmds >= MAX_SCD_CMDS)
        return;
    cmd->cmd = 'L';
    num_scd_cmds++;
}

int scd_flush_cmd_queue(void)
{
    int i;
    scd_cmd_t *cmd;

    if (!num_scd_cmds) {
        return 0;
    }

    write_byte(0xA12010, 1);
    wait_do_cmd('E'); // suspend the mixer/decoder
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result

    for (i = 0, cmd = scd_cmds; i < num_scd_cmds; i++, cmd++) {
        switch (cmd->cmd) {
            case 'A':
                scd_src_play(cmd->arg[0], cmd->arg[1], cmd->arg[2], cmd->arg[3], cmd->arg[4], cmd->arg[5]);
                break;
            case 'U':
                scd_src_update(cmd->arg[0], cmd->arg[1], cmd->arg[2], cmd->arg[3], cmd->arg[4]);
                break;
            case 'S':
                scd_src_stop(cmd->arg[0]);
                break;
            case 'L':
                scd_clear_pcm();
                break;
            default:
                break;
        }
    }

    write_byte(0xA12010, 0);
    wait_do_cmd('E'); // unsuspend
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result

    num_scd_cmds = 0;
    return i;
}

