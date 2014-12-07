#ifndef __CTRL_STACK_H
#define __CTRL_STACK_H

#include "c_types.h"

#define TMR_DATA_EXPECTER_MS	10000 // for how long should we expect data from socket in case it didn't fully arrive

typedef struct {
	unsigned short length;
	char header;
	unsigned long TXsender;
	char *data;
} tCtrlMessage;

typedef struct {
	void(*message_received)(tCtrlMessage *);
	void(*message_acked)(tCtrlMessage *);
	char(*send_data)(char *, unsigned short);
	void(*auth_response)(void);
} tCtrlCallbacks;

// CTRL Protocol Header Field bits
#define CH_SYNC 			0x01
#define CH_ACK 				0x02
#define CH_PROCESSED 		0x04
#define CH_OUT_OF_SYNC 		0x08
#define CH_NOTIFICATION 	0x10
#define CH_SYSTEM_MESSAGE 	0x20
#define CH_BACKOFF 			0x40
#define CH_SAVE_TXSERVER	0x80

// private
static unsigned short ctrl_find_message(char *, unsigned short);
static void ctrl_stack_process_message(tCtrlMessage *);
static unsigned char ctrl_stack_send_msg(tCtrlMessage *);

// public
void reverse_buffer(char *, unsigned short);
void ctrl_stack_backoff(unsigned char);
void ctrl_stack_keepalive(unsigned char);
unsigned char ctrl_stack_send(char *, unsigned short, unsigned long, unsigned char);
void ctrl_stack_recv(char *, unsigned short);
void ctrl_stack_authorize(char *, char *, unsigned char);
void ctrl_stack_init(tCtrlCallbacks *);

#endif
