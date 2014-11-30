// This file has been prepared for Doxygen automatic documentation generation.
/*! \file ********************************************************************
*
* \brief  Header file for AES public functions.
*
* This file contains the function prototypes for the AES
* algorithm. The implementation is found in the aes.c file.
* It supports encryption and decryption using on-the-fly calculation of
* the key schedule. Precalculation of key schedules for all associated
* transmitters' secret keys would use too much memory.
* Encryption is used for generating the CMAC while decryption is used
* when in learn mode and a transmitter's secret key is encrypted using the
* system's shared key. Note that the last round key of the key schedule
* must be prepared using prepareInvCipher() before decrypting using the
* invCipher() function. Encryption using the cipher() funciton is
* straight-forward and only needs an SRAM workspace and the encryption key.
*
* - File:              aes.h
* - Compiler:          IAR EWAVR 4.11B
* - Supported devices: ATtiny45/85
* - AppNote:           AVR411 - Secure Rolling Code Algorithm
*                      for Wireless Link
*
* \author              Atmel Corporation: http://www.atmel.com \n
*                      Support email: avr@atmel.com
*
* $Name:  $
* $Revision: 1193 $
* $Date: 2006-10-31 14:21:08 +0100 (ti, 31 okt 2006) $
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
#ifndef AES_H
#define AES_H

#include "c_types.h"

typedef uint8_t byte; //!< Handy typedef. Very readable.

//! Encrypt data block with on-the-fly calculation of key schedule in internal temp buffer
void cipher( byte *, const byte *);
//! Decrypt data block by also preparing the key schedule state in internal temp buffer
void invCipher( byte *, const byte *);

#endif
