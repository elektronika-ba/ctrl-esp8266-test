#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"

#include "temperature_logger.h"
#include "ctrl_platform.h"
#include "driver/uart.h"

os_timer_t tmr;

static void ICACHE_FLASH_ATTR temperature_logger_simulate(void *arg)
{
	unsigned long temper;
	temper = rand();

	reverse_buffer((char *)&temper, 4); // fix endianness

	/*
	char temp[50];
	os_sprintf(temp, "TEMPER: 0x%X\r\n", temper);
	uart0_sendStr(temp);
	*/

	// send via CTRL stack to Server
	if(ctrl_platform_send((char *)&temper, 4, 0))
	{
		uart0_sendStr("> Failed to send temperature!\r\n");
		//os_timer_disarm(&tmr);
	}
	else
	{
		uart0_sendStr("> Temperature sent.\r\n");
	}
}

// entry point to the temperature logger app
void ICACHE_FLASH_ATTR temperature_logger_init(void)
{
	os_timer_disarm(&tmr);
	os_timer_setfn(&tmr, (os_timer_func_t *)temperature_logger_simulate, NULL);
	os_timer_arm(&tmr, 5000, 1); // 1 = repeat automatically
}
