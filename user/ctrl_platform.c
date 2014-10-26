#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"

#include "ctrl_platform.h"
#include "ctrl_config_server.h"
#include "driver/uart.h"

os_timer_t tmrWifiStatus;

static void tcpclient_discon_cb(void *arg);

// this function is executed in timer regularly and manages the WiFi connection
void ICACHE_FLASH_ATTR wifi_status(void *arg)
{
	os_timer_disarm(&tmrWifiStatus);

	uint8 state = wifi_station_get_connect_status();
	if(state == STATION_GOT_IP)
	{
		// start blinking status LED for WIFI connected (~1Hz)
		ctrl_setup_connection();
	}
	else if(state == STATION_NO_AP_FOUND || state == STATION_WRONG_PASSWORD || state == STATION_CONNECT_FAIL)
	{
		// start blinking status LED for error (~5Hz)
	}

	os_timer_arm(&tmrWifiStatus, TMR_WIFI_STATUS_MS, 0);
}

// this sets up a TCP connection
void ICACHE_FLASH_ATTR ctrl_setup_connection()
{

}

/**
  * @brief  Tcp client connect success callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
static void ICACHE_FLASH_ATTR tcpclient_connect_cb(void *arg)
{
	struct espconn *pespconn = (struct espconn *)arg;
	at_linkConType *linkTemp = (at_linkConType *)pespconn->reverse;
/*
	os_printf("tcp client connect\r\n");
	os_printf("pespconn %p\r\n", pespconn);

	linkTemp->linkEn = TRUE;
	linkTemp->teType = teClient;
	linkTemp->repeaTime = 0;
	espconn_regist_disconcb(pespconn, at_tcpclient_discon_cb);
	espconn_regist_recvcb(pespconn, at_tcpclient_recv);////////
	espconn_regist_sentcb(pespconn, at_tcpclient_sent_cb);///////

	mdState = m_linked;
	//  at_linkNum++;
	at_backOk;
	uart0_sendStr("Linked\r\n");
	specialAtState = TRUE;
	at_state = at_statIdle;
*/
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

		uart0_sendStr("Connecting to WIFI...");

		// set a timer to check wifi connection progress
		os_timer_disarm(&tmrWifiStatus);
		os_timer_setfn(&tmrWifiStatus, (os_timer_func_t *)wifi_status, NULL);
		os_timer_arm(&tmrWifiStatus, TMR_WIFI_STATUS_MS, 0); // 0=we will repeat it on our own
	}
}
