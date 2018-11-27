#ifndef _LANTASK_H
#define _LANTASK_H


/* Application specific status/error codes */
typedef enum {
    // Choosing -0x7D0 to avoid overlap w/ host-driver's error codes
    LAN_CONNECTION_FAILED = -0x7D0,
    INTERNET_CONNECTION_FAILED = LAN_CONNECTION_FAILED - 1,
    DEVICE_NOT_IN_STATION_MODE = INTERNET_CONNECTION_FAILED - 1,
    SOCKET_CREATE_ERROR = -0x7D0,
    BIND_ERROR = SOCKET_CREATE_ERROR - 1,
    SEND_ERROR = BIND_ERROR - 1,
    RECV_ERROR = SEND_ERROR - 1,
    SOCKET_CLOSE = RECV_ERROR - 1,
    STATUS_CODE_MAX = -0xBB8
} e_AppStatusCodes;



void xchg_start(void);

#endif /* lantask.h */
