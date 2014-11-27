#include "ets_sys.h"
#include "osapi.h"

#include "user_main.h"
#include "ctrl_platform.h"
#include "driver/uart.h"

void user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	#ifdef CTRL_LOGGING
		uart0_sendStr("CTRL platform starting...\r\n");
	#endif

	ctrl_platform_init();

	#ifdef CTRL_LOGGING
		uart0_sendStr("CTRL platform started!\r\n");
	#endif
}
