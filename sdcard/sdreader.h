#ifndef _SDREADER_H
#define _SDREADER_H

#include "globdefs.h"
#include "main.h"
#include "twi.h"

typedef enum {
  SD_CARD_TO_CR,
  SD_CARD_TO_MCU,
  SD_CARD_UNKNOWN
} SD_CARD_STATUS_EN;


int set_sd_to_cr(void);
int set_sd_to_mcu(void);
int get_sd_cable(void);
int get_sd_state(void);
void vTaskCardReader(void *);

#endif /* SDREADER.h */
