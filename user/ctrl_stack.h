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

void ctrl_stack_recv(char *, unsigned short);
void ctrl_stack_authorize(void);
void ctrl_stack_init(char *, unsigned long);

#endif
