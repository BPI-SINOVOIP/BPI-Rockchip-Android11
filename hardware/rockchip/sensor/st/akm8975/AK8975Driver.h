/******************************************************************************
 *
 * $Id: AK8975Driver.h 217 2010-03-26 07:11:56Z rikita $
 *
 * -- Copyright Notice --
 *
 * Copyright (c) 2009 Asahi Kasei Microdevices Corporation, Japan
 * All Rights Reserved.
 *
 * This software program is proprietary program of Asahi Kasei Microdevices 
 * Corporation("AKM") licensed to authorized Licensee under Software License
 * Agreement (SLA) executed between the Licensee and AKM.
 *
 * Use of the software by unauthorized third party, or use of the software
 * beyond the scope of the SLA is strictly prohibited.
 *
 * -- End Asahi Kasei Microdevices Copyright Notice --
 *
 ******************************************************************************/
#ifndef AKMD_INC_AK8975DRIVER_H
#define AKMD_INC_AK8975DRIVER_H

#include <stdint.h>		// int8_t, int16_t etc.
#include "akm8975_kernel.h"


/*** Constant definition ******************************************************/

#define BYTE		char	/*!< Unsigned 8-bit char */
#define TRUE		1		/*!< Represents true */
#define FALSE		0		/*!< Represents false */
#define AKD_SUCCESS	1		/*!< Represents success.*/
#define AKD_FAIL	0		/*!< Represents fail. */
#define AKD_ERROR	-1		/*!< Represents error. */

#define AK8975_MEASUREMENT_TIME  10
#define AK8975_MEASURE_TIMEOUT   100

/*** Type declaration *********************************************************/

/*** Global variables *********************************************************/

/*** Prototype of Function  ***************************************************/

int16_t AKD_InitDevice(void);

void AKD_DeinitDevice(void);

int16_t AKD_TxData(const BYTE address, 
				 const BYTE* data, 
				 const uint16_t numberOfBytesToWrite);

int16_t AKD_RxData(const BYTE address, 
				 BYTE* data,
				 const uint16_t numberOfBytesToRead);

int16_t AKD_GetMagneticData(BYTE data[SENSOR_DATA_SIZE]);

int16_t AKD_SetMode(const BYTE mode);

int16_t AKD_GetDelay(int32_t* delay);

int16_t AKD_GetAccelerationData(int16_t data[3]);


#endif //AKMD_INC_AK8975DRIVER_H
