#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"
#include "driver/uart.h"

#include "ctrl_config_server.h"
#include "flash_param.h"
#include "ctrl_platform.h"
#include "wifi.h"

//Max amount of connections
#define MAX_CONN 8

//Private data for http connection
struct HttpdPriv {
	char *sendBuff;
	int sendBuffLen;
};

//Connection pool
static HttpdPriv connPrivData[MAX_CONN];
static HttpdConnData connData[MAX_CONN];

os_timer_t returnToNormalModeTimer;
static struct espconn esp_conn;
static esp_tcp esptcp;
static unsigned char killConn;
static unsigned char returnToNormalMode;
// html page header and footer
static const char *pageStart = "<html><head><title>CTRL Base Config</title><style>body{font-family: Arial}</style></head><body><form method=\"get\" action=\"/\"><input type=\"hidden\" name=\"save\" value=\"1\">\r\n";
static const char *pageEnd = "</form><hr><a href=\"https://my.ctrl.ba\" target=\"_blank\"><i>my.ctrl.ba</i></a>\r\n</body></html>\r\n";
// html pages (NOTE: make sure you don't have the '{' without the closing '}' !
static const char *pageIndex = "<h2>Welcome to CTRL Base Config</h2><ul><li><a href=\"?page=wifi\">WIFI Settings</a></li><li><a href=\"?page=ctrl\">CTRL Settings</a></li><li><a href=\"?page=return\">Return Normal Mode</a></li></ul>\r\n";
static const char *pageSetWifi = "<h2><a href=\"/\">Home</a> / WIFI Settings</h2><input type=\"hidden\" name=\"page\" value=\"wifi\"><table border=\"0\"><tr><td><b>SSID:</b></td><td><input type=\"text\" name=\"ssid\" value=\"{ssid}\" size=\"40\"></td></tr><tr><td><b>Password:</b></td><td><input type=\"text\" name=\"pass\" value=\"***\" size=\"40\"></td></tr><tr><td><b>Status:</b></td><td>{status} <a href=\"?page=wifi\">[refresh]</a></td></tr><tr><td></td><td><input type=\"submit\" value=\"Save\"></td></tr></table>\r\n";
static const char *pageSetCtrl = "<h2><a href=\"/\">Home</a> / CTRL Settings</h2><input type=\"hidden\" name=\"page\" value=\"ctrl\"><table border=\"0\"><tr><td><b>Base ID:</b></td><td><input type=\"text\" name=\"baseid\" value=\"{baseid}\" size=\"40\"></td><td>(get from <a href=\"https://my.ctrl.ba\" target=\"_blank\">my.ctrl.ba</a>)</td></tr><tr><td><b>AES-128 Key:</b></td><td><input type=\"text\" name=\"crypt\" value=\"{crypt}\" size=\"40\"></td><td>(get from <a href=\"https://my.ctrl.ba\" target=\"_blank\">my.ctrl.ba</a>)</td></tr><tr><td><b>Server IP:</b></td><td><input type=\"text\" name=\"ip\" value=\"{ip}\" size=\"18\"></td><td>(78.47.48.138)</td></tr><tr><td><b>Port:</b></td><td><input type=\"text\" name=\"port\" value=\"{port}\" size=\"5\"></td><td>(8000)</td></tr><tr><td></td><td><input type=\"submit\" value=\"Save\"></td><td></td></tr></table>\r\n";
static const char *pageResetStarted = "<h1>Returning to Normal Mode...</h1>You can close this window now.\r\n";
static const char *pageSavedInfo = "<br><b style=\"color: green\">Settings Saved!</b>\r\n";

static void ICACHE_FLASH_ATTR return_to_normal_mode_cb(void *arg)
{
	wifi_station_disconnect();
	wifi_set_opmode(STATION_MODE);

	#ifdef CTRL_LOGGING
		os_printf("Restarting system...\r\n");
	#endif

	system_restart();
}

static void ICACHE_FLASH_ATTR ctrl_config_server_recon(void *arg, sint8 err)
{
	HttpdConnData *conn=httpdFindConnData(arg);
	#ifdef CTRL_LOGGING
		os_printf("ctrl_config_server_recon\r\n");
	#endif
	if (conn==NULL)
		return;
}

static void ICACHE_FLASH_ATTR ctrl_config_server_discon(void *arg)
{
	#ifdef CTRL_LOGGING
		os_printf("ctrl_config_server_discon\r\n");
	#endif
#if 0
	//Stupid esp sdk passes through wrong arg here, namely the one of the *listening* socket.
	//If it ever gets fixed, be sure to update the code in this snippet; it's probably out-of-date.
	HttpdConnData *conn=httpdFindConnData(arg);
	#ifdef CTRL_LOGGING
		os_printf("Disconnected, conn=%p\n", conn);
	#endif
	if (conn==NULL)
		return;
	conn->conn=NULL;
#endif
	//Just look at all the sockets and kill the slot if needed.
	int i;
	for (i=0; i<MAX_CONN; i++) {
		if (connData[i].conn!=NULL) {
			//Why the >=ESPCONN_CLOSE and not ==? Well, seems the stack sometimes de-allocates
			//espconns under our noses, especially when connections are interrupted. The memory
			//is then used for something else, and we can use that to capture *most* of the
			//disconnect cases.
			if (connData[i].conn->state==ESPCONN_NONE || connData[i].conn->state>=ESPCONN_CLOSE) {
				connData[i].conn=NULL;
				httpdRetireConn(&connData[i]);
			}
		}
	}
}

static void ICACHE_FLASH_ATTR ctrl_config_server_recv(void *arg, char *data, unsigned short len)
{
	char sendBuff[MAX_SENDBUFF_LEN];
	HttpdConnData *conn = httpdFindConnData(arg);
	#ifdef CTRL_LOGGING
		os_printf("ctrl_config_server_recv\r\n");
	#endif

	if (conn==NULL)
		return;
	conn->priv->sendBuff = sendBuff;
	conn->priv->sendBuffLen = 0;

	/*os_printf("RECV: ");
	unsigned short i;
	for(i=0; i<len; i++)
	{
		char tmp2[5];
		os_sprintf(tmp2, "%c", data[i]);
		os_printf(tmp2);
	}
	os_printf("\r\n");*/

	// working only with GET data
	if( os_strncmp(data, "GET ", 4) == 0 )
	{
		char page[16];
		os_memset(page, 0, sizeof(page));
		ctrl_config_server_get_key_val("page", 15, data, page);
		ctrl_config_server_process_page(conn, page, data);
	}
	else
	{
		const char *notfound="404 Not Found (or method not implemented).";
		httpdStartResponse(conn, 404);
		httpdHeader(conn, "Content-Type", "text/plain");
		httpdEndHeaders(conn);
		httpdSend(conn, notfound, -1);
		killConn = 1;
	}
	xmitSendBuff(conn);
	return;
}

static void ICACHE_FLASH_ATTR ctrl_config_server_process_page(struct HttpdConnData *conn, char *page, char *request)
{
	#ifdef CTRL_LOGGING
	os_printf("ctrl_config_server_process_page start\r\n");
	#endif

	httpdStartResponse(conn, 200);
	httpdHeader(conn, "Content-Type", "text/html");
	httpdEndHeaders(conn);
	// page header
	char buff[1024];
	char html_buff[1024];
	int len;
	len = os_sprintf(buff, pageStart);
	if(!httpdSend(conn, buff, len)) {
		#ifdef CTRL_LOGGING
		os_printf("Error httpdSend: pageStart out-of-memory\r\n");
		#endif
	}

	// arriving data for saving?
	char save[2] = {'0', '\0'};
	ctrl_config_server_get_key_val("save", 1, request, save);

	// wifi settings page
	if( os_strncmp(page, "wifi", 4) == 0 )
	{
		struct station_config stationConf;
		wifi_station_get_config(&stationConf);

		// saving data?
		if( save[0] == '1' )
		{
			os_memset(stationConf.ssid, 0, sizeof(stationConf.ssid));

			// copy parameters from URL GET to actual destination in structure
			ctrl_config_server_get_key_val("ssid", sizeof(stationConf.ssid), request, stationConf.ssid); //32

			// set password? we have to hide it...
			char pass[64];
			ctrl_config_server_get_key_val("pass", sizeof(pass), request, pass); //64
			if( os_strncmp(pass, "***", 3) != 0 )
			{
				os_memset(stationConf.password, 0, sizeof(stationConf.password));
				// copy parameters from URL GET to actual destination in structure
				ctrl_config_server_get_key_val("pass", sizeof(stationConf.password), request, stationConf.password); //64
			}
			// Init WiFi in STA mode
			setup_wifi_st_mode(stationConf);
			wifi_station_get_config(&stationConf);
		}

		os_sprintf(html_buff, "%s", str_replace(pageSetWifi, "{ssid}", stationConf.ssid));
		os_sprintf(html_buff, "%s", str_replace(html_buff, "{pass}", stationConf.password));
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
			os_sprintf(status, "Connect Failed");
		}
		else
		{
			os_sprintf(status, "Not Connected");
		}
		os_sprintf(html_buff, "%s", str_replace(html_buff, "{status}", status));

		// was saving?
		if(save[0] == '1')
		{
			char buff_saved[512];
			os_sprintf(buff_saved, "%s%s", html_buff, pageSavedInfo);
			len = os_sprintf(buff, buff_saved);
			httpdSend(conn, buff, len);
		} else {
			len = os_sprintf(buff, html_buff);
			httpdSend(conn, buff, len);
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

			// aes 128 key
			char aes128key[33];
			char *aes128keyptr = aes128key;
			ctrl_config_server_get_key_val("crypt", 32, request, aes128key);
			i = 0;
			while(i < 16)
			{
				char one[3] = {'0', '0', '\0'};
				one[0] = *aes128keyptr;
				aes128keyptr++;
				one[1] = *aes128keyptr;
				aes128keyptr++;
				ctrlSetup.aes128Key[i++] = strtol(one, NULL, 16);
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

		char i;
		char *result;
		bin2strhex((char *)ctrlSetup.baseid, sizeof(ctrlSetup.baseid), &result);
		os_sprintf(html_buff, "%s", str_replace(pageSetCtrl, "{baseid}", result));
		os_free(result);
		bin2strhex((char *)ctrlSetup.aes128Key, sizeof(ctrlSetup.aes128Key), &result);
		os_sprintf(html_buff, "%s", str_replace(html_buff, "{crypt}", result));
		os_free(result);
		char serverIp[16];
		os_sprintf(serverIp, "%u.%u.%u.%u", ctrlSetup.serverIp[0], ctrlSetup.serverIp[1], ctrlSetup.serverIp[2], ctrlSetup.serverIp[3]);
		os_sprintf(html_buff, "%s", str_replace(html_buff, "{ip}", serverIp));
		char serverPort[6];
		os_sprintf(serverPort, "%u", ctrlSetup.serverPort);
		os_sprintf(html_buff, "%s", str_replace(html_buff, "{port}", serverPort));

		// was saving?
		if( save[0] == '1' )
		{
			char buff_saved[1024];
			os_sprintf(buff_saved, "%s%s", html_buff, pageSavedInfo);
			len = os_sprintf(buff, buff_saved);
			httpdSend(conn, buff, len);
		} else {
			len = os_sprintf(buff, html_buff);
			httpdSend(conn, buff, len);
		}

	}
	// reset and return to normal mode of the Base station
	else if( os_strncmp(page, "return", 3) == 0 )
	{
		len = os_sprintf(buff, pageResetStarted);
		httpdSend(conn, buff, len);
		returnToNormalMode = 1; // after killing the connection, we will restart and return to normal mode.
	}
	// start page (= 404 page, for simplicity)
	else
	{
		len = os_sprintf(buff, pageIndex);
		if(!httpdSend(conn, buff, len)){
			#ifdef CTRL_LOGGING
				os_printf("Error httpdSend: pageIndex out-of-memory\r\n");
			#endif
		}
	}

	// page footer
	len = os_sprintf(buff, pageEnd);
	if(!httpdSend(conn, buff, len)){
		#ifdef CTRL_LOGGING
			os_printf("Error httpdSend: pageEnd out-of-memory\r\n");
		#endif
	}
	killConn = 1;
	#ifdef CTRL_LOGGING
	os_printf("ctrl_config_server_process_page end\r\n");
	#endif
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
	HttpdConnData *conn = httpdFindConnData(arg);
	#ifdef CTRL_LOGGING
		os_printf("ctrl_config_server_sent\r\n");
	#endif

	if (conn==NULL) return;

	if(killConn)
	{
		espconn_disconnect(conn->conn);
		if(returnToNormalMode)
		{
			os_timer_arm(&returnToNormalModeTimer, 500, 0);
		}
	}
}

static void ICACHE_FLASH_ATTR ctrl_config_server_connect(void *arg)
{
	struct espconn *conn=arg;
	int i;
	//Find empty conndata in pool
	for (i=0; i<MAX_CONN; i++)
		if (connData[i].conn==NULL) break;
	#ifdef CTRL_LOGGING
		os_printf("Con req, conn=%p, pool slot %d\n", conn, i);
	#endif
	connData[i].priv = &connPrivData[i];
	if (i==MAX_CONN) {
		#ifdef CTRL_LOGGING
			os_printf("Conn pool overflow!\r\n");
		#endif
		espconn_disconnect(conn);
		return;
	}
	connData[i].conn = conn;
	connData[i].postLen = 0;

	#ifdef CTRL_LOGGING
		os_printf("ctrl_config_server_connect\r\n");
	#endif

    espconn_regist_recvcb(conn, ctrl_config_server_recv);
    espconn_regist_reconcb(conn, ctrl_config_server_recon);
    espconn_regist_disconcb(conn, ctrl_config_server_discon);
    espconn_regist_sentcb(conn, ctrl_config_server_sent);
}

// all socket data which is received is flushed into this function
void ICACHE_FLASH_ATTR ctrl_config_server_init()
{
	int i;

	#ifdef CTRL_LOGGING
		os_printf("ctrl_config_server_init()\r\n");
	#endif

	os_timer_disarm(&returnToNormalModeTimer);
	os_timer_setfn(&returnToNormalModeTimer, return_to_normal_mode_cb, NULL);

	for (i=0; i<MAX_CONN; i++) {
		connData[i].conn=NULL;
	}

    esptcp.local_port = 80;
    esp_conn.type = ESPCONN_TCP;
    esp_conn.state = ESPCONN_NONE;
    esp_conn.proto.tcp = &esptcp;
    espconn_regist_connectcb(&esp_conn, ctrl_config_server_connect);

    espconn_accept(&esp_conn);
}

//Looks up the connData info for a specific esp connection
static HttpdConnData ICACHE_FLASH_ATTR *httpdFindConnData(void *arg) {
	int i;
	for (i=0; i<MAX_CONN; i++) {
		if (connData[i].conn==(struct espconn *)arg)
			return &connData[i];
	}
	#ifdef CTRL_LOGGING
		os_printf("FindConnData: Couldn't find connection for %p\n", arg);
	#endif
	return NULL; //WtF?
}

//Add data to the send buffer. len is the length of the data. If len is -1
//the data is seen as a C-string.
//Returns 1 for success, 0 for out-of-memory.
int ICACHE_FLASH_ATTR httpdSend(HttpdConnData *conn, const char *data, int len) {
	if (len<0)
		len = strlen(data);
	if (conn->priv->sendBuffLen+len > MAX_SENDBUFF_LEN)
		return 0;
	os_memcpy(conn->priv->sendBuff+conn->priv->sendBuffLen, data, len);
	conn->priv->sendBuffLen += len;
	return 1;
}

//Helper function to send any data in conn->priv->sendBuff
static void ICACHE_FLASH_ATTR xmitSendBuff(HttpdConnData *conn) {
	if (conn->priv->sendBuffLen != 0) {
		#ifdef CTRL_LOGGING
			os_printf("xmitSendBuff\r\n");
		#endif
		espconn_sent(conn->conn, (uint8_t*)conn->priv->sendBuff, conn->priv->sendBuffLen);
		conn->priv->sendBuffLen = 0;
	}
}

//Start the response headers.
void ICACHE_FLASH_ATTR httpdStartResponse(HttpdConnData *conn, int code) {
	char buff[128];
	int l;
	l = os_sprintf(buff, "HTTP/1.0 %d OK\r\nServer: CTRL-Config-Server/0.1\r\n", code);
	httpdSend(conn, buff, l);
}

//Send a http header.
void ICACHE_FLASH_ATTR httpdHeader(HttpdConnData *conn, const char *field, const char *val) {
	char buff[256];
	int l;
	l = os_sprintf(buff, "%s: %s\r\n", field, val);
	httpdSend(conn, buff, l);
}

//Finish the headers.
void ICACHE_FLASH_ATTR httpdEndHeaders(HttpdConnData *conn) {
	httpdSend(conn, "\r\n", -1);
}

static void ICACHE_FLASH_ATTR httpdRetireConn(HttpdConnData *conn) {
	conn->conn=NULL;
}

// You must free the result if result is non-NULL.
char *str_replace(const char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep
    int len_with; // length of with
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    if (!orig)
        return NULL;
    if (!rep)
        rep = "";
    len_rep = strlen(rep);
    if (!with)
        with = "";
    len_with = strlen(with);

    ins = (char*)orig;
    for (count = 0; tmp = strstr(ins, rep); ++count) {
        ins = tmp + len_rep;
    }

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    tmp = result = (char *)os_malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    while (count--) {
        ins = (char *)os_strstr(orig, rep);
        len_front = ins - orig;
        tmp = (char *)os_strncpy(tmp, orig, len_front) + len_front;
        tmp = (char *)os_strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    os_strcpy(tmp, orig);
    return result;
}

void bin2strhex(unsigned char *bin, unsigned int binsz, char **result)
{
	char hex_str[]= "0123456789abcdef";
	unsigned int i;
	*result = (char *)os_malloc(binsz*2+1);
	(*result)[binsz*2] = 0;
	if (!binsz)
		return;
	for (i = 0; i < binsz; i++)
	{
		(*result)[i*2+0] = hex_str[(bin[i] >> 4) & 0x0F];
		(*result)[i*2+1] = hex_str[(bin[i]) & 0x0F];
	}
}
