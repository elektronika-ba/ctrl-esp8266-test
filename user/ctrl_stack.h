#ifndef __CTRL_STACK_H
#define __CTRL_STACK_H

#include "c_types.h"

/*
struct {
	void(*ctrl_message_extracted)(char *)
} tCallbacks;
*/

typedef struct {
	unsigned short dataLength;
	unsigned char header;
	unsigned long TXsender;
	char *data;
} tCtrlMessage;


#define CH_SYNC 			0x01;
#define CH_ACK 				0x02;
#define CK_PROCESSED 		0x04;
#define CK_OUT_OF_SYNC 		0x08;
#define CK_NOTIFICATION 	0x10;
#define CK_SYSTEM_MESSAGE 	0x20;
#define CK_BACKOFF 			0x40;
#define CK_RESERVED 		0x80;


void ctrl_stack_recv(char *, unsigned short);
void ctrl_stack_authorize(void);
void ctrl_stack_init(char *, unsigned long);

#endif
