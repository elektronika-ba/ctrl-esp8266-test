#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"
#include "driver/uart.h"

#include "ctrl_app_temperature_simulator.h"
#include "ctrl_platform.h"
#include "ctrl_stack.h"

os_timer_t tmr;

static void ICACHE_FLASH_ATTR ctrl_app_temperature_simulator_simulate(void *arg)
{
	unsigned long temper;
	temper = rand();

	// send via CTRL stack to Server
	if(ctrl_platform_send((char *)&temper, 4, 0)) // send as notification
	{
		#ifdef CTRL_LOGGING
		os_printf("> Failed to send the temperature!\r\n");
		#endif
	}
	else
	{
		#ifdef CTRL_LOGGING
		os_printf("> Temperature sent :)\r\n");
		#endif
	}
}

static unsigned char ICACHE_FLASH_ATTR ctrl_app_message_received(tCtrlMessage *msg)
{
	// my custom app receives a MSG!
	#ifdef CTRL_LOGGING
	os_printf("APP GOT NEW MSG: ");

	unsigned short i;
	for(i=0; i<msg->length-1-4; i++)
	{
		char tmp2[10];
		os_sprintf(tmp2, " 0x%X", msg->data[i]);
		os_printf(tmp2);
	}
	os_printf(".\r\n");
	#endif

	return 0; // lets say everything is OK and we don't need the Server to backoff next message (and resend this one too)
}

// entry point to the temperature logger app
void ICACHE_FLASH_ATTR ctrl_app_init(tCtrlAppCallbacks *ctrlAppCallbacks)
{
	ctrlAppCallbacks->message_received = ctrl_app_message_received;

	#ifdef CTRL_LOGGING
	os_printf("ctrl_app_init()\r\n");
	#endif

	os_timer_disarm(&tmr);
	os_timer_setfn(&tmr, (os_timer_func_t *)ctrl_app_temperature_simulator_simulate, NULL);
	os_timer_arm(&tmr, 5000, 1); // 1 = repeat automatically
}
