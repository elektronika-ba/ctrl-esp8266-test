#ifndef __CTRL_APP_API_H
#define __CTRL_APP_API_H

#include "c_types.h"

// User parameters sector addresses (from documentation, valid is 0~3). Each sector holds 4KB
#define USER_PARAM_SEC_0				0
#define USER_PARAM_SEC_1				1
#define USER_PARAM_SEC_2				2
#define USER_PARAM_SEC_3				3

#define USER_PARAM_EXISTS				0xAA	// when settings exist in flash this is the valid-flag

#define WIFI_STATUS_INTERVAL_MS			500		// how often should wifi status checker function execute
#define	CONNECTION_STARTER_INTERVAL_MS	1000	// how often should connection starter execute

typedef struct {
	char _ok; // this holds the USER_PARAM_EXISTS value if settings are OK in flash memory

	// CTRL related
	uint8 baseid[32];
	uint8 serverIp[4];
	uint16 serverPort;

	// WiFi related
	uint8 ssid[32+1]; // +1 for sprintf string-termination
	uint8 password[64+1]; // +1 for sprintf string-termination

	// Misc
	uint8 opMode; // STATION_MODE, [SOFTAP_MODE = not allowed for CTRL project], STATIONAP_MODE

} tCtrlSetup;

typedef enum {
	ssIdle,
	ssError,
	ssGotIp
} tSysState;

void ctrl_app_api_init(void);

#endif
