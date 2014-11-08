/*
#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "driver/uart.h"

#include "ctrl_app.h"
#include "ctrl_platform.h"
#include "ctrl_stack.h"

// DO NOT CHANGE THIS FILE, THIS SHOULD BE A TEMPLATE FOR YOUR OWN APP.

static unsigned char ICACHE_FLASH_ATTR ctrl_app_message_received(tCtrlMessage *msg)
{
	// app receives MSG

	return 0; // 0=OK, 1=SERVER NEEDS TO BACKOFF AND RESEND THIS MESSAGE AGAIN
}

void ICACHE_FLASH_ATTR ctrl_app_init(tCtrlAppCallbacks *ctrlAppCallbacks)
{
	ctrlAppCallbacks->message_received = &ctrl_app_message_received;

	// your custom code here...
}
*/