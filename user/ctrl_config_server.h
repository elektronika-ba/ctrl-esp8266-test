#ifndef __CTRL_CONFIG_SERVER_H
#define __CTRL_CONFIG_SERVER_H

#include "c_types.h"

// private
static void ctrl_config_server_process_page(struct espconn *, char *, char *);
static unsigned char ctrl_config_server_get_key_val(char *, unsigned char, char *, char *);
static void ctrl_config_server_sent(void *);
static void ctrl_config_server_recon(void *, sint8);
static void ctrl_config_server_discon(void *);
static void ctrl_config_server_recv(void *, char *, unsigned short);
static void ctrl_config_server_listen(void *);

// public
void ctrl_config_server_init();

#endif
