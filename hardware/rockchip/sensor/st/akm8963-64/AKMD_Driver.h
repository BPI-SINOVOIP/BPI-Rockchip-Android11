/******************************************************************************
 *
 *  $Id: $
 *
 * -- Copyright Notice --
 *
 * Copyright (c) 2004 Asahi Kasei Microdevices Corporation, Japan
 * All Rights Reserved.
 *
 * This software program is the proprietary program of Asahi Kasei Microdevices
 * Corporation("AKM") licensed to authorized Licensee under the respective
 * agreement between the Licensee and AKM only for use with AKM's electronic
 * compass IC.
 *
 * THIS SOFTWARE IS PROVIDED TO YOU "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABLITY, FITNESS FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT OF
 * THIRD PARTY RIGHTS, AND WE SHALL NOT BE LIABLE FOR ANY LOSSES AND DAMAGES
 * WHICH MAY OCCUR THROUGH USE OF THIS SOFTWARE.
 *
 * -- End Asahi Kasei Microdevices Copyright Notice --
 *
 ******************************************************************************/
#ifndef AKMD_INC_AKMD_DRIVER_H
#define AKMD_INC_AKMD_DRIVER_H

/* Device driver */
#ifdef AKMD_FOR_AK8963
//#include "akm8963.h"

#elif AKMD_FOR_AK8975
#include "akm8975.h"

#elif AKMD_FOR_AK09911
//KY TODO #include "akm09911.h"

#elif AKMD_FOR_AK09912
#include "akm09912.h"

#else
#error "No devices are defined."
#endif

//KY TODO
#include "sensors_io.h"	/* Device driver */
#include <stdint.h>			/* int8_t, int16_t etc. */

#define SENSOR_DATA_SIZE		9	/* Rx buffer size, i.e from ST1 to ST2 */
#define RWBUF_SIZE				16	/* Read/Write buffer size.*/
#define CALIBRATION_DATA_SIZE	12

/*! \name AK09911 register address
\anchor AK09911_REG
Defines a register address of the AK09911.*/
/*! @{*/
/* Device specific constant values */
#define AK09911_REG_WIA1			0x00
#define AK09911_REG_WIA2			0x01
#define AK09911_REG_INFO1			0x02
#define AK09911_REG_INFO2			0x03
#define AK09911_REG_ST1				0x10
#define AK09911_REG_HXL				0x11
#define AK09911_REG_HXH				0x12
#define AK09911_REG_HYL				0x13
#define AK09911_REG_HYH				0x14
#define AK09911_REG_HZL				0x15
#define AK09911_REG_HZH				0x16
#define AK09911_REG_TMPS			0x17
#define AK09911_REG_ST2				0x18
#define AK09911_REG_CNTL1			0x30
#define AK09911_REG_CNTL2			0x31
#define AK09911_REG_CNTL3			0x32

/*! \name AK09911 fuse-rom address
\anchor AK09911_FUSE
Defines a read-only address of the fuse ROM of the AK09911.*/
#define AK09911_FUSE_ASAX			0x60
#define AK09911_FUSE_ASAY			0x61
#define AK09911_FUSE_ASAZ			0x62

/*! \name AK09911 operation mode
 \anchor AK09911_Mode
 Defines an operation mode of the AK09911.*/
#define AK09911_MODE_SNG_MEASURE	0x01
#define AK09911_MODE_SELF_TEST		0x10
#define AK09911_MODE_FUSE_ACCESS	0x1F
#define AK09911_MODE_POWERDOWN		0x00
#define AK09911_RESET_DATA			0x01

#define AK09911_REGS_SIZE		13
#define AK09911_WIA1_VALUE		0x48
#define AK09911_WIA2_VALUE		0x05

/* Device specific constant values for AK8963*/
#define AK8963_REG_WIA		0x00
#define AK8963_REG_INFO		0x01
#define AK8963_REG_ST1		0x02
#define AK8963_REG_HXL		0x03
#define AK8963_REG_HXH		0x04
#define AK8963_REG_HYL		0x05
#define AK8963_REG_HYH		0x06
#define AK8963_REG_HZL		0x07
#define AK8963_REG_HZH		0x08
#define AK8963_REG_ST2		0x09
#define AK8963_REG_CNTL1	0x0A
#define AK8963_REG_CNTL2	0x0B
#define AK8963_REG_ASTC		0x0C
#define AK8963_REG_TS1		0x0D
#define AK8963_REG_TS2		0x0E
#define AK8963_REG_I2CDIS	0x0F

#define AK8963_FUSE_ASAX	0x10
#define AK8963_FUSE_ASAY	0x11
#define AK8963_FUSE_ASAZ	0x12

#define AK8963_MODE_SNG_MEASURE		0x01
#define AK8963_MODE_SELF_TEST		0x08
#define AK8963_MODE_FUSE_ACCESS		0x0F
#define AK8963_MODE_POWERDOWN		0x00
#define AK8963_RESET_DATA			0x01

#define AK8963_REGS_SIZE		13
#define AK8963_WIA_VALUE		0x48


/* To avoid device dependency, convert to general name */
#define AKM_I2C_NAME			"akm8963"
#define AKM_MISCDEV_NAME		"akm8963_dev"
#define AKM_SYSCLS_NAME			"compass"
#define AKM_SYSDEV_NAME			"akm8963"
#define AKM_REG_MODE			AK8963_REG_CNTL1
#define AKM_REG_RESET			AK8963_REG_CNTL2
#define AKM_REG_STATUS			AK8963_REG_ST1
#define AKM_MEASURE_TIME_US		10000
#define AKM_DRDY_IS_HIGH(x)		((x) & 0x01)
#define AKM_SENSOR_INFO_SIZE	2
#define AKM_SENSOR_CONF_SIZE	3
#define AKM_SENSOR_DATA_SIZE	9

#define AKM_YPR_DATA_SIZE		26
#define AKM_RWBUF_SIZE			16
#define AKM_REGS_SIZE			AK8963_REGS_SIZE
#define AKM_REGS_1ST_ADDR		AK8963_REG_WIA
#define AKM_FUSE_1ST_ADDR		AK8963_FUSE_ASAX

#define AKM_MODE_SNG_MEASURE	AK8963_MODE_SNG_MEASURE
#define AKM_MODE_SELF_TEST		AK8963_MODE_SELF_TEST
#define AKM_MODE_FUSE_ACCESS	AK8963_MODE_FUSE_ACCESS
#define AKM_MODE_POWERDOWN		AK8963_MODE_POWERDOWN
#define AKM_RESET_DATA			AK8963_RESET_DATA

#define ACC_DATA_FLAG		0
#define MAG_DATA_FLAG		1
#define FUSION_DATA_FLAG	2
#define AKM_NUM_SENSORS		3

#define ACC_DATA_READY		(1<<(ACC_DATA_FLAG))
#define MAG_DATA_READY		(1<<(MAG_DATA_FLAG))
#define FUSION_DATA_READY	(1<<(FUSION_DATA_FLAG))

/*** Constant definition ******************************************************/
#define AKD_TRUE	1		/*!< Represents true */
#define AKD_FALSE	0		/*!< Represents false */
#define AKD_SUCCESS	1		/*!< Represents success.*/
#define AKD_FAIL	0		/*!< Represents fail. */
#define AKD_ERROR	-1		/*!< Represents error. */

#define AKD_ENABLE	1
#define AKD_DISABLE	0

/*! 0:Don't Output data, 1:Output data */
#define AKD_DBG_DATA	0
/*! Typical interval in ns */
#define AKM_MEASUREMENT_TIME_NS	((AKM_MEASURE_TIME_US) * 1000)


/*** Type declaration *********************************************************/
typedef unsigned char BYTE;

/*!
 Open device driver.
 This function opens device driver of other sensor.
 @return If this function succeeds, the return value is #AKD_SUCCESS. Otherwise
 the return value is #AKD_FAIL.
 */
typedef int16_t(*ACCFNC_INITDEVICE)(void);

/*!
 Close device driver.
 This function closes device drivers of acceleration sensor.
 */
typedef void(*ACCFNC_DEINITDEVICE)(void);

/*!
 Enable or disable sensor
*/
typedef int16_t(*ACCFNC_SET_ENABLE)(const int8_t enabled);

/*!
 Set delay value
*/
typedef int16_t(*ACCFNC_SET_DELAY)(const int64_t ns);

/*!
 Acquire data from other sensor.
 @return If this function succeeds, the return value is #AKD_SUCCESS. Otherwise
 the return value is #AKD_FAIL.
 @param[out] data A data array. The coordinate system and the unit 
 follows the sensor local definition.
 */
typedef int16_t(*ACCFNC_GETACCDATA)(int16_t data[3]);

/*!
 Acquire offset from other sensor.
 @return If this function succeeds, the return value is #AKD_SUCCESS. Otherwise
 the return value is #AKD_FAIL.
 @param[out] offset A offset data array. The coordinate system and the unit 
 follows the sensor local definition.
 */
typedef int16_t(*ACCFNC_GETACCOFFSET)(int16_t offset[3]);

/*!
 Convert from sensor native format to AKSC format
 @param[out] vec A data array. The coordinate system of the
 acquired data follows the definition of Android. Unit is SmartCompass.
*/
typedef void(*ACCFNC_GETACCVEC)(
		const int16_t data[3],
		const int16_t offset[3],
		int16_t vec[3]);

/*** Global variables *********************************************************/

/*** Prototype of Function  ***************************************************/

int16_t AKD_InitDevice(void);

void AKD_DeinitDevice(void);

int16_t AKD_TxData(
		const BYTE address,
		const BYTE* data,
		const uint16_t numberOfBytesToWrite);

int16_t AKD_RxData(
		const BYTE address,
		BYTE* data,
		const uint16_t numberOfBytesToRead);

int16_t AKD_Reset(void);

int16_t AKD_GetSensorInfo(BYTE data[AKM_SENSOR_INFO_SIZE]);

int16_t AKD_GetSensorConf(BYTE data[AKM_SENSOR_CONF_SIZE]);

int16_t AKD_GetMagneticData(BYTE data[AKM_SENSOR_DATA_SIZE]);

void AKD_SetYPR(const int buf[AKM_YPR_DATA_SIZE]);

int16_t AKD_GetOpenStatus(int* status);

int16_t AKD_GetCloseStatus(int* status);

int16_t AKD_SetMode(const BYTE mode);

int16_t AKD_GetDelay(int64_t delay[AKM_NUM_SENSORS]);

int16_t AKD_GetLayout(int16_t* layout);

int16_t AKD_AccSetEnable(int8_t enabled);

int16_t AKD_AccSetDelay(int64_t delay);

int16_t AKD_GetAccelerationData(int16_t data[3]);

int16_t AKD_GetAccelerationOffset(int16_t offset[3]);

void AKD_GetAccelerationVector(
		const int16_t data[3],
		const int16_t offset[3],
		int16_t vec[3]);

#endif //AKMD_INC_AKMD_DRIVER_H

