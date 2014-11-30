// This file has been prepared for Doxygen automatic documentation generation.
/*! \file ********************************************************************
*
* \brief  Source file for AES public and support functions.
*
* This file contains the function implementation for the AES
* algorithm. Refer to the aes.h file for more details.
*
* - File:              aes.c
* - Compiler:          IAR EWAVR 4.11B
* - Supported devices: ATtiny45/85
* - AppNote:           AVR411 - Secure Rolling Code Algorithm
*                      for Wireless Link
*
* \author              Atmel Corporation: http://www.atmel.com \n
*                      Support email: avr@atmel.com
*
* $Name:  $
* $Revision: 4337 $
* $Date: 2008-08-08 10:52:47 +0200 (fr, 08 aug 2008) $
*
* Copyright (c) 2006, Atmel Corporation All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* 3. The name of ATMEL may not be used to endorse or promote products derived
* from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY ATMEL ``AS IS'' AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY AND
* SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/
/*
	WARNING: This is a modified Atmel's implementation of AES.
	Changed: Removed everything and left only 128-bit wide key AES implementation.
*/
#include "aes.h"

//! Lower 8 bits of AES polynomial (x^8+x^4+x^3+x+1), ie. (x^4+x^3+x+1).
#define BPOLY 0x1b

//! S-Box lookup table.
const byte sBox[256] = {
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5,
	0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
	0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
	0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
	0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc,
	0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a,
	0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
	0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
	0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
	0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
	0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85,
	0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
	0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
	0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
	0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17,
	0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
	0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
	0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
	0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
	0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9,
	0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6,
	0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
	0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
	0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
	0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94,
	0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68,
	0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

//! Inverse S-Box lookup table.
const byte sBoxInv[256] = {
	0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38,
	0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
	0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87,
	0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
	0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d,
	0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
	0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2,
	0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
	0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16,
	0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
	0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda,
	0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
	0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a,
	0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
	0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
	0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
	0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea,
	0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
	0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85,
	0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
	0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89,
	0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
	0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20,
	0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
	0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31,
	0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
	0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d,
	0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
	0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0,
	0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
	0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26,
	0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

//! XORs 'count' bytes of 'bytes' with 'constant'.
static void ICACHE_FLASH_ATTR addConstant( byte * bytes, const byte * constant, byte count )
{
	// Copy to temporary variables for optimization.
	byte * tempBlock = bytes;
	const byte * tempSource = constant;
	byte tempCount = count;
	byte tempValue;

	do {
		 // Add in GF(2), ie. XOR.
		tempValue = *tempBlock ^ *tempSource++;
		*tempBlock++ = tempValue;
	} while( --tempCount );
}

//! Copy 'count' bytes from 'source' to 'destionation'.
static void ICACHE_FLASH_ATTR copyBytes( byte * destination, const byte * source, byte count)
{
	// Copy to temporary variables for optimization.
	byte * tempDest = destination;
	const byte * tempSrc = source;
	byte tempCount = count;

	do {
		*tempDest++ = *tempSrc++;
	} while( --tempCount );
}

//! Cycle a 4-byte array left once.
static void ICACHE_FLASH_ATTR cycleLeft( byte * row )
{
	// Cycle 4 bytes in an array left once.
	byte temp = row[0];
	row[0] = row[1];
	row[1] = row[2];
	row[2] = row[3];
	row[3] = temp;
}

//! Perform an AES column mix operation on the 4 bytes in 'column' buffer.
static void ICACHE_FLASH_ATTR mixColumn( byte * column )
{
	byte result0, result1, result2, result3;
	byte column0, column1, column2, column3;
	byte xor;

	// This generates more effective code, at least
	// with the IAR C compiler.
	column0 = column[0];
	column1 = column[1];
	column2 = column[2];
	column3 = column[3];

	// Partial sums (modular addition using XOR).
	result0 = column1 ^ column2 ^ column3;
	result1 = column0 ^ column2 ^ column3;
	result2 = column0 ^ column1 ^ column3;
	result3 = column0 ^ column1 ^ column2;

	// Multiply column bytes by 2 modulo BPOLY.
	// This operation is done the following way to ensure cycle count
	// independent from data contents. Take care when changing this code.
	xor = 0;
	if (column0 & 0x80) {
		xor = BPOLY;
	}
	column0 <<= 1;
	column0  ^= xor;

	xor = 0;
	if (column1 & 0x80) {
		xor = BPOLY;
	}
	column1 <<= 1;
	column1  ^= xor;

	xor = 0;
	if (column2 & 0x80) {
		xor = BPOLY;
	}
	column2 <<= 1;
	column2  ^= xor;

	xor = 0;
	if (column3 & 0x80) {
		xor = BPOLY;
	}
	column3 <<= 1;
	column3  ^= xor;

	// Final sums stored into original column bytes.
	column[0] = result0 ^ column0 ^ column1;
	column[1] = result1 ^ column1 ^ column2;
	column[2] = result2 ^ column2 ^ column3;
	column[3] = result3 ^ column0 ^ column3;
}

//! Perform AES column mixing for the whole AES block/state.
static void ICACHE_FLASH_ATTR mixColumns( byte * state )
{
	mixColumn( state + 0*4 );
	mixColumn( state + 1*4 );
	mixColumn( state + 2*4 );
	mixColumn( state + 3*4 );
}

//! Substitute 'count' bytes in the 'bytes' buffer in SRAM using the S-Box.
static void ICACHE_FLASH_ATTR subBytes( byte * bytes, byte count )
{
	// Copy to temporary variables for optimization.
	byte * tempPtr = bytes;
	byte tempCount = count;

	do {
		*tempPtr = sBox[ *tempPtr ]; // Substitute every byte in state.
		++tempPtr;
	} while( --tempCount );
}

//! Perform AES shift-rows operation for the whole AES block/state.
static void ICACHE_FLASH_ATTR shiftRows( byte * state )
{
	byte temp;

	// Note: State is arranged column by column.

	// Cycle second row left one time.
	temp = state[ 1 + 0*4 ];
	state[ 1 + 0*4 ] = state[ 1 + 1*4 ];
	state[ 1 + 1*4 ] = state[ 1 + 2*4 ];
	state[ 1 + 2*4 ] = state[ 1 + 3*4 ];
	state[ 1 + 3*4 ] = temp;

	// Cycle third row left two times.
	temp = state[ 2 + 0*4 ];
	state[ 2 + 0*4 ] = state[ 2 + 2*4 ];
	state[ 2 + 2*4 ] = temp;
	temp = state[ 2 + 1*4 ];
	state[ 2 + 1*4 ] = state[ 2 + 3*4 ];
	state[ 2 + 3*4 ] = temp;

	// Cycle fourth row left three times, ie. right once.
	temp = state[ 3 + 3*4 ];
	state[ 3 + 3*4 ] = state[ 3 + 2*4 ];
	state[ 3 + 2*4 ] = state[ 3 + 1*4 ];
	state[ 3 + 1*4 ] = state[ 3 + 0*4 ];
	state[ 3 + 0*4 ] = temp;
}

//! Perform an AES inverse column mix operation on the 4 bytes in 'column' buffer.
static void ICACHE_FLASH_ATTR invMixColumn( byte * column )
{
	byte result0, result1, result2, result3;
	byte column0, column1, column2, column3;
	byte xor;

	// This generates more effective code, at least
	// with the IAR C compiler.
	column0 = column[0];
	column1 = column[1];
	column2 = column[2];
	column3 = column[3];

	// Partial sums (modular addition using XOR).
	result0 = column1 ^ column2 ^ column3;
	result1 = column0 ^ column2 ^ column3;
	result2 = column0 ^ column1 ^ column3;
	result3 = column0 ^ column1 ^ column2;

	// Multiply column bytes by 2 modulo BPOLY.
	// This operation is done the following way to ensure cycle count
	// independent from data contents. Take care when changing this code.
	xor = 0;
	if (column0 & 0x80) {
		xor = BPOLY;
	}
	column0 <<= 1;
	column0  ^= xor;

	xor = 0;
	if (column1 & 0x80) {
		xor = BPOLY;
	}
	column1 <<= 1;
	column1  ^= xor;

	xor = 0;
	if (column2 & 0x80) {
		xor = BPOLY;
	}
	column2 <<= 1;
	column2  ^= xor;

	xor = 0;
	if (column3 & 0x80) {
		xor = BPOLY;
	}
	column3 <<= 1;
	column3  ^= xor;

	// More partial sums.
	result0 ^= column0 ^ column1;
	result1 ^= column1 ^ column2;
	result2 ^= column2 ^ column3;
	result3 ^= column0 ^ column3;

	// Multiply column bytes by 2 modulo BPOLY.
	// This operation is done the following way to ensure cycle count
	// independent from data contents. Take care when changing this code.
	xor = 0;
	if (column0 & 0x80) {
		xor = BPOLY;
	}
	column0 <<= 1;
	column0  ^= xor;

	xor = 0;
	if (column1 & 0x80) {
		xor = BPOLY;
	}
	column1 <<= 1;
	column1  ^= xor;

	xor = 0;
	if (column2 & 0x80) {
		xor = BPOLY;
	}
	column2 <<= 1;
	column2  ^= xor;

	xor = 0;
	if (column3 & 0x80) {
		xor = BPOLY;
	}
	column3 <<= 1;
	column3  ^= xor;

	// More partial sums.
	result0 ^= column0 ^ column2;
	result1 ^= column1 ^ column3;
	result2 ^= column0 ^ column2;
	result3 ^= column1 ^ column3;

	// Multiply column bytes by 2 modulo BPOLY.
	// This operation is done the following way to ensure cycle count
	// independent from data contents. Take care when changing this code.
	xor = 0;
	if (column0 & 0x80) {
		xor = BPOLY;
	}
	column0 <<= 1;
	column0  ^= xor;

	xor = 0;
	if (column1 & 0x80) {
		xor = BPOLY;
	}
	column1 <<= 1;
	column1  ^= xor;

	xor = 0;
	if (column2 & 0x80) {
		xor = BPOLY;
	}
	column2 <<= 1;
	column2  ^= xor;

	xor = 0;
	if (column3 & 0x80) {
		xor = BPOLY;
	}
	column3 <<= 1;
	column3  ^= xor;

	// Final partial sum.
	column0 ^= column1 ^ column2 ^ column3;

	// Final sums stored indto original column bytes.
	column[0] = result0 ^ column0;
	column[1] = result1 ^ column0;
	column[2] = result2 ^ column0;
	column[3] = result3 ^ column0;
}

//! Perform AES inverse column mixing for the whole AES block/state.
static void ICACHE_FLASH_ATTR invMixColumns( byte * state )
{
	invMixColumn( state + 0*4 );
	invMixColumn( state + 1*4 );
	invMixColumn( state + 2*4 );
	invMixColumn( state + 3*4 );
}

//! Perform AES inverse shift rows operation for the whole AES block/state.
static void ICACHE_FLASH_ATTR invShiftRows( byte * state )
{
	byte temp;

	// Note: State is arranged column by column.

	// Cycle second row right one time.
	temp = state[ 1 + 3*4 ];
	state[ 1 + 3*4 ] = state[ 1 + 2*4 ];
	state[ 1 + 2*4 ] = state[ 1 + 1*4 ];
	state[ 1 + 1*4 ] = state[ 1 + 0*4 ];
	state[ 1 + 0*4 ] = temp;

	// Cycle third row right two times.
	temp = state[ 2 + 0*4 ];
	state[ 2 + 0*4 ] = state[ 2 + 2*4 ];
	state[ 2 + 2*4 ] = temp;
	temp = state[ 2 + 1*4 ];
	state[ 2 + 1*4 ] = state[ 2 + 3*4 ];
	state[ 2 + 3*4 ] = temp;

	// Cycle fourth row right three times, ie. left once.
	temp = state[ 3 + 0*4 ];
	state[ 3 + 0*4 ] = state[ 3 + 1*4 ];
	state[ 3 + 1*4 ] = state[ 3 + 2*4 ];
	state[ 3 + 2*4 ] = state[ 3 + 3*4 ];
	state[ 3 + 3*4 ] = temp;
}

//! XOR 'count' bytes from 'buf1' and 'buf2' buffers and copy result to both buffers.
static void ICACHE_FLASH_ATTR addAndCopy( byte * buf1, byte * buf2, byte count )
{
	// Copy to temporary variables for optimization.
	byte * tempBuf1 = buf1;
	byte * tempBuf2 = buf2;
	byte tempCount = count;
	byte tempValue;

	do {
		 // Add in GF(2), ie. XOR.
		tempValue = *tempBuf1 ^ *tempBuf2;
		*tempBuf1++ = tempValue;
		*tempBuf2++ = tempValue;
	} while( --tempCount );
}

//! XOR 'count' bytes from 'constant' buffer into 'bytes' and substitute using S-Box.
static void ICACHE_FLASH_ATTR addConstantAndSubstitute( byte * bytes, const byte * constant, byte count )
{
	// Copy to temporary variables for optimization.
	byte * tempDestination = bytes;
	const byte * tempSource = constant;
	byte tempCount = count;
	byte tempValue;

	do {
		// Add in GF(2), ie. XOR.
		tempValue = *tempDestination ^ *tempSource++;
		*tempDestination++ = sBox[ tempValue ];
	} while( --tempCount );
}

//! Substitute 'count' bytes from 'bytes' using inverse S-Box, XOR with 'constant' and store in 'bytes'.
static void ICACHE_FLASH_ATTR invSubstituteAndAddConstant( byte * bytes, const byte * constant, byte count )
{
	// Copy to temporary variables for optimization.
	byte * tempDestination = bytes;
	const byte * tempSource = constant;
	byte tempCount = count;
	byte tempValue;

	do {
		// Add in GF(2), ie. XOR.
		tempValue = *tempDestination;
		*tempDestination++ = sBoxInv[ tempValue ] ^ *tempSource++;
	} while( --tempCount );
}

//! Calculates next round key from current round key in 'scheduleBuffer' and 'roundConstant'.
static void ICACHE_FLASH_ATTR keyExpansion( byte * scheduleBuffer, byte * roundConstant )
{
	byte tempWord[4];
	byte schedulePos = 0;

	// Get last word from previous schedule buffer.
	copyBytes( tempWord, scheduleBuffer + 12, 4 );

	// Transform it, since we are at a KEY_SIZE boundary in the schedule.
	cycleLeft( tempWord );
	subBytes( tempWord, 4 );
	addConstant( tempWord, roundConstant, 4 );

	// Update round constant.
	if( roundConstant[0] & 0x80 ) {
		roundConstant[0] <<= 1;
		roundConstant[0] ^= BPOLY;
	} else {
		roundConstant[0] <<= 1;
	}

	do {
		// Add value from one KEY_SIZE backwards, ie. in last schedule buffer.
		// Store in buffer, replacing old value, which was one KEY_SIZE backwards.
		addAndCopy( tempWord, scheduleBuffer, 4 );

		// Move to next word in schedule buffer.
		scheduleBuffer += 4;
		schedulePos += 4;

	} while( schedulePos < 16 );
}

//! Calculate previous round key from current round key in 'scheduleBuffer' and 'roundConstant'.
static void ICACHE_FLASH_ATTR invKeyExpansion( byte * scheduleBuffer, byte * roundConstant )
{
	byte tempWord[4];

	byte schedulePos = 12;
	scheduleBuffer += 12;

	do {
		// Add with previous word.
		addConstant( scheduleBuffer, scheduleBuffer - 4, 4 );

		// Move to previous word in schedule.
		scheduleBuffer -= 4;
		schedulePos -= 4;

	} while( schedulePos > 0 );

	// Prepare round constant for transformation that follows.
	if( (roundConstant[0] ^ BPOLY) == 0 ) {
		roundConstant[0] = 0x80;
	} else {
		roundConstant[0] >>= 1;
	}

	// Prepare transformation of previous word (which is now at end of buffer).
	copyBytes( tempWord, scheduleBuffer + 12, 4 );
	cycleLeft( tempWord );
	subBytes( tempWord, 4 );
	addConstant( tempWord, roundConstant, 4 );

	// Apply transformation result to current word.
	addConstant( scheduleBuffer, tempWord, 4 );
}

//! Prepare last round key in schedule given the initial key in 'scheduleBuffer' and 'roundConstant'.
static void ICACHE_FLASH_ATTR calcLastRoundKey( byte * scheduleBuffer )
{
	byte roundConstant[ 4 ] = { 0x01, 0x00, 0x00, 0x00 };

	byte round;
	for( round = 1; round < 10+1; round ++) {
		keyExpansion( scheduleBuffer, roundConstant );
	}
}

void ICACHE_FLASH_ATTR cipher( byte * block, const byte * key )
{
	byte scheduleBuffer[16];
	byte roundConstant[4] = { 0x01, 0x00, 0x00, 0x00 };

	copyBytes( scheduleBuffer, key, 16 );

	byte round;
	for( round = 0; round < 9; ++round ) {
		addConstantAndSubstitute( block, scheduleBuffer, 16 );
		shiftRows( block );
		mixColumns( block );

		keyExpansion( scheduleBuffer, roundConstant );
	}

	addConstantAndSubstitute( block, scheduleBuffer, 16 );
	shiftRows( block );

	keyExpansion( scheduleBuffer, roundConstant );
	addConstant( block, scheduleBuffer, 16 );
}

void ICACHE_FLASH_ATTR invCipher( byte * block, const byte * key )
{
	byte scheduleBuffer[16];
	byte roundConstant[4] = { 0x6C, 0x00, 0x00, 0x00 };

	copyBytes( scheduleBuffer, key, 16 );
	calcLastRoundKey( scheduleBuffer );

	addConstant( block, scheduleBuffer, 16 );

	byte round;
	for( round = 0; round < 9; ++round ) {
		invKeyExpansion( scheduleBuffer, roundConstant );

		invShiftRows( block );
		invSubstituteAndAddConstant( block, scheduleBuffer, 16 );
		invMixColumns( block );
	}

	invKeyExpansion( scheduleBuffer, roundConstant );

	invShiftRows( block );
	invSubstituteAndAddConstant( block, scheduleBuffer, 16 );
}
