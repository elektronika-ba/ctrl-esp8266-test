#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"

#include "ctrl_platform.h"
#include "ctrl_config_server.h"
#include "driver/uart.h"

os_timer_t tmrWifiStatus;

// this function is executed in timer regularly and manages the WiFi connection
void ICACHE_FLASH_ATTR wifi_status(void *arg)
{
	os_timer_disarm(&tmrWifiStatus);

	uint8 state = wifi_station_get_connect_status();

	os_timer_arm(&tmrWifiStatus, TMR_WIFI_STATUS_MS, 0);
}

// entry point to the ctrl platform
void ICACHE_FLASH_ATTR ctrl_platform_init(void)
{
	struct station_config stationConf;
	tCtrlSetup ctrlSetup;

#ifndef CTRL_DEBUG
	load_user_param(USER_PARAM_SEC_1, 0, &ctrlSetup, sizeof(tCtrlSetup));
	wifi_station_get_config(&stationConf);
#else
	uart0_sendStr("Debugging, will not start configuration web server.\r\n");

	ctrlSetup.setupOk = SETUP_OK_KEY;

	ctrlSetup.serverIp[0] = 78; // ctrlSetup.serverIp = {78, 47, 48, 138};
	ctrlSetup.serverIp[1] = 47;
	ctrlSetup.serverIp[2] = 48;
	ctrlSetup.serverIp[3] = 138;

	ctrlSetup.serverPort = 8000;

	os_memset(stationConf.ssid, 0, sizeof(stationConf.ssid));
	os_memset(stationConf.password, 0, sizeof(stationConf.password));

	os_sprintf(stationConf.ssid, "%s", "myssid");
	os_sprintf(stationConf.password, "%s", "mypass");

	wifi_station_set_config(&stationConf);
#endif

	if(ctrlSetup.setupOk != SETUP_OK_KEY || wifi_get_opmopde() == SOFTAP || stationConf.ssid == 0)
	{
#ifdef CTRL_DEBUG
		uart0_sendStr("I am in SOFTAP, restarting into STATION mode...\r\n");
		wifi_set_opmode(STATION);
		system_restart();
#endif
		uart0_sendStr("Starting configuration web server...\r\n");

		// The device is now booted into "configuration mode" so it acts as Access Point
		// and starts a web server to accept connection from browser.
		// After the user configures the device, the browser will change the configuration
		// of the ESP device and reboot into normal-working mode. When and if user wants
		// to make additional changes to the device, it must be booted into "configuration
		// mode" by pressing a button on the device which will restart it and get in here.
		ctrl_config_server_init();
	}
	else
	{
		uart0_sendStr("Starting in normal mode...\r\n");

		// The device is now booted into "normal mode" where it acts as a STATION or STATIONAP.
		// It connects to the CTRL IoT platform via TCP socket connection. All incoming data is
		// then forwarded into the CTRL Stack and the stack will then call a callback function
		// for every received CTRL packet. That callback function is the actuall business-end
		// of the user-application.

		char temp[100];

		os_sprintf(temp, "MODE: %u [1=STATION, 3=STATION+AP]\r\n", wifi_get_opmopde());
		uart0_sendStr(temp);
		os_sprintf(temp, "SSID: %s\r\n", stationConf.ssid);
		uart0_sendStr(temp);
#ifdef CTRL_DEBUG
		os_sprintf(temp, "PWD: %s\r\n", stationConf.password);
		uart0_sendStr(temp);
#endif

#if 0
		// TODO: if we are in STATIONAP mode, do we need to set softap parameters in order for
		// another nearby ESP module to connect to us??? (Mesh option they advertised in Chinese PDF)
#endif

		uart0_sendStr("Connecting to WIFI...");

		// set a timer to check wifi connection progress
		os_timer_disarm(&tmrWifiStatus);
		os_timer_setfn(&tmrWifiStatus, (os_timer_func_t *)wifi_status, NULL);
		os_timer_arm(&tmrWifiStatus, TMR_WIFI_STATUS_MS, 0); // 0=we will repeat it on our own
	}
}
