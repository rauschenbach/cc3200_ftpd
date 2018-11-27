#ifndef _SD_CARD_H
#define _SD_CARD_H

#include "globdefs.h"

// перекидывается и CMD и CLK. теперь и DATA0
#define   SD_CARD_DATA0_PIN		PIN_06
#define   SD_CARD_DATA0_MODE		PIN_MODE_8

#define   SD_CARD_CLK_PIN		PIN_01
#define   SD_CARD_CLK_MODE		PIN_MODE_6

#define   SD_CARD_CMD_PIN		PIN_02
#define   SD_CARD_CMD_MODE		PIN_MODE_6


/* Описывает когда случился таймаут карты и прочие ошибки */
typedef struct {
    u32 cmd_error;
    u32 read_timeout;
    u32 write_timeout;
    u32 any_error;
} SD_CARD_ERROR_STRUCT;


void sd_card_mux_config(void);

#endif /* sd_card.h */
