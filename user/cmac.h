#ifndef __CMAC_H
#define __CMAC_H

#include "c_types.h"

// private
static void cmac_left_shift_buffer(unsigned char *, unsigned char *, unsigned short);
static void cmac_xor_buffers(unsigned char *, unsigned char *, unsigned char *, unsigned short);
static void cmac_generate_sub_keys(unsigned char *, unsigned char *, unsigned char *);

// public
void cmac_generate(unsigned char *, unsigned char *, unsigned short, unsigned char *);

#endif
