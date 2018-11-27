#ifndef _FTP_H
#define _FTP_H

#include "main.h"



#define FTP_CMD_PORT		21
#define FTP_DATA_PORT		(FTP_CMD_PORT - 1) /* Меньше на единицу всегда */
#define	FTP_PASSV_PORT		10000
#define FTP_SRV_ROOT		"/"
#define FTP_MAX_CONNECTION	1
#define FTP_USER		"erik"
#define FTP_PASSWORD		"erik"
#define FTP_WELCOME_MSG		"220 Test FTP server for CC3200 ready.\r\n"
#define FTP_BUFFER_SIZE		 (256 * 2)	/* если 512 байт - не рушица буфер при передаче на stm32 */

/* Задержка во время чтения или записи */ 
#if 1
#define SLEEP(x) 		    osi_Sleep(x) /* или vTaskDelay(x) */
#else
#define SLEEP(x)		    vTaskDelay(x)	
#endif



/**
 * Состояние FTP, чтобы не было возможности войти без пароля
 */
typedef enum {
    ANON_STAT,
    USER_STAT,
    LOGGED_STAT,
    QUIT_STAT,
} ftp_state_en;


/**
 * Команды FTP
 */
typedef enum {
    CMD_USER,
    CMD_PASS,
    CMD_LIST,
    CMD_NLST,
    CMD_PWD,
    CMD_TYPE,
    CMD_PASV,
    CMD_RETR,
    CMD_STOR,
    CMD_SIZE,
    CMD_MDTM,
    CMD_SYST,
    CMD_CWD,
    CMD_NOOP,	
    CMD_CDUP,
    CMD_PORT,
    CMD_REST,
    CMD_MKD,
    CMD_DELE,
    CMD_RMD,

    CMD_RNFR,
    CMD_RNTO,

    CMD_FEAT,
    CMD_QUIT,
    CMD_UKNWN = -1,
} ftp_cmd_user;


/**
 * Связный список. Можно упаковать
 */
struct conn {
    int    cmd_sockfd;          /* Командный сокет */
    struct sockaddr_in remote;
    ftp_state_en status;	/* Вошел с паролем */
    char pasv_active;		/* Открыт сокет данных */
    bool rn_file;			/* Переименовать файл */
    int  data_sockfd;   	/* Сокет данных - пассивный илиактивный */
    u16  pasv_port;		/* Порт данных */
    size_t offset;
    char   currentdir[FTP_BUFFER_SIZE];	/* current directory */
    char   file_to_rename[FTP_BUFFER_SIZE];	/* Имя файла для переименования. перетащить в глобальную переменную? */
    struct conn *next;
};


void ftpd_thread(void *);

#endif				/* ftp.h */
