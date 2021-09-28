/******************************************************************************
 *
 * $Id: AK8963Driver.c 325 2011-08-24 09:34:36Z yamada.rj $
 *
 * -- Copyright Notice --
 *
 * Copyright (c) 2004 Asahi Kasei Microdevices Corporation, Japan
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
#include <fcntl.h>
#include "AKCommon.h"		// DBGPRINT()
#include "AK8963Driver.h"

#define MSENSOR_NAME	"/dev/akm8963_dev"

int s_fdDev = -1;

void Android2AK(const float fData[], int16_t data[3]);

#if 0
/* Include proper acceleration file. */
#ifdef AKMD_ACC_COMBINED
#include "Acc_adxl34x.c"
static ACCFNC_INITDEVICE Acc_InitDevice = Acc_InitDevice;
static ACCFNC_DEINITDEVICE Acc_DeinitDevice = Acc_DeinitDevice;
static ACCFNC_GETACCDATA Acc_GetAccelerationData = Acc_GetAccelerationData;
#else
#include "Acc_aot.c"
static ACCFNC_INITDEVICE Acc_InitDevice = AOT_InitDevice;
static ACCFNC_DEINITDEVICE Acc_DeinitDevice = AOT_DeinitDevice;
static ACCFNC_GETACCDATA Acc_GetAccelerationData = AOT_GetAccelerationData;
#endif
#endif
/*!
 Open device driver.
 This function opens both device drivers of magnetic sensor and acceleration
 sensor. Additionally, some initial hardware settings are done, such as
 measurement range, built-in filter function and etc.
 @return If this function succeeds, the return value is #AKD_SUCCESS.
 Otherwise the return value is #AKD_FAIL.
 */
int16_t AKD_InitDevice(void)
{
	if (s_fdDev < 0) {
		// Open magnetic sensor's device driver.
		if ((s_fdDev = open(MSENSOR_NAME, O_RDWR)) < 0) {
			AKMERROR_STR("open");
			return AKD_FAIL;
		}
	}
	return AKD_SUCCESS;
}

/*!
 Close device driver.
 This function closes both device drivers of magnetic sensor and acceleration
 sensor.
 */
void AKD_DeinitDevice(void)
{
	if (s_fdDev >= 0) {
		close(s_fdDev);
		s_fdDev = -1;
	}
}

/*!
 Writes data to a register of the AK8963.  When more than one byte of data is
 specified, the data is written in contiguous locations starting at an address
 specified in \a address.
 @return If this function succeeds, the return value is #AKD_SUCCESS. Otherwise
 the return value is #AKD_FAIL.
 @param[in] address Specify the address of a register in which data is to be
 written.
 @param[in] data Specify data to write or a pointer to a data array containing
 the data.  When specifying more than one byte of data, specify the starting
 address of the array.
 @param[in] numberOfBytesToWrite Specify the number of bytes that make up the
 data to write.  When a pointer to an array is specified in data, this argument
 equals the number of elements of the array.
 */
int16_t AKD_TxData(
				const BYTE address,
				const BYTE * data,
				const uint16_t numberOfBytesToWrite)
{
	int i;
	char buf[RWBUF_SIZE];

	if (s_fdDev < 0) {
		ALOGE("%s: Device file is not opened.", __FUNCTION__);
		return AKD_FAIL;
	}
	if (numberOfBytesToWrite > (RWBUF_SIZE-2)) {
		ALOGE("%s: Tx size is too large.", __FUNCTION__);
		return AKD_FAIL;
	}

	buf[0] = numberOfBytesToWrite + 1;
	buf[1] = address;

	for (i = 0; i < numberOfBytesToWrite; i++) {
		buf[i + 2] = data[i];
	}
	if (ioctl(s_fdDev, ECS_IOCTL_WRITE, buf) < 0) {
		AKMERROR_STR("ioctl");
		return AKD_FAIL;
	} else {

#if ENABLE_AKMDEBUG
		AKMDATA(AKMDATA_MAGDRV, "addr(HEX)=%02x data(HEX)=", address);
		for (i = 0; i < numberOfBytesToWrite; i++) {
			AKMDATA(AKMDATA_MAGDRV, " %02x", data[i]);
		}
		AKMDATA(AKMDATA_MAGDRV, "\n");
#endif
		return AKD_SUCCESS;
	}
}

/*!
 Acquires data from a register or the EEPROM of the AK8963.
 @return If this function succeeds, the return value is #AKD_SUCCESS. Otherwise
 the return value is #AKD_FAIL.
 @param[in] address Specify the address of a register from which data is to be
 read.
 @param[out] data Specify a pointer to a data array which the read data are
 stored.
 @param[in] numberOfBytesToRead Specify the number of bytes that make up the
 data to read.  When a pointer to an array is specified in data, this argument
 equals the number of elements of the array.
 */
int16_t AKD_RxData(
				const BYTE address,
				BYTE * data,
				const uint16_t numberOfBytesToRead)
{
	int i;
	char buf[RWBUF_SIZE];

	memset(data, 0, numberOfBytesToRead);

	if (s_fdDev < 0) {
		ALOGE("%s: Device file is not opened.", __FUNCTION__);
		return AKD_FAIL;
	}
	if (numberOfBytesToRead > (RWBUF_SIZE-1)) {
		ALOGE("%s: Rx size is too large.", __FUNCTION__);
		return AKD_FAIL;
	}

	buf[0] = numberOfBytesToRead;
	buf[1] = address;

	if (ioctl(s_fdDev, ECS_IOCTL_READ, buf) < 0) {
		AKMERROR_STR("ioctl");
		return AKD_FAIL;
	} else {
		for (i = 0; i < numberOfBytesToRead; i++) {
			data[i] = buf[i + 1];
		}
#if ENABLE_AKMDEBUG
		AKMDATA(AKMDATA_MAGDRV, "addr(HEX)=%02x len=%d data(HEX)=",
				address, numberOfBytesToRead);
		for (i = 0; i < numberOfBytesToRead; i++) {
			AKMDATA(AKMDATA_MAGDRV, " %02x", data[i]);
		}
		AKMDATA(AKMDATA_MAGDRV, "\n");
#endif
		return AKD_SUCCESS;
	}
}

/*!
 Reset the e-compass. 
 @return If this function succeeds, the return value is #AKD_SUCCESS. Otherwise 
 the return value is #AKD_FAIL.
 */
int16_t AKD_ResetAK8963(void) {
	if (s_fdDev < 0) {
		ALOGE("Magnetometer is not opened.\n");
		return AKD_FAIL;
	}
	if (ioctl(s_fdDev, ECS_IOCTL_RESET, NULL) < 0) {
		AKMERROR_STR("ioctl");
		return AKD_FAIL;
	}
	return AKD_SUCCESS;
}

/*!
 Acquire magnetic data from AK8963. If measurement is not done, this function
 waits until measurement completion.
 @return If this function succeeds, the return value is #AKD_SUCCESS. Otherwise
 the return value is #AKD_FAIL.
 @param[out] data A magnetic data array. The size should be larger than #SENSOR_DATA_SIZE.
 */
int16_t AKD_GetMagneticData(BYTE data[SENSOR_DATA_SIZE])
{
	memset(data, 0, SENSOR_DATA_SIZE);

	if (s_fdDev < 0) {
		ALOGE("%s: Device file is not opened.", __FUNCTION__);
		return AKD_FAIL;
	}

	if (ioctl(s_fdDev, ECS_IOCTL_GETDATA, data) < 0) {
		AKMERROR_STR("ioctl");
		return AKD_FAIL;
	}

	AKMDATA(AKMDATA_MAGDRV,
		"bdata(HEX)= %02x %02x %02x %02x %02x %02x %02x %02x\n",
		data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

	return AKD_SUCCESS;
}

/*!
 Set calculated data to device driver.
 @param[in] buf
 */
void AKD_SetYPR(const int buf[YPR_DATA_SIZE])
{
	if (ioctl(s_fdDev, ECS_IOCTL_SET_YPR, buf) < 0) {
		AKMERROR_STR("ioctl");
	}
}

/*!
 */
int AKD_GetOpenStatus(int* status)
{
	if (ioctl(s_fdDev, ECS_IOCTL_GET_OPEN_STATUS, status) < 0) {
		AKMERROR_STR("ioctl");
		return AKD_FAIL;
	}
	return AKD_SUCCESS;
}

/*!
 */
int AKD_GetCloseStatus(int* status)
{
	if (ioctl(s_fdDev, ECS_IOCTL_GET_CLOSE_STATUS, status) < 0) {
		AKMERROR_STR("ioctl");
		return AKD_FAIL;
	}
	return AKD_SUCCESS;
}

/*!
 Set AK8963 to the specific mode.
 @return If this function succeeds, the return value is #AKD_SUCCESS. Otherwise
 the return value is #AKD_FAIL.
 @param[in] mode This value should be one of the AK8963_Mode which is defined in
 akm8963.h file.
 */
int16_t AKD_SetMode(const BYTE mode)
{
	if (s_fdDev < 0) {
		ALOGE("%s: Device file is not opened.", __FUNCTION__);
		return AKD_FAIL;
	}

	if (ioctl(s_fdDev, ECS_IOCTL_SET_MODE, &mode) < 0) {
		AKMERROR_STR("ioctl");
		return AKD_FAIL;
	}

	return AKD_SUCCESS;
}

/*!
 Acquire delay
 @return If this function succeeds, the return value is #AKD_SUCCESS. Otherwise
 the return value is #AKD_FAIL.
 @param[out] delay A delay in microsecond.
 */
int16_t AKD_GetDelay(int64_t delay[AKM_NUM_SENSORS])
{
	if (s_fdDev < 0) {
		ALOGE("%s: Device file is not opened.\n", __FUNCTION__);
		return AKD_FAIL;
	}
	if (ioctl(s_fdDev, ECS_IOCTL_GET_DELAY, delay) < 0) {
		AKMERROR_STR("ioctl");
		return AKD_FAIL;
	}
	return AKD_SUCCESS;
}

/*!
 Get layout information from device driver, i.e. platform data.
 */
int16_t AKD_GetLayout(int16_t* layout)
{
	char tmp;

	if (s_fdDev < 0) {
		ALOGE("%s: Device file is not opened.", __FUNCTION__);
		return AKD_FAIL;
	}

	if (ioctl(s_fdDev, ECS_IOCTL_GET_LAYOUT, &tmp) < 0) {
		AKMERROR_STR("ioctl");
		return AKD_FAIL;
	}

	*layout = tmp;
	return AKD_SUCCESS;
}

/*!
 Get outbit information from device driver, i.e. platform data.
 */
int16_t AKD_GetOutbit(int16_t* outbit)
{
	char tmp;

	if (s_fdDev < 0) {
		ALOGE("%s: Device file is not opened.", __FUNCTION__);
		return AKD_FAIL;
	}

	if (ioctl(s_fdDev, ECS_IOCTL_GET_OUTBIT, &tmp) < 0) {
		AKMERROR_STR("ioctl");
		return AKD_FAIL;
	}

	*outbit = tmp;
	return AKD_SUCCESS;
}
#if 0
/* Get measurement data. See "AK8963Driver.h" */
int16_t AKD_GetAccelerationData(int16_t data[3])
{
	return Acc_GetAccelerationData(data);
}
#endif
/*!
 Acquire acceleration data from acceleration sensor.
 @return If this function succeeds, the return value is #AKD_SUCCESS. Otherwise 
 the return value is #AKD_FAIL.
 @param[out] data A acceleration data array. The coordinate system of the 
 acquired data follows the definition of AK. 
 */
int16_t AKD_GetAccelerationData(int16_t data[3])
{
	return AKD_SUCCESS;
}

/*!
 Convert Acceleration sensor coordinate system from Android's one to AK's one.
 In Android coordinate system, 1G = 9.8f (m/s^2). In AK coordinate system, 
 1G = 720 (LSB).
 @param[in] fData A acceleration data array. The coordinate system of this data 
 should follows the definition of Android.
 @param[out] data A acceleration data array. The coordinate system of the 
 acquired data follows the definition of AK. 
 */
void Android2AK(const float fData[], int16_t data[3])
{
    /*
     * 若将送入 AKSC_DirectionS3() 的 acc 数据使用 SmartCompass 坐标系, ...
     * .! : 这里 acc 的坐标定义有蹊跷, 下面的实现是正确的.
     */
	data[0] = fData[0] / 9.8f * 720;
	data[1] = fData[1] / 9.8f * 720;
	data[2] = fData[2] / 9.8f * 720;
}

