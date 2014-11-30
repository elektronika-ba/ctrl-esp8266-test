#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"

#include "aes.h"
#include "aes_cbc.h"

// AES 128 in CBC-mode (as seen here: https://polarssl.org/aes-source-code)
// "data" must be prepared in 16 byte blocks (16, 32, 48, ...)!
void ICACHE_FLASH_ATTR aes128_cbc_encrypt(unsigned char *data, unsigned int length, const char *key)
{
	unsigned char iv[16];
	unsigned int i;

	os_memset(iv, 0, 16);

	while (length > 0)
	{
		for( i = 0; i < 16; i++ )
		{
			data[i] = (unsigned char)( data[i] ^ iv[i] );
		}

		cipher(data, key);
		os_memcpy(iv, data, 16);

		data += 16;
		length -= 16;
	}
}

// AES 128 in CBC-mode (as seen here: https://polarssl.org/aes-source-code)
// "data" must be prepared in 16 byte blocks (16, 32, 48, ...)!
void ICACHE_FLASH_ATTR aes128_cbc_decrypt(unsigned char *data, unsigned int length, const char *key)
{
	unsigned char iv[16];
	unsigned char temp[16];
	unsigned int i;

	os_memset(iv, 0, 16);

	while (length > 0)
	{
		os_memcpy(temp, data, 16);

		invCipher(data, key);

		for( i = 0; i < 16; i++ )
		{
			data[i] = (unsigned char)( data[i] ^ iv[i] );
		}

		os_memcpy(iv, temp, 16);

		data += 16;
		length -= 16;
	}
}
