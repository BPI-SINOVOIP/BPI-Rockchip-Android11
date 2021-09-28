/******************************************************************************
 *
 * $Id: AK8975Driver.c 217 2010-03-26 07:11:56Z rikita $
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
// 对 设备文件操作的 封装实现. 

#include "AK8975Driver.h"
#include "AKCommon.h"		// DBGPRINT()
#include <fcntl.h>

/* Include proper acceleration file. */
//#include "Acc_bma150.c"
//#include "Acc_adxl345.c"
// #include "Acc_dummy.c"
// #include "Acc_mma8452.c"

#include "Acc_mma8452.h"

#define DBG_DATA_MONITOR  0
#define MSENSOR_NAME      "/dev/akm8975_dev"
//#define GETMAG_WITH_POLLING

int g_file = -1;

void Android2AK(const float fData[], int16_t data[3]);

/*!
 Open device driver.
 This function opens both device drivers of magnetic sensor and acceleration
 sensor. Additionally, some initial hardware settings are done, such as
 measurement range, built-in filter function and etc.
 @return If this function succeeds, the return value is #AKD_SUCCESS. 
 If device driver is already opened, the return value is #AKD_ERROR.
 Otherwise the return value is #AKD_FAIL.  
 */
int16_t AKD_InitDevice(void)
{
	if (g_file >= 0) {
		// Already initialized.
		return AKD_ERROR;
	}
	// Open magnetic sensor's device driver.
	if ((g_file = open(MSENSOR_NAME, O_RDWR)) < 0) {
		DBGPRINT(DBG_LEVEL0, "open error.\n");
		return AKD_FAIL;
	}
	
	// Open acceleration sensor's device driver.
	if (Acc_InitDevice() != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL0, "Acc initialize error.\n");
		return AKD_FAIL;
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
	if (g_file >= 0) {
		close(g_file);
		g_file = -1;
	}
	
	Acc_DeinitDevice();
}

/*!
 Writes data to a register of the AK8975.  When more than one byte of data is 
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
int16_t AKD_TxData(const BYTE address,
				 const BYTE * data,
				 const uint16_t numberOfBytesToWrite)
{
	int i;
	char buf[RWBUF_SIZE];
	
	if (g_file < 0) {
		DBGPRINT(DBG_LEVEL0, "Device file is not opened.\n");
		return AKD_FAIL;
	}
	if (numberOfBytesToWrite > (RWBUF_SIZE-2)) {
		DBGPRINT(DBG_LEVEL1, "Tx size is too large.\n");
		return AKD_FAIL;
	}

	buf[0] = numberOfBytesToWrite + 1;
	buf[1] = address;
	
	for (i = 0; i < numberOfBytesToWrite; i++) {
		buf[i + 2] = data[i];
	}
	if (ioctl(g_file, ECS_IOCTL_WRITE, buf) < 0) {
		DBGPRINT(DBG_LEVEL1, "ioctl error.\n");
		return AKD_FAIL;
	} else {
#if DBG_DATA_MONITOR
		printf("addr=%02x data=", address);
		for (i = 0; i < numberOfBytesToWrite; i++) {
			printf(" %02x", data[i]);
		}
		printf("\n");
#endif
		return AKD_SUCCESS;
	}
}

/*!
 Acquires data from a register or the EEPROM of the AK8975.
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
int16_t AKD_RxData(const BYTE address, 
				 BYTE * data,
				 const uint16_t numberOfBytesToRead)
{
	int i;
	char buf[RWBUF_SIZE];
	
	memset(data, 0, numberOfBytesToRead);
	
	if (g_file < 0) {
		DBGPRINT(DBG_LEVEL0, "Device file is not opened.\n");
		return AKD_FAIL;
	}
	if (numberOfBytesToRead > (RWBUF_SIZE-1)) {
		DBGPRINT(DBG_LEVEL1, "Rx size is too large.\n");
		return AKD_FAIL;
	}

	buf[0] = numberOfBytesToRead;
	buf[1] = address;
	
	if (ioctl(g_file, ECS_IOCTL_READ, buf) < 0) {
		DBGPRINT(DBG_LEVEL1, "ioctl error.\n");
		return AKD_FAIL;
	} else {
		for (i = 0; i < numberOfBytesToRead; i++) {
			data[i] = buf[i + 1];
		}
#if DBG_DATA_MONITOR
		printf("addr=%02x len=%d data=", address, numberOfBytesToRead);
		for (i = 0; i < numberOfBytesToRead; i++) {
			printf(" %02x", data[i]);
		}
		printf("\n");
#endif
		return AKD_SUCCESS;
	}
}

/*!
 Acquire magnetic data from AK8975. If measurement is not done, this function 
 waits until measurement completion.
 @return If this function succeeds, the return value is #AKD_SUCCESS. Otherwise 
 the return value is #AKD_FAIL.
 @param[out] data A magnetic data array. The size should be larger than #SENSOR_DATA_SIZE.
 */
int16_t AKD_GetMagneticData(BYTE data[SENSOR_DATA_SIZE])
{
	int i;
	
	memset(data, 0, SENSOR_DATA_SIZE);
	
	if (g_file < 0) {
		DBGPRINT(DBG_LEVEL0, "Device file is not opened.\n");
		return AKD_FAIL;
	}
	
#ifdef GETMAG_WITH_POLLING
#warning "get magnetic data with polling!"
	// Wait until measurement done.
	msleep(AK8975_MEASUREMENT_TIME);
	
	for (i = 0; i < AK8975_MEASURE_TIMEOUT; i++) {
		if (AKD_RxData(AK8975_REG_ST1, data, 1) != AKD_SUCCESS) {
			DBGPRINT(DBG_LEVEL1, "ioctl error.\n");
			return AKD_FAIL;
		}
		if (data[0] & 0x01) {
			if (AKD_RxData(AK8975_REG_ST1, data, SENSOR_DATA_SIZE) != AKD_SUCCESS) {
				DBGPRINT(DBG_LEVEL1, "ioctl error.\n");
				return AKD_FAIL;
			}
			break;
		}
		usleep(1000);
	}
	if (i >= AK8975_MEASURE_TIMEOUT) {
		DBGPRINT(DBG_LEVEL1, "DRDY timeout.\n");
		return AKD_FAIL;
	}
#else
	if (ioctl(g_file, ECS_IOCTL_GETDATA, data) < 0) {
		DBGPRINT(DBG_LEVEL1, "ioctl error.\n");
		return AKD_FAIL;
	}
#endif
	
#if DBG_DATA_MONITOR
	printf("bdata=");
	for (i = 0; i < SENSOR_DATA_SIZE; i++) {
		printf(" %02x", data[i]);
	}
	printf("\n");
#endif
	
	return AKD_SUCCESS;
}

/*!
 Set AK8975 to the specific mode. 
 @return If this function succeeds, the return value is #AKD_SUCCESS. Otherwise 
 the return value is #AKD_FAIL.
 @param[in] mode This value should be one of the AK8975_Mode which is defined in
 akm8975.h file.
 */
int16_t AKD_SetMode(const BYTE mode)
{
	if (g_file < 0) {
		DBGPRINT(DBG_LEVEL0, "Device file is not opened.\n");
		return AKD_FAIL;
	}
	
	if (ioctl(g_file, ECS_IOCTL_SET_MODE, &mode) < 0) {
		DBGPRINT(DBG_LEVEL1, "ioctl error.\n");
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
int16_t AKD_GetDelay(int32_t* delay)
{
	short tmp;

	if (g_file < 0) {
		DBGPRINT(DBG_LEVEL0, "Device file is not opened.\n");
		return AKD_FAIL;
	}
	
	if (ioctl(g_file, ECS_IOCTL_GET_DELAY, &tmp) < 0) {
		DBGPRINT(DBG_LEVEL1, "ioctl error.\n");
		return AKD_FAIL;
	}
	// The API returns delay time in millisecond.
	*delay = tmp * 1000;

	return AKD_SUCCESS;
}

/*!
 Acquire acceleration data from acceleration sensor.
 @return If this function succeeds, the return value is #AKD_SUCCESS. Otherwise 
 the return value is #AKD_FAIL.
 @param[out] data A acceleration data array. The coordinate system of the 
 acquired data follows the definition of AK. 
 */
int16_t AKD_GetAccelerationData(int16_t data[3])
{
	float fData[3];
	
	if (Acc_GetAccelerationData(fData) != AKD_SUCCESS) {
		return AKD_FAIL;
	} else {
		Android2AK(fData, data);
		return AKD_SUCCESS;
	}
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

