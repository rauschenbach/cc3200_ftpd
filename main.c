#include <hw_common_reg.h>
#include <hw_memmap.h>
#include <uart.h>
#include <interrupt.h>
#include <utils.h>
#include <prcm.h>
#include <string.h>
#include <device.h>

#include "userfunc.h"
#include "sdreader.h"
#include "sdcard.h"
#include "board.h"
#include "twi.h"
#include "pin.h"
#include "ff.h"
#include "main.h"
#include "lantask.h"


#define         NUM_BYTES		128

/** 
 * Запускаем задачи здесь
 */
int main(void)
{

    /* Initailizing the board */
    board_init();

    /* Инициализация терминала на выход. Только на время отладки */
    com_mux_config();
    PRINTF("INFO: Program %s %s start OK\n", __DATE__, __TIME__);

    led_init();
    led_off(0);
    led_off(1);    

    /* Инициализация i2c также настройка пинов 2 и 3  + Создадим Lock объект */
    // twi_init();

    /* SD карту к процессору */
    // set_sd_to_mcu();

    sd_card_mux_config(); 
    
    xchg_start();

    /* Start the task scheduler */
    osi_start();
    return 0;
}
