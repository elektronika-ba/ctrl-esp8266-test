#ifndef __CTRL_PLATFORM_H
#define __CTRL_PLATFORM_H

#include "c_types.h"
#include "ctrl_stack.h"

// When defined, spits out various debugging messages on UART
// and also doesn't use the WEB SERVER to setup the system but
// uses defined values in code.
// When defined, you also need to have wifi_debug_params.h file
// with two string-defines: WIFI_SSID and WIFI_PASS.
//#define CTRL_DEBUG

// When defined (also include "ctrl_database.h"!), the platform stores
// outgoing messages in database before sending them to CTRL Server.
// This means that you don't need to think about messages being
// delivered to Server because they WILL BE.
// When not defined, you need to handle acknowledging current
// transmission and re-transmitting it if something happens.
#define USE_DATABASE_APPROACH

#define SETUP_OK_KEY					0xAA4529BA	// MAGIC VALUE. When settings exist in flash this is the valid-flag.

#ifdef USE_DATABASE_APPROACH
	#define TMR_ITEMS_SENDER_MS			300		// sending of all outgoing items when using the database approach
#endif

typedef enum {
	CTRL_WIFI_CONNECTING,
	CTRL_WIFI_CONNECTING_ERROR,
	CTRL_TCP_DISCONNECTED,
	CTRL_TCP_CONNECTING,
	CTRL_TCP_CONNECTING_ERROR,
	CTRL_TCP_CONNECTED,
	CTRL_AUTHENTICATED,
	CTRL_AUTHENTICATION_ERROR
} tCtrlConnState;

// WARNING: this structure's memory amount must be dividable by 4 in order to save to FLASH memory!!!
typedef struct {
	unsigned long stationSetupOk; // this holds the SETUP_OK_KEY value if settings are OK in flash memory
	char baseid[16];
	char aes128Key[16];
	char serverIp[4];
	unsigned int serverPort;

	char pad[2]; // added for the structure to be dividable by 4!
} tCtrlSetup;

typedef struct {
	unsigned char(*message_received)(tCtrlMessage *);
} tCtrlAppCallbacks;

// private
static void ctrl_platform_check_ip(void *);
static void ctrl_platform_recon_cb(void *, sint8);
static void ctrl_platform_sent_cb(void *);
static void ctrl_platform_recv_cb(void *, char *, unsigned short);
static void ctrl_platform_connect_cb(void *);
static void ctrl_platform_discon_cb(void *);
static void ctrl_status_led_blinker(void *);
// CTRL stack callbacks
static void ctrl_message_recv_cb(tCtrlMessage *);
static void ctrl_message_ack_cb(tCtrlMessage *);
static void ctrl_auth_response_cb(unsigned char);
static char ctrl_send_data_cb(char *, unsigned short);
static void ctrl_save_TXserver_cb(unsigned long);
static unsigned long ctrl_restore_TXserver_cb(void);

// public
unsigned char ctrl_platform_send(char *, unsigned short, unsigned char);
void ctrl_platform_init(void);

#endif
