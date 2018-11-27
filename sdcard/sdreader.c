/*******************************************************************************
 * Переключение на картридер и обратно
 * Если в режиме картридер и вытащить USB - переключить на процессор
 * проверить в режиме картридера VBUS - момент переключение - пропадание VBUS
 ******************************************************************************/

#include "sdreader.h"

/* Адрес микросхемы */
#define        	MAX14502_ADDR   	0x70

/* Адреса регистров */
#define		CONTROL_REG		0x00
#define		CONFIG1_REG		0x01
#define		CONFIG2_REG		0x02
#define		CONFIG3_REG		0x03
#define		IE1_REG			0x04
#define		IE2_REG			0x05
#define		USBVIDH_REG		0x06
#define		USBVIDL_REG		0x07
#define		USBPIDH_REG		0x08
#define		USBPIDL_REG		0x09


#define		MAX14502_STATUS		0x12


static int sd_card_state = SD_CARD_UNKNOWN;


/* Установить карту в кардридер */
int set_sd_to_cr(void)
{
    int res;
    u8 data = 0x03;

    /* Ждем */
    while (Osi_twi_obj_lock() != OSI_OK);

    res = twi_write_reg_value(MAX14502_ADDR, CONTROL_REG, data);
    if (res != SUCCESS) {
	PRINTF("Error set SD to CardReader\r\n");
    } else {
	sd_card_state = SD_CARD_TO_CR;
    }
    Osi_twi_obj_unlock();
    return res;
}

/* Подключить карту к процессору */
int set_sd_to_mcu(void)
{
    int res;
    u8 data = 0x18;


    while (Osi_twi_obj_lock() != OSI_OK);
    res = twi_write_reg_value(MAX14502_ADDR, CONTROL_REG, data);
    if (res != SUCCESS) {
	PRINTF("Error set SD to CPU\r\n");
    } else {
	sd_card_state = SD_CARD_TO_MCU;
    }

    Osi_twi_obj_unlock();

    return res;
}


/**
 * USB кабель вставлен?
 * Читаем статус, если регистр по адресу 0x12, бит 2 - VBUS.
 * Если устройство в режиме кард ридера и VBUS == FALSE,
 * то переключаться обратно на MCU
 * 0 - нужно переключить
 */
int get_sd_cable(void)
{
    int res = -1;
    u8 val;

    res = twi_read_reg_value(MAX14502_ADDR, MAX14502_STATUS, &val);
    if (res == SUCCESS) {
	/* разбираем val */
	if (val & 0x04) {
	    res = 1; // есть VBUS - кабель подключен
	} else {
	    res = 0;
	}
    }

    return res;
}


/**
 * Возвратить состояние картридера
 */
int get_sd_state(void)
{
    return sd_card_state;
}

