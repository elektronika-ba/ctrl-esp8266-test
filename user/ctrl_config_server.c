#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"
#include "driver/uart.h"

#include "ctrl_config_server.h"
#include "flash_param.h"

static struct espconn esp_conn;
static esp_tcp esptcp;
static unsigned char killConn;
static const char *http404Header = "HTTP/1.0 404 Not Found\r\nServer: CTRL-Config-Server\r\nContent-Type: text/plain\r\n\r\nNot Found.\r\n";
static const char *http200Header = "HTTP/1.0 200 OK\r\nServer: CTRL-Config-Server/0.1\r\nContent-Type: text/html\r\n";

static void ICACHE_FLASH_ATTR ctrl_config_server_recon(void *arg, sint8 err)
{
    struct espconn *pesp_conn = (struct espconn *)arg;

	char tmp[100];

    os_sprintf(tmp, "webserver's %d.%d.%d.%d:%d err %d reconnect\n", pesp_conn->proto.tcp->remote_ip[0],
    		pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
    		pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port, err);

    uart0_sendStr(tmp);
}

static void ICACHE_FLASH_ATTR ctrl_config_server_discon(void *arg)
{
    struct espconn *pesp_conn = (struct espconn *)arg;

	char tmp[100];

    os_sprintf(tmp, "webserver's %d.%d.%d.%d:%d disconnect\n", pesp_conn->proto.tcp->remote_ip[0],
        		pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
        		pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port);

    uart0_sendStr(tmp);
}

static void ICACHE_FLASH_ATTR ctrl_config_server_recv(void *arg, char *data, unsigned short len)
{
	struct espconn *ptrespconn = (struct espconn *)arg;

	/*
	uart0_sendStr("RECV: ");
	unsigned short i;
	for(i=0; i<len; i++)
	{
		char tmp2[5];
		os_sprintf(tmp2, "%c", data[i]);
		uart0_sendStr(tmp2);
	}
	uart0_sendStr("\r\n");
	*/

	// working only with GET data
	if( os_strncmp(data, "GET ", 4) == 0 ) {
		uart0_sendStr("GET METHOD.\r\n");

		char page[16+1];
		if( len<10 || ctrl_config_server_get_key_val("page", 16, data, page, '&') == 0 )
		{
			uart0_sendStr("Page param not provided!\r\n");
			espconn_sent(ptrespconn, (uint8 *)http404Header, os_strlen(http404Header));
			killConn = 1;
			return;
		}

		ctrl_config_server_process_page(ptrespconn, page);
		return;
	}
	else
	{
		uart0_sendStr("Error, only GET method implemented!\r\n");
		espconn_sent(ptrespconn, (uint8 *)http404Header, os_strlen(http404Header));
		killConn = 1;
		return;
	}
}

static void ICACHE_FLASH_ATTR ctrl_config_server_process_page(struct espconn *ptrespconn, char *page)
{
	espconn_sent(ptrespconn, (uint8 *)http200Header, os_strlen(http200Header));
	espconn_sent(ptrespconn, (uint8 *)"\r\n", 2);

	char mure[100];
	os_sprintf(mure, "You requested Page <strong>%s</strong>\r\n", page);
	espconn_sent(ptrespconn, mure, os_strlen(mure));

	killConn = 1;
}

// search for a string of the form key=value in
// a string that looks like q?xyz=abc&uvw=defgh HTTP/1.1\r\n
//
// The returned value is stored in retval. You must allocate
// enough storage for retval, maxlen is the size of retval.
//
// It can also work like this: p=param1=value1$param2=value2$param3=value3 ... if ampchar='$' instead of '&' :)
// 																	 ("page", os_strlen(page), data, page, '&')
// Return LENGTH of found value of seeked parameter. this can return 0 if parameter was there but the value was missing!
static unsigned char ICACHE_FLASH_ATTR ctrl_config_server_get_key_val(char *key, unsigned char maxlen, char *str, char *retval, char ampchar)
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
			if(keyptr == key && !( prev_char == '?' || prev_char == ampchar ) ) // trax: accessing (str-1) can be a problem if the incoming string starts with the key itself!
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
		while( *str && *str!='\r' && *str!='\n' && *str!=' ' && *str!=ampchar && maxlen>0 )
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

    esptcp.local_port = 80;

    esp_conn.type = ESPCONN_TCP;
    esp_conn.state = ESPCONN_NONE;
    esp_conn.proto.tcp = &esptcp;
    espconn_regist_connectcb(&esp_conn, ctrl_config_server_connect);

    espconn_accept(&esp_conn);
}
