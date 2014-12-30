#ifndef __CTRL_CONFIG_SERVER_H
#define __CTRL_CONFIG_SERVER_H

#include "c_types.h"

typedef struct HttpdPriv HttpdPriv;
typedef struct HttpdConnData HttpdConnData;

//A struct describing a http connection. This gets passed to cgi functions.
struct HttpdConnData {
	struct espconn *conn;
	HttpdPriv *priv;
	int postLen;
};

//Max send buffer len
#define MAX_SENDBUFF_LEN 2048

// private
static void ctrl_config_server_process_page(struct HttpdConnData *, char *, char *);
static unsigned char ctrl_config_server_get_key_val(char *, unsigned char, char *, char *);
static void ctrl_config_server_sent(void *);
static void ctrl_config_server_recon(void *, sint8);
static void ctrl_config_server_discon(void *);
static void ctrl_config_server_recv(void *, char *, unsigned short);
static void ctrl_config_server_listen(void *);
static HttpdConnData *httpdFindConnData(void *arg);
static void xmitSendBuff(HttpdConnData *conn);
int httpdSend(HttpdConnData *conn, const char *data, int len);
void httpdStartResponse(HttpdConnData *conn, int code);
void httpdHeader(HttpdConnData *conn, const char *field, const char *val);
void httpdEndHeaders(HttpdConnData *conn);
static void httpdRetireConn(HttpdConnData *conn);
char *str_replace(const char *orig, char *rep, char *with);
void bin2strhex(unsigned char *bin, unsigned int binsz, char **result);

// public
void ctrl_config_server_init();

#endif
