#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"

#include "ctrl_stack.h"
#include "driver/uart.h"

static unsigned long TXbase;
static unsigned long TXserver;
static char baseid[32];
static char *rxBuff = NULL;
static unsigned short rxBuffLen;
static unsigned char authMode;

void(*ctrl_message_extracted)(char *) = NULL;
void(*ctrl_send_data)(char *, unsigned short) = NULL;

// registers a callback fn that will be called with the extracted message
void ctrl_reg_message_extracted_cb(void *cb)
{
	ctrl_message_extracted = cb;
}

/*
// will walk through the provided memory space and cound the number of fully available messages within
static unsigned char ICACHE_FLASH_ATTR ctrl_count_messages(char *data, unsigned short len)
{
	unsigned char cnt = 0;
	char *d = data;

	if(len < 2) return 0;

	while(len > 0)
	{
		unsigned short dataLength;
		os_memcpy(&dataLength, d, 2); // hopefully endiannes will match between this compiler and NodeJS on the Cloud server
		len -= 2;
		d += 2;

		// entire message available in buffer?
		if(dataLength <= len)
		{
			len = len - dataLength;
			d = d + dataLength;

			cnt++;
		}
	}

	return cnt;
}
*/

// find first message and return its length. 0 = not found, since CTRL message always has a length (it has at least header byte)!
static unsigned short ICACHE_FLASH_ATTR ctrl_find_message(char *data, unsigned short len)
{
	unsigned short dataLength;

	if(len < 2) return 0;

	os_memcpy(&dataLength, data, 2); // hopefully endiannes will match between this compiler and NodeJS on the Cloud server

	// entire message available in buffer?
	if(dataLength <= len)
	{
		return dataLength + 2; // +2 because Length field of CTRL protocol is 2 bytes long
	}

	return 0;
}

// internal function to process extracted message from the received socket stream
static void ICACHE_FLASH_ATTR ctrl_stack_process_message(char *data, unsigned short len)
{
	// if we are currently in the authorization mode, process received data differently
	if(authMode)
	{
		// process answer to our authentication message
		// TODO:
	}
	else
	{
		// push the received message to callback
		if(ctrl_message_extracted != NULL)
		{
			ctrl_message_extracted(data, len);
		}
	}
}

// all socket data which is received is flushed into this function
void ICACHE_FLASH_ATTR ctrl_stack_recv(char *data, unsigned short len)
{
	// Optimised for RAM memory usage. Reallocating memory only if messages
	// come in multiple TCP segments (multiple calls of this function for one CTRL message)

	unsigned char shouldFree = 0;

	if(rxBuff == NULL)
	{
		// fresh data arrived
		rxBuff = data;
		rxBuffLen = len;
	}
	else
	{
		// concatenate
		shouldFree = 1;
		char *newRxBuff = (char *)malloc(rxBuffLen+len);
		os_memcpy(newRxBuff, rxBuff, rxBuffLen);
		os_memcpy(newRxBuff+rxBuffLen, data, len);
		os_free(rxBuff);

		rxBuff = newRxBuff;
		rxBuffLen = rxBuffLen+len;
	}

	// process data we have in rxBuff buffer
	unsigned short processedLen = 0;
	while(processedLen < rxBuffLen)
	{
		unsigned short msgLen = ctrl_find_message(rxBuff+processedLen, rxBuffLen-processedLen);
		if(msgLen > 0)
		{
			// entire message found in buffer, lets process it
			char *msg = (char *)os_malloc(msgLen);
			os_memcpy(msg, rxBuff+processedLen, msgLen);
			ctrl_stack_process_message(msg, msgLen);
			os_free(msg);
			processedLen += msgLen;
		}
		else
		{
			// has remaining data in buffer (beginning of another message but not entire message)
			if(rxBuffLen-processedLen > 0)
			{
				newRxBuff = (char *)os_malloc(rxBuffLen-processedLen);
				os_memcpy(newRxBuff, rxBuff+processedLen, rxBuffLen-processedLen);
				if(shouldFree)
				{
					os_free(rxBuff);
				}

				rxBuff = newRxBuff;
				rxBuffLen = rxBuffLen-processedLen;
			}
			break;
		}
	}

	if(shouldFree && rxBuffLen == 0)
	{
		os_free(rxBuff);
		rxBuff = NULL;
	}
}

// calls a pre-set callback that sends data to socket
void ICACHE_FLASH_ATTR ctrl_stack_send(tCtrlMessage *msg)
{
	if(ctrl_send_data == NULL)
	{
		return;
	}

	// Length
	char dataLength[2];
	os_memcpy(dataLength, msg->dataLength, 2);
	ctrl_send_data(dataLength, 2);

	// Header
	ctrl_send_data(msg->header, 1);

	// TXsender
	char TXsender[4];
	os_memcpy(TXsender, msg->TXsender, 4);
	ctrl_send_data(TXsender, 4);

	ctrl_send_data(msg->data, msg->dataLength-5);
}

// authorize connection and synchronize TXsender fields in both directions
void ICACHE_FLASH_ATTR ctrl_stack_authorize()
{
	tCtrlMessage msg;
	msg.dataLength = 1 + 4 + 16;
	msg.header = 0x00;
	msg.TXsender = 0; // not important during authentication procedure
	msg.data = (char *)malloc(16);

	if(TXbase == 0)
	{
		msg.header |= CH_SYNC;
	}

	ctrl_stack_send(&msg);
}

// init the CTRL stack
void ctrl_stack_init(char *baseid_, unsigned long TXbase_)
{
	TXbase = TXbase_;
	os_memcpy(baseid, baseid_, 32);
}
