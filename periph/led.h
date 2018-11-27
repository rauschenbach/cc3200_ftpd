#ifndef _LED_H
#define _LED_H

#include <hw_memmap.h>
#include <hw_common_reg.h>
#include <hw_uart.h>
#include <hw_types.h>
#include <hw_ints.h>
#include <gpio.h>
#include <prcm.h>

#define		LED1    	0
#define		LED_ORANGE	LED1

#define		LED2 	        1
#define		LED_GREEN	LED2




void led_init(void);
void led_on(int);
void led_off(int);
void led_toggle(int);


#endif /* led.h */
