#ifndef _HW_MD_H
#define _HW_MD_H

#ifdef __cplusplus
extern "C" {
#endif

extern void write_byte(unsigned int dst, unsigned char val);
extern void write_word(unsigned int dst, unsigned short val);
extern void write_long(unsigned int dst, unsigned int val);
extern unsigned char read_byte(unsigned int src);
extern unsigned short read_word(unsigned int src);
extern unsigned int read_long(unsigned int src);

#ifdef __cplusplus
}
#endif

#endif
