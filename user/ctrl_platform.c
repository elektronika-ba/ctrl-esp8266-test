#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"

#include "ctrl_database.h"
#include "ctrl_stack.h"
#include "ctrl_platform.h"
#include "ctrl_config_server.h"
#include "driver/uart.h"

// user-ctrl-application
#include "temperature_logger.h"

#ifdef USE_DATABASE_APPROACH
	#ifndef CTRL_DATABASE_CAPACITY
		#error You probably forgot to include ctrl_database.h in ctrl_platform.c file?
	#endif
	os_timer_t tmrDatabaseItemSender;
#else
	static unsigned long TXbase;
#endif

struct espconn *ctrlConn;
os_timer_t tmrSysStatusChecker;
os_timer_t tmrSysStatusLedBlinker;
tCtrlSetup ctrlSetup;
tCtrlCallbacks ctrlCallbacks;
static char outOfSyncCounter;
static char ctrlAuthorized;
static tCtrlConnState connState = CTRL_TCP_DISCONNECTED;

#ifdef USE_DATABASE_APPROACH
	static void ICACHE_FLASH_ATTR sys_database_item_sender(void *arg)
	{
		uart0_sendStr("sys_database_item_sender()\r\n");

		os_timer_disarm(&tmrDatabaseItemSender);
		uart0_sendStr("sys_database_item_sender() - OFF\r\n");

		if(connState != CTRL_TCP_CONNECTED || ctrlAuthorized != 1)
		{
			uart0_sendStr("sys_database_item_sender() - NO CONN\r\n");
			return;
		}

		// get next item to send from DB, send it and mark as SENT even if it doesn't actually get sent to socket!
		tDatabaseRow *row = (tDatabaseRow *)ctrl_database_get_next_txbase2server();
		if(row != NULL)
		{
			ctrl_stack_send(row->data, row->len, row->TXbase, row->notification);

			// set us up to execute again
			os_timer_arm(&tmrDatabaseItemSender, TMR_ITEMS_SENDER_MS, 0); // 0 = don't repeat automatically
			uart0_sendStr("sys_database_item_sender() - ON\r\n");
		}
	}
#endif

// blinking status led according to the status of wifi and tcp connection
static void ICACHE_FLASH_ATTR sys_status_led_blinker(void *arg)
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
static void ICACHE_FLASH_ATTR sys_status_checker(void *arg)
{
	static char prevWifiState = STATION_IDLE;
	static tCtrlConnState prevConnState = CTRL_TCP_DISCONNECTED;
	static unsigned char prevCtrlAuthorized;

	unsigned int tmrInterval;

	char debugy[75];

	if(connState == CTRL_TCP_CONNECTED && wifi_station_get_connect_status() == STATION_GOT_IP) // also check for WIFI state here, because TCP connection might think it is still connected even after WIFI loses the connection with AP
	{
		// TCP connection is established, set timer if it has changed since the last time we've set it
		if(ctrlAuthorized)
		{
			tmrInterval = 1000;
			os_sprintf(debugy, "%s", "STATE = TCP CONNECTED, AUTHORIZED");
		}
		else
		{
			tmrInterval = 2000;
			os_sprintf(debugy, "%s", "STATE = TCP CONNECTED, UNAUTHORIZED");
		}
	}
	else
	{
		// WIFI connected
		if(wifi_station_get_connect_status() == STATION_GOT_IP)
		{
			tmrInterval = 500;
			os_sprintf(debugy, "%s", "STATE = WIFI CONNECTED");

			// check TCP connection status and initiate connecting procedure if not connected
			if(connState == CTRL_TCP_DISCONNECTED)
			{
				os_sprintf(debugy, "%s", "STATE = TCP DISCONNECTED, RECREATING TCP & CONNECTING");

				uart0_sendStr("sys_status_checker() recreating and connecting\r\n");
				tcp_connection_recreate();

				espconn_connect(ctrlConn);
				connState = CTRL_TCP_CONNECTING;
			}
		}
		// WIFI not connected (yet)
		else
		{
			tmrInterval = 200;
			os_sprintf(debugy, "%s", "STATE = NO WIFI");

			// TCP thinks it is still connected? Disconnect it!
			if(connState != CTRL_TCP_DISCONNECTED)
			{
				os_sprintf(debugy, "%s", "STATE = NO WIFI, DISCONNECTING TCP");

				uart0_sendStr("sys_status_checker() stopping sender and disconnecting\r\n");
				#ifdef USE_DATABASE_APPROACH
					os_timer_disarm(&tmrDatabaseItemSender);
				#endif

				ctrlAuthorized = 0;
				espconn_disconnect(ctrlConn);				
				connState = CTRL_TCP_DISCONNECTED;
			}
		}
	}

	// change timer timeout?
	if(prevWifiState != wifi_station_get_connect_status() || prevConnState != connState || prevCtrlAuthorized != ctrlAuthorized)
	{
		uart0_sendStr("### ");
		uart0_sendStr(debugy);
		uart0_sendStr("\r\n");

		os_timer_disarm(&tmrSysStatusLedBlinker);
		os_timer_arm(&tmrSysStatusLedBlinker, tmrInterval, 1);
	}

	prevCtrlAuthorized = ctrlAuthorized;
	prevWifiState = wifi_station_get_connect_status();
	prevConnState = connState;
}

static void ICACHE_FLASH_ATTR tcp_connection_recreate(void)
{
	ctrlAuthorized = 0;
	#ifdef USE_DATABASE_APPROACH
		os_timer_disarm(&tmrDatabaseItemSender);
	#endif

	espconn_disconnect(ctrlConn); // will not cause a problem if ctrlConn is NULL here... also, this fn will os_free() the ctrlConn for us
	connState = CTRL_TCP_DISCONNECTED;

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

	uart0_sendStr("TCP connection recreated.\r\n");
}

static void ICACHE_FLASH_ATTR tcpclient_connect_cb(void *arg)
{
	//struct espconn *pespconn = (struct espconn *)arg;
	// Since we have only one connection, it is stored in ctrlConn.
	// So no need to get it from the *arg parameter!

	connState = CTRL_TCP_CONNECTED;

	uart0_sendStr("TCP Connected!\r\n");

	espconn_regist_disconcb(ctrlConn, tcpclient_discon_cb);
	espconn_regist_recvcb(ctrlConn, tcpclient_recv);
	espconn_regist_sentcb(ctrlConn, tcpclient_sent_cb);

	unsigned char sync = 0;

	#ifdef USE_DATABASE_APPROACH
		// 1. Flush acked transmissions until we get to the first unacked (or until the end)
		// 2. Mark all unacked database items as unsent (not a problem if we unsend even the acked ones since that is a problem and a probably situation that will never happen)
		// 3. Count pending items to see if we need to tell server to Sync
		ctrl_database_flush_acked();
		ctrl_database_unsend_all();
		if(ctrl_database_count_unacked_items() == 0)
		{
			sync = 1;
		}
	#else
		// no database = always sync
		sync = 1;
		TXbase = 1;
	#endif

	ctrl_stack_authorize(ctrlSetup.baseid, sync);
}

static void ICACHE_FLASH_ATTR tcpclient_recon_cb(void *arg, sint8 errType)
{
	//struct espconn *pespconn = (struct espconn *)arg;
	// Since we have only one connection, it is stored in ctrlConn.
	// So no need to get it from the *arg parameter!

	#ifdef USE_DATABASE_APPROACH
		os_timer_disarm(&tmrDatabaseItemSender);
	#endif
	connState = CTRL_TCP_DISCONNECTED;
	ctrlAuthorized = 0;

	uart0_sendStr("TCP Reconnect! Will recreate and start later...\r\n");
	char tmp[50];
	os_sprintf(tmp, "Reason: %d\r\n", errType);
	uart0_sendStr(tmp);
}

static void ICACHE_FLASH_ATTR tcpclient_discon_cb(void *arg)
{
	//struct espconn *pespconn = (struct espconn *)arg;
	// Since we have only one connection, it is stored in ctrlConn.
	// So no need to get it from the *arg parameter!

	#ifdef USE_DATABASE_APPROACH
		os_timer_disarm(&tmrDatabaseItemSender);
	#endif
	connState = CTRL_TCP_DISCONNECTED;
	ctrlAuthorized = 0;

	uart0_sendStr("TCP Disconnected normally! Will recreate and start later...\r\n");
}

static void ICACHE_FLASH_ATTR tcpclient_recv(void *arg, char *pdata, unsigned short len)
{
	//struct espconn *pespconn = (struct espconn *)arg;
	// Since we have only one connection, it is stored in ctrlConn.
	// So no need to get it from the *arg parameter!

	//uart0_sendStr("tcpclient_recv(): ");

	// forward data to CTRL stack
	ctrl_stack_recv(pdata, len);
}

static void ICACHE_FLASH_ATTR tcpclient_sent_cb(void *arg)
{
	//struct espconn *pespconn = (struct espconn *)arg;
	// Since we have only one connection, it is stored in ctrlConn.
	// So no need to get it from the *arg parameter!

	//uart0_sendStr("tcpclient_sent_cb()!\r\n");
}

static void ICACHE_FLASH_ATTR ctrl_message_recv_cb(tCtrlMessage *msg)
{
	// do something with msg now
	char tmp[100];
	os_sprintf(tmp, "GOT MSG. Length: %u, Header: 0x%X, TXsender: %u, Data:", msg->length, msg->header, msg->TXsender);
	uart0_sendStr(tmp);

	unsigned short i;
	for(i=0; i<msg->length-1-4; i++)
	{
		char tmp2[10];
		os_sprintf(tmp2, " 0x%X", msg->data[i]);
		uart0_sendStr(tmp2);
	}
	uart0_sendStr(".\r\n");

	// after we finish, and if we find out that we don't have enough
	// storage to process next message that will arive, we can simply
	// make a call to ctrl_stack_backoff(1); and it will acknowledge to
	// any following messages with BACKOFF which will tell server
	// to re-send that message and postpone sending any following messages
	// untill we call ctrl_stack_backoff(0);
}

static void ICACHE_FLASH_ATTR ctrl_message_ack_cb(tCtrlMessage *msg)
{
	// do something with ack now
	char tmp[100];
	os_sprintf(tmp, "GOT ACK. Length: %u, Header: 0x%X, TXsender: %u\r\n", msg->length, msg->header, msg->TXsender);
	uart0_sendStr(tmp);

	// hendliraj out_of_sync koji nam server moze poslati
	if((msg->header) & CH_OUT_OF_SYNC)
	{
		uart0_sendStr("Server is complaining that we are OUT OF SYNC!\r\n");

		if(++outOfSyncCounter > 3)
		{
			uart0_sendStr("Flushing outgoing queue and will restart the connection later!\r\n");

			#ifdef USE_DATABASE_APPROACH
				os_timer_disarm(&tmrDatabaseItemSender);
				// Flush the outgoing queue, that's all we can do about it really.
				ctrl_database_delete_all();
			#endif

			connState = CTRL_TCP_DISCONNECTED;
			espconn_disconnect(ctrlConn); // do this right here right now
		}
		else
		{
			char tmp[30];
			os_sprintf(tmp, "Re-sending outgoing queue! Attempt %u/3.\r\n", outOfSyncCounter);
			uart0_sendStr(tmp);

			#ifdef USE_DATABASE_APPROACH
				// Re-send all unacked outgoing messages, maybe that will resolve the sync problem
				os_timer_disarm(&tmrDatabaseItemSender);
				ctrl_database_unsend_all();
				os_timer_arm(&tmrDatabaseItemSender, TMR_ITEMS_SENDER_MS, 0); // 0 = don't repeat automatically
			#endif
		}
	}
	else
	{
		outOfSyncCounter = 0;

		#ifdef USE_DATABASE_APPROACH
				ctrl_database_ack_row(msg->TXsender);
		#endif
	}
}

static void ICACHE_FLASH_ATTR ctrl_auth_response_cb(unsigned char auth_err)
{
	// auth_err = 0x00 (AUTH OK) or 0x01 (AUTH ERROR)
	if(auth_err)
	{
		uart0_sendStr("CTRL AUTH ERR!\r\n");
		ctrlAuthorized = 0;
	}
	else
	{
		uart0_sendStr("AUTH OK :)\r\n");
		ctrlAuthorized = 1;
		ctrl_stack_keepalive(1); // lets enable keepalive for our connection because that's what all cool kids do these days

		#ifdef USE_DATABASE_APPROACH
			if(ctrl_database_count_unacked_items() > 0)
			{
				// start pending item sender, because we have something to send
				os_timer_disarm(&tmrDatabaseItemSender);
				os_timer_arm(&tmrDatabaseItemSender, TMR_ITEMS_SENDER_MS, 0); // 0 = don't repeat automatically
			}
		#endif
	}
}

static char ICACHE_FLASH_ATTR ctrl_send_data_cb(char *data, unsigned short len)
{
	if(connState != CTRL_TCP_CONNECTED)
	{
		return ESPCONN_CONN;
	}

	uart0_sendStr("SENDING: ");
	unsigned short i;
	for(i=0; i<len; i++)
	{
		char tmp2[10];
		os_sprintf(tmp2, " 0x%X", data[i]);
		uart0_sendStr(tmp2);
	}
	uart0_sendStr(".\r\n");

	return espconn_sent(ctrlConn, data, len);
}

static void ICACHE_FLASH_ATTR ctrl_save_TXserver_cb(unsigned long TXserver)
{
	// we don't store TXserver in FLASH as it would wear out the FLASH memory... don't know how to handle this. Maybe save every other value and in different locations?
	// by saving every other value system would still work since it is configured to try re-sending 3 times until flushing all data
}

static unsigned long ICACHE_FLASH_ATTR ctrl_restore_TXserver_cb(void)
{
	// since we don't store it, we don't load it either :-)
	return 0; // return zero (default) as if CTRL Server told us to re-sync. in case it had pending messages we would send him OUT_OF_SYNC 3 times and server will then flush and break the connection, and we would reconnect.
}

// all user CTRL messages is sent to Server through this function
// returns: 1 on error, 0 on success
unsigned char ICACHE_FLASH_ATTR ctrl_platform_send(char *data, unsigned short len, unsigned char notification)
{
	#ifdef USE_DATABASE_APPROACH
		if(notification)
		{
			return ctrl_stack_send(data, len, 0, notification); // send notifications immediatelly, no queue
		}
		else
		{
			unsigned char ret = ctrl_database_add_row(data, len);
			// no point in starting timer if we couldn't add data to DB
			if(ret == 0 && connState == CTRL_TCP_CONNECTED && ctrlAuthorized == 1)
			{
				os_timer_disarm(&tmrDatabaseItemSender);
				os_timer_arm(&tmrDatabaseItemSender, TMR_ITEMS_SENDER_MS, 0); // 0 = don't repeat automatically
			}
			return ret;
		}
	#else
		TXbase++;
		return ctrl_stack_send(data, len, TXbase, notification);
	#endif
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

		ctrlSetup.baseid[0] = 0x00;
		ctrlSetup.baseid[1] = 0xcc;
		ctrlSetup.baseid[2] = 0xa5;
		ctrlSetup.baseid[3] = 0x39;
		ctrlSetup.baseid[4] = 0xd1;
		ctrlSetup.baseid[5] = 0x59;
		ctrlSetup.baseid[6] = 0xa7;
		ctrlSetup.baseid[7] = 0xca;
		ctrlSetup.baseid[8] = 0x30;
		ctrlSetup.baseid[9] = 0x0a;
		ctrlSetup.baseid[10] = 0xee;
		ctrlSetup.baseid[11] = 0x98;
		ctrlSetup.baseid[12] = 0xde;
		ctrlSetup.baseid[13] = 0xda;
		ctrlSetup.baseid[14] = 0x7e;
		ctrlSetup.baseid[15] = 0x92;

		os_memset(stationConf.ssid, 0, sizeof(stationConf.ssid));
		os_memset(stationConf.password, 0, sizeof(stationConf.password));

		os_sprintf(stationConf.ssid, "%s", "asd.AP5a");
		os_sprintf(stationConf.password, "%s", "asdasd");

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

		// Init the database (a RAM version of DB for now)
		ctrl_database_init();

		// Init the CTRL stack with callback functions it requires
		ctrlCallbacks.message_received = &ctrl_message_recv_cb; // when CTRL stack receives a fresh message it will call this function
		ctrlCallbacks.send_data = &ctrl_send_data_cb; // when CTRL stack wants to send some data to socket it will use this function
		ctrlCallbacks.auth_response = &ctrl_auth_response_cb; // when CTRL stack gets an authentication response from Server it will call this function
		ctrlCallbacks.message_acked = &ctrl_message_ack_cb; // when CTRL stack receives an acknowledgement for a message it previously sent it will call this function
		ctrlCallbacks.save_TXserver = &ctrl_save_TXserver_cb; // when CTRL stack receives data from Server and increases the TXserver value, it will call this function in case we want to store it in FLASH/EEPROM/NVRAM memory
		ctrlCallbacks.restore_TXserver = &ctrl_restore_TXserver_cb; // when CTRL stack authenticates and in case when Server doesn't want us to re-sync, we must load TXserver from FLASH/EEPROM/NVRAM in order to continue receiving messages. For that, CTRL stack will call this function and read the value we provide to it
		ctrl_stack_init(&ctrlCallbacks);

// example app
		uart0_sendStr("Temperature logger init...\r\n");
		temperature_logger_init();
//--

		uart0_sendStr("System initialization done!\r\n");

		// set a timer to check wifi connection progress
		os_timer_disarm(&tmrSysStatusChecker);
		os_timer_setfn(&tmrSysStatusChecker, (os_timer_func_t *)sys_status_checker, NULL);
		os_timer_arm(&tmrSysStatusChecker, TMR_SYS_STATUS_CHECKER_MS, 1); // 1 = repeat automatically

		// set a timer for a LED status blinking. it will be armed from sys_status_checker() once it executes
		os_timer_disarm(&tmrSysStatusLedBlinker);
		os_timer_setfn(&tmrSysStatusLedBlinker, (os_timer_func_t *)sys_status_led_blinker, NULL);
		os_timer_arm(&tmrSysStatusLedBlinker, 200, 1); // actually lets start it right now to blink like WIFI is not available yet. 1 = repeat automatically

		#ifdef USE_DATABASE_APPROACH
				// set a timer that will send items from the database (if database approach is used)
				os_timer_disarm(&tmrDatabaseItemSender);
				os_timer_setfn(&tmrDatabaseItemSender, (os_timer_func_t *)sys_database_item_sender, NULL);
				//WILL BE STARTED WHEN SOMETHING INSERTS INTO DATABASE
		#endif
	}
}
