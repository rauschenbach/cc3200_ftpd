#include <hw_memmap.h>
#include <hw_common_reg.h>
#include <hw_uart.h>
#include <hw_types.h>
#include <hw_ints.h>
#include <pin.h>
#include <gpio.h>
#include <interrupt.h>
#include <utils.h>
#include <prcm.h>
#include "globdefs.h"
#include "led.h"

#define LED1_PIN				PIN_59	/* GPIO_4 */
#define LED1_BIT				0x10
#define LED1_BASE				GPIOA0_BASE
#define LED1_MODE				PIN_MODE_0


#define LED2_PIN				PIN_60	/* GPIO_5 */
#define LED2_BIT				0x20
#define LED2_BASE				GPIOA0_BASE
#define LED2_MODE				PIN_MODE_0



/**
 * Лампочки, инициализация
 */
void led_init(void)
{
    /* PRCM_GPIOA0 - для ноги LED Если поменяются PIN - то сменить! */
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA0, PRCM_RUN_MODE_CLK);

    /* Configure PIN_08 for SPI0 GLED_ на выход и ставим в "1" для выкл  */
    MAP_PinTypeGPIO(LED1_PIN, LED1_MODE, true);
    MAP_GPIODirModeSet(LED1_BASE, LED1_BIT, GPIO_DIR_MODE_OUT);
    MAP_GPIOPinWrite(LED1_BASE, LED1_BIT, LED1_BIT);	/* Ставим в 1 */


    MAP_PinTypeGPIO(LED2_PIN, LED2_MODE, true);
    MAP_GPIODirModeSet(LED2_BASE, LED2_BIT, GPIO_DIR_MODE_OUT);
    MAP_GPIOPinWrite(LED2_BASE, LED2_BIT, LED2_BIT);	/* Ставим в 0 */

    MAP_PinConfigSet(LED1_PIN, PIN_STRENGTH_2MA, PIN_TYPE_STD_PU);
    MAP_PinConfigSet(LED2_PIN, PIN_STRENGTH_2MA, PIN_TYPE_STD_PU); // _PU
}


/**
 * Включить LED
 */
void led_off(int led)
{
    if (led == LED1) {
	MAP_GPIOPinWrite(LED1_BASE, LED1_BIT, ~LED1_BIT);	/* Ставим в 0 */
    } else {
	MAP_GPIOPinWrite(LED2_BASE, LED2_BIT, ~LED2_BIT);	/* Ставим в 0 */
    }
}



/**
 * вЫключить LED
 */
void led_on(int led)
{
    if (led == LED1) {
	MAP_GPIOPinWrite(LED1_BASE, LED1_BIT, LED1_BIT);	/* Ставим в 1 */
    } else {
	MAP_GPIOPinWrite(LED2_BASE, LED2_BIT, LED2_BIT);	/* Ставим в 1 */
    }
}



/**
 * Переключить LED
 */
void led_toggle(int led)
{
    u8 bits;

    if (led == LED1) {
	bits = MAP_GPIOPinRead(LED1_BASE, LED1_BIT);

	// вЫключить бит единицей если включен
	if (bits) {
	    MAP_GPIOPinWrite(LED1_BASE, LED1_BIT, ~LED1_BIT);
	} else {
	    MAP_GPIOPinWrite(LED1_BASE, LED1_BIT, LED1_BIT);
	}
    } else {
	bits = MAP_GPIOPinRead(LED2_BASE, LED2_BIT);

	// вЫключить бит единицей если включен
	if (bits) {	// если 1
	    MAP_GPIOPinWrite(LED2_BASE, LED2_BIT, ~LED2_BIT);
	} else {		// если 0
	    MAP_GPIOPinWrite(LED2_BASE, LED2_BIT, LED2_BIT);
	}
    }
}
