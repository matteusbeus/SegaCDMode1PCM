/*
 * SEGA CD Mode 1 PCM Player
 * by Victor Luchits
 * and Chilly Willy
 * and Matt Bennion (Matteusbeus)
 */

#include <genesis.h>
#include "hw_md.h"
#include "scd_pcm.h"

extern u32 vblank_vector;
extern u32 gen_lvl2;
extern u32 Sub_Start;
extern u32 Sub_End;

extern volatile u32 gTicks;             /* incremented every vblank */

void clear_program_ram(void);
extern void Kos_Decomp(u8 *src, u8 *dst);
s32 memcmp(const void *s1, const void *s2, u32 n);

extern u8 stereo_test_u8_wav;
extern int stereo_test_u8_wav_len;
extern u8 macabre_ima_wav;
extern int macabre_ima_wav_len;

extern u8 macabre_sb4_wav;
extern int macabre_sb4_wav_len;

#define PROGRAM_RAM_ADDRESS  ((volatile u8 *)0x420000)
#define PROGRAM_RAM_SIZE     (0x20000)

void clear_program_ram(void) {
    for (u32 i = 0; i < PROGRAM_RAM_SIZE; i++) {
        PROGRAM_RAM_ADDRESS[i] = 0;
    }
}

s32 memcmp(const void *s1, const void *s2, u32 n)
{
    const u8 *p1 = s1, *p2 = s2;

    while (n--)
    {
        if (*p1 != *p2) return *p1 - *p2;
        else p1++, p2++;
    }

    return 0;
}

int main(void)
{
    u16 buttons = 0, previous = 0;
    u8 *bios;
    char text[44];
    u8 last_src = 0;
    s16 pan = 128, vol = 255;
    u8 src_paused[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
   
    /*
     * Check for CD BIOS
     * When a cart is inserted in the MD, the CD hardware is mapped to
     * 0x400000 instead of 0x000000. So the BIOS ROM is at 0x400000, the
     * Program RAM bank is at 0x420000, and the Word RAM is at 0x600000.
     */
    bios = (u8 *)0x415800;
    if (memcmp(bios + 0x6D, "SEGA", 4) == 0)  // Check if the BIOS starts with "SEGA"
    {
        bios = (u8 *)0x416000;
        if (memcmp(bios + 0x6D, "SEGA", 4) == 0)  // Check if the BIOS starts with "SEGA"
        {
            // Check for WonderMega/X'Eye
            if (memcmp(bios + 0x6D, "WONDER", 6) == 0)  // Check if the BIOS starts with "WONDER"
            {
                bios = (u8 *)0x41AD00; // Might also be 0x40D500
                // Check for LaserActive
                if (memcmp(bios + 0x6D, "SEGA", 4) == 0)  // Check if the BIOS starts with "SEGA"
                {
                    VDP_drawText("No CD detected!", 20-7, 12);
                    while (1);  // Infinite loop to halt execution
                }
            }
        }
    }
    sprintf(text, "CD Sub-CPU BIOS detected at 0x%lX", (u32)bios);
    VDP_setTextPalette(PAL2);
    VDP_drawText(text, 2, 2);

	/*
	 * Reset the Gate Array - this specific sequence of writes is recognized by
	 * the gate array as a reset sequence, clearing the entire internal state -
	 * this is needed for the LaserActive
	 */
	write_word(0xA12002, 0xFF00);
	write_byte(0xA12001, 0x03);
	write_byte(0xA12001, 0x02);
	write_byte(0xA12001, 0x00);

    /*
     * Reset the Sub-CPU, request the bus
     */
    write_byte(0xA12001, 0x02);
    while (!(read_byte(0xA12001) & 2)) write_byte(0xA12001, 0x02); // wait on bus acknowledge

    /*
    * Decompress Sub-CPU BIOS to Program RAM at 0x00000
    */ 
    VDP_drawText("Decompressing Sub-CPU BIOS", 2, 3);
    write_word(0xA12002, 0x0002); // no write-protection, bank 0, 2M mode, Word RAM assigned to Sub-CPU
    clear_program_ram(); // clear program ram first bank - needed for the LaserActive
    Kos_Decomp(bios, (u8 *)0x420000);

    /*
     * Copy Sub-CPU program to Program RAM at 0x06000
     */
    VDP_drawText("Copying Sub-CPU Program", 2, 4);
    memcpy((void *)0x426000, &Sub_Start, (int)&Sub_End - (int)&Sub_Start);
    if (memcmp((void *)0x426000, &Sub_Start, (int)&Sub_End - (int)&Sub_Start))
    {
        sprintf(text, "len %d!", (int)&Sub_End - (int)&Sub_Start);
        VDP_drawText("Failed writing Program RAM!", 20-13, 12);
        VDP_drawText(text, 20-13, 13);
        while (1);
    }

    write_byte(0xA1200E, 0x00); // clear main comm port
    write_byte(0xA12002, 0x2A); // write-protect up to 0x05400
    write_byte(0xA12001, 0x01); // clear bus request, deassert reset - allow CD Sub-CPU to run
    while (!(read_byte(0xA12001) & 1)) write_byte(0xA12001, 0x01); // wait on Sub-CPU running
    VDP_drawText("Sub-CPU started", 2, 5);
    
    /*
     * Set the vertical blank handler to generate Sub-CPU level 2 ints.
     * The Sub-CPU BIOS needs these in order to run.
     */
    write_long((uint32_t)&vblank_vector, (uint32_t)&gen_lvl2);
    //set_sr(0x2000); // enable interrupts
    SYS_enableInts();

    /*
     * Wait for Sub-CPU program to set sub comm port indicating it is running -
     * note that unless there's something wrong with the hardware, a timeout isn't
     * needed... just loop until the Sub-CPU program responds, but 2000000 is about
     * ten times what the LaserActive needs, and the LA is the slowest unit to
     * initialize
     */
    while (read_byte(0xA1200F) != 'I')
    {
        static int timeout = 0;
        timeout++;
        if (timeout > 2000000)
        {
            VDP_drawText("CD failed to start!", 20-9, 12);
            while (1) ;
        }
    }

    /*
     * Wait for Sub-CPU to indicate it is ready to receive commands
     */
    while (read_byte(0xA1200F) != 0x00) ;
    VDP_drawText("CD initialized and ready to go!", 20-15, 12);

    /*
    * Initialize the PCM driver
    */
    scd_init_pcm();

    VDP_drawText("Uploading samples...", 20-15, 13);

    /*
    * Upload test samples
    */
    scd_upload_buf(1, &macabre_ima_wav, macabre_ima_wav_len);
    scd_upload_buf(2, &macabre_sb4_wav, macabre_sb4_wav_len);
    scd_upload_buf(3, &stereo_test_u8_wav, stereo_test_u8_wav_len);

    VDP_resetScreen();

    VDP_drawText("Mode 1 PCM Player", 20-8, 2);

    VDP_drawText("Sample A: IMA ADPCM Mono", 2, 16);
    VDP_drawText("Sample B: SB4 ADPCM Mono", 2, 17);
    VDP_drawText("Sample C: 8bit PCM Stereo", 2, 18);

    VDP_drawText("START = Pause last source", 2, 20);
    VDP_drawText("A/B/C = Play Smpl A/B/C on src 1/2/3", 2, 21);
    VDP_drawText("X/Y/Z = Play Smpl A/B/C on free src", 2, 22);
    VDP_drawText("L/R   = pan left/right", 2, 23);
    VDP_drawText("U/D   = volume incr/decr", 2, 24);
    VDP_drawText("MODE  = clear", 2, 25);

    while (1)
    {
        int new_src = 0;
/*
        delay(2);
        buttons = get_pad(0) & SEGA_CTRL_BUTTONS;

        if (buttons & SEGA_CTRL_LEFT)
            pan--;
        if (buttons & SEGA_CTRL_RIGHT)
            pan++;

        if (pan < 0)
            pan = 0;
        if (pan > 255)
            pan = 255;

        if (buttons & SEGA_CTRL_DOWN)
            vol--;
        if (buttons & SEGA_CTRL_UP)
            vol++;

        if (vol < 0)
            vol = 0;
        if (vol > 255)
            vol = 255;

        if (((buttons ^ previous) & SEGA_CTRL_A) && (buttons & SEGA_CTRL_A))
        {
            last_src = scd_play_src(1, 1, 0, pan, vol, 0);
            src_paused[last_src] = 0;
        }
        if (((buttons ^ previous) & SEGA_CTRL_B) && (buttons & SEGA_CTRL_B))
        {
            last_src = scd_play_src(2, 2, 0, pan, vol, 0);
            src_paused[last_src] = 0;
        }
        if (((buttons ^ previous) & SEGA_CTRL_C) && (buttons & SEGA_CTRL_C))
        {
            last_src = scd_play_src(3, 3, 0, pan, vol, 0);
            src_paused[last_src] = 0;
        }

        if (((buttons ^ previous) & SEGA_CTRL_X) && (buttons & SEGA_CTRL_X))
        {
            new_src = scd_play_src(255, 1, 0, pan, vol, 0);
        }
        if (((buttons ^ previous) & SEGA_CTRL_Y) && (buttons & SEGA_CTRL_Y))
        {
            new_src = scd_play_src(255, 2, 0, pan, vol, 0);
        }
        if (((buttons ^ previous) & SEGA_CTRL_Z) && (buttons & SEGA_CTRL_Z))
        {
            new_src = scd_play_src(255, 3, 0, pan, vol, 0);
        }

        if (new_src != 0) {
            last_src = new_src;
            src_paused[last_src] = 0;
        }

        if (((buttons ^ previous) & SEGA_CTRL_START) && (buttons & SEGA_CTRL_START))
        {
            scd_punpause_src(last_src, !src_paused[last_src]);
            src_paused[last_src] = !src_paused[last_src];
        }

        if (((buttons ^ previous) & SEGA_CTRL_MODE) && (buttons & SEGA_CTRL_MODE))
        {
            scd_clear_pcm();
        }
*/
        scd_update_src(last_src, 0, pan, vol, 0);

        sprintf(text, "%d", last_src);
        VDP_drawText("Last Source:   ", 2, 6);
        VDP_drawText(text, 15, 6);
        sprintf(text, "%04X", scd_getpos_for_src(last_src));
        VDP_drawText("Position:    ", 2, 7);
        VDP_drawText(text, 15, 7);

        sprintf(text, "%03d", (int)pan);
        VDP_drawText("Panning:       ", 2, 9);
        VDP_drawText(text, 15, 9);
        sprintf(text, "%03d", (int)vol);
        VDP_drawText("Volume:        ", 2, 10);
        VDP_drawText(text, 15, 10);

        previous = buttons;
    }

    /*
     * Should never reach here due to while condition
     */
    VDP_resetScreen();
    return 0;
}
