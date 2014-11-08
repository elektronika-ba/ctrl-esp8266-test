#ifndef __CTRL_APP_TEMPERATURE_SIMULATOR_H
#define __CTRL_APP_TEMPERATURE_SIMULATOR_H

#include "c_types.h"
#include "ctrl_platform.h"
#include "ctrl_stack.h"

// custom functions for this app
static void ICACHE_FLASH_ATTR ctrl_app_temperature_simulator_simulate(void *);

// required functions used by ctrl_platform.c
static unsigned char ctrl_app_message_received(tCtrlMessage *);
void ctrl_app_init(tCtrlAppCallbacks *);

#endif
