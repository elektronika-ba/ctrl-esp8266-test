#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"

#include "ctrl_platform.h"
#include "ctrl_config_server.h"
#include "driver/uart.h"

os_timer_t tmrSysStatusChecker;
os_timer_t tmrSysStatusLedBlinker;
os_timer_t tmrTcpConnectionInitter;

struct espconn *ctrlConn;

static void tcpclient_discon_cb(void *arg);

// timer that checks for WIFI connection and then starts a TCP connection.
// re-connecting is done by re-starting this timer within the callback of espconn_regist_reconcb() and espconn_regist_disconcb()
void ICACHE_FLASH_ATTR tcp_connection_initter(void *arg)
{
	char wifiState = wifi_station_get_connect_status();

	if(wifiState == STATION_GOT_IP)
	{
		os_timer_disarm(&tmrTcpConnectionInitter);
		tcp_connection_start();
	}
}

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

	ledStatusState = ~ledStatusState; // toggle state
}

// this function is executed in timer regularly and manages the WiFi connection
void ICACHE_FLASH_ATTR sys_status_checker(void *arg)
{
	char wifiState = wifi_station_get_connect_status();
	static char prevWifiState = wifi_state;

	remot_info *ri = (struct remot_info *)os_zalloc(sizeof(struct remot_info));
	espconn_get_connection_info(ctrlConn, &ri, 0);
	enum espconn_state connInfo = ri->state;
	static enum espconn_state prevConnInfo = ri->state;	

	char changeLedInterval = 0;
	if(prevWifiState != wifiState)
	{
		changeLedInterval = 1;
	}

	unsigned int tmrInterval;

	if(connInfo == ESPCONN_ISCONN)
	{
		// TCP connection is established, set timer if it has changed since the last time we've set it
		tmrInterval = 1500;
	}
	else
	{
		// WIFI connected, but still not TCP connected
		if(wifiState == STATION_GOT_IP)
		{
			tmrInterval = 500;
		}
		// WIFI not connected (yet)
		else
		{
			tmrInterval = 200;
		}
	}

	if(changeLedInterval)
	{
		os_timer_disarm(&tmrSysStatusLedBlinker);
		os_timer_arm(&tmrSysStatusLedBlinker, tmrInterval, 1);
	}
}

// this starts a TCP connection
void ICACHE_FLASH_ATTR tcp_connection_start()
{
	// close possibly active connection
	remot_info *ri = (struct remot_info *)os_zalloc(sizeof(struct remot_info));
	espconn_get_connection_info(ctrlConn, &ri, 0);
	enum espconn_state connInfo = ri->state;

	if(connInfo != ESPCONN_CLOSE && connInfo != ESPCONN_NONE)
	{
		espconn_disconnect(ctrlConn);
	}

	espconn_connect(ctrlConn);
}

/**
* @brief  Tcp client connect success callback function.
* @param  arg: contain the ip link information
* @retval None
*/
static void ICACHE_FLASH_ATTR tcpclient_connect_cb(void *arg)
{
	//struct espconn *pespconn = (struct espconn *)arg;

	// Since we have only one connection, it is stored in ctrlConn.
	// So no need to get it from the *arg parameter!

	uart0_sendStr("TCP Connected!\r\n");

	espconn_regist_disconcb(ctrlConn, tcpclient_discon_cb);
	//espconn_regist_recvcb(pespconn, tcpclient_recv);
	//espconn_regist_sentcb(pespconn, tcpclient_sent_cb);
}

/**
* @brief  Tcp client connect repeat callback function.
* @param  arg: contain the ip link information
* @retval None
*/
static void ICACHE_FLASH_ATTR tcpclient_recon_cb(void *arg, sint8 errType)
{
	struct espconn *pespconn = (struct espconn *)arg;
	at_linkConType *linkTemp = (at_linkConType *) pespconn->reverse;
	struct ip_info ipconfig;
	os_timer_t sta_timer;
	/*
	os_printf(
	"at_tcpclient_recon_cb %p\r\n",
	arg);

	if(linkTemp->teToff == TRUE)
	{
	linkTemp->teToff = FALSE;
	linkTemp->repeaTime = 0;
	if(pespconn->proto.tcp != NULL)
	{
	os_free(pespconn->proto.tcp);
	}
	os_free(pespconn);
	linkTemp->linkEn = false;
	at_linkNum--;
	if(at_linkNum == 0)
	{
	at_backOk;
	mdState = m_unlink; //////////////////////
	uart0_sendStr("Unlink\r\n");
	disAllFlag = false;
	specialAtState = TRUE;
	at_state = at_statIdle;
	}
	}
	else
	{
	linkTemp->repeaTime++;
	if(linkTemp->repeaTime >= 3)
	{
	os_printf("repeat over %d\r\n", linkTemp->repeaTime);
	specialAtState = TRUE;
	at_state = at_statIdle;
	linkTemp->repeaTime = 0;
	at_backError;
	if(pespconn->proto.tcp != NULL)
	{
	os_free(pespconn->proto.tcp);
	}
	os_free(pespconn);
	linkTemp->linkEn = false;
	os_printf("disconnect\r\n");
	//  os_printf("con EN? %d\r\n", pLink[0].linkEn);
	at_linkNum--;
	if (at_linkNum == 0)
	{
	mdState = m_unlink; //////////////////////

	uart0_sendStr("Unlink\r\n");
	//    specialAtState = true;
	//    at_state = at_statIdle;
	disAllFlag = false;
	//    specialAtState = true;
	//    at_state = at_statIdle;
	//    return;
	}
	specialAtState = true;
	at_state = at_statIdle;
	return;
	}
	os_printf("link repeat %d\r\n", linkTemp->repeaTime);
	pespconn->proto.tcp->local_port = espconn_port();
	espconn_connect(pespconn);
	}
	*/
}

/**
* @brief  Tcp client disconnect success callback function.
* @param  arg: contain the ip link information
* @retval None
*/
static void ICACHE_FLASH_ATTR tcpclient_discon_cb(void *arg)
{
	/*
	struct espconn *pespconn = (struct espconn *)arg;
	at_linkConType *linkTemp = (at_linkConType *)pespconn->reverse;

	if(pespconn == NULL)
	{
		return;
	}

	if(pespconn->proto.tcp != NULL)
	{
		os_free(pespconn->proto.tcp);
	}
	os_free(pespconn);

	if(disAllFlag)
	{
		idTemp = linkTemp->linkId + 1;
		for(; idTemp<at_linkMax; idTemp++)
		{
			if(pLink[idTemp].linkEn)
			{
				if(pLink[idTemp].teType == teServer)
				{
					continue;
				}
				if(pLink[idTemp].pCon->type == ESPCONN_TCP)
				{
					specialAtState = FALSE;
					espconn_disconnect(pLink[idTemp].pCon);
					break;
				}
				else
				{
					pLink[idTemp].linkEn = FALSE;
					espconn_delete(pLink[idTemp].pCon);
					os_free(pLink[idTemp].pCon->proto.udp);
					os_free(pLink[idTemp].pCon);
					at_linkNum--;
					if(at_linkNum == 0)
					{
						mdState = m_unlink;
						at_backOk;
						uart0_sendStr("Unlink\r\n");
						uart0_sendStr("Unlink nesto jebiga\r\n"); // trax added
						disAllFlag = FALSE;
						//            specialAtState = TRUE;
						//            at_state = at_statIdle;
						//            return;
					}
				}
			}
		}
	}
	*/
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

	if(ctrlSetup.setupOk != SETUP_OK_KEY || wifi_get_opmopde() == SOFTAP_MODE || stationConf.ssid == 0)
	{
#ifdef CTRL_DEBUG
		uart0_sendStr("I am in SOFTAP mode, restarting in STATION mode...\r\n");
		wifi_set_opmode(STATION_MODE);
		system_restart();
#endif
		// make sure we are in SOFTAP_MODE
		if(wifi_get_opmopde() != SOFTAP_MODE)
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

		// Prepare the TCP connection context
		enum espconn_type linkType = ESPCONN_TCP;
		ctrlConn = (struct espconn *)os_zalloc(sizeof(struct espconn));
		if(ctrlConn == NULL)
		{
			os_timer_disarm(&tmrSysStatusLedBlinker);
			os_timer_setfn(&tmrSysStatusLedBlinker, (os_timer_func_t *)sys_status_led_blinker, NULL);
			os_timer_arm(&tmrSysStatusLedBlinker, 100, 1); // 1 = repeat automatically
			uart0_sendStr("Failed to allocate memory for ctrlConn. Aborted!\r\n");
			return;
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
		//espconn_connect(ctrlConn); // don't call this here!
		uart0_sendStr("TCP connection prepared, will connect later...\r\n");

		uart0_sendStr("Connecting to WIFI...\r\n");

		// set a timer to check wifi connection progress
		os_timer_disarm(&tmrSysStatusChecker);
		os_timer_setfn(&tmrSysStatusChecker, (os_timer_func_t *)sys_status_checker, NULL);
		os_timer_arm(&tmrSysStatusChecker, TMR_SYS_STATUS_CHECKER_MS, 1); // 1 = repeat automatically

		// set a timer for a LED status blinking. it will be armed from sys_status_checker() once it executes
		os_timer_disarm(&tmrSysStatusLedBlinker);
		os_timer_setfn(&tmrSysStatusLedBlinker, (os_timer_func_t *)sys_status_led_blinker, NULL);

		// set a timer to init a TCP connection once WIFI gets connected
		os_timer_disarm(&tmrTcpConnectionInitter);
		os_timer_setfn(&tmrTcpConnectionInitter, (os_timer_func_t *)tcp_connection_initter, NULL);
		os_timer_arm(&tmrTcpConnectionInitter, 1000, 1); // 1 = repeat automatically
	}
}
