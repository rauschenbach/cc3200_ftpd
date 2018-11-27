/* Driverlib includes */
#include <hw_types.h>
#include <hw_ints.h>
#include <rom.h>
#include <rom_map.h>
#include <interrupt.h>
#include <prcm.h>
#include "rtos_handlers.h"
#include "main.h"


static unsigned long g_ulStatus = 0;	/* SimpleLink Status */
static long time_tick = 0;		/* Считает тики */

my_ip_addr ip_addr;

unsigned long get_wlan_status(void)
{
    return g_ulStatus;
}


long get_time_tick(void)
{
   return time_tick;
}

#ifdef USE_FREERTOS
//*****************************************************************************
// FreeRTOS User Hook Functions enabled in FreeRTOSConfig.h
//*****************************************************************************

//*****************************************************************************
//
//! \brief Application defined hook (or callback) function - assert
//!
//! \param[in]  pcFile - Pointer to the File Name
//! \param[in]  ulLine - Line Number
//!
//! \return none
//!
//*****************************************************************************
void vAssertCalled(const char *pcFile, unsigned long ulLine)
{
    PRINTF("Assert: file: %s, str %d\n", pcFile, ulLine);
    //Handle Assert here
    while (1) {
    }
}

//*****************************************************************************
//
//! \brief Application defined idle task hook
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
void vApplicationIdleHook(void)
{
    //Handle Idle Hook for Profiling, Power Management etc
}


/**
 * Вызывается из операционной системы
 * ИЛИ:	Decrement the TimingDelay variable
 */
void vApplicationTickHook(void)
{
	time_tick++;
}


//*****************************************************************************
//
//! \brief Application defined malloc failed hook
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
void vApplicationMallocFailedHook()
{
    //Handle Memory Allocation Errors
    while (1) {
    }
}

//*****************************************************************************
//
//! \brief Application defined stack overflow hook
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
void vApplicationStackOverflowHook(OsiTaskHandle * pxTask, signed char *pcTaskName)
{
    //Handle FreeRTOS Stack Overflow
    while (1) {
    }
}

#endif				/* USE_FREERTOS */



//*****************************************************************************
//
//! \brief The Function Handles WLAN Events
//!
//! \param[in]  evt - Pointer to WLAN Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t * evt)
{
    u8 ssid[SSID_LEN_MAX + 1] = { 0 };	//Connection SSID
    u8 arp[BSSID_LEN_MAX] = { 0 };	//Connection BSSID

    switch (evt->Event) {
    case SL_WLAN_CONNECT_EVENT:
	{
	    SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);

	    //
	    // Information about the connected AP (like name, MAC etc) will be
	    // available in 'slWlanConnectAsyncResponse_t'-Applications
	    // can use it if required
	    //
	    //  slWlanConnectAsyncResponse_t *pEventData = NULL;
	    // pEventData = &evt->EventData.STAandP2PModeWlanConnected;
	    //
	    // Copy new connection SSID and BSSID to global parameters
	    memcpy(ssid, evt->EventData.STAandP2PModeWlanConnected.ssid_name, evt->EventData.STAandP2PModeWlanConnected.ssid_len);
	    memcpy(arp, evt->EventData.STAandP2PModeWlanConnected.bssid, SL_BSSID_LENGTH);

	    PRINTF("[WLAN EVENT] STA Connected to the AP: %s BSSID: %x:%x:%x:%x:%x:%x\n\r", ssid, arp[0], arp[1], arp[2], arp[3], arp[4], arp[5]);
	}
	break;

    case SL_WLAN_DISCONNECT_EVENT:
	{
	    slWlanConnectAsyncResponse_t *pEventData = NULL;

	    CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
	    CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

	    pEventData = &evt->EventData.STAandP2PModeDisconnected;

	    // If the user has initiated 'Disconnect' request, 
	    //'reason_code' is SL_USER_INITIATED_DISCONNECTION 
	    if (SL_USER_INITIATED_DISCONNECTION == pEventData->reason_code) {
		PRINTF
		    ("[WLAN EVENT]Device disconnected from the AP: %s, BSSID: %x:%x:%x:%x:%x:%x on application's request\n\r",
		     ssid, arp[0], arp[1], arp[2], arp[3], arp[4], arp[5]);
	    } else {
		PRINTF
		    ("[WLAN ERROR]Device disconnected from the AP AP: %s, BSSID: %x:%x:%x:%x:%x:%x on an ERROR..!!\n\r",
		     ssid, arp[0], arp[1], arp[2], arp[3], arp[4], arp[5]);
	    }
	}
	break;

    default:
	{
	    PRINTF("[WLAN EVENT] Unexpected event [0x%x]\n\r", evt->Event);
	}
	break;
    }
}



//*****************************************************************************
//
//! \brief This function handles network events such as IP acquisition, IP
//!           leased, IP released etc.
//!
//! \param[in]  pNetAppEvent - Pointer to NetApp Event Info 
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t * pNetAppEvent)
{

    switch (pNetAppEvent->Event) {
    case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
	{
	    SlIpV4AcquiredAsync_t *pEventData = NULL;

	    SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

	    //Ip Acquired Event Data
	    pEventData = &pNetAppEvent->EventData.ipAcquiredV4;

	    PRINTF("[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d , "
		   "Gateway=%d.%d.%d.%d\n\r",
		   SL_IPV4_BYTE(pEventData->ip, 3),
		   SL_IPV4_BYTE(pEventData->ip, 2),
		   SL_IPV4_BYTE(pEventData->ip, 1),
		   SL_IPV4_BYTE(pEventData->ip, 0),
		   SL_IPV4_BYTE(pEventData->gateway, 3),
		   SL_IPV4_BYTE(pEventData->gateway, 2), SL_IPV4_BYTE(pEventData->gateway, 1), SL_IPV4_BYTE(pEventData->gateway, 0));

	    /* Сохраняем используемый IP  */
	    ip_addr.ip = pEventData->ip;

/*						
				   SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 3),
				   SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 2),
				   SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 1),
				   SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 0),
				   
				   SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 3),
				   SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 2),
				   SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 1),
				   SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 0));
*/
	}
	break;

    default:
	{
	    PRINTF("[NETAPP EVENT] Unexpected event [0x%x] \n\r", pNetAppEvent->Event);
	}
	break;
    }
}


//*****************************************************************************
//
//! \brief This function handles HTTP server events
//!
//! \param[in]  pServerEvent - Contains the relevant event information
//! \param[in]    pServerResponse - Should be filled by the user with the
//!                                      relevant response information
//!
//! \return None
//!
//****************************************************************************
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t * pHttpEvent, SlHttpServerResponse_t * pHttpResponse)
{
    // Unused in this application
}

//*****************************************************************************
//
//! \brief This function handles General Events
//!
//! \param[in]     pDevEvent - Pointer to General Event Info 
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t * pDevEvent)
{
    // Most of the general errors are not FATAL are are to be handled
    // appropriately by the application
    PRINTF("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n", pDevEvent->EventData.deviceEvent.status, pDevEvent->EventData.deviceEvent.sender);
}


//*****************************************************************************
//
//! This function handles socket events indication
//!
//! \param[in]      pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t * pSock)
{
    // This application doesn't work w/ socket - Events are not expected
    switch (pSock->Event) {
    case SL_SOCKET_TX_FAILED_EVENT:
	switch (pSock->socketAsyncEvent.SockTxFailData.status) {
	case SL_ECLOSE:
	    PRINTF("[SOCK ERROR] - close socket (%d) operation failed to transmit all queued packets\n\n", 
			pSock->socketAsyncEvent.SockTxFailData.sd);
	    break;
	default:
	    PRINTF("[SOCK ERROR] - TX FAILED:  socket %d, reason (%d)\n\n", 
			pSock->socketAsyncEvent.SockTxFailData.sd, pSock->socketAsyncEvent.SockTxFailData.status);
	    break;
	}
	break;

    default:
	PRINTF("[SOCK EVENT] - Unexpected Event [%x0x]\n\n", pSock->Event);
	break;
    }
}
