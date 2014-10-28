#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"

#include "ctrl_stack.h"
#include "driver/uart.h"

static unsigned long TXbase;
static unsigned long TXserver;
static char baseid[32];

static char *rxbuff = NULL;

void(*ctrl_message_extracted)(char *) = NULL;

void ctrl_reg_message_extracted_cb(void *cb)
{
	ctrl_message_extracted = cb;
}

// will walk through the provided memory space and cound the number of fully available messages within
static unsigned char ICACHE_FLASH_ATTR ctrl_count_messages(char *data, unsigned short len)
{
	unsigned char cnt = 0;
	char *d = data;

	while(len > 0)
	{
		unsigned short dataLength;
		os_memcpy(&dataLength, d, 2); // hopefully endiannes will match between this compiler and NodeJS on the Cloud server
		len -= 2;
		d += 2;

		if(dataLength <= len)
		{
			len = len - dataLength;
			d = d + dataLength;

			cnt++;
		}
	}

	return cnt;
}
/*
// find first message and return its length. 0 = not found, since CTRL message always has a length (it has at least header byte)!
static unsigned short ICACHE_FLASH_ATTR ctrl_find_message(char *data, unsigned short len)
{
	char *d = data;

	while(len > 0)
	{
		unsigned short dataLength;
		os_memcpy(&dataLength, d, 2); // hopefully endiannes will match between this compiler and NodeJS on the Cloud server
		len -= 2;
		d += 2;

		if(dataLength <= len)
		{
			len = len - dataLength;
			d = d + dataLength;

			cnt++;
		}
	}

	return 0;
}
*/
// all socket data which is received is flushed into this function
void ICACHE_FLASH_ATTR ctrl_stack_recv(char *data, unsigned short len)
{
	// probaj otpakovati komandu iz tog memoriskog prostora koji nam dolazi (ali samo ako nam je buffer trenutno prazan!!!)
	// ako uspijes, alociraj novu memoriju za taj message i kopiraj je
	// ako ima pocetak narednog message-a u tom istom memoriskom prostoru njega kopiraj u buffer
	// iteriraj sad po tom bufferu da vidimo imal jos sta da se extraktuje iz nje, i ako ima za svaku extrakciju pozovi tamo callback funkciju ctrl_message_extracted(msg)
	// poslije poziva te callback funkcije oslobodi memoriju te "msg" and that's it MAYBE

	if(rxbuff == NULL)
	{
		/*unsigned char msgCount = ctrl_count_messages(data, len);
		if(msgCount>0)
		{
			char *msg = os_malloc();
		}*/
	}

/*
	// append to working buffer, allocate memory it if it doesn't exits already
	if(rxbuff == NULL)
	{
		rxbuff = (char *)os_malloc(len);
	}
	os_memcpy(rxbuff, data, len);
*/
	// if we are currently in the authorization mode, process differently
	if(0) //authMode
	{

	}
	else
	{
		// push the received message to callback
		if(ctrl_message_extracted != NULL)
		{
			ctrl_message_extracted(rxbuff); // TODO: proslijedi i rxbuff, odnosno samo komandu koju extraktujes!
		}
	}
}

// authorize connection and synchronize TXsender fields in both directions
void ICACHE_FLASH_ATTR ctrl_stack_authorize()
{
	//
}

// init the CTRL stack
void ctrl_stack_init(char *baseid_, unsigned long TXbase_)
{
	TXbase = TXbase_;
	os_memcpy(baseid, baseid_, 32);
}
