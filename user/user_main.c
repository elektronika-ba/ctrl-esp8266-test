#include "ets_sys.h"
#include "osapi.h"

#include "user_main.h"
#include "ctrl_app_api.h"
#include "driver/uart.h"

void user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	uart0_sendStr("CTRL starting...\r\n");

	ctrl_app_api_init();

	uart0_sendStr("CTRL starting... Done.\r\n");
}
