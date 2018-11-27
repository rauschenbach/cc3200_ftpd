#ifndef RTOS_HANDLERS_H
#define RTOS_HANDLERS_H

//Driverlib includes
#include <hw_types.h>
#include <hw_ints.h>
#include <rom.h>
#include <rom_map.h>
#include <interrupt.h>
#include <prcm.h>


#include <simplelink.h>
#include <wlan.h>
#include <user.h>
#include <common.h>


unsigned long get_wlan_status(void);
long get_time_tick(void);
void SimpleLinkWlanEventHandler(SlWlanEvent_t*);
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t*);
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t*, SlHttpServerResponse_t*);
void SimpleLinkSockEventHandler(SlSockEvent_t*);
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t*);

#endif /* rtos_handlers.h  */
