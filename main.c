/*
 * SEGA CD Mode 1 PCM Player
 * by Victor Luchits
 * and Chilly Willy
 * with SGDK mods by Matt Bennion (Matteusbeus)
 */
#include "hw_md.h"
#include "hw_scd.h"
#include "scd_pcm.h"

// Text positions
#define POS_TITLE_1 2
#define POS_TITLE_3 3

#define POS_LAST_SOURCE 6
#define POS_POSITION 7
#define POS_PANNING 9
#define POS_VOLUME 10

#define POS_SPCM_STATUS 12

#define POS_SAMPLE_A 14
#define POS_SAMPLE_B 15
#define POS_SAMPLE_C 16
#define POS_START 18

#define POS_AB 19
#define POS_XY 20
#define POS_CZ 21
#define POS_LR 22
#define POS_UD 23
#define POS_MODE 24

u16 cd_ok;
char text[44];
u16 buttons = 0, previous = 0, first_track = 0, last_track = 0, curr_track = 1, prev_track = 0;
u8 last_src = 0;
s16 pan = 128, vol = 255;
u8 src_paused[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u16 new_src = 0;
u8 mode = 0;
u8 data_type = 255;

union {
    s16 lo[4];
    long long int value;
} disc_info;

union {
    s32 lo[2];
    long long int value;
} track_info;

u16 bios_stat, bios_previous_stat = 0;

void delay(s16 vblanks);
int pcmDisplay();
void pcmCtrlInput();
void pcmModejoyEvent(u16 joy, u16 changed, u16 state);
int cddaDisplay();
void cddaCtrlInput();
void cddaModejoyEvent(u16 joy, u16 changed, u16 state);

void delay(s16 vblanks)
{
    for(s16 i = 0; i != vblanks; i++)
    {
        SYS_doVBlankProcess();
    }
}

int pcmDisplay()
{

    disc_info.value = scd_get_disc_info();
        
    bios_stat = (u16)disc_info.lo[0]; // Status 

    VDP_drawText("Loading Disc Information", 20-15, 13);

    delay(300);

    VDP_drawText("Uploading samples...    ", 20-15, 13);

    /*
    * Upload test samples
    */
    // Load wav from Cart ROM to CD RAM
    //scd_upload_buf(1, (u8 *)&macabre_ima_wav, sizeof(macabre_ima_wav));
    
    // Load wavs from CD to CD RAM
    scd_src_load_file("MACABRE.WAV", 1);
    scd_src_load_file("STEREOU8.WAV", 2);
    
    SYS_doVBlankProcess();

    // Clear inital text
    VDP_drawText("                     ", 20-15, 13);

    // Draw text
    VDP_drawText("Mode 1 PCM Player", 20-8, POS_TITLE_1);
    VDP_drawText("SGDK Edition", 14, POS_TITLE_3);

    // Change to green text using PAL 2
    VDP_setTextPalette(PAL2);

    // Draw green text
    VDP_drawText("Last Source:", 2, POS_LAST_SOURCE);
    VDP_drawText("Position:", 2, POS_POSITION);

    VDP_drawText("Panning:", 2, POS_PANNING);
    VDP_drawText("Volume:", 2, POS_VOLUME);

    VDP_drawText("SPCM Status:", 2, POS_SPCM_STATUS);

    // Change to white text using PAL 0
    VDP_setTextPalette(PAL0);

    // Draw white text
    VDP_drawText("Sample A: IMA ADPCM Mono from CD", 2, POS_SAMPLE_A);
    VDP_drawText("Sample B: 8bit PCM Stereo", 2, POS_SAMPLE_B);
    VDP_drawText("Sample C: SPCM from CD", 2, POS_SAMPLE_C);

    VDP_drawText("START = Pause last source", 2, POS_START);
    VDP_drawText("A/B   = Play Smpl A/B on src 1/2", 2, POS_AB);
    VDP_drawText("X/Y   = Play Smpl A/B on free src", 2, POS_XY);
    VDP_drawText("C/Z   = Play/Stop SPCM", 2, POS_CZ);
    VDP_drawText("L/R   = Pan left/right", 2, POS_LR);
    VDP_drawText("U/D   = Volume incr/decr", 2, POS_UD);
    VDP_drawText("MODE  = Clear", 2, POS_MODE);

    while (1)
    {

        // set source to zero
        new_src = 0;

        // delay next action by two vblanks
        delay(2);
        
        // get controller input
        pcmCtrlInput();

        // update source
        scd_src_update(last_src, 0, pan, vol, 0);

        // check if SPCM is playing
        if (scd_spcm_get_playback_status() & 1) {
            VDP_drawText("PLAYING", 15, POS_SPCM_STATUS);
        } else {
            VDP_drawText("STOPPED", 15, POS_SPCM_STATUS);
        }

        // Draw the postion in the last source (SPCM not supported)
        sprintf(text, "%04X", scd_src_get_pos(last_src));
        VDP_drawText(text, 15, POS_LAST_SOURCE);

        // Draw last position
        sprintf(text, "%d", last_src);
        VDP_drawText(text, 15, POS_POSITION);
        
        // Draw pan value
        sprintf(text, "%03d", (u16)pan);
        VDP_drawText(text, 15, POS_PANNING);

        // Draw volume value
        sprintf(text, "%03d", (u16)vol);
        VDP_drawText(text, 15, POS_VOLUME);

    }

    /*
     * Should never reach here due to while condition
     */
    VDP_resetScreen();
    return 0;
}

void pcmCtrlInput()
{

    buttons = JOY_readJoypad(JOY_1) & BUTTON_ALL;

    if (((buttons ^ previous) & BUTTON_A) && (buttons & BUTTON_A))
    {
        last_src = scd_src_play(1, 1, 0, pan, vol, 0);
        src_paused[last_src] = 0;
    }
    if (((buttons ^ previous) & BUTTON_B) && (buttons & BUTTON_B))
    {
        last_src = scd_src_play(2, 2, 0, pan, vol, 0);
        src_paused[last_src] = 0;
    }
    if (((buttons ^ previous) & BUTTON_C) && (buttons & BUTTON_C))
    {   
        scd_cdda_set_volume(1024);
        scd_spcm_play_track("ZAMBOLIN.PCM", 0);
    }
    
    if (((buttons ^ previous) & BUTTON_X) && (buttons & BUTTON_X))
    {
        new_src = scd_src_play(255, 1, 0, pan, vol, 0);
    }
    if (((buttons ^ previous) & BUTTON_Y) && (buttons & BUTTON_Y))
    {
        new_src = scd_src_play(255, 2, 0, pan, vol, 0);    
    }
    if (((buttons ^ previous) & BUTTON_Z) && (buttons & BUTTON_Z))
    {
        scd_spcm_stop_track();
    }
    
    if (new_src != 0) 
    {
        last_src = new_src;
        src_paused[last_src] = 0;
    }
    
    if (((buttons ^ previous) & BUTTON_START) && (buttons & BUTTON_START))
    {
        scd_src_toggle_pause(last_src, !src_paused[last_src]);
        src_paused[last_src] = !src_paused[last_src];
    }
    
    if (((buttons ^ previous) & BUTTON_MODE) && (buttons & BUTTON_MODE))
    {
        scd_clear_pcm();
    }

    previous = buttons;

}

void pcmModejoyEvent(u16 joy, u16 changed, u16 state)
{

    if (changed & state & BUTTON_LEFT) 
    {  
        pan--;
    }
    if (changed & state & BUTTON_RIGHT) 
    {
        pan++;
    }

    if (pan < 0) 
    {
        pan = 0;
    }
    if (pan > 255) 
    {
        pan = 255;
    }

    if (changed & state & BUTTON_DOWN) 
    {
        vol--;
    }
    if (changed & state & BUTTON_UP) 
    {
        vol++;
    }

    if (vol < 0) 
    {
        vol = 0;
    }
    if (vol > 255) 
    {
        vol = 255;
    }

}

int cddaDisplay() {

    VDP_setTextPalette(2);
    VDP_drawText("Loading Disc Information", 20-15, 13);

    delay(300);

    VDP_drawText("                        ", 20-15, 13);
    VDP_setTextPalette(0);
    VDP_drawText("Mode 1 CDDA Player", 20-8, POS_TITLE_1);
    VDP_drawText("SGDK Edition", 14, POS_TITLE_3);

    VDP_setTextPalette(2);
    VDP_drawText("Status:", 2, 5);
    VDP_drawText("First Track:", 2, 6);
    VDP_drawText("Last Track:", 2, 7);
    VDP_drawText("Current Track:", 2, 8);
    VDP_drawText("Drive Version:", 2, 11);
    VDP_setTextPalette(0);
    VDP_drawText("Current Track", 2, 13);
    VDP_setTextPalette(2);
    VDP_drawText("MM SS FF TN:", 2, 14);
    VDP_drawText("Data Type:", 2, 15);
    VDP_setTextPalette(0);
    VDP_drawText("A     = Play", 2, 17);
    VDP_drawText("B     = Pause", 2, 18);
    VDP_drawText("C     = Stop", 2, 19);
    VDP_drawText("L/R   = Track previous/next", 2, 20);

    u8 first_tno = 255, last_tno = 255, drv_ver = 255, flag = 255;
    u8 minutes = 255, seconds = 255, frames = 255, track_no = 255;

    while (1)
    {

        // Get information regarding the disc
        disc_info.value = scd_get_disc_info();
        
        bios_stat = disc_info.lo[0];                  // Status 
        first_tno = ((disc_info.lo[1] >> 8) & 0xFF);  // First track number  
        last_tno = (disc_info.lo[1] & 0xFF);          // Last track number
        drv_ver = (disc_info.lo[2] & 0xFF);           // Drive version (low byte)
        flag = ((disc_info.lo[2] >> 8) & 0xFF);       // Flag (high byte)
    
        first_track = (u16)first_tno;
        last_track = (u16)last_tno;

        // delay next action by two vblanks
        delay(2);
        
        // get controller input
        cddaCtrlInput();
        
        // check if the CD's status has change since the last time and update if it has
        if (bios_previous_stat != bios_stat)
        {
            
            VDP_setTextPalette(0);
            
            VDP_drawText("               ", 17, 5);
            
            switch (bios_stat >> 8)
            {
                case 0:
                    VDP_drawText("STOPPED", 17, 5);
                    break;
                case 1:
                    VDP_drawText("PLAYING", 17, 5);
                    break;
                case 3:
                    VDP_drawText("SCANNING", 17, 5);   
                    break;
                case 5:
                    VDP_drawText("PAUSED", 17, 5);
                    break;
                case 8:
                    VDP_drawText("SEEKING", 17, 5);
                    break;
                case 16:
                    VDP_drawText("NO DISC", 17, 5);
                    break;
                case 32:
                    VDP_drawText("READING TOC", 17, 5);
                    break;
                case 64:
                    VDP_drawText("OPEN", 17, 5);
                    break;
                default:
                    if (bios_stat & 0x8000) {
                        VDP_drawText("NOT READY", 17, 5);
                    } else {
                        VDP_drawText("UNDEFINED", 17, 5);
                    }
                    break;
            }
            
            sprintf(text, "%d", first_tno);
            VDP_drawText("               ", 17, 6);
            VDP_drawText(text, 17, 6);
            
            sprintf(text, "%d", last_tno);
            VDP_drawText("               ", 17, 7);
            VDP_drawText(text, 17, 7);
            
            sprintf(text, "%d", curr_track);
            VDP_drawText("               ", 17, 8);
            VDP_drawText(text, 17, 8);

            sprintf(text, "%s %s", (flag & 4) ? "DATA" : "CDDA", (flag & 2) ? "EMP" : "LIN");
            VDP_drawText("               ", 2, 10);
            VDP_drawText(text, 2, 10);

            sprintf(text, "%d", drv_ver);
            VDP_drawText("               ", 17, 11);
            VDP_drawText(text, 17, 11);

            bios_previous_stat = bios_stat;

        }

        // check if the track has change
        if (curr_track != prev_track) 
        {

            sprintf(text, "%d", curr_track);
            VDP_drawText("               ", 17, 8);
            VDP_drawText(text, 17, 8);

            track_info.value = scd_cdda_get_track_info(curr_track);

            minutes   = (track_info.lo[0] >> 24) & 0xFF;  // Byte 1 (minutes)
            seconds   = (track_info.lo[0] >> 16) & 0xFF;  // Byte 2 (seconds)
            frames    = (track_info.lo[0] >> 8)  & 0xFF;  // Byte 3 (frames)
            track_no  = track_info.lo[0] & 0xFF;          // Byte 4 (track number)
            data_type = track_info.lo[1] & 0xFF;          // Byte 5 (track type)

            sprintf(text, "%02X : %02X : %02X : %02X", minutes, seconds, frames, track_no);
            VDP_drawText("               ", 17, 14);
            VDP_drawText(text, 17, 14);

            sprintf(text, "%s", data_type ? "DATA" : "CDDA");
            VDP_drawText("               ", 17, 15);
            VDP_drawText(text, 17, 15);

            prev_track = curr_track;

        }
    }

    /*
     * Should never reach here due to while condition
     */
    VDP_resetScreen();
    return 0;

}

void cddaCtrlInput()
{

    buttons = JOY_readJoypad(JOY_1) & BUTTON_ALL;

    if (((buttons ^ previous) & BUTTON_START) && (buttons & BUTTON_START))
    {

    }
    if (((buttons ^ previous) & BUTTON_A) && (buttons & BUTTON_A))
    {
        if (data_type == 0) {
            scd_cdda_play_track(curr_track, 0);
        }
    }
    if (((buttons ^ previous) & BUTTON_B) && (buttons & BUTTON_B))
    {
        scd_cdda_toggle_pause();
    }
    if (((buttons ^ previous) & BUTTON_C) && (buttons & BUTTON_C))
    {   
        scd_cdda_stop_track();
    }

    previous = buttons;

}

void cddaModejoyEvent(u16 joy, u16 changed, u16 state)
{

    if (changed & state & BUTTON_LEFT) 
    {  
        curr_track--;
        if (curr_track < first_track)
        {
            curr_track = first_track;
        } 
    }
    else if (changed & state & BUTTON_RIGHT) 
    {
        curr_track++;
        if (curr_track > last_track)
        {
            curr_track = last_track;
        }
    }
    else if (changed & state & BUTTON_DOWN) 
    {
        vol--;
        if (vol < 0) 
        {
            vol = 0;
        }
    }
    else if (changed & state & BUTTON_UP) 
    {
        vol++;      
        if (vol > 255) 
        {
            vol = 255;
        }
    }

}

int main(void) {

    /*
    * Initialize the CD
    */
    cd_ok = InitCd();

    /*
    * Initialize the PCM driver
    */
    scd_init_pcm();

    // Swap between PCM (0) and CDDA (1) Mode
    mode = 1;

    switch(mode) 
    {
        case 0: // PCM MODE
            JOY_setEventHandler(pcmModejoyEvent);
            pcmDisplay();
        break;
        case 1: // CDDA MODE
            JOY_setEventHandler(cddaModejoyEvent);
            cddaDisplay();
        break;
    }  

}

