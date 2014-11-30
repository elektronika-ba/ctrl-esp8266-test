#ifndef __AES_CBC_H
#define __AES_CBC_H

#include "c_types.h"

void aes128_cbc_encrypt(unsigned char *, unsigned int, const char *);
void aes128_cbc_decrypt(unsigned char *, unsigned int, const char *);

#endif
