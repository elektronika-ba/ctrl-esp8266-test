#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"

#include "aes_cbc.h"
#include "cmac.h"

/*
	The AES-CMAC Algorithm
	http://www.ietf.org/rfc/rfc4493.txt

	Implementation of CMAC, partially taken from KIRK-Engine:
	https://code.google.com/p/kirk-engine/source/browse/trunk/cmac.c?r=9
	combined with implementation from AVR411 app note and
	http://www.ietf.org/rfc/rfc4493.txt.

	Depends on library "aes_cbc.h > aes.h" originally taken from AVR's AVR411 app note,
	and modified to work only with 128 bit key.
*/

/*
	Left-shift entire buffer once. Buffer is "length" bytes wide.
*/
static void ICACHE_FLASH_ATTR cmac_left_shift_buffer(unsigned char *input, unsigned char *output, unsigned short length) {
	unsigned char overflow = 0;
	unsigned short i;

	for (i=length; i>0; i--)
	{
		output[i-1] = input[i-1] << 1;
		output[i-1] |= overflow;
		overflow = (input[i-1] & 0x80) ? 1 : 0;
	}
}

/*
	XOR buff1 and buff2, fill result in out_buff. Length of buffers buff1 and buff2 should be equal,
	and length(out_buff) >= length(buff*)
	One of in_buff1 and in_buff2 can be the same array as out_buff.
*/
static void ICACHE_FLASH_ATTR cmac_xor_buffers(unsigned char *in_buff1, unsigned char *in_buff2, unsigned char *out_buff, unsigned short length) {
	do {
		unsigned char in_val1 = *in_buff1;
		unsigned char in_val2 = *in_buff2;

		*out_buff = in_val1 ^ in_val2;

		out_buff++;
		in_buff1++;
		in_buff2++;
	} while(--length);
}

/*
   The subkey generation algorithm, Generate_Subkey(), takes a secret
   key, K, which is just the key for AES-128.

   The outputs of the subkey generation algorithm are two subkeys, K1
   and K2.  We write (K1,K2) := Generate_Subkey(K).

   KEY, out_K1, out_K2 must be 16 bytes long arrays.
*/
static void ICACHE_FLASH_ATTR cmac_generate_sub_keys(unsigned char *KEY, unsigned char *out_K1, unsigned char *out_K2) {
	// Step 1. (using out_K2 as L buffer for generating K1)
	unsigned char i;
	for(i=0; i<16; i++) {
		out_K2[i] = 0;
	}
	aes128_cbc_encrypt(out_K2, 16, KEY);

	// Step 2.
	cmac_left_shift_buffer(out_K2, out_K1, 16);
	if (out_K2[0] & 0x80) {
		out_K1[15] = out_K1[15] ^ 0x87;
	}

 	// Step 3.
	cmac_left_shift_buffer(out_K1, out_K2, 16);
	if (out_K1[0] & 0x80) {
		out_K2[15] = out_K2[15] ^ 0x87;
	}

	// Step 4. (return K1, K2) - already done
}

void ICACHE_FLASH_ATTR cmac_generate(unsigned char *KEY, unsigned char *input, unsigned short length, unsigned char *result) {
	unsigned char K1[16], K2[16];
	cmac_generate_sub_keys(KEY, K1, K2);

	unsigned short n = (length + 15) / 16; // n is number of rounds
	unsigned char lenMod16 = length % 16; // will need later (optimization for speed)

	unsigned char flag;
	if(n == 0) {
		n = 1;
		flag = 0;
	}
	else {
		if (lenMod16 == 0) {
			flag = 1; // last block is a complete block
		}
		else {
			flag = 0; // last block is not complete block
		}
	}

	unsigned char *M_last;
	unsigned char index = 16 * (n-1);

	// last block is complete block
	if (flag) {
		M_last = &K2[0]; // using the same RAM space for M_last as that of K2 - size optimization
		cmac_xor_buffers(&input[index], K1, M_last, 16);
	}
	else {
		M_last = &K1[0]; // using the same RAM space for M_last as that of K1 - size optimization

		// padding input and xoring with K2 at the same time
		unsigned char j;
		for (j=0; j<16; j++ ) {
			unsigned char temp;
			if ( j < lenMod16 ) { // we have this byte index in input - take it
				temp = input[index + j];
			}
			else if ( j == lenMod16 ) { // last byte of input is padded with 0x80
				temp = 0x80;
			}
			else {
				temp = 0x00; // the rest is padded with 0x00
			}

			M_last[j] = temp ^ K2[j];
		}
	}

	unsigned char i;
	for (i=0; i<16; i++) {
		result[i] = 0;
	}

	for (i=0; i<n-1; i++) {
		cmac_xor_buffers(result, &input[16*i], result, 16); // Y := Mi (+) X
		aes128_cbc_encrypt(result, 16, KEY); // X := AES-128(KEY, Y);
	}

	cmac_xor_buffers(result, M_last, result, 16);
	aes128_cbc_encrypt(result, 16, KEY);

	// Step 7. return T (already done)
}
