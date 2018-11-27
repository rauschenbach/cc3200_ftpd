//*****************************************************************************
// twi.c
//
// I2C interface APIs. Operates in a polled mode.
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 


#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_ints.h"
#include "hw_i2c.h"
#include "i2c.h"
#include "pin.h"
#include "rom.h"
#include "rom_map.h"
#include "prcm.h"

// Common interface include
#include "twi.h"


//*****************************************************************************
//                      MACRO DEFINITIONS
//*****************************************************************************
#define I2C_BASE                I2CA0_BASE
#define SYS_CLK                 80000000
#define RETERR_IF_TRUE(condition) {if(condition) return FAILURE;}
#define RET_IF_ERR(Func)          {int iRetVal = (Func); \
                                   if (SUCCESS != iRetVal) \
                                     return  iRetVal;}

/* Ноги SCL и SDA */
#define 	TWI_SCL_PIN		PIN_03
#define 	TWI_SCL_MODE		PIN_MODE_5
#define 	TWI_SDA_PIN		PIN_04
#define 	TWI_SDA_MODE    	PIN_MODE_5


/* Объект блокировки для разделения различных потоков которые читают TWI */
static OsiLockObj_t twi_lock_obj;



//****************************************************************************
//                      LOCAL FUNCTION DEFINITIONS                          
//****************************************************************************
static int I2CTransact(unsigned long ulCmd);


//****************************************************************************
//
//! Invokes the transaction over I2C
//!
//! \param ulCmd is the command to be executed over I2C
//! 
//! This function works in a polling mode,
//!    1. Initiates the transfer of the command.
//!    2. Waits for the I2C transaction completion
//!    3. Check for any error in transaction
//!    4. Clears the master interrupt
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
static int I2CTransact(unsigned long ulCmd)
{
    //
    // Clear all interrupts
    //
    MAP_I2CMasterIntClearEx(I2C_BASE, MAP_I2CMasterIntStatusEx(I2C_BASE, false));
    //
    // Set the time-out. Not to be used with breakpoints.
    //
    MAP_I2CMasterTimeoutSet(I2C_BASE, TWI_TIMEOUT_VAL);
    //
    // Initiate the transfer.
    //
    MAP_I2CMasterControl(I2C_BASE, ulCmd);
    //
    // Wait until the current byte has been transferred.
    // Poll on the raw interrupt status.
    //
    while ((MAP_I2CMasterIntStatusEx(I2C_BASE, false)
	    & (I2C_INT_MASTER | TWI_MRIS_CLKTOUT)) == 0) {
    }
    //
    // Check for any errors in transfer
    //
    if (MAP_I2CMasterErr(I2C_BASE) != I2C_MASTER_ERR_NONE) {
	switch (ulCmd) {
	case I2C_MASTER_CMD_BURST_SEND_START:
	case I2C_MASTER_CMD_BURST_SEND_CONT:
	case I2C_MASTER_CMD_BURST_SEND_STOP:
	    MAP_I2CMasterControl(I2C_BASE, I2C_MASTER_CMD_BURST_SEND_ERROR_STOP);
	    break;
	case I2C_MASTER_CMD_BURST_RECEIVE_START:
	case I2C_MASTER_CMD_BURST_RECEIVE_CONT:
	case I2C_MASTER_CMD_BURST_RECEIVE_FINISH:
	    MAP_I2CMasterControl(I2C_BASE, I2C_MASTER_CMD_BURST_RECEIVE_ERROR_STOP);
	    break;
	default:
	    break;
	}
	return FAILURE;
    }

    return SUCCESS;
}

//****************************************************************************
//
//! Invokes the I2C driver APIs to write to the specified address
//!
//! \param ucDevAddr is the 7-bit I2C slave address
//! \param pucData is the pointer to the data to be written
//! \param ucLen is the length of data to be written
//! \param ucStop determines if the transaction is followed by stop bit
//! 
//! This function works in a polling mode,
//!    1. Writes the device register address to be written to.
//!    2. In a loop, writes all the bytes over I2C
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int TWI_IF_Write(unsigned char ucDevAddr, unsigned char *pucData, unsigned char ucLen, unsigned char ucStop)
{
    RETERR_IF_TRUE(pucData == NULL);
    RETERR_IF_TRUE(ucLen == 0);
    //
    // Set I2C codec slave address
    //
    MAP_I2CMasterSlaveAddrSet(I2C_BASE, ucDevAddr, false);
    //
    // Write the first byte to the controller.
    //
    MAP_I2CMasterDataPut(I2C_BASE, *pucData);
    //
    // Initiate the transfer.
    //
    RET_IF_ERR(I2CTransact(I2C_MASTER_CMD_BURST_SEND_START));
    //
    // Decrement the count and increment the data pointer
    // to facilitate the next transfer
    //
    ucLen--;
    pucData++;
    //
    // Loop until the completion of transfer or error
    //
    while (ucLen) {
	//
	// Write the next byte of data
	//
	MAP_I2CMasterDataPut(I2C_BASE, *pucData);
	//
	// Transact over I2C to send byte
	//
	RET_IF_ERR(I2CTransact(I2C_MASTER_CMD_BURST_SEND_CONT));
	//
	// Decrement the count and increment the data pointer
	// to facilitate the next transfer
	//
	ucLen--;
	pucData++;
    }
    //
    // If stop bit is to be sent, send it.
    //
    if (ucStop == true) {
	RET_IF_ERR(I2CTransact(I2C_MASTER_CMD_BURST_SEND_STOP));
    }

    return SUCCESS;
}

//****************************************************************************
//
//! Invokes the I2C driver APIs to read from the device. This assumes the 
//! device local address to read from is set using the I2CWrite API.
//!
//! \param ucDevAddr is the 7-bit I2C slave address
//! \param pucData is the pointer to the read data to be placed
//! \param ucLen is the length of data to be read
//! 
//! This function works in a polling mode,
//!    1. Writes the device register address to be written to.
//!    2. In a loop, reads all the bytes over I2C
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int TWI_IF_Read(unsigned char ucDevAddr, unsigned char *pucData, unsigned char ucLen)
{
    unsigned long ulCmdID;

    RETERR_IF_TRUE(pucData == NULL);
    RETERR_IF_TRUE(ucLen == 0);
    //
    // Set I2C codec slave address
    //
    MAP_I2CMasterSlaveAddrSet(I2C_BASE, ucDevAddr, true);
    //
    // Check if its a single receive or burst receive
    //
    if (ucLen == 1) {
	//
	// Configure for a single receive
	//
	ulCmdID = I2C_MASTER_CMD_SINGLE_RECEIVE;
    } else {
	//
	// Initiate a burst receive sequence
	//
	ulCmdID = I2C_MASTER_CMD_BURST_RECEIVE_START;
    }
    //
    // Initiate the transfer.
    //
    RET_IF_ERR(I2CTransact(ulCmdID));
    //
    // Decrement the count and increment the data pointer
    // to facilitate the next transfer
    //
    ucLen--;
    //
    // Loop until the completion of reception or error
    //
    while (ucLen) {
	//
	// Receive the byte over I2C
	//
	*pucData = MAP_I2CMasterDataGet(I2C_BASE);
	//
	// Decrement the count and increment the data pointer
	// to facilitate the next transfer
	//
	ucLen--;
	pucData++;
	if (ucLen) {
	    //
	    // Continue the reception
	    //
	    RET_IF_ERR(I2CTransact(I2C_MASTER_CMD_BURST_RECEIVE_CONT));
	} else {
	    //
	    // Complete the last reception
	    //
	    RET_IF_ERR(I2CTransact(I2C_MASTER_CMD_BURST_RECEIVE_FINISH));
	}
    }
    //
    // Receive the byte over I2C
    //
    *pucData = MAP_I2CMasterDataGet(I2C_BASE);

    return SUCCESS;
}

//****************************************************************************
//
//! Invokes the I2C driver APIs to read from a specified address the device. 
//! This assumes the device local address to be of 8-bit. For other 
//! combinations use I2CWrite followed by I2CRead.
//!
//! \param ucDevAddr is the 7-bit I2C slave address
//! \param pucWrDataBuf is the pointer to the data to be written (reg addr)
//! \param ucWrLen is the length of data to be written
//! \param pucRdDataBuf is the pointer to the read data to be placed
//! \param ucRdLen is the length of data to be read
//! 
//! This function works in a polling mode,
//!    1. Writes the data over I2C (device register address to be read from).
//!    2. In a loop, reads all the bytes over I2C
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int TWI_IF_ReadFrom(unsigned char ucDevAddr,
		    unsigned char *pucWrDataBuf, unsigned char ucWrLen, unsigned char *pucRdDataBuf,
		    unsigned char ucRdLen)
{
    //
    // Write the register address to be read from.
    // Stop bit implicitly assumed to be 0.
    //
    RET_IF_ERR(TWI_IF_Write(ucDevAddr, pucWrDataBuf, ucWrLen, 0));
    //
    // Read the specified length of data
    //
    RET_IF_ERR(TWI_IF_Read(ucDevAddr, pucRdDataBuf, ucRdLen));

    return SUCCESS;
}

//****************************************************************************
//
//! Enables and configures the I2C peripheral
//!
//! \param ulMode is the mode configuration of I2C
//! The parameter \e ulMode is one of the following
//! - \b I2C_MASTER_MODE_STD for 100 Kbps standard mode.
//! - \b I2C_MASTER_MODE_FST for 400 Kbps fast mode.
//! 
//! This function works in a polling mode,
//!    1. Powers ON the I2C peripheral.
//!    2. Configures the I2C peripheral
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int TWI_IF_Open(unsigned long ulMode)
{
    //
    // Enable I2C Peripheral
    //           
    //MAP_HwSemaphoreLock(HWSEM_I2C, HWSEM_WAIT_FOR_EVER);
    MAP_PRCMPeripheralClkEnable(PRCM_I2CA0, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(PRCM_I2CA0);

    //
    // Configure I2C module in the specified mode
    //
    switch (ulMode) {
    case TWI_MASTER_MODE_STD:	/* 100000 */
	MAP_I2CMasterInitExpClk(I2C_BASE, SYS_CLK, false);
	break;

    case TWI_MASTER_MODE_FST:	/* 400000 */
	MAP_I2CMasterInitExpClk(I2C_BASE, SYS_CLK, true);
	break;

    default:
	MAP_I2CMasterInitExpClk(I2C_BASE, SYS_CLK, true);
	break;
    }
    //
    // Disables the multi-master mode
    //
    //MAP_I2CMasterDisable(I2C_BASE);

    return SUCCESS;
}

//****************************************************************************
//
//! Disables the I2C peripheral
//!
//! \param None
//! 
//! This function works in a polling mode,
//!    1. Powers OFF the I2C peripheral.
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int TWI_IF_Close()
{
    // Power OFF the I2C peripheral
    MAP_PRCMPeripheralClkDisable(PRCM_I2CA0, PRCM_RUN_MODE_CLK);

    return SUCCESS;
}

/* Инициализация TWI/I2C - выключаем дефолтные ноги и включаем на выводах 2 и 3 */
void twi_init(void)
{
    MAP_PRCMPeripheralClkEnable(PRCM_I2CA0, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(PRCM_I2CA0);

    /* Выключаем все I2C с ног 1 и 2 */
    MAP_PinModeSet(PIN_01, PIN_MODE_0);
    MAP_PinModeSet(PIN_02, PIN_MODE_0);
    MAP_PinModeSet(PIN_03, PIN_MODE_0);
    MAP_PinModeSet(PIN_04, PIN_MODE_0);

    MAP_PRCMPeripheralClkEnable(PRCM_I2CA0, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(PRCM_I2CA0);


    /* Configure TWI_SCL */
    MAP_PinTypeI2C(TWI_SCL_PIN, TWI_SCL_MODE);

    /* Configure TWI_SDA  */
    MAP_PinTypeI2C(TWI_SDA_PIN, TWI_SDA_MODE);


    /* I2C Init */
    TWI_IF_Open(TWI_MASTER_MODE_FST);

    /* Создаем объект блокировки */
    Osi_twi_obj_create();
}

/* Закрываем TWI и удаляем блокировку  */
void twi_close(void)
{
    TWI_IF_Close();

    /* Выключаем все I2C с ног 1 и 2 */
    MAP_PinModeSet(PIN_01, PIN_MODE_0);
    MAP_PinModeSet(PIN_02, PIN_MODE_0);
    MAP_PinModeSet(PIN_03, PIN_MODE_0);
    MAP_PinModeSet(PIN_04, PIN_MODE_0);
 
    Osi_twi_obj_delete();
}


/**
 * Sets the value in the specified register
 */
int twi_write_reg_value(u8 dev, u8 ucRegAddr, u8 ucRegValue)
{
    u8 ucData[2];
    int res;
    // Select the register to be written followed by the value.
    ucData[0] = ucRegAddr;
    ucData[1] = ucRegValue;
    // Initiate the I2C write
    if (TWI_IF_Write(dev, ucData, 2, 1) == 0) {
	res = SUCCESS;
    } else {
	PRINTF("ERROR: I2C write failed\n\r");
	res = FAILURE;
    }

    return res;
}


/**
 * Returns the value in the specified register
 */
int twi_read_reg_value(u8 dev, u8 ucRegAddr, u8 * pucRegValue)
{
    // Invoke the readfrom  API to get the required byte
    if (TWI_IF_ReadFrom(dev, &ucRegAddr, 1, pucRegValue, 1) != 0) {
	PRINTF("ERROR: I2C readfrom failed\n\r");
	return FAILURE;
    }

    return SUCCESS;
}

/**
 * Прочитать блок данных
 */
int twi_read_block_data(u8 dev, u8 ucRegAddr, u8 * pucBlkData, u8 ucBlkDataSz)
{
    // Invoke the readfrom I2C API to get the required bytes
    if (TWI_IF_ReadFrom(dev, &ucRegAddr, 1, pucBlkData, ucBlkDataSz) != 0) {
	PRINTF("ERROR: I2C readfrom failed\n");
	return FAILURE;
    }

    return SUCCESS;
}

/****************************** работа с LOCK объектом ***********************************/

/* Ждем объекта блокировки */
OsiReturnVal_e Osi_twi_obj_lock(void)
{
    return osi_LockObjLock(&twi_lock_obj, OSI_WAIT_FOREVER);
}

/* Отдаем объект блокировки */
OsiReturnVal_e Osi_twi_obj_unlock(void)
{
    return osi_LockObjUnlock(&twi_lock_obj);
}

/* Создадим Lock объект */
OsiReturnVal_e Osi_twi_obj_create(void)
{
    OsiReturnVal_e ret;

    ret = osi_LockObjCreate(&twi_lock_obj);
    if (ret == OSI_OK) {
	PRINTF("INFO: Create Lock obj OK\r\n");
    }
    return ret;
}


/* Удалим Lock объект */
OsiReturnVal_e Osi_twi_obj_delete(void)
{
    OsiReturnVal_e ret;

    ret = osi_LockObjDelete(&twi_lock_obj);
    if (ret == OSI_OK) {
	PRINTF("INFO: Delete Lock obj OK\r\n");
    }
    return ret;
}

