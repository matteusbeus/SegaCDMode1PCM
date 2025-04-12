#include "hw_md.h"
#include "hw_scd.h"
#include "res/resources.h"

extern void Kos_Decomp(u8 *src, u8 *dst);

int memcmp(const void *ptr1, const void *ptr2, u32 num);

void *memset2(void *ptr, int value, u32 num) {
    unsigned char *p = (unsigned char *)ptr;
    while (num--) {
        *p++ = (unsigned char)value;
    }
    return ptr;
}

int memcmp(const void *ptr1, const void *ptr2, u32 num) {
    const unsigned char *p1 = (const unsigned char *)ptr1;
    const unsigned char *p2 = (const unsigned char *)ptr2;

    while (num--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }

    return 0;
}

void *custom_memcpy(void *dest, const void *src, u32 n) {
    // Cast source and destination to char pointers
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    // Copy data byte by byte
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }

    return dest;
}

void secondInt() {
    __asm__ (
        "lea 0xA12000, %a0\n"   // Load address 0xA12000 into register a0
        "move.w (%a0), %d0\n"    // Load 16-bit value from address in a0 into d0
        "ori.w #0x0100, %d0\n"   // OR the value in d0 with 0x0100
        "move.w %d0, (%a0)\n"    // Store the modified value back into address a0
    );
}

u16 InitCd(void) {
    
    char *bios;

    /*
     * Check for CD BIOS
     * When a cart is inserted in the MD, the CD hardware is mapped to
     * 0x400000 instead of 0x000000. So the BIOS ROM is at 0x400000, the
     * Program RAM bank is at 0x420000, and the Word RAM is at 0x600000.
     */
    bios = (char *)0x415800;
    if (memcmp(bios + 0x6D, "SEGA", 4))  // Check if the BIOS starts with "SEGA"
    {
        bios = (char *)0x416000;
        if (memcmp(bios + 0x6D, "SEGA", 4))  // Check if the BIOS starts with "SEGA"
        {
            // Check for WonderMega/X'Eye
            if (memcmp(bios + 0x6D, "WONDER", 6))  // Check if the BIOS starts with "WONDER"
            {
                bios = (char *)0x41AD00; // Might also be 0x40D500
                // Check for LaserActive
                if (memcmp(bios + 0x6D, "SEGA", 4))  // Check if the BIOS starts with "SEGA"
                    return 0; // no CD
            }
        }
    }

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
    write_word(0xA12002, 0x0002); // no write-protection, bank 0, 2M mode, Word RAM assigned to Sub-CPU
    memset2((char *)0x420000, 0, 0x20000); // clear program ram first bank - needed for the LaserActive
    Kos_Decomp((u8 *)bios, (u8 *)0x420000);

    /*
    * Copy Sub-CPU program (fusion driver) to Program RAM at 0x06000
    */
    custom_memcpy((void *)0x426000, &scd_fusion_driver, sizeof(scd_fusion_driver));

    write_byte(0xA1200E, 0x00); // clear main comm port
    write_byte(0xA12002, 0x2A); // write-protect up to 0x05400
    write_byte(0xA12001, 0x01); // clear bus request, deassert reset - allow CD Sub-CPU to run
    while (!(read_byte(0xA12001) & 1)) write_byte(0xA12001, 0x01); // wait on Sub-CPU running

    /*
     * Set the vertical blank handler to generate Sub-CPU level 2 ints.
     * The Sub-CPU BIOS needs these in order to run.
     */
    SYS_setVIntCallback(secondInt);
    SYS_doVBlankProcess();

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
            return 0; // no CD
        }
    }

    /*
     * Wait for Sub-CPU to indicate it is ready to receive commands
     */
    while (read_byte(0xA1200F) != 0x00) ;

    return 0x1; // CD ready to go!

}