#ifndef _BOARD_H
#define _BOARD_H


#include <hw_types.h>
#include <hw_ints.h>
#include <interrupt.h>
#include <systick.h>
#include <rom_map.h>
#include <prcm.h>
#include <rom.h>
#include <pin.h>

void board_init(void);
void board_reset(void);

void com_init(void);
void com_mux_config(void);


#endif /* board.h */
