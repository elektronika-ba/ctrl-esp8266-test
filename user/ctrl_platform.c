#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"

#include "ctrl_platform.h"
#include "ctrl_stack.h"
#include "ctrl_config_server.h"
#include "driver/uart.h"

os_timer_t tmrSysStatusChecker;
os_timer_t tmrSysStatusLedBlinker;
tCtrlSetup ctrlSetup;
struct espconn *ctrlConn;

// blinking status led according to the status of wifi and tcp connection
void ICACHE_FLASH_ATTR sys_status_led_blinker(void *arg)
{
	static unsigned char ledStatusState;

	if(ledStatusState % 2) // & 0x01
	{
		// LED ON
	}
	else
	{
		// LED OFF
	}

	ledStatusState++;
}

// this function is executed in timer regularly and manages the WiFi connection
void ICACHE_FLASH_ATTR sys_status_checker(void *arg)
{
	static char prevWifiState = STATION_IDLE;
	static enum espconn_state prevConnState = ESPCONN_NONE;	

	unsigned int tmrInterval;
	enum espconn_state connState = ESPCONN_NONE;
	if(ctrlConn != NULL)
	{
		connState = ctrlConn->state;
	}

	if(connState != ESPCONN_NONE && connState != ESPCONN_CLOSE && wifi_station_get_connect_status() == STATION_GOT_IP) // also check for WIFI state here, because TCP connection might think it is still connected even after WIFI loses the connection with AP
	{
		// TCP connection is established, set timer if it has changed since the last time we've set it
		tmrInterval = 1500;
		uart0_sendStr("\tSTATE = TCP CONNECTED\r\n");
	}
	else
	{
		// WIFI connected, but still not TCP connected
		if(wifi_station_get_connect_status() == STATION_GOT_IP)
		{
			tmrInterval = 500;
			uart0_sendStr("\tSTATE = WIFI CONNECTED\r\n");

			// check TCP connection status and initiate connecting procedure if not connected
			if(connState == ESPCONN_NONE || connState == ESPCONN_CLOSE)
			{
				uart0_sendStr("\tSTATE = recreating TCP...\r\n");
				ctrl_connection_recreate();
				espconn_connect(ctrlConn);
			}
		}
		// WIFI not connected (yet)
		else
		{
			tmrInterval = 200;
			uart0_sendStr("\tSTATE = NO WIFI YET\r\n");

			// TCP thinks it is still connected? Disconnect it!
			if(connState != ESPCONN_NONE && connState != ESPCONN_CLOSE)
			{
				uart0_sendStr("\tSTATE = disconnecting TCP...\r\n");
				espconn_disconnect(ctrlConn); // will not cause a problem if ctrlConn is NULL here... also, this fn will os_free() the ctrlConn for us
			}
		}
	}

	// change timer timeout?
	if(prevWifiState != wifi_station_get_connect_status() || prevConnState != connState)
	{
		os_timer_disarm(&tmrSysStatusLedBlinker);
		os_timer_arm(&tmrSysStatusLedBlinker, tmrInterval, 1);
	}

	prevWifiState = wifi_station_get_connect_status();
	prevConnState = connState;
}

static void ICACHE_FLASH_ATTR ctrl_connection_recreate(void)
{
	espconn_disconnect(ctrlConn); // will not cause a problem if ctrlConn is NULL here... also, this fn will os_free() the ctrlConn for us

	enum espconn_type linkType = ESPCONN_TCP;
	ctrlConn = (struct espconn *)os_zalloc(sizeof(struct espconn));
	if(ctrlConn == NULL)
	{
		os_timer_disarm(&tmrSysStatusLedBlinker);
		os_timer_setfn(&tmrSysStatusLedBlinker, (os_timer_func_t *)sys_status_led_blinker, NULL);
		os_timer_arm(&tmrSysStatusLedBlinker, 100, 1); // 1 = repeat automatically
		uart0_sendStr("Failed to os_zalloc() for espconn. Looping here forever!\r\n");
		while(1);
	}

	ctrlConn->type = linkType;
	ctrlConn->state = ESPCONN_NONE;
	ctrlConn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	ctrlConn->proto.tcp->local_port = espconn_port();
	ctrlConn->proto.tcp->remote_port = ctrlSetup.serverPort;
	os_memcpy(ctrlConn->proto.tcp->remote_ip, ctrlSetup.serverIp, 4);
	ctrlConn->reverse = NULL; // don't need this, right?
	espconn_regist_connectcb(ctrlConn, tcpclient_connect_cb);
	espconn_regist_reconcb(ctrlConn, tcpclient_recon_cb);

	//espconn_connect(ctrlConn); // don't call this here!!! it will be called once WIFI gets connected
	uart0_sendStr("TCP connection recreated.\r\n");
}

static void ICACHE_FLASH_ATTR tcpclient_connect_cb(void *arg)
{
	//struct espconn *pespconn = (struct espconn *)arg;
	// Since we have only one connection, it is stored in ctrlConn.
	// So no need to get it from the *arg parameter!

	uart0_sendStr("TCP Connected!\r\n");

	espconn_regist_disconcb(ctrlConn, tcpclient_discon_cb);
	espconn_regist_recvcb(ctrlConn, tcpclient_recv);
	espconn_regist_sentcb(ctrlConn, tcpclient_sent_cb);

	uart0_sendStr("Calling CTRL authorization!\r\n");
	ctrl_stack_authorize();
}

static void ICACHE_FLASH_ATTR tcpclient_recon_cb(void *arg, sint8 errType)
{
	//struct espconn *pespconn = (struct espconn *)arg;
	// Since we have only one connection, it is stored in ctrlConn.
	// So no need to get it from the *arg parameter!

	uart0_sendStr("TCP Reconnect Event! Recreating the connection, will start later...\r\n");
	char tmp[50];
	os_sprintf(tmp, "Reason: %d\r\n", errType);
	uart0_sendStr(tmp);
	ctrl_connection_recreate();
}

static void ICACHE_FLASH_ATTR tcpclient_discon_cb(void *arg)
{
	//struct espconn *pespconn = (struct espconn *)arg;
	// Since we have only one connection, it is stored in ctrlConn.
	// So no need to get it from the *arg parameter!

	uart0_sendStr("TCP Disconnected! Recreating the connection, will start later...\r\n");
	ctrl_connection_recreate();
}

static void ICACHE_FLASH_ATTR tcpclient_recv(void *arg, char *pdata, unsigned short len)
{
	//struct espconn *pespconn = (struct espconn *)arg;
	// Since we have only one connection, it is stored in ctrlConn.
	// So no need to get it from the *arg parameter!

	uart0_sendStr("tcpclient_recv(): ");
	uart0_tx_buffer(pdata, len);
	uart0_sendStr("\r\n");

	// forward data to CTRL stack
	uart0_sendStr("Calling CTRL stack with this data...\r\n");
	ctrl_stack_recv(pdata, len);
}

static void ICACHE_FLASH_ATTR tcpclient_sent_cb(void *arg)
{
	//struct espconn *pespconn = (struct espconn *)arg;
	// Since we have only one connection, it is stored in ctrlConn.
	// So no need to get it from the *arg parameter!

	uart0_sendStr("tcpclient_sent_cb()!\r\n");
}

// entry point to the ctrl platform
void ICACHE_FLASH_ATTR ctrl_platform_init(void)
{
	struct station_config stationConf;

#ifndef CTRL_DEBUG
	load_user_param(USER_PARAM_SEC_1, 0, &ctrlSetup, sizeof(tCtrlSetup));
	wifi_station_get_config(&stationConf);
#else
	uart0_sendStr("Debugging, will not start configuration web server.\r\n");

	ctrlSetup.setupOk = SETUP_OK_KEY;

	// tcp://ctrl.ba:8000
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

	if(ctrlSetup.setupOk != SETUP_OK_KEY || wifi_get_opmode() == SOFTAP_MODE || stationConf.ssid == 0)
	{
#ifdef CTRL_DEBUG
		uart0_sendStr("I am in SOFTAP mode, restarting in STATION mode...\r\n");
		wifi_set_opmode(STATION_MODE);
		system_restart();
#endif
		// make sure we are in SOFTAP_MODE
		if(wifi_get_opmode() != SOFTAP_MODE)
		{
			uart0_sendStr("Restarting in SOFTAP mode...\r\n");
			wifi_set_opmode(SOFTAP_MODE);
			system_restart();
		}

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

		os_sprintf(temp, "SSID: %s\r\n", stationConf.ssid);
		uart0_sendStr(temp);
#ifdef CTRL_DEBUG
		os_sprintf(temp, "PWD: %s\r\n", stationConf.password);
		uart0_sendStr(temp);
#endif

		// Create the TCP connection (but not start it yet)
		ctrl_connection_recreate();

		// Init the CTRL stack
		ctrl_stack_init(ctrlSetup.baseid, 1); // we are not saving any unsent data and we always start from TXbase = 1

		uart0_sendStr("Connecting to WIFI...\r\n");

		// set a timer to check wifi connection progress
		os_timer_disarm(&tmrSysStatusChecker);
		os_timer_setfn(&tmrSysStatusChecker, (os_timer_func_t *)sys_status_checker, NULL);
		os_timer_arm(&tmrSysStatusChecker, TMR_SYS_STATUS_CHECKER_MS, 1); // 1 = repeat automatically

		// set a timer for a LED status blinking. it will be armed from sys_status_checker() once it executes
		os_timer_disarm(&tmrSysStatusLedBlinker);
		os_timer_setfn(&tmrSysStatusLedBlinker, (os_timer_func_t *)sys_status_led_blinker, NULL);
	}
}
