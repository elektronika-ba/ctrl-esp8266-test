#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"
#include "driver/uart.h"

#include "ctrl_config_server.h"
#include "flash_param.h"
#include "ctrl_platform.h"

os_timer_t returnToNormalModeTimer;
static struct espconn esp_conn;
static esp_tcp esptcp;
static unsigned char killConn;
static unsigned char returnToNormalMode;
// http headers
static const char *http404Header = "HTTP/1.0 404 Not Found\r\nServer: CTRL-Config-Server\r\nContent-Type: text/plain\r\n\r\nNot Found (or method not implemented).\r\n";
static const char *http200Header = "HTTP/1.0 200 OK\r\nServer: CTRL-Config-Server/0.1\r\nContent-Type: text/html\r\n";
// html page header and footer
static const char *pageStart = "<html><head><title>CTRL Base Config</title><link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.1/css/bootstrap.min.css\"></head><body><form method=\"get\" action=\"/\"><input type=\"hidden\" name=\"save\" value=\"1\">\r\n";
static const char *pageEnd = "</form><br><hr><a href=\"http://my.ctrl.ba\" target=\"_blank\"><i>MY.CTRL.BA</i></a>\r\n<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.1/js/bootstrap.min.js\"></script>\r\n</body></html>\r\n";
// html pages (NOTE: make sure you don't have the '{' without the closing '}' !
static const char *pageIndex = "<h1>Welcome to CTRL Base Config</h1><ul><li><a href=\"?page=wifi\">WIFI Settings</a></li><li><a href=\"?page=ctrl\">CTRL Settings</a></li><li><a href=\"?page=return\">Return Normal Mode</a></li></ul>\r\n";
static const char *pageSetWifi = "<h1><a href=\"/\">Home</a> / WIFI Settings</h1><input type=\"hidden\" name=\"page\" value=\"wifi\"><b>SSID:</b> <input type=\"text\" name=\"ssid\" value=\"{ssid}\"><br><b>Password:</b> <input type=\"text\" name=\"pass\" value=\"{pass}\"><br><b>Status:</b> <a href=\"?page=wifi\"><u>{status}</u></a><br><br><input type=\"submit\" value=\"Save\">\r\n";
static const char *pageSetCtrl = "<h1><a href=\"/\">Home</a> / CTRL Settings</h1><input type=\"hidden\" name=\"page\" value=\"ctrl\"><b>BaseID:</b> <input type=\"text\" name=\"baseid\" value=\"{baseid}\"> (get from <a href=\"http://my.ctrl.ba\" target=\"_blank\">my.ctrl.ba</a>)<br><b>Server IP:</b> <input type=\"text\" name=\"ip\" value=\"{ip}\"> (78.47.48.138)<br><b>Port:</b> <input type=\"text\" name=\"port\" value=\"{port}\"> (8000)<br><br><input type=\"submit\" value=\"Save\">\r\n";
static const char *pageResetStarted = "<h1>Returning to Normal Mode...</h1>You can close this window now.\r\n";
static const char *pageSavedInfo = "<b style=\"color: green\">Settings Saved!</b>\r\n";

static void ICACHE_FLASH_ATTR return_to_normal_mode_cb(void *arg)
{
	wifi_station_disconnect();
	wifi_set_opmode(STATION_MODE);

	uart0_sendStr("Restarting system...\r\n");
	system_restart();
}

static void ICACHE_FLASH_ATTR ctrl_config_server_recon(void *arg, sint8 err)
{
	/*
    struct espconn *pesp_conn = (struct espconn *)arg;

	char tmp[100];

    os_sprintf(tmp, "webserver's %d.%d.%d.%d:%d err %d reconnect\n", pesp_conn->proto.tcp->remote_ip[0],
    		pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
    		pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port, err);

    uart0_sendStr(tmp);
    */

    uart0_sendStr("ctrl_config_server_recon\r\n");
}

static void ICACHE_FLASH_ATTR ctrl_config_server_discon(void *arg)
{
    /*
    struct espconn *pesp_conn = (struct espconn *)arg;

	char tmp[100];

    os_sprintf(tmp, "webserver's %d.%d.%d.%d:%d disconnect\n", pesp_conn->proto.tcp->remote_ip[0],
        		pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
        		pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port);

    uart0_sendStr(tmp);
    */

    uart0_sendStr("ctrl_config_server_discon\r\n");
}

static void ICACHE_FLASH_ATTR ctrl_config_server_recv(void *arg, char *data, unsigned short len)
{
	struct espconn *ptrespconn = (struct espconn *)arg;

	/*uart0_sendStr("RECV: ");
	unsigned short i;
	for(i=0; i<len; i++)
	{
		char tmp2[5];
		os_sprintf(tmp2, "%c", data[i]);
		uart0_sendStr(tmp2);
	}
	uart0_sendStr("\r\n");*/

	// working only with GET data
	if( os_strncmp(data, "GET ", 4) == 0 )
	{
		char page[16];
		os_memset(page, 0, sizeof(page));
		ctrl_config_server_get_key_val("page", 15, data, page);
		ctrl_config_server_process_page(ptrespconn, page, data);
		return;
	}
	else
	{
		espconn_sent(ptrespconn, (uint8 *)http404Header, os_strlen(http404Header));
		killConn = 1;
		return;
	}
}

static void ICACHE_FLASH_ATTR ctrl_config_server_process_page(struct espconn *ptrespconn, char *page, char *request)
{
	espconn_sent(ptrespconn, (uint8 *)http200Header, os_strlen(http200Header));
	espconn_sent(ptrespconn, (uint8 *)"\r\n", 2);

	// page header
	espconn_sent(ptrespconn, (uint8 *)pageStart, os_strlen(pageStart));

	// arriving data for saving?
	char save[2] = {'0', '\0'};
	ctrl_config_server_get_key_val("save", 1, request, save);

	// wifi settings page
	if( os_strncmp(page, "wifi", 4) == 0 )
	{
		struct station_config stationConf;

		// saving data?
		if( save[0] == '1' )
		{
			os_memset(stationConf.ssid, 0, sizeof(stationConf.ssid));
			os_memset(stationConf.password, 0, sizeof(stationConf.password));

			// copy parameters from URL GET to actual destination in structure
			ctrl_config_server_get_key_val("ssid", sizeof(stationConf.ssid), request, stationConf.ssid); //32
			ctrl_config_server_get_key_val("pass", sizeof(stationConf.password), request, stationConf.password); //64

			wifi_station_disconnect();
			wifi_station_set_config(&stationConf);
			wifi_station_connect();
		}
		wifi_station_get_config(&stationConf);

		// send page back to the browser, byte by byte because we will do templating here
		char *stream = (char *)pageSetWifi;
		char templateKey[16]; // 16 should be enough for: "{these_keys}"
		os_memset(templateKey, 0, sizeof(templateKey));
		unsigned char templateKeyIdx;
		while(*stream)
		{
			// start of template key?
			if(*stream == '{')
			{
				// fetch the key
				templateKeyIdx = 0;
				stream++;
				while(*stream != '}')
				{
					templateKey[templateKeyIdx++] = *stream;
					stream++;
				}

				// send the replacing value now
				if( os_strncmp(templateKey, "ssid", 4) == 0 )
				{
					espconn_sent(ptrespconn, (uint8 *)stationConf.ssid, os_strlen(stationConf.ssid));
				}
				else if( os_strncmp(templateKey, "pass", 4) == 0 )
				{
					espconn_sent(ptrespconn, (uint8 *)stationConf.password, os_strlen(stationConf.password));
				}
				else if( os_strncmp(templateKey, "status", 6) == 0 )
				{
					char status[32];
					int x = wifi_station_get_connect_status();
					if (x == STATION_GOT_IP)
					{
						os_sprintf(status, "Connected");
					}
					else if(x == STATION_WRONG_PASSWORD)
					{
						os_sprintf(status, "Wrong Password");
					}
					else if(x == STATION_NO_AP_FOUND)
					{
						os_sprintf(status, "AP Not Found");
					}
					else if(x == STATION_CONNECT_FAIL)
					{
						os_sprintf(status, "Connect failed");
					}
					else
					{
						os_sprintf(status, "Not connected");
					}

					espconn_sent(ptrespconn, (uint8 *)status, os_strlen(status));
				}
			}
			else
			{
				espconn_sent(ptrespconn, (uint8 *)stream, 1);
			}

			stream++;
		}

		// was saving?
		if( save[0] == '1' )
		{
			espconn_sent(ptrespconn, (uint8 *)pageSavedInfo, os_strlen(pageSavedInfo));
		}
	}
	// ctrl settings page
	else if( os_strncmp(page, "ctrl", 4) == 0 )
	{
		tCtrlSetup ctrlSetup;

		// saving data?
		if( save[0] == '1' )
		{
			ctrlSetup.stationSetupOk = SETUP_OK_KEY; // say that there is *some* setup in flash... hopefully it is OK as we don't validate it

			// baseid
			char baseid[33];
			char *baseidptr = baseid;
			ctrl_config_server_get_key_val("baseid", 32, request, baseid);
			unsigned char i = 0;
			while(i < 16)
			{
				char one[3] = {'0', '0', '\0'};
				one[0] = *baseidptr;
				baseidptr++;
				one[1] = *baseidptr;
				baseidptr++;
				ctrlSetup.baseid[i++] = strtol(one, NULL, 16);
			}

			// server ip
			char serverIp[16];
			ctrl_config_server_get_key_val("ip", 16, request, serverIp);
			uint32 iServerIp = ipaddr_addr(serverIp);

			char *ipParts = (char *)&iServerIp;
			ctrlSetup.serverIp[0] = ipParts[0];
			ctrlSetup.serverIp[1] = ipParts[1];
			ctrlSetup.serverIp[2] = ipParts[2];
			ctrlSetup.serverIp[3] = ipParts[3];

			// server port
			char serverPort[6];
			ctrl_config_server_get_key_val("port", 5, request, serverPort);
			ctrlSetup.serverPort = atoi(serverPort);

			save_flash_param(ESP_PARAM_SAVE_1, (uint32 *)&ctrlSetup, sizeof(tCtrlSetup));
		}

		load_flash_param(ESP_PARAM_SAVE_1, (uint32 *)&ctrlSetup, sizeof(tCtrlSetup));

		// send page back to the browser, byte by byte because we will do templating here
		char *stream = (char *)pageSetCtrl;
		char templateKey[16]; // 16 should be enough for: "{these_keys}"
		os_memset(templateKey, 0, sizeof(templateKey));
		unsigned char templateKeyIdx;
		while(*stream)
		{
			// start of template key?
			if(*stream == '{')
			{
				// fetch the key
				templateKeyIdx = 0;
				stream++;
				while(*stream != '}')
				{
					templateKey[templateKeyIdx++] = *stream;
					stream++;
				}

				// send the replacing value now
				if( os_strncmp(templateKey, "baseid", 6) == 0 )
				{
					char baseid[3];
					char i;
					for(i=0; i<16; i++)
					{
						os_sprintf(baseid, "%02x", ctrlSetup.baseid[i]);
						espconn_sent(ptrespconn, (uint8 *)baseid, os_strlen(baseid));
					}
				}
				else if( os_strncmp(templateKey, "ip", 2) == 0 )
				{
					char serverIp[16];
					os_sprintf(serverIp, "%u.%u.%u.%u", ctrlSetup.serverIp[0], ctrlSetup.serverIp[1], ctrlSetup.serverIp[2], ctrlSetup.serverIp[3]);
					espconn_sent(ptrespconn, (uint8 *)serverIp, os_strlen(serverIp));
				}
				else if( os_strncmp(templateKey, "port", 4) == 0 )
				{
					char serverPort[6];
					os_sprintf(serverPort, "%u", ctrlSetup.serverPort);
					espconn_sent(ptrespconn, (uint8 *)serverPort, os_strlen(serverPort));
				}
			}
			else
			{
				espconn_sent(ptrespconn, (uint8 *)stream, 1);
			}

			stream++;
		}

		// was saving?
		if( save[0] == '1' )
		{
			espconn_sent(ptrespconn, (uint8 *)pageSavedInfo, os_strlen(pageSavedInfo));
		}
	}
	// reset and return to normal mode of the Base station
	else if( os_strncmp(page, "return", 3) == 0 )
	{
		espconn_sent(ptrespconn, (uint8 *)pageResetStarted, os_strlen(pageResetStarted));
		returnToNormalMode = 1; // after killing the connection, we will restart and return to normal mode.
	}
	// start page (= 404 page, for simplicity)
	else
	{
		espconn_sent(ptrespconn, (uint8 *)pageIndex, os_strlen(pageIndex));
	}

	// page footer
	espconn_sent(ptrespconn, (uint8 *)pageEnd, os_strlen(pageEnd));
	killConn = 1;
}

// search for a string of the form key=value in
// a string that looks like q?xyz=abc&uvw=defgh HTTP/1.1\r\n
//
// The returned value is stored in retval. You must allocate
// enough storage for retval, maxlen is the size of retval.
//
// Return LENGTH of found value of seeked parameter. this can return 0 if parameter was there but the value was missing!
static unsigned char ICACHE_FLASH_ATTR ctrl_config_server_get_key_val(char *key, unsigned char maxlen, char *str, char *retval)
{
	unsigned char found = 0;
	char *keyptr = key;
	char prev_char = '\0';
	*retval = '\0';

	while( *str && *str!='\r' && *str!='\n' && !found )
	{
		// GET /whatever?page=wifi&action=search HTTP/1.1\r\n
		if(*str == *keyptr)
		{
			// At the beginning of the key we must check if this is the start of the key otherwise we will
			// match on 'foobar' when only looking for 'bar', by andras tucsni, modified by trax
			if(keyptr == key && !( prev_char == '?' || prev_char == '&' ) ) // trax: accessing (str-1) can be a problem if the incoming string starts with the key itself!
			{
				str++;
				continue;
			}

			keyptr++;

			if (*keyptr == '\0')
			{
				str++;
				keyptr = key;
				if (*str == '=')
				{
					found = 1;
				}
			}
		}
		else
		{
			keyptr = key;
		}
		prev_char = *str;
		str++;
	}

	if(found == 1)
	{
		found = 0;

		// copy the value to a buffer and terminate it with '\0'
		while( *str && *str!='\r' && *str!='\n' && *str!=' ' && *str!='&' && maxlen>0 )
		{
			*retval = *str;
			maxlen--;
			str++;
			retval++;
			found++;
		}
		*retval = '\0';
	}

	return found;
}

static void ICACHE_FLASH_ATTR ctrl_config_server_sent(void *arg)
{
    struct espconn *pesp_conn = (struct espconn *)arg;

	if (pesp_conn == NULL)
	{
		return;
	}

	if(killConn)
	{
		espconn_disconnect(pesp_conn);

		if(returnToNormalMode)
		{
			os_timer_arm(&returnToNormalModeTimer, 500, 0);
		}
	}
}

static void ICACHE_FLASH_ATTR ctrl_config_server_connect(void *arg)
{
    struct espconn *pesp_conn = (struct espconn *)arg;

	uart0_sendStr("ctrl_config_server_connect\r\n");

    espconn_regist_recvcb(pesp_conn, ctrl_config_server_recv);
    espconn_regist_reconcb(pesp_conn, ctrl_config_server_recon);
    espconn_regist_disconcb(pesp_conn, ctrl_config_server_discon);
    espconn_regist_sentcb(pesp_conn, ctrl_config_server_sent);
}

// all socket data which is received is flushed into this function
void ICACHE_FLASH_ATTR ctrl_config_server_init()
{
	uart0_sendStr("ctrl_config_server_init()\r\n");

	os_timer_disarm(&returnToNormalModeTimer);
	os_timer_setfn(&returnToNormalModeTimer, return_to_normal_mode_cb, NULL);

    esptcp.local_port = 80;

    esp_conn.type = ESPCONN_TCP;
    esp_conn.state = ESPCONN_NONE;
    esp_conn.proto.tcp = &esptcp;
    espconn_regist_connectcb(&esp_conn, ctrl_config_server_connect);

    espconn_accept(&esp_conn);
}
