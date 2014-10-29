#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"

#include "ctrl_stack.h"
#include "driver/uart.h"

static unsigned long TXbase;
static unsigned long TXserver;
static char *baseid;
static char *rxBuff = NULL;
static unsigned short rxBuffLen;
static unsigned char authMode;
static tCtrlCallbacks *ctrlCallbacks;
static unsigned char backoff;

// find first message and return its length. 0 = not found, since CTRL message always has a length (it has at least header byte)!
static unsigned short ICACHE_FLASH_ATTR ctrl_find_message(char *data, unsigned short len)
{
	unsigned short length;

	if(len < 2) return 0;

	os_memcpy(&length, data, 2); // hopefully endiannes will match between this compiler and NodeJS on the Cloud server

	// entire message available in buffer?
	if(length <= len)
	{
		return length + 2; // +2 because Length field of CTRL protocol is 2 bytes long
	}

	return 0;
}

// internal function to process extracted message from the received socket stream
static void ICACHE_FLASH_ATTR ctrl_stack_process_message(tCtrlMessage *msg)
{
	// if we are currently in the authorization mode, process received data differently
	if(authMode)
	{
		if(ctrlCallbacks->auth_response != NULL)
		{
			ctrlCallbacks->auth_response(*(msg->data));
		}
		authMode = 0;
	}
	else
	{
		// we received an ACK?
		if((msg->header) & CH_ACK)
		{

		}
		// fresh message, lets take it and reply with an ACK
		else
		{

		}

		// push the received message to callback
		if(ctrlCallbacks->message_extracted != NULL)
		{
			ctrlCallbacks->message_extracted(msg);
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
		char *newRxBuff = (char *)os_malloc(rxBuffLen+len);
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
			//char *msg = (char *)os_malloc(msgLen);
			//os_memcpy(msg, rxBuff+processedLen, msgLen);

			// Lets parse it into tCtrlMessage type
			tCtrlMessage msg;
			os_memcpy(msg.length, rxBuff+processedLen, 2);
			os_memcpy(msg.header, rxBuff+processedLen+2, 1);
			os_memcpy(msg.TXsender, rxBuff+processedLen+2+1, 4);
			msg.data = rxBuff+processedLen+2+1+4; // omg

			ctrl_stack_process_message(&msg);

			//os_free(msg);
			processedLen += msgLen;
		}
		else
		{
			// has remaining data in buffer (beginning of another message but not entire message)
			if(rxBuffLen-processedLen > 0)
			{
				char *newRxBuff = (char *)os_malloc(rxBuffLen-processedLen);
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
static void ICACHE_FLASH_ATTR ctrl_stack_send_msg(tCtrlMessage *msg)
{
	if(ctrlCallbacks->send_data == NULL)
	{
		return;
	}

	// Length
	char length[2];
	os_memcpy(length, msg->length, 2);
	ctrlCallbacks->send_data(length, 2);

	// Header
	ctrlCallbacks->send_data(&msg->header, 1);

	// TXsender
	char TXsender[4];
	os_memcpy(TXsender, msg->TXsender, 4);
	ctrlCallbacks->send_data(TXsender, 4);

	// Data
	ctrlCallbacks->send_data(msg->data, msg->length-5);
}

// this sets or clears the backoff!
void ICACHE_FLASH_ATTR ctrl_stack_backoff(unsigned char backoff_)
{
	backoff = backoff_;
}

// authorize connection and synchronize TXsender fields in both directions
void ICACHE_FLASH_ATTR ctrl_stack_authorize(char *baseid_, unsigned long TXbase_)
{
	TXbase = TXbase_;
	baseid = baseid_;

	authMode = 1; // used in our local ctrl_stack_process_message() to know how to parse incoming data from server

	tCtrlMessage msg;
	msg.length = 1 + 4 + 16;
	msg.header = 0x00;
	msg.TXsender = 0; // not relevant during authentication procedure
	msg.data = baseid; //(char *)os_malloc(16); os_memcpy(dest, baseid, 16);

	if(TXbase == 1)
	{
		msg.header |= CH_SYNC;
	}

	ctrl_stack_send_msg(&msg);
}

// init the CTRL stack
void ICACHE_FLASH_ATTR ctrl_stack_init(tCtrlCallbacks *cc)
{
	ctrlCallbacks = cc;
}
