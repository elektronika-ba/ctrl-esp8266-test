#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"
#include "os_type.h"
#include "driver/uart.h"

#include "flash_param.h"
#include "ctrl_database.h"
#include "ctrl_stack.h"
#include "ctrl_platform.h"
#include "ctrl_config_server.h"

// Include your own custom app here
#include "ctrl_app_temperature_simulator.h"

#ifdef CTRL_DEBUG
	#include "wifi_debug_params.h" // DEBUG PARAMETERS
#endif

#ifdef USE_DATABASE_APPROACH
	#ifndef CTRL_DATABASE_CAPACITY
		#error You probably forgot to include ctrl_database.h in ctrl_platform.c file?
	#endif
	os_timer_t tmrDatabaseItemSender;
#else
	static unsigned long TXbase;
#endif

struct espconn ctrlConn;
esp_tcp ctrlTcp;
os_timer_t tmrLinker;
static unsigned char tcpReconCount;
static tCtrlConnState connState = CTRL_WIFI_CONNECTING;

static unsigned long gTXserver;
os_timer_t tmrStatusLedBlinker;
tCtrlSetup ctrlSetup;
tCtrlCallbacks ctrlCallbacks;
static unsigned char outOfSyncCounter;
static unsigned char ctrlSynchronized;

tCtrlAppCallbacks ctrlAppCallbacks;

static void ctrl_platform_reconnect(struct espconn *);
static void ctrl_platform_discon(struct espconn *);

static void ICACHE_FLASH_ATTR ctrl_platform_check_ip(void *arg)
{
    struct ip_info ipconfig;
    static tCtrlConnState prevConnState = CTRL_AUTHENTICATION_ERROR; // anything other than the startup value "CTRL_WIFI_CONNECTING"

	unsigned int ledBlinkerInterval;

    os_timer_disarm(&tmrLinker);

    wifi_get_ip_info(STATION_IF, &ipconfig);

    if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0)
    {
        connState = CTRL_TCP_CONNECTING;
        #ifdef CTRL_DEBUG
        	uart0_sendStr("TCP CONNECTING...\r\n");
        #endif

        ledBlinkerInterval = 1000;

        ctrlConn.proto.tcp = &ctrlTcp;
        ctrlConn.type = ESPCONN_TCP;
        ctrlConn.state = ESPCONN_NONE;
        os_memcpy(ctrlConn.proto.tcp->remote_ip, ctrlSetup.serverIp, 4);
        ctrlConn.proto.tcp->local_port = espconn_port();
        ctrlConn.proto.tcp->remote_port = ctrlSetup.serverPort;

		espconn_regist_connectcb(&ctrlConn, ctrl_platform_connect_cb);
		espconn_regist_reconcb(&ctrlConn, ctrl_platform_recon_cb);
		espconn_regist_disconcb(&ctrlConn, ctrl_platform_discon_cb);

		espconn_connect(&ctrlConn);
    }
    else
    {
        if(wifi_station_get_connect_status() == STATION_WRONG_PASSWORD ||
           		wifi_station_get_connect_status() == STATION_NO_AP_FOUND ||
           		wifi_station_get_connect_status() == STATION_CONNECT_FAIL)
		{
            connState = CTRL_WIFI_CONNECTING_ERROR;
            #ifdef CTRL_DEBUG
            	uart0_sendStr("WIFI CONNECTING ERROR\r\n");
            #endif

            // maybe this will help my ~22second reconnect?
            //wifi_station_disconnect();
            //wifi_station_connect();
            // NOPE IT DOESN'T :(

            os_timer_setfn(&tmrLinker, (os_timer_func_t *)ctrl_platform_check_ip, NULL);
            os_timer_arm(&tmrLinker, 1000, 0); // try now slower

            ledBlinkerInterval = 500;
        }
        else {
            os_timer_setfn(&tmrLinker, (os_timer_func_t *)ctrl_platform_check_ip, NULL);
            os_timer_arm(&tmrLinker, 100, 0);

			ledBlinkerInterval = 1500;

            connState = CTRL_WIFI_CONNECTING;
            #ifdef CTRL_DEBUG
            	uart0_sendStr("WIFI CONNECTING...\r\n");
            #endif
        }
    }

	if(prevConnState != connState)
	{
		os_timer_disarm(&tmrStatusLedBlinker);
		os_timer_arm(&tmrStatusLedBlinker, ledBlinkerInterval, 1);
	}

    prevConnState = connState;
}

static void ICACHE_FLASH_ATTR ctrl_platform_recon_cb(void *arg, sint8 err)
{
    struct espconn *pespconn = (struct espconn *)arg;

	#ifdef CTRL_DEBUG
    	uart0_sendStr("ctrl_platform_recon_cb\r\n");
    #endif

	connState = CTRL_TCP_DISCONNECTED;

    if (++tcpReconCount >= 5)
    {
        connState = CTRL_TCP_CONNECTING_ERROR;
        tcpReconCount = 0;

		os_timer_disarm(&tmrStatusLedBlinker);
		os_timer_arm(&tmrStatusLedBlinker, 1000, 1);

        #ifdef CTRL_DEBUG
        	uart0_sendStr("ctrl_platform_recon_cb, 5 failed TCP attempts!\n");
        	uart0_sendStr("Will reconnect in 10s...\r\n");
        #endif

		os_timer_disarm(&tmrLinker);
		os_timer_setfn(&tmrLinker, (os_timer_func_t *)ctrl_platform_reconnect, pespconn);
		os_timer_arm(&tmrLinker, 10000, 0);
    }
    else
    {
		#ifdef CTRL_DEBUG
			uart0_sendStr("Will reconnect in 1s...\r\n");
		#endif

		os_timer_disarm(&tmrLinker);
		os_timer_setfn(&tmrLinker, (os_timer_func_t *)ctrl_platform_reconnect, pespconn);
		os_timer_arm(&tmrLinker, 1000, 0);
	}
}

static void ICACHE_FLASH_ATTR ctrl_platform_sent_cb(void *arg)
{
	struct espconn *pespconn = arg;

	#ifdef CTRL_DEBUG
    	uart0_sendStr("ctrl_platform_sent_cb\r\n");
    #endif
}

static void ICACHE_FLASH_ATTR ctrl_platform_recv_cb(void *arg, char *pdata, unsigned short len)
{
	struct espconn *pespconn = arg;

	#ifdef CTRL_DEBUG
		uart0_sendStr("ctrl_platform_recv_cb\r\n");
	#endif

	// forward data to CTRL stack
	ctrl_stack_recv(pdata, len);
}

static void ICACHE_FLASH_ATTR ctrl_platform_connect_cb(void *arg)
{
    struct espconn *pespconn = arg;

	#ifdef CTRL_DEBUG
		uart0_sendStr("ctrl_platform_connect_cb\r\n");
	#endif

    tcpReconCount = 0;

    espconn_regist_recvcb(pespconn, ctrl_platform_recv_cb);
    espconn_regist_sentcb(pespconn, ctrl_platform_sent_cb);

	connState = CTRL_TCP_CONNECTED;

	os_timer_disarm(&tmrStatusLedBlinker);
	os_timer_arm(&tmrStatusLedBlinker, 3000, 1);

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

static void ICACHE_FLASH_ATTR ctrl_platform_reconnect(struct espconn *pespconn)
{
	#ifdef CTRL_DEBUG
    	uart0_sendStr("ctrl_platform_reconnect\r\n");
    #endif

    ctrl_platform_check_ip(NULL);
}

static void ICACHE_FLASH_ATTR ctrl_platform_discon_cb(void *arg)
{
    struct espconn *pespconn = arg;

	#ifdef CTRL_DEBUG
    	uart0_sendStr("ctrl_platform_discon_cb\r\n");
    #endif

	connState = CTRL_TCP_DISCONNECTED;

    if (pespconn == NULL)
    {
		#ifdef CTRL_DEBUG
	    	uart0_sendStr("ctrl_platform_discon_cb - conn is NULL!\r\n");
	    #endif
        return;
    }

    //pespconn->proto.tcp->local_port = espconn_port(); // do I need this?
	#ifdef CTRL_DEBUG
		uart0_sendStr("Will reconnect in 1s...\r\n");
	#endif

    os_timer_disarm(&tmrLinker);
    os_timer_setfn(&tmrLinker, (os_timer_func_t *)ctrl_platform_reconnect, pespconn);
    os_timer_arm(&tmrLinker, 1000, 0);
}

static void ICACHE_FLASH_ATTR ctrl_platform_discon(struct espconn *pespconn)
{
	#ifdef CTRL_DEBUG
    	uart0_sendStr("ctrl_platform_discon\r\n");
    #endif

	connState = CTRL_TCP_DISCONNECTED;

    espconn_disconnect(pespconn);

    // hopefully now the ctrl_platform_discon_cb will trigger and re-connect us!?
}

#ifdef USE_DATABASE_APPROACH
	static void ICACHE_FLASH_ATTR ctrl_database_item_sender(void *arg)
	{
		os_timer_disarm(&tmrDatabaseItemSender);

		#ifdef CTRL_DEBUG
			uart0_sendStr("ctrl_database_item_sender\r\n");
		#endif

		if(connState != CTRL_AUTHENTICATED || !ctrlSynchronized)
		{
			#ifdef CTRL_DEBUG
				uart0_sendStr("ctrl_database_item_sender - not authed or synced\r\n");
			#endif
			return;
		}

		// get next item to send from DB, send it and mark as SENT even if it doesn't actually get sent to socket!
		tDatabaseRow *row = (tDatabaseRow *)ctrl_database_get_next_txbase2server();
		if(row != NULL)
		{
			ctrl_stack_send(row->data, row->len, row->TXbase, 0);

			// set us up to execute again
			os_timer_arm(&tmrDatabaseItemSender, TMR_ITEMS_SENDER_MS, 0); // 0 = don't repeat automatically
			#ifdef CTRL_DEBUG
				uart0_sendStr("ctrl_database_item_sender - ON again\r\n");
			#endif
		}
	}
#endif

// blinking status led according to the status of wifi and tcp connection
static void ICACHE_FLASH_ATTR ctrl_status_led_blinker(void *arg)
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

static void ICACHE_FLASH_ATTR ctrl_message_recv_cb(tCtrlMessage *msg)
{
	// do something with msg now
	#ifdef CTRL_DEBUG
		char tmp[100];
		os_sprintf(tmp, "GOT MSG. Length: %u, Header: 0x%X, TXsender: %u, Data:", msg->length, msg->header, msg->TXsender);
		uart0_sendStr(tmp);
	#endif

	/*
	unsigned short i;
	for(i=0; i<msg->length-1-4; i++)
	{
		char tmp2[10];
		os_sprintf(tmp2, " 0x%X", msg->data[i]);
		uart0_sendStr(tmp2);
	}
	uart0_sendStr(".\r\n");
	*/

	// after we finish, and if we find out that we don't have enough
	// storage to process next message that will arive, we can simply
	// make a call to ctrl_stack_backoff(1); and it will acknowledge to
	// any following messages with BACKOFF which will tell server
	// to re-send that message and postpone sending any following messages
	// untill we call ctrl_stack_backoff(0);

	// push message to ctrl-user-application
	if(ctrlAppCallbacks.message_received != NULL)
	{
		unsigned char ok = ctrlAppCallbacks.message_received(msg);
		ctrl_stack_backoff(!ok);
	}
}

static void ICACHE_FLASH_ATTR ctrl_message_ack_cb(tCtrlMessage *msg)
{
	// do something with ack now
	#ifdef CTRL_DEBUG
		char tmp[100];
		os_sprintf(tmp, "GOT ACK on TXsender: %u\r\n", msg->TXsender);
		uart0_sendStr(tmp);
	#endif

	// hendliraj out_of_sync koji nam server moze poslati
	if((msg->header) & CH_OUT_OF_SYNC)
	{
		#ifdef CTRL_DEBUG
			uart0_sendStr("Server is complaining that we are OUT OF SYNC!\r\n");
		#endif

		if(++outOfSyncCounter >= 3)
		{
			outOfSyncCounter = 0;

			#ifdef USE_DATABASE_APPROACH
				#ifdef CTRL_DEBUG
					uart0_sendStr("Out of sync (3): Flushing outgoing queue.\r\n");
				#endif

				os_timer_disarm(&tmrDatabaseItemSender);

				// Flush the outgoing queue, that's all we can do about it really.
				ctrl_database_delete_all();
			#endif

			#ifdef CTRL_DEBUG
				uart0_sendStr("Out of sync (3): Disconnecting!\r\n");
			#endif

			ctrl_platform_discon(&ctrlConn);
		}
		else
		{
			#ifdef CTRL_DEBUG
				char tmp[50];
				os_sprintf(tmp, "Out of sync report %u/3.\r\n", outOfSyncCounter);
				uart0_sendStr(tmp);
			#endif

			#ifdef USE_DATABASE_APPROACH
				#ifdef CTRL_DEBUG
					uart0_sendStr("Re-sending outgoing queue...\r\n");
				#endif

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
		#ifdef CTRL_DEBUG
			uart0_sendStr("CTRL AUTH ERR!\r\n");
		#endif
		connState = CTRL_AUTHENTICATION_ERROR;
	}
	else
	{
		#ifdef CTRL_DEBUG
			uart0_sendStr("CTRL AUTH OK!\r\n");
		#endif

		connState = CTRL_AUTHENTICATED;
		ctrl_stack_keepalive(1); // lets enable keepalive for our connection because that's what all cool kids do these days

		ctrlSynchronized = 1;

		#ifdef USE_DATABASE_APPROACH
			if(ctrl_database_count_unacked_items() > 0)
			{
				// start sending pending data right now
				os_timer_disarm(&tmrDatabaseItemSender);
				os_timer_arm(&tmrDatabaseItemSender, TMR_ITEMS_SENDER_MS, 0); // 0 = don't repeat automatically
			}
		#endif
	}
}

static char ICACHE_FLASH_ATTR ctrl_send_data_cb(char *data, unsigned short len)
{
	#ifdef CTRL_DEBUG
		uart0_sendStr("ctrl_send_data_cb\r\n");
	#endif

	if(connState != CTRL_TCP_CONNECTED && connState != CTRL_AUTHENTICATED)
	{
		#ifdef CTRL_DEBUG
			uart0_sendStr("ctrl_send_data_cb - not conn or not authed\r\n");
		#endif
		return ESPCONN_CONN;
	}

	/*
	uart0_sendStr("SENDING: ");
	unsigned short i;
	for(i=0; i<len; i++)
	{
		char tmp2[10];
		os_sprintf(tmp2, " 0x%X", data[i]);
		uart0_sendStr(tmp2);
	}
	uart0_sendStr(".\r\n");
	*/

	return espconn_sent(&ctrlConn, data, len);
}

static void ICACHE_FLASH_ATTR ctrl_save_TXserver_cb(unsigned long TXserver)
{
	// we don't store TXserver in FLASH as it would wear out the FLASH memory... don't know how to handle this. Maybe save every other value and in different locations?
	// by saving every other value system would still work since it is configured to try re-sending 3 times until flushing all data

	gTXserver = TXserver; // for now, store it in RAM
}

static unsigned long ICACHE_FLASH_ATTR ctrl_restore_TXserver_cb(void)
{
	// since we don't store it, we don't load it either :-)

	return gTXserver; // for now, return the last one we had from RAM
}

// all user CTRL messages is sent to Server through this function
// returns: 1 on error, 0 on success
unsigned char ICACHE_FLASH_ATTR ctrl_platform_send(char *data, unsigned short len, unsigned char notification)
{
	#ifdef CTRL_DEBUG
		uart0_sendStr("ctrl_platform_send\r\n");
	#endif

	#ifdef USE_DATABASE_APPROACH
		if(notification)
		{
			if(connState != CTRL_AUTHENTICATED || !ctrlSynchronized)
			{
				#ifdef CTRL_DEBUG
					uart0_sendStr("ctrl_platform_send not authed or not synced\r\n");
				#endif
				return 1;
			}

			return ctrl_stack_send(data, len, 0, 1); // Send notifications immediatelly, no queue and no delivery order here
		}
		else
		{
			if(!ctrlSynchronized)
			{
				#ifdef CTRL_DEBUG
					uart0_sendStr("ctrl_platform_send not synced\r\n");
				#endif
				return 1;
			}

			unsigned char ret = ctrl_database_add_row(data, len);
			// No point in starting timer if we couldn't add data to DB. If we are not authenticated
			// or synched the timer itself has that check so no problem starting it now
			if(ret == 0)
			{
				os_timer_disarm(&tmrDatabaseItemSender);
				os_timer_arm(&tmrDatabaseItemSender, TMR_ITEMS_SENDER_MS, 0); // 0 = don't repeat automatically
			}
			return ret;
		}
	#else
		if(connState != CTRL_AUTHENTICATED)
		{
			#ifdef CTRL_DEBUG
				uart0_sendStr("ctrl_platform_send not authed\r\n");
			#endif
			return 1;
		}

		unsigned char ret = ctrl_stack_send(data, len, TXbase, notification);
		TXbase++;

		return ret;
	#endif
}

// entry point to the ctrl platform
void ICACHE_FLASH_ATTR ctrl_platform_init(void)
{
	struct station_config stationConf;

	#ifndef CTRL_DEBUG
		load_flash_param(ESP_PARAM_SAVE_1, (uint32 *)&ctrlSetup, sizeof(tCtrlSetup));
		wifi_station_get_config(&stationConf);
	#else
		uart0_sendStr("Debugging, will not start configuration web server.\r\n");

		ctrlSetup.stationSetupOk = SETUP_OK_KEY;

		// tcp://ctrl.ba:8000
		ctrlSetup.serverIp[0] = 78; // ctrlSetup.serverIp = {78, 47, 48, 138};
		ctrlSetup.serverIp[1] = 47;
		ctrlSetup.serverIp[2] = 48;
		ctrlSetup.serverIp[3] = 138;

		ctrlSetup.serverPort = 8000;

		// my test Base
		ctrlSetup.baseid[0] = 0xaa;
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

		os_sprintf(stationConf.ssid, "%s", WIFI_SSID);
		os_sprintf(stationConf.password, "%s", WIFI_PASS);

		wifi_station_set_config(&stationConf);
	#endif

// debugging, to always enter config server mode
//ctrlSetup.stationSetupOk = 0;
//--

	// Must enter into Configuration mode?
	if(ctrlSetup.stationSetupOk != SETUP_OK_KEY || wifi_get_opmode() != STATION_MODE)
	{
		#ifdef CTRL_DEBUG
			wifi_set_opmode(STATION_MODE);

			uart0_sendStr("I am not in STATION mode, restarting in STATION mode...\r\n");
			system_restart();
		#endif

		// make sure we are in STATIONAP_MODE for configuration server to work
		if(wifi_get_opmode() != STATIONAP_MODE)
		{
			//wifi_station_disconnect();
			wifi_set_opmode(STATIONAP_MODE);
			#ifdef CTRL_DEBUG
				uart0_sendStr("Restarting in STATIONAP mode...\r\n");
			#endif
			system_restart();
		}

		char macaddr[6];
		wifi_get_macaddr(SOFTAP_IF, macaddr);

		struct softap_config apConfig;
		os_memset(apConfig.ssid, 0, sizeof(apConfig.ssid));
		os_sprintf(apConfig.ssid, "CTRL_%02x%02x%02x%02x%02x%02x", MAC2STR(macaddr));
		os_memset(apConfig.password, 0, sizeof(apConfig.password));
		os_sprintf(apConfig.password, "%02x%02x%02x%02x%02x%02x", MAC2STR(macaddr));
		apConfig.authmode = AUTH_WPA_PSK;
		apConfig.channel = 7;
		apConfig.max_connection = 255; // 1?
		apConfig.ssid_hidden = 0;

		wifi_softap_set_config(&apConfig);

		#ifdef CTRL_DEBUG
			char temp[80];
			os_sprintf(temp, "OPMODE: %u\r\n", wifi_get_opmode());
			uart0_sendStr(temp);
			os_sprintf(temp, "SSID: %s\r\n", apConfig.ssid);
			uart0_sendStr(temp);
			os_sprintf(temp, "PWD: %s\r\n", apConfig.password);
			uart0_sendStr(temp);
			os_sprintf(temp, "CHAN: %u\r\n", apConfig.channel);
			uart0_sendStr(temp);

			uart0_sendStr("Starting configuration web server...\r\n");
		#endif

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
		#ifdef CTRL_DEBUG
			uart0_sendStr("Starting in normal mode...\r\n");
		#endif

		// The device is now booted into "normal mode" where it acts as a STATION or STATIONAP.
		// It connects to the CTRL IoT platform via TCP socket connection. All incoming data is
		// then forwarded into the CTRL Stack and the stack will then call a callback function
		// for every received CTRL packet. That callback function is the actuall business-end
		// of the user-application.

		if(wifi_get_opmode() != STATION_MODE)
		{
			#ifdef CTRL_DEBUG
				uart0_sendStr("Restarting in STATION mode...\r\n");
			#endif
			//wifi_station_disconnect();
			wifi_set_opmode(STATION_MODE);
			system_restart();
		}

		#ifdef CTRL_DEBUG
			char temp[80];
			os_sprintf(temp, "OPMODE: %u\r\n", wifi_get_opmode());
			uart0_sendStr(temp);
			os_sprintf(temp, "SSID: %s\r\n", stationConf.ssid);
			uart0_sendStr(temp);
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

		// Init the user-app callbacks
		ctrl_app_init(&ctrlAppCallbacks);

		#ifdef CTRL_DEBUG
			uart0_sendStr("System initialization done!\r\n");
		#endif

		// Wait for WIFI connection and start TCP connection
		os_timer_disarm(&tmrLinker);
		os_timer_setfn(&tmrLinker, (os_timer_func_t *)ctrl_platform_check_ip, NULL);
		os_timer_arm(&tmrLinker, 100, 0);

		// set a timer for a LED status blinking. it will be armed from sys_status_checker() once it executes
		os_timer_disarm(&tmrStatusLedBlinker);
		os_timer_setfn(&tmrStatusLedBlinker, (os_timer_func_t *)ctrl_status_led_blinker, NULL);
		os_timer_arm(&tmrStatusLedBlinker, 200, 1); // actually lets start it right now to blink like WIFI is not available yet. 1 = repeat automatically

		#ifdef USE_DATABASE_APPROACH
			// set a timer that will send items from the database (if database approach is used)
			os_timer_disarm(&tmrDatabaseItemSender);
			os_timer_setfn(&tmrDatabaseItemSender, (os_timer_func_t *)ctrl_database_item_sender, NULL);
		#endif
	}
}
