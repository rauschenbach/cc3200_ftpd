/* Команды:  http://nsftools.com/tips/RawFTP.htm */

#include "ftpd.h"
#include "cmds.h"


static struct conn *conn_list = NULL;
static char ftp_buf[FTP_BUFFER_SIZE];	/* Буфер, чтобы не выделять память через malloc  */
static FATFS fatfs;

static int ftp_process_request(struct conn *session, char *buf);


/**
 * Создать новое FTP соединение на каждую новую команду
 */
static struct conn *alloc_new_conn(void)
{
	struct conn *conn;
	conn = (struct conn *) calloc(sizeof(struct conn), 1);
	conn->next = conn_list;
	conn->offset = 0;
	conn_list = conn;
	return conn;
}

/** 
 * Закрыть текущее FTP соединение
 */
static void destroy_conn(struct conn *conn)
{
	struct conn *list;

	if (conn_list == conn) {
		conn_list = conn_list->next;
		conn->next = NULL;
	} else {
		list = conn_list;
		while (list->next != conn)
			list = list->next;

		list->next = conn->next;
		conn->next = NULL;
	}

	free(conn);
}


/**
 * Функция основного потока
 */
void ftpd_thread(void *par)
{
	int sockfd;
	int com_socket;
	int num;
	int res;
	struct sockaddr_in addr;
	u32 addr_len = sizeof(struct sockaddr);
	struct conn *conn;
	struct conn *next;
	bool lock = false;


#if 0
	char *ftp_buf = (char *) malloc(FTP_BUFFER_SIZE);	/* Буфер, чтобы не выделять память через malloc */
#endif

	/* Сначала подмонтируем ФС */
	if (f_mount(&fatfs, "0", 1) != FR_OK) {
		PRINTF("ERROR: Mount fs\n\r");
		return;
	}
	PRINTF("SUCCESS: Mount fs\n\r");

	/* Создаем сокет TCP для команд с этими настойками */
	addr.sin_port = htons(FTP_CMD_PORT);
	addr.sin_family = PF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		PRINTF("ERROR: Create socket\r\n");
		return;
	}
	PRINTF("SUCCESS: Create socket\r\n");

	/* Привяжем на 21 порт */
	res = bind(sockfd, (struct sockaddr *) &addr, addr_len);
	if (res < 0) {
		PRINTF("ERROR: Bind socket\r\n");
	}
	PRINTF("SUCCESS: Bind socket\r\n");

	res = listen(sockfd, FTP_MAX_CONNECTION);
	if (res < 0) {
		PRINTF("ERROR: Listen %d socket connections\r\n", FTP_MAX_CONNECTION);
	}
	PRINTF("SUCCESS: Listen socket\r\n");

	/* Ждем входящего звонка - lock нужен для блокирования всех входящих соединений,
	 * пока у нас есть одно рабочее. На функции select() зависает и вылетает сетевой стек
	 * Если участок кода ниже сделать отдельной задачей (где while (conn != NULL) ...)
	 * lock можно убрать, а при соединении на сервер одновременно другого клиента - 
	 * можно посылать сообщение 500 или 503:  "Sorry, only one transfer at a time." 
	 */
	for (;;) {
		if (!lock) {
			com_socket = accept(sockfd, (struct sockaddr *) &addr, (socklen_t *) & addr_len);
			if (com_socket == -1) {
				PRINTF("ERROR:  accept()...try ones more\r\n");
				continue;
			} else {
				PRINTF("SUCCESS: Got connection from %s\r\n", inet_ntoa(addr.sin_addr));
				send(com_socket, FTP_WELCOME_MSG, strlen(FTP_WELCOME_MSG), 0);

				/* new conn */
				conn = alloc_new_conn();
				if (conn != NULL) {
					strcpy(conn->currentdir, FTP_SRV_ROOT);
					conn->cmd_sockfd = com_socket;
					conn->remote = addr;
					lock = true;	/* Заблокируем accept() */
				}
			}
		}


		/* Этот участок кода можно сделать отдельной задачей - в task передавать параметр "conn" */
		conn = conn_list;
		while (conn != NULL) {

			next = conn->next;
			num = recv(conn->cmd_sockfd, ftp_buf, FTP_BUFFER_SIZE, 0);
			if (num == 0 || num == -1) {
				PRINTF("INFO: Client %s disconnected with recv\r\n", inet_ntoa(conn->remote.sin_addr));
				lock = false;
				close(conn->cmd_sockfd);
				destroy_conn(conn);
				led_off(0);
			} else {
				ftp_buf[num] = 0;
				led_on(0);

				res = ftp_process_request(conn, ftp_buf);
				if (res != 0) {
					PRINTF("INFO: Client %s disconnected with %s\r\n", inet_ntoa(conn->remote.sin_addr), (res == 1) ? "quit" : "error");
					close(conn->cmd_sockfd);
					destroy_conn(conn);
					led_off(0);
					lock = false;
				} else {
					PRINTF("INFO: Wait next conn\r\n");
				}
				SLEEP(1);
			}
			conn = next;
		}
	}
}

/**
 * Сессия для каждого соединения - здесь нужен большой стек!!!
 */
static int ftp_process_request(struct conn *conn, char *buf)
{
	FRESULT rc;
	FIL file;
	ftp_cmd_user cmd;
	int num_bytes;
	char spare_buf[256];	/* И для имени файла и для обработки других команд */
	struct timeval tv;
	fd_set readfds;
	c8 *sbuf;
	char *parameter_ptr, *ptr;
	struct sockaddr_in pasvremote, local;
	u32 addr_len = sizeof(struct sockaddr_in);
	int ret = 0;		/* Результат выполнения этой функции */
	int led_r = 0, led_w = 0;	/* Для моргания ламп при чтении и записи */

	sbuf = (c8 *) malloc(FTP_BUFFER_SIZE);
	if (sbuf == NULL) {
		PRINTF("ERROR: malloc for conn\r\n");
		return -1;
	}

	tv.tv_sec = 3;
	tv.tv_usec = 0;

	/* remove \r\n */
	ptr = buf;
	while (*ptr) {
		if (*ptr == '\r' || *ptr == '\n')
			*ptr = 0;
		ptr++;
	}

	/* Какой параметр после команды и пробела  */
	parameter_ptr = strchr(buf, ' ');
	if (parameter_ptr != NULL)
		parameter_ptr++;

	/* debug: */
	PRINTF("INFO: %s requested \"%s\"\r\n", inet_ntoa(conn->remote.sin_addr), buf);

	/* Разбор входящих команд */
	cmd = (ftp_cmd_user) do_parse_command(buf);
#if 0
	/* Выкидывать пользователя, если он не залогинен - на всякий! */
	if (cmd > CMD_PASS && conn->status != LOGGED_STAT) {
		do_send_reply(conn->cmd_sockfd, "550 Permission denied.\r\n");
		free(sbuf);
		return 0;
	}
#endif
	switch (cmd) {

		/* Посылка логина */
	case CMD_USER:
		PRINTF("INFO: %s sent login \"%s\"\r\n", inet_ntoa(conn->remote.sin_addr), parameter_ptr);
#if 0
		/* login correct */
		if (strcmp(parameter_ptr, FTP_USER) == 0) {
			do_send_reply(conn->cmd_sockfd, "331 Password required for \"%s\"\r\n", parameter_ptr);
			conn->status = USER_STAT;
		} else {
			/* incorrect login */
			do_send_reply(conn->cmd_sockfd, "530 Login incorrect. Bye.\r\n");
			ret = -1;
		}
#endif
		do_send_reply(conn->cmd_sockfd, "331 Password required for \"%s\"\r\n", parameter_ptr);
		break;

		/* Посылка пароля */
	case CMD_PASS:
		PRINTF("INFO: %s sent password \"%s\"\r\n", inet_ntoa(conn->remote.sin_addr), parameter_ptr);
#if 0
		/* password correct */
		if (strcmp(parameter_ptr, FTP_PASSWORD) == 0) {
			do_send_reply(conn->cmd_sockfd, "230 User logged in\r\n");
			conn->status = LOGGED_STAT;
			PRINTF("INFO: %s Password correct\r\n", inet_ntoa(conn->remote.sin_addr));
		} else {
			/* incorrect password */
			do_send_reply(conn->cmd_sockfd, "530 Login or Password incorrect. Bye!\r\n");
			conn->status = ANON_STAT;
			ret = -1;
		}
#endif
		do_send_reply(conn->cmd_sockfd, "230 Password OK. Current directory is %s\r\n", FTP_SRV_ROOT);
		break;

		/* Расширенный листинг */
	case CMD_LIST:
		do_send_reply(conn->cmd_sockfd, "150 Opening Binary mode connection for file list.\r\n");
		do_full_list(conn->currentdir, conn->data_sockfd);
		close(conn->data_sockfd);
		conn->pasv_active = 0;
		do_send_reply(conn->cmd_sockfd, "226 Transfer complete.\r\n");
		break;

		/* Простой листинг */
	case CMD_NLST:
		do_send_reply(conn->cmd_sockfd, "150 Opening Binary mode connection for file list.\r\n");
		do_simple_list(conn->currentdir, conn->data_sockfd);
		close(conn->data_sockfd);
		conn->pasv_active = 0;
		do_send_reply(conn->cmd_sockfd, "226 Transfer complete.\r\n");
		break;

		/* Ничего не делаем - это опция клиента */
	case CMD_TYPE:
		if (strcmp(parameter_ptr, "I") == 0) {
			do_send_reply(conn->cmd_sockfd, "200 Type set to binary.\r\n");
		} else {
			do_send_reply(conn->cmd_sockfd, "200 Type set to ascii.\r\n");
		}
		break;

		/* Чтение выбранного файла */
	case CMD_RETR:
		strcpy(spare_buf, buf + 5);
		do_full_path(conn, parameter_ptr, spare_buf, 256);
		num_bytes = do_get_filesize(spare_buf);

		if (num_bytes == -1) {
			do_send_reply(conn->cmd_sockfd, "550 \"%s\" : not a regular file\r\n", spare_buf);
			conn->offset = 0;
			break;
		}

		/* Открыли файл на чтение */
		rc = f_open(&file, spare_buf, FA_READ);
		if (rc != FR_OK) {
			PRINTF("ERROR: open file %s for reading\r\n", spare_buf);
			break;
		}

		/* Двигаемся по файлу с начала */
		if (conn->offset > 0 && conn->offset < num_bytes) {
			f_lseek(&file, conn->offset);
			do_send_reply(conn->cmd_sockfd, "150 Opening binary mode data connection for partial \"%s\" (%d/%d bytes).\r\n",
				      spare_buf, num_bytes - conn->offset, num_bytes);
		} else {
			do_send_reply(conn->cmd_sockfd, "150 Opening binary mode data connection for \"%s\" (%d bytes).\r\n", spare_buf, num_bytes);
		}
		/* Передаем данные файла. Перед посылков в сокет необходимо проверять num_bytes
		 * иначе можно послать 0 байт и будет ошибка передачи  */
		led_r = 0;
		do {
			f_read(&file, sbuf, FTP_BUFFER_SIZE, (unsigned *) &num_bytes);
			if (num_bytes > 0) {
				send(conn->data_sockfd, sbuf, num_bytes, 0);
				if (led_r++ % 50 == 0) {
					//led_toggle(LED2);
				}
			}
			SLEEP(1);
		} while (num_bytes > 0);

		do_send_reply(conn->cmd_sockfd, "226 Finished.\r\n");
		f_close(&file);
		close(conn->data_sockfd);
		break;


		/* Запись файла в каталог FTP */
	case CMD_STOR:
		do_full_path(conn, parameter_ptr, spare_buf, 256);

		/* Открываем файло на запись */
		rc = f_open(&file, spare_buf, FA_WRITE | FA_OPEN_ALWAYS);
		if (rc != FR_OK) {
			do_send_reply(conn->cmd_sockfd, "550 Cannot open \"%s\" for writing.\r\n", spare_buf);
			break;
		}
		do_send_reply(conn->cmd_sockfd, "150 Opening binary mode data connection for \"%s\".\r\n", spare_buf);

		FD_ZERO(&readfds);
		FD_SET(conn->data_sockfd, &readfds);
		PRINTF("INFO: Waiting %d seconds for data...\r\n", tv.tv_sec);
		led_w = 0;

		while (select(conn->data_sockfd + 1, &readfds, 0, 0, &tv) > 0) {
			if ((num_bytes = recv(conn->data_sockfd, sbuf, FTP_BUFFER_SIZE, 0)) > 0) {
				unsigned bw;
				f_write(&file, sbuf, num_bytes, &bw);
				if (led_w++ % 50 == 0) {
					//led_toggle(LED1);
				}
			} else if (num_bytes == 0) {
				f_close(&file);
				do_send_reply(conn->cmd_sockfd, "226 Finished.\r\n");
				break;
			} else if (num_bytes == -1) {
				f_close(&file);
				ret = -1;
				break;
			}
		}
		close(conn->data_sockfd);
		break;

		/* Размер файла */
	case CMD_SIZE:
		do_full_path(conn, parameter_ptr, spare_buf, 256);
		num_bytes = do_get_filesize(spare_buf);
		if (num_bytes == -1) {
			do_send_reply(conn->cmd_sockfd, "550 \"%s\" : not a regular file\r\n", spare_buf);
		} else {
			do_send_reply(conn->cmd_sockfd, "213 %d\r\n", num_bytes);
		}
		break;

		/* Вернуть время модификации файла */
	case CMD_MDTM:
		do_send_reply(conn->cmd_sockfd, "550 \"/\" : not a regular file\r\n");
		break;

		/* Система, на чем работает */
	case CMD_SYST:
		do_send_reply(conn->cmd_sockfd, "215 UNIX Type: L8\r\n");
		break;

		/* Текущий рабочий каталог */
	case CMD_PWD:
		do_send_reply(conn->cmd_sockfd, "257 \"%s\" is current directory.\r\n", conn->currentdir);
		break;

		/* Сменить директорию */
	case CMD_CWD:
		do_full_path(conn, parameter_ptr, spare_buf, 256);
		do_send_reply(conn->cmd_sockfd, "250 Changed to directory \"%s\"\r\n", spare_buf);
		strcpy(conn->currentdir, spare_buf);
		PRINTF("INFO: Changed working directory to %s\r\n", spare_buf);
		break;

		/* Сменить директорию вниз */
	case CMD_CDUP:
		//sprintf(spare_buf, "%s/%s", conn->currentdir, "..");
		sprintf(spare_buf, "%s", conn->currentdir);
		do_step_down(spare_buf);
		strcpy(conn->currentdir, spare_buf);

		do_send_reply(conn->cmd_sockfd, "250 Changed to directory \"%s\"\r\n", spare_buf);
		PRINTF("INFO: Changed to directory %s\r\n", spare_buf);
		break;


		/* Установить позицию в файле с какого читать */
	case CMD_REST:
		if (atoi(parameter_ptr) >= 0) {
			conn->offset = atoi(parameter_ptr);
			do_send_reply(conn->cmd_sockfd, "350 Send RETR or STOR to start transfert.\r\n");
		}
		break;

		/* Создать директорию */
	case CMD_MKD:
		do_full_path(conn, parameter_ptr, spare_buf, 256);
		if (f_mkdir(spare_buf)) {
			do_send_reply(conn->cmd_sockfd, "550 File \"%s\" exists.\r\n", spare_buf);
		} else {
			do_send_reply(conn->cmd_sockfd, "257 directory \"%s\" was successfully created.\r\n", spare_buf);
		}
		break;

		/* Удалить файл */
	case CMD_DELE:
		do_full_path(conn, parameter_ptr, spare_buf, 256);
		if (f_unlink(spare_buf) == FR_OK)
			do_send_reply(conn->cmd_sockfd, "250 file \"%s\" was successfully deleted.\r\n", spare_buf);
		else {
			do_send_reply(conn->cmd_sockfd, "550 Not such file or directory: %s.\r\n", spare_buf);
		}
		break;

		/* Удаляем директорию */
	case CMD_RMD:
		do_full_path(conn, parameter_ptr, spare_buf, 256);
		if (f_unlink(spare_buf)) {
			do_send_reply(conn->cmd_sockfd, "550 Directory \"%s\" doesn't exist.\r\n", spare_buf);
		} else {
			do_send_reply(conn->cmd_sockfd, "257 directory \"%s\" was successfully deleted.\r\n", spare_buf);
		}
		break;

		/* Активный режим - сервер открывает соединение. параметры порта передаются в строке 
		 * сделать СВОЙ порт 20 иначе некоторые клиенты не работают */
	case CMD_PORT:
		{
			int portcom[6];
			num_bytes = 0;
			portcom[num_bytes++] = atoi(strtok(parameter_ptr, ".,;()"));
			for (; num_bytes < 6; num_bytes++) {
				portcom[num_bytes] = atoi(strtok(0, ".,;()"));
			}
			sprintf(spare_buf, "%d.%d.%d.%d", portcom[0], portcom[1], portcom[2], portcom[3]);

			FD_ZERO(&readfds);
			if ((conn->data_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
				do_send_reply(conn->cmd_sockfd, "425 Can't open data connection.\r\n");
				close(conn->data_sockfd);
				conn->pasv_active = 0;
				break;
			}


			/* Получим адрес как число long и удаленный порт 
			 * Свой порт нужно обязательно поставить на 20!
			 * попробую сделать bind
			 */
			local.sin_port = htons(FTP_DATA_PORT);
			local.sin_addr.s_addr = INADDR_ANY;
			local.sin_family = PF_INET;
			if (bind(conn->data_sockfd, (struct sockaddr *) &local, addr_len) < 0) {
				PRINTF("ERROR: Bind socket\r\n");
			}
			PRINTF("SUCCESS: Bind socket to %d port\r\n", FTP_DATA_PORT);

			pasvremote.sin_addr.s_addr = ((u32) portcom[3] << 24) | ((u32) portcom[2] << 16) | ((u32) portcom[1] << 8) | ((u32) portcom[0]);
			pasvremote.sin_port = htons(portcom[4] * 256 + portcom[5]);
			pasvremote.sin_family = PF_INET;
			if (connect(conn->data_sockfd, (struct sockaddr *) &pasvremote, addr_len) == -1) {
				pasvremote.sin_addr = conn->remote.sin_addr;
				if (connect(conn->data_sockfd, (struct sockaddr *) &pasvremote, addr_len) == -1) {
					do_send_reply(conn->cmd_sockfd, "425 Can't open data connection.\r\n");
					close(conn->data_sockfd);
					break;
				}
			}
			conn->pasv_active = 1;
			conn->pasv_port = portcom[4] * 256 + portcom[5];
			PRINTF("INFO: Active data connection to %s @ %d\r\n", spare_buf, portcom[4] * 256 + portcom[5]);
			do_send_reply(conn->cmd_sockfd, "200 Port Command Successful.\r\n");
		}
		break;

#if 0
		/* Пассивный режим - мы говорим к какому порту к нам подцепиться чтобы передать данные 
		 * Хром, мозилла и прочие браузеры некоректно заканчивают работу (без QUIT),
		 * и следующий клиент виснет на вызове select()
		 */
	case CMD_PASV:
		do {
			int dig1, dig2;
			int sockfd;
			extern my_ip_addr ip_addr;

			if (conn->pasv_active) {
				PRINTF("WARN: Already open PASV connection. Prev socket doesn't close!\r\n");
				do_send_reply(conn->cmd_sockfd, "503 Sorry, only one transfer at a time.\r\n");
				return 0;
			}
			conn->pasv_port = FTP_PASSV_PORT;
			local.sin_port = htons(conn->pasv_port);
			local.sin_addr.s_addr = INADDR_ANY;
			local.sin_family = PF_INET;

			dig1 = (int) (conn->pasv_port / 256);
			dig2 = conn->pasv_port % 256;

			FD_ZERO(&readfds);
			if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
				do_send_reply(conn->cmd_sockfd, "425 Can't open data connection.\r\n");
				PRINTF("ERROR: Can't make passive TCP socket\r\n");
				ret = 1;
				break;
			}

			if (bind(sockfd, (struct sockaddr *) &local, addr_len) == -1) {
				do_send_reply(conn->cmd_sockfd, "425 Can't open data connection.\r\n");
				PRINTF("ERROR: Can't bind passive socket\r\n");
				ret = 3;
			}
			if (listen(sockfd, 1) == -1) {
				do_send_reply(conn->cmd_sockfd, "425 Can't open data connection.\r\n");
				PRINTF("ERROR: Can't listen passive socket\r\n");
				ret = 4;
				break;
			}


			/* Послать свой IP адрес и свой порт к которому нужно прицепиться */
			do_send_reply(conn->cmd_sockfd, "227 Entering passive mode (%d,%d,%d,%d,%d,%d)\r\n",
				      ip_addr.bIp[3], ip_addr.bIp[2], ip_addr.bIp[1], ip_addr.bIp[0], dig1, dig2);


			FD_SET(sockfd, &readfds);
			PRINTF("INFO: Listening %d seconds @ port %d\r\n", tv.tv_sec, conn->pasv_port);
			select(sockfd + 1, &readfds, 0, 0, &tv);
			if (FD_ISSET(sockfd, &readfds)) {
				conn->data_sockfd = accept(sockfd, (struct sockaddr *) &pasvremote, (socklen_t *) & addr_len);
				if (conn->data_sockfd < 0) {
					PRINTF("ERROR: Can't accept socket\r\n");
					do_send_reply(conn->cmd_sockfd, "425 Can't open data connection.\r\n");
					ret = 5;
					break;
				} else {
					PRINTF("INFO: Passive data connection from %s\r\n", inet_ntoa(pasvremote.sin_addr));
					conn->pasv_active = 1;
					close(sockfd);
				}
			} else {
				ret = 6;
				break;
			}
		} while (0);

		/* Если ошибка, ставим ret = 0 и пробуем команду PORT */
		if (ret) {
			PRINTF("ERROR: # %d\r\n", ret);
			close(conn->data_sockfd);
			ret = 0;
		}
		break;
#endif
		/* Если идут запросы */
	case CMD_NOOP:
		do_send_reply(conn->cmd_sockfd, "200 Command okay. I'm live.\r\n");
		break;

		/* Переименовать ИЗ */
	case CMD_RNFR:
		do {
			/* Не задано имя файла */
			if (parameter_ptr == NULL) {
				do_send_reply(conn->cmd_sockfd, "501 Syntax error in parameters or no file name\r\n");
				break;
			}

			/* Заданное имя файла */
			strcpy(spare_buf, buf + 5);
			do_full_path(conn, parameter_ptr, spare_buf, 256);

			/* Если такой файл есть - у него есть размер  */
			num_bytes = do_get_filesize(spare_buf);
			if (num_bytes == -1) {
				do_send_reply(conn->cmd_sockfd, "550 \"%s\" file not found\r\n", spare_buf);
				break;
			}
			/* Файл нашли. Ждем команду во что переименовать. У нас нет StateMachine, 
			 * поэтому следующая команда должна быть RNTO */
			strcpy(conn->file_to_rename, spare_buf);
			conn->rn_file = true;
			do_send_reply(conn->cmd_sockfd, "350 RNFR accepted - file exists, ready for destination\r\n");
		} while (0);
		break;


		/* Переименовать В */
	case CMD_RNTO:
		do {
			/* Не задано имя файла во что переименовать и нет исходного  */
			if (conn->rn_file == false) {
				do_send_reply(conn->cmd_sockfd, "503 Need RNFR before RNTO\r\n");
			} else if (parameter_ptr == NULL) {
				do_send_reply(conn->cmd_sockfd, "501 Syntax error in parameters or no file name\r\n");
			} else {
                            
				/* Rename a file: old path->new_path */
				strcpy(spare_buf, buf + 5);
				do_full_path(conn, parameter_ptr, spare_buf, 256);


				if (f_rename(conn->file_to_rename, spare_buf) == FR_OK) {
					do_send_reply(conn->cmd_sockfd, "250 File successfully renamed or moved\r\n");
				} else {
					do_send_reply(conn->cmd_sockfd, "451 Rename file failure\r\n");
				}                               
                                
                                /* Обязательно! */ 
                                do_send_reply(conn->cmd_sockfd, "226 Finished.\r\n");
			}
			conn->rn_file = false;
			conn->file_to_rename[0] = 0;
		} while (0);
		break;

		/* Никаких фич у нас нет */
	case CMD_FEAT:
		do_send_reply(conn->cmd_sockfd, "211 No-features\r\n");
		break;

		/* Завершение работы */
	case CMD_QUIT:
		do_send_reply(conn->cmd_sockfd, "221 Adios!\r\n");
		ret = 1;	/* В этом случае пишем 1-цу */
		break;

		/* НЕИЗВЕСТНАЯ КОМАНДА */
	default:
		do_send_reply(conn->cmd_sockfd, "502 Not Implemented yet.\r\n");
		break;
	}

	if (sbuf) {
		free(sbuf);
	}
	return ret;
}
