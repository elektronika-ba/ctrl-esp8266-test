#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"

#include "ctrl_app_api.h"
#include "driver/uart.h"

tCtrlSetup ctrlSetup;
tSysState sysState = ssIdle;
static os_timer_t tmrWifiStatus;
static os_timer_t tmrConnectionStarter;

// this function is executed in timer regularly and manages the WiFi connection (reconnects in case of disconnect and so on)
void ICACHE_FLASH_ATTR wifi_status(void *arg)
{
	os_timer_disarm(&tmrWifiStatus);

	uint8 state = wifi_station_get_connect_status();
	switch(state)
	{
		case STATION_IDLE:
			uart0_sendStr("WIFI: Station Idle!\r\n");
			sysState = ssIdle;
			break;

		case STATION_WRONG_PASSWORD:
			uart0_sendStr("WIFI: Wrong Password!\r\n");
			sysState = ssError;
			break;

		case STATION_NO_AP_FOUND:
			uart0_sendStr("WIFI: No AP found!\r\n");
			sysState = ssError;
			break;

		case STATION_CONNECT_FAIL:
			uart0_sendStr("WIFI: Connect Failed!\r\n");
			sysState = ssError;
			break;

		case STATION_GOT_IP:
			uart0_sendStr("WIFI: Got IP!\r\n");
			sysState = ssGotIp;
			break;

		default:
			uart0_sendStr("WIFI: Default switch...\r\n");
	}

	// TODO: report tSysState to some status LED
	// yep...

	os_timer_arm(&tmrWifiStatus, WIFI_STATUS_INTERVAL_MS, 0);
}

// this function is executed in timer until a TCP socket connection is started and then stopped
void ICACHE_FLASH_ATTR connection_starter(void *arg)
{
	os_timer_disarm(&tmrConnectionStarter);
/*
	// finally got WIFI connection?
	if(sysState == ssGotIp)
	{
		struct espconn pCon;

		pCon.type = ESPCONN_TCP;
		pCon.state = ESPCONN_NONE;
		pCon.proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
		pCon.proto.tcp->local_port = espconn_port();
		pCon.proto.tcp->remote_port = ctrlSetup.serverPort;

		os_memcpy(&(pCon.proto.tcp->remote_ip), &(ctrlSetup.serverIp), 4);

		//espconn_regist_connectcb(&pCon, at_tcpclient_connect_cb);
		//espconn_regist_reconcb(&pCon, at_tcpclient_recon_cb);

		espconn_connect(pCon);
	}
	else
	{
		os_timer_arm(&tmrConnectionStarter, CONNECTION_STARTER_INTERVAL_MS, 0);
	}
*/
}

// entry point to the app
void ICACHE_FLASH_ATTR ctrl_app_api_init(void)
{
	/*
	// load system setup parameters
	load_user_param(USER_PARAM_SEC_1, 0, &ctrlSetup, sizeof(tCtrlSetup));
	if(ctrlSetup._ok != USER_PARAM_EXISTS)
	{
		// load default... actually go into SETUP mode by starting a web server TODO
		//ctrl_init_setup_webserver();
		uart0_sendStr("No Setup, loading defaults...\r\n");
	}
	else
	{
		uart0_sendStr("Loaded Setup from FLASH.\r\n");
	*/
		// debug section
		uart0_sendStr("Loading debugging values.\r\n");

		os_sprintf(ctrlSetup.ssid, "%s", "linksys");
		os_sprintf(ctrlSetup.password, "%s", "nobodyknowsit");

		ctrlSetup.opMode = 1; // STATION_MODE

		ctrlSetup.serverIp[0] = 78;
		ctrlSetup.serverIp[1] = 47;
		ctrlSetup.serverIp[2] = 48;
		ctrlSetup.serverIp[3] = 138;

		ctrlSetup.serverPort = 8000;
		// end debug section

		char temp[255];

		ctrlSetup.ssid[32] = 0; // null-terminate for sprintf bellow in case ssid is 32 bytes long
		ctrlSetup.password[64] = 0;

		uart0_sendStr("Setup parameters loaded: ");
		os_sprintf(temp, "SSID: %s, PWD: %s, MODE: %u\r\n", ctrlSetup.ssid, ctrlSetup.password, ctrlSetup.opMode);
		uart0_sendStr(temp);

		wifi_set_opmode(ctrlSetup.opMode);

		struct station_config stationConf;
		os_memcpy(&stationConf.ssid, &ctrlSetup.ssid, 32);
		os_memcpy(&stationConf.password, &ctrlSetup.password, 64);
		wifi_station_set_config(&stationConf);

		uart0_sendStr("Connecting to WIFI...");
		uart0_sendStr(temp);

		wifi_station_connect(); // this line restarts my chip...

		// set a timer to check for wifi connection state
		os_timer_disarm(&tmrWifiStatus);
		os_timer_setfn(&tmrWifiStatus, (os_timer_func_t *)wifi_status, NULL);
		os_timer_arm(&tmrWifiStatus, WIFI_STATUS_INTERVAL_MS, 0); // 0=we will repeat it on our own
/*
		// set a timer to check when wifi is connected and to start TCP socket connection to CTRL server. I still don't know if I will need this...
		os_timer_disarm(&tmrConnectionStarter);
		os_timer_setfn(&tmrConnectionStarter, (os_timer_func_t *)connection_starter, NULL);
		os_timer_arm(&tmrConnectionStarter, CONNECTION_STARTER_INTERVAL_MS, 0); // 0=we will repeat it on our own
*/
	/*
	}
	*/
}
