#include "ets_sys.h"
#include "osapi.h"

#include "user_main.h"
#include "ctrl_platform.h"
#include "driver/uart.h"

void user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	uart0_sendStr("CTRL platform starting...\r\n");

	ctrl_platform_init();

	uart0_sendStr("CTRL platform started. Now callbacks rule!\r\n");
}
