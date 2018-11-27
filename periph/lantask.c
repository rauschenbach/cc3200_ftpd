#include "rtos_handlers.h"
#include "lantask.h"
#include "ftpd.h"
#include "main.h"

#if 10
#define WLAN_NAME     "CONSTELLATION"
#define WLAN_PASS     "inventor"
#define WLAN_SEC       SL_SEC_TYPE_WPA_WPA2
#else
#define WLAN_NAME     "Nordlab"
#define WLAN_PASS     "WelcometoNordlab!"
#define WLAN_SEC       SL_SEC_TYPE_WPA_WPA2
#endif


#define UDP_ECHO_PORT	7

static void vUdpEchoTask(void *);
static void vWlanStationTask(void *);
static long wlan_connect(void *);
static long ConfigureSimpleLinkToDefaultState(void);

static OsiTaskHandle gFtpTaskHandle;

static char buf[256];

/**
 * Подсоединиться к точке доступа, указанной в файле recparam.ini
 */
void xchg_start(void)
{
	long ret = -1;

	/* Start the SimpleLink Host */
	ret = VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);
	if (ret < 0) {
		LOOP_FOREVER();
	}

	/* Start the WlanStationMode task */
	ret = osi_TaskCreate(vWlanStationTask, (const signed char *) "WlanStationTask", OSI_STACK_SIZE * 2, NULL, WLAN_STATION_TASK_PRIORITY, NULL);
	if (ret < 0) {
		LOOP_FOREVER();
	}
}


/**
 * Сделаем UDP Эхо сервер
 */
static void vUdpEchoTask(void *par)
{
	int n, sockfd, rx = 0;
	struct sockaddr_in local;
	u32 addr_len = sizeof(struct sockaddr);

	/* Создаем сокет UDP  */
	local.sin_port = htons(UDP_ECHO_PORT);
	local.sin_family = PF_INET;
	local.sin_addr.s_addr = INADDR_ANY;

	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd < 0) {
		PRINTF("ERROR: Create socket\r\n");
		return;
	}
	PRINTF("SUCCESS: Create socket\r\n");

	/* bind to port at any interface */
	if (bind(sockfd, (struct sockaddr *) &local, addr_len) < 0) {
		PRINTF("ERROR: bind\r\n");
	}
	PRINTF("SUCCESS: bind\r\n");

	/* received data to buffer */
	while (1) {
		n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *) &local, (socklen_t *) & addr_len);
		if (n > 0) {
			buf[n] = 0;
                        rx += n;
                if(rx> 1000) {
                  PRINTF("END task\r\n");
                  break;
                }


			PRINTF("INFO: %d bytes recv from (%s) : %s\r\n", n, inet_ntoa(local.sin_addr), buf);
			if (sendto(sockfd, buf, n, 0, (struct sockaddr *) &local, addr_len) < 0) {
				PRINTF("ERROR: sendto\r\n");
			}
		}
		SLEEP(5);
	}
        
        close(sockfd);
        vTaskDelete();
}



/**
 * Start simplelink, connect to the ap and run the ping test
 */
static void vWlanStationTask(void *par)
{
	long /*OsiReturnVal_e */ ret;


	while (1) {
		osi_Sleep(500);
		ret = ConfigureSimpleLinkToDefaultState();
		if (ret < 0) {
			if (DEVICE_NOT_IN_STATION_MODE == ret) {
				PRINTF("FAILED: Configure the device in its default state\n");
			}
			continue;
		}
		PRINTF("INFO: Device is configured in default state \n\r");

		/* Assumption is that the device is configured in station mode already and it is in its default state */
		ret = sl_Start(0, 0, 0);
		if (ret < 0 || ROLE_STA != ret) {
			PRINTF("FAILED: Start the device\n");
			continue;
		}
		PRINTF("INFO: Device started as STATION\n");

		/* Connecting to WLAN AP */
		ret = wlan_connect(par);
		if (ret < 0) {
			PRINTF("FAILED: Establish connection w/ an AP\n");
			continue;
		}
		PRINTF("INFO: Connection established w/ AP and IP is aquired\n");


		/* Создаем задачу ftp */
		if (gFtpTaskHandle == NULL) {
			ret = osi_TaskCreate(ftpd_thread, "Ftpd Task", OSI_STACK_SIZE * 8, par, TCP_RX_TASK_PRIORITY, &gFtpTaskHandle);
			if (ret != OSI_OK) {
				continue;
			} else {
				PRINTF("INFO: Ftp Task create OK\n\r");
			}
		}

#if 0
		/* Start the UDP task */
		ret = osi_TaskCreate(vUdpEchoTask, (const signed char *) "UdpEchoTask", OSI_STACK_SIZE, NULL, UDP_RX_TASK_PRIORITY, NULL);
		if (ret < 0) {
			LOOP_FOREVER();
		} else {
			PRINTF("INFO: UDP Echo Task create OK\n\r");
		}
#endif

		/* Пока не рухнет  */
		while (IS_CONNECTED(get_wlan_status())) {
			//PRINTF(".");
			osi_Sleep(1000);
		}

		/* power off the network processor */
		ret = sl_Stop(SL_STOP_TIMEOUT);
		PRINTF("INFO: Stop network processor\r\n");
		PRINTF("INFO: End....Try to reconnect\r\n");
		osi_Sleep(500);
	}

}


/**
 * Connecting to a WLAN Accesspoint
 */
static long wlan_connect(void *v)
{
	char ssid_name[] = WLAN_NAME;
	char ssid_pass[] = WLAN_PASS;
	int ssid_sec_type = WLAN_SEC;
	SlSecParams_t secParams = { 0 };
	long lRetVal = 0;


	PRINTF("INFO: SSID Name:   %s\n", ssid_name);
	PRINTF("INFO: SSID Passwd: %s\n", ssid_pass);
	PRINTF("INFO: Sec  Type:   %d\n", ssid_sec_type);

	secParams.Key = (_i8 *) ssid_pass;
	secParams.KeyLen = strlen(ssid_pass);
	secParams.Type = ssid_sec_type;
	lRetVal = sl_WlanConnect((_i8 *) ssid_name, strlen(ssid_name), 0, &secParams, 0);
	ASSERT_ON_ERROR(lRetVal);

	/* Wait for WLAN Event */
	while ((!IS_CONNECTED(get_wlan_status())) || (!IS_IP_ACQUIRED(get_wlan_status()))) {
		osi_Sleep(50);
	}

	PRINTF("INFO: Connected OK\n");
	return SUCCESS;
}


/**
 * This function puts the device in its default state. It:
 * Following function configure the device to default state by cleaning
 * the persistent settings stored in NVMEM (viz. connection profiles &
 * policies, power policy etc)
 * Applications may choose to skip this step if the developer is sure
 * that the device is in its default state at start of applicaton
 * Note that all profiles and persistent settings that were done on the
 * device will be lost
 */
static long ConfigureSimpleLinkToDefaultState(void)
{
	SlVersionFull ver = { 0 };
	_WlanRxFilterOperationCommandBuff_t RxFilterIdMask = { 0 };
	unsigned char ucVal = 1;
	unsigned char ucConfigOpt = 0;
	unsigned char ucConfigLen = 0;
	unsigned char ucPower = 0;
	long ret = -1;
	long lMode = -1;

	// Подключаем в режиме станции
	lMode = sl_Start(0, 0, 0);
	ASSERT_ON_ERROR(lMode);
	/* Get the device's version-information */
	ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
	ucConfigLen = sizeof(ver);
	ret = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt, &ucConfigLen, (unsigned char *) (&ver));
	ASSERT_ON_ERROR(ret);
	PRINTF("Host Driver Version: %s\n\r", SL_DRIVER_VERSION);
	PRINTF("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
	       ver.NwpVersion[0], ver.NwpVersion[1], ver.NwpVersion[2], ver.NwpVersion[3],
	       ver.ChipFwAndPhyVersion.FwVersion[0], ver.ChipFwAndPhyVersion.FwVersion[1],
	       ver.ChipFwAndPhyVersion.FwVersion[2], ver.ChipFwAndPhyVersion.FwVersion[3],
	       ver.ChipFwAndPhyVersion.PhyVersion[0], ver.ChipFwAndPhyVersion.PhyVersion[1],
	       ver.ChipFwAndPhyVersion.PhyVersion[2], ver.ChipFwAndPhyVersion.PhyVersion[3]);
	// Set connection policy to Auto + SmartConfig 
	//      (Device's default connection policy)
	ret = sl_WlanPolicySet(SL_POLICY_CONNECTION, SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
	ASSERT_ON_ERROR(ret);
	// Remove all profiles
	ret = sl_WlanProfileDel(0xFF);
	ASSERT_ON_ERROR(ret);

	//
	// Device in station-mode. Disconnect previous connection if any
	// The function returns 0 if 'Disconnected done', negative number if already
	// disconnected Wait for 'disconnection' event if 0 is returned, Ignore 
	// other return-codes
	//
	ret = sl_WlanDisconnect();
	if (0 == ret) {
		// Wait
		while (IS_CONNECTED(get_wlan_status())) {
		}
	}
	// Enable DHCP client
	ret = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE, 1, 1, &ucVal);
	ASSERT_ON_ERROR(ret);
	// Disable scan
	ucConfigOpt = SL_SCAN_POLICY(0);
	ret = sl_WlanPolicySet(SL_POLICY_SCAN, ucConfigOpt, NULL, 0);
	ASSERT_ON_ERROR(ret);
	// Set Tx power level for station mode
	// Number between 0-15, as dB offset from max power - 0 will set max power
	ucPower = 0;
	ret = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (unsigned char *) &ucPower);
	ASSERT_ON_ERROR(ret);

	// Set PM policy
	ret = sl_WlanPolicySet(SL_POLICY_PM, SL_ALWAYS_ON_POLICY, NULL, 0);
	ASSERT_ON_ERROR(ret);

	// Unregister mDNS services
	ret = sl_NetAppMDNSUnRegisterService(0, 0);
	ASSERT_ON_ERROR(ret);

	// Remove  all 64 filters (8*8)
	memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
	ret = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *) & RxFilterIdMask, sizeof(_WlanRxFilterOperationCommandBuff_t));
	ASSERT_ON_ERROR(ret);
	ret = sl_Stop(SL_STOP_TIMEOUT);
	ASSERT_ON_ERROR(ret);
	return ret;		/* Success */
}
