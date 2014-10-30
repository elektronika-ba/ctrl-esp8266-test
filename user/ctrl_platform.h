#ifndef __CTRL_PLATFORM_H
#define __CTRL_PLATFORM_H

#include "c_types.h"

#define CTRL_DEBUG						1

// User parameters sector addresses (from documentation, valid is 0~3). Each sector holds 4KB
#define USER_PARAM_SEC_0				0
#define USER_PARAM_SEC_1				1
#define USER_PARAM_SEC_2				2
#define USER_PARAM_SEC_3				3

#define SETUP_OK_KEY					0xAA	// when settings exist in flash this is the valid-flag

#define TMR_SYS_STATUS_CHECKER_MS		1000	// how often should wifi status checker function execute in ms

typedef struct {
	char setupOk; // this holds the USER_PARAM_EXISTS value if settings are OK in flash memory

	// CTRL related
	uint8 baseid[32];
	uint8 serverIp[4];
	uint16 serverPort;
} tCtrlSetup;

// private
static void tcpclient_discon_cb(void *);
static void tcpclient_recon_cb(void *, sint8);
static void tcpclient_connect_cb(void *);
static void tcpclient_recv(void *, char *, unsigned short);
static void tcpclient_sent_cb(void *);
static char ctrl_send_data_cb(char *, unsigned short);
static void ctrl_connection_recreate(void);

// public
void ctrl_platform_init(void);

#endif
