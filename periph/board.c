// Driverlib includes
#include <hw_types.h>
#include <rom.h>
#include <rom_map.h>
#include <hw_gprcm.h>
#include <prcm.h>
#include "board.h"

#include <hw_memmap.h>
#include <hw_common_reg.h>
#include <hw_types.h>
#include <hw_ints.h>
#include <uart.h>
#include <interrupt.h>
#include <utils.h>
#include <prcm.h>


//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
#if defined(ccs)
extern void (*const g_pfnVectors[]) (void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif


#define UART_BAUD_RATE  115200

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
void board_init(void)
{
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
    //
    // Set vector table base
    //
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long) &g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long) &__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);
    
    PRCMCC3200MCUInit();
}

/**
 * Сброс CPU
 */
void board_reset(void)
{
	    HWREG(GPRCM_BASE+ GPRCM_O_MCU_GLOBAL_SOFT_RESET) |= 0x1;
}


/**
 * Затактировать ноги Com - порта 
 */
void com_mux_config(void)
{
    /* Затактировать ноги UART */
    MAP_PRCMPeripheralReset(PRCM_UARTA0);

    /* Enable Peripheral Clocks */
    MAP_PRCMPeripheralClkEnable(PRCM_UARTA0, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA0, PRCM_RUN_MODE_CLK);

    /* Configure PIN_55 for UART0 UART0_TX */
    MAP_PinTypeUART(PIN_55, PIN_MODE_3);

    /* Configure PIN_57 for UART0 UART0_RX */
    MAP_PinTypeUART(PIN_57, PIN_MODE_3);

    /* Настроить UART - 115200 - 8N2 */
    MAP_UARTConfigSetExpClk(UARTA0_BASE, MAP_PRCMPeripheralClockGet(PRCM_UARTA0), UART_BAUD_RATE,
			    UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_TWO | UART_CONFIG_PAR_NONE);

}

/*
 * Выключить com порт
 */
void com_mux_unconfig(void)
{
    /* Убрать тактирование */
    MAP_PRCMPeripheralClkDisable(PRCM_UARTA0, PRCM_RUN_MODE_CLK);
}

