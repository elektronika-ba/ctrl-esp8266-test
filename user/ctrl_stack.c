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

void ICACHE_FLASH_ATTR reverse_buffer(char *data, unsigned short len)
{
	// http://stackoverflow.com/questions/2182002/convert-big-endian-to-little-endian-in-c-without-using-provided-func
	char *p = data;
    size_t lo, hi;
    for(lo=0, hi=len-1; hi>lo; lo++, hi--)
    {
        char tmp = p[lo];
        p[lo] = p[hi];
        p[hi] = tmp;
    }
}

// find first message and return its length. 0 = not found, since CTRL message always has a length (it has at least header byte)!
static unsigned short ICACHE_FLASH_ATTR ctrl_find_message(char *data, unsigned short len)
{
	unsigned short length;

	if(len < 2) return 0;

	os_memcpy(&length, data, 2); // hopefully endiannes will match between this compiler and NodeJS on the Cloud server
	reverse_buffer((char *)&length, 2); // nope, it doesn't so lets fix endianness now

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
		if((msg->header) & CH_SYNC)
		{
			TXserver = 0;
		}
		else
		{
			// reload TXserver from non-volatile memory
			// TODO: TXserver = load_from_flash_maybe, if we don't server will flush all pending data
		}

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
			// push the received ack to callback
			if(ctrlCallbacks->message_acked != NULL)
			{
				ctrlCallbacks->message_acked(msg);
			}
		}
		// fresh message, acknowledge and push it to the app
		else
		{
			// Reply with an ACK on received message. Mind the global backoff variable!
			tCtrlMessage ack;
			ack.header = CH_ACK;
			if(backoff)
			{
				ack.header |= CH_BACKOFF;
			}
			ack.TXsender = msg->TXsender;

			// is this NOT a notification message?
			if(!(msg->header & CH_NOTIFICATION))
			{
				if (msg->TXsender <= TXserver)
                {
                    ack.header &= ~CH_PROCESSED;
                    //uart0_sendStr("ERROR: Re-transmitted message!\r\n");
                }
                else if (msg->TXsender > (TXserver + 1))
                {
					// SYNC PROBLEM! Server sent higher than we expected! This means we missed some previous Message!
                    // This part should be handled on Server's side.
                    // Server will flush all data after receiving X successive out-of-sync messages
                    // and break the connection. Re-sync should then naturally occur
                    // in auth procedure as there would be nothing pending in queue to send to us.

                    ack.header |= CH_OUT_OF_SYNC;
                    ack.header &= ~CH_PROCESSED;
                    //uart0_sendStr("ERROR: Out-of-sync message!\r\n");
                }
                else
                {
                	ack.header |= CH_PROCESSED;
                	TXserver++; // next package we will receive should be +1 of current value, so lets ++
                }

                // send reply
                ack.length = 1+4; // fixed ACK length, without payload (data)
                ctrl_stack_send_msg(&ack);

                //uart0_sendStr("ACKed to a msg!\r\n");
			}
			else
			{
				ack.header |= CH_PROCESSED; // need this for code bellow to execute
				//uart0_sendStr("Didn't ACK because this is a notification-type msg!\r\n");
			}

			// received a message which is new and as expected?
			if(ack.header & CH_PROCESSED)
			{
				if(msg->header & CH_SYSTEM_MESSAGE)
				{
					//uart0_sendStr("Got system message - NOT IMPLEMENTED!\r\n");
				}
				else
				{
					//uart0_sendStr("Got fresh message!\r\n");

					// push the received message to callback
					if(ctrlCallbacks->message_extracted != NULL)
					{
						ctrlCallbacks->message_extracted(msg);
					}
				}
			}
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

			// Lets parse it into tCtrlMessage type
			tCtrlMessage msg;
			os_memcpy((char *)&msg.length, rxBuff+processedLen, 2);
			reverse_buffer((char *)&msg.length, 2); // lets fix endianness
			os_memcpy(&msg.header, rxBuff+processedLen+2, 1);
			os_memcpy((char *)&msg.TXsender, rxBuff+processedLen+2+1, 4);
			reverse_buffer((char *)&msg.TXsender, 4); // lets fix endianness
			msg.data = rxBuff+processedLen+2+1+4; // omg

			ctrl_stack_process_message(&msg);

			processedLen += msgLen;
			rxBuffLen = rxBuffLen-processedLen;
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

	if(rxBuffLen == 0)
	{
		if(shouldFree)
		{
			os_free(rxBuff);
		}
		rxBuff = NULL;
	}
}

// creates a message from data and sends it to Server
void ICACHE_FLASH_ATTR ctrl_stack_send(char *data, unsigned short len)
{
	tCtrlMessage msg;
	msg.header = 0;
	msg.TXsender = TXbase;

	TXbase++; // FIND A SMARTER WAY TO DO THIS!!!

	//TXbase++; // lets to it again to cause OUT OF SYNC situation and see what happens. as expected, after 3 fails we will reconnect :) cool, it works

	msg.data = data;

	msg.length = 1+4+len;

	ctrl_stack_send_msg(&msg);
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
	os_memcpy(length, &msg->length, 2);
	reverse_buffer(length, 2); // convert to LITTLE ENDIAN!
	ctrlCallbacks->send_data(length, 2);

	// Header
	ctrlCallbacks->send_data((char *)&msg->header, 1);

	// TXsender
	char TXsender[4];
	os_memcpy(TXsender, &msg->TXsender, 4);
	reverse_buffer(TXsender, 4); // convert to LITTLE ENDIAN!
	ctrlCallbacks->send_data(TXsender, 4);

	// Data (if there is data)
	if(msg->length-5 > 0)
	{
		ctrlCallbacks->send_data(msg->data, msg->length-5);
	}
}

// this sets or clears the backoff!
void ICACHE_FLASH_ATTR ctrl_stack_backoff(unsigned char backoff_)
{
	backoff = backoff_;
}

// this enables or disables the keep-alive on server's side
void ICACHE_FLASH_ATTR ctrl_stack_keepalive(unsigned char keepalive)
{
	tCtrlMessage msg;
	msg.header = CH_SYSTEM_MESSAGE | CH_NOTIFICATION; // lets set NOTIFICATION type also, because we don't need ACKs on this system command, not a VERY big problem if it doesn't get through really
	msg.TXsender = 0; // since we set NOTIFICATION type, this is not relevant

	char d = 0x03; // disable
	if(keepalive)
	{
		d = 0x02; // enable
	}
	msg.data = (char *)&d;

	msg.length = 1+4+1;

	ctrl_stack_send_msg(&msg);
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
