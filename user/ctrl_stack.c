#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h" // only for ESPCONN_OK enum
#include "driver/uart.h"

#include "ctrl_stack.h"

os_timer_t tmrDataExpecter;

static unsigned long TXserver;
static char *baseid;
static char *aes128Key;
static char *rxBuff = NULL;
static unsigned short rxBuffLen;
static unsigned char authMode;
static tCtrlCallbacks *ctrlCallbacks;
static unsigned char backoff;

// for changing endianness
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
			if(ctrlCallbacks->restore_TXserver != NULL)
			{
				TXserver = ctrlCallbacks->restore_TXserver();
			}
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
                    //uart0_sendStr("ERROR: Re-transmitted message, ignoring!\r\n");
                }
                else if (msg->TXsender > (TXserver + 1))
                {
					// SYNC PROBLEM! Server sent higher than we expected! This means we missed some previous Message!
                    // This part should be handled on Server's side.
                    // Server will flush all data after receiving X successive out-of-sync messages from us,
                    // and he will break the connection. Re-sync should then naturally occur
                    // in auth procedure as there would be nothing pending in queue to send to us.

                    ack.header |= CH_OUT_OF_SYNC;
                    ack.header &= ~CH_PROCESSED;
                    //uart0_sendStr("ERROR: Out-of-sync message!\r\n");
                }
                else
                {
                	ack.header |= CH_PROCESSED;
                	TXserver++; // next package we will receive must be +1 of current value, so lets ++

					// maybe there is a callback defined that will save this value in case we get power loss so server doesn't have to flush the pending queue when connection gets restored
                	if(ctrlCallbacks->save_TXserver != NULL)
                	{
						ctrlCallbacks->save_TXserver(TXserver);
					}
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
					if(ctrlCallbacks->message_received != NULL)
					{
						ctrlCallbacks->message_received(msg);
					}
				}
			}
		}
	}
}

// data expecter timeout, in case it triggers things aren't going well
static void ICACHE_FLASH_ATTR data_expecter_timeout(void *arg)
{
	uart0_sendStr("data_expecter_timeout() - FLUSH RX BUFF\r\n");
	if(rxBuff != NULL)
	{
		os_free(rxBuff);
		rxBuff = NULL;
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
			os_timer_disarm(&tmrDataExpecter);

			// TODO: When everything is finished, add DECRYPTION function here that will decrypt HEADER+TXSENDER+DATA buffer stream.
			// "Length" part of the message can't be encrypted because message stream might come in segmented TCP packages.
			// Who cares if they can see the length of the message anyway, right? They can easily calculate the length by simply counting
			// the bytes that arrive, but the CTRL stack (we) can't rely on that as data might arrive segmented, as previously noted.

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
				os_timer_disarm(&tmrDataExpecter);
				os_timer_arm(&tmrDataExpecter, TMR_DATA_EXPECTER_MS, 0); // 0 = do not repeat automatically

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
		//os_timer_disarm(&tmrDataExpecter); // TODO: see if required here as well?
		if(shouldFree)
		{
			os_free(rxBuff);
		}
		rxBuff = NULL;
	}
}

// creates a message from data and sends it to Server
unsigned char ICACHE_FLASH_ATTR ctrl_stack_send(char *data, unsigned short len, unsigned long TXbase, unsigned char notification)
{
	tCtrlMessage msg;
	msg.header = 0;

	if(notification)
	{
		msg.header |= CH_NOTIFICATION;
	}

	msg.TXsender = TXbase;

	msg.data = data;

	msg.length = 1+4+len;

	return ctrl_stack_send_msg(&msg);
}

// calls a pre-set callback that sends data to socket
// returns: 1 on error, 0 on success
static unsigned char ICACHE_FLASH_ATTR ctrl_stack_send_msg(tCtrlMessage *msg)
{
	if(ctrlCallbacks->send_data == NULL)
	{
		return 1;
	}

	// Length
	char length[2];
	os_memcpy(length, &msg->length, 2);
	reverse_buffer(length, 2); // convert to LITTLE ENDIAN!
	if(ctrlCallbacks->send_data(length, 2) != ESPCONN_OK)
	{
		return 1; // abort further sending
	}

	// TODO: Once everything is finished, add ENCRYPTION here. Data to be encrypted is Header+TXsender+Data. Length is not encrypted!
	// I will probably have to concatenate the data bellow into one byte-stream for encryption procedure to be possible.

	// Header
	if(ctrlCallbacks->send_data((char *)&msg->header, 1) != ESPCONN_OK)
	{
		return 1; // abort further sending
	}

	// TXsender
	char TXsender[4];
	os_memcpy(TXsender, &msg->TXsender, 4);
	reverse_buffer(TXsender, 4); // convert to LITTLE ENDIAN!
	if(ctrlCallbacks->send_data(TXsender, 4) != ESPCONN_OK)
	{
		return 1; // abort further sending
	}

	// Data (if there is data)
	if(msg->length-5 > 0)
	{
		if(ctrlCallbacks->send_data(msg->data, msg->length-5))
		{
			return 1;
		}
	}

	return 0;
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
void ICACHE_FLASH_ATTR ctrl_stack_authorize(char *baseid_, char *aes128Key_, unsigned char sync)
{
	baseid = baseid_;
	aes128Key = aes128Key_;

	authMode = 1; // used in our local ctrl_stack_process_message() to know how to parse incoming data from server

	tCtrlMessage msg;
	msg.length = 1 + 4 + 16;
	msg.header = 0x00;
	msg.TXsender = 0; // not relevant during authentication procedure
	msg.data = baseid; //(char *)os_malloc(16); os_memcpy(msg.data, baseid, 16);

	// We have nothing pending to send? (TXbase is 1 in that case)
	if(sync == 1)
	{
		msg.header |= CH_SYNC;
	}

	// In case we already have something partial in rxBuff, we must free it since the remaining partial data will never arrive.
	// We will never have this != NULL in case there was a full message available there, because it would be parsed at the time
	// it arrived into this buffer!
	if(rxBuff != NULL)
	{
		os_timer_disarm(&tmrDataExpecter);
		os_free(rxBuff);
		rxBuff = NULL;
	}

	ctrl_stack_send_msg(&msg);
}

// init the CTRL stack
void ICACHE_FLASH_ATTR ctrl_stack_init(tCtrlCallbacks *cc)
{
	ctrlCallbacks = cc;

	os_timer_disarm(&tmrDataExpecter);
	os_timer_setfn(&tmrDataExpecter, (os_timer_func_t *)data_expecter_timeout, NULL);
}
