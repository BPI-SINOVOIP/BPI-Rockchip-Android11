/******************************************************************************
 *
 * $Id: Acc_dummy.c 181 2010-03-01 06:27:13Z rikita $
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

#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/mma8452.h>

#include "AKCommon.h"
#include "AK8975Driver.h"
#include "Acc_mma8452.h"

/* Path to device file */
#define ASENSOR_NAME	"/dev/accel"

/* Definitions for accelerometer */
#define ACC_IOC_MAGIC	'A'

/* Variant for accelerometer */
int fd_acc = -1;

/* Function declaration */
int16_t Acc_InitDevice(void);
void Acc_DeinitDevice(void);
int16_t Acc_GetAccelerationData(float fData[3]);

/*!
 Open device driver.
 This function opens device driver of acceleration sensor. Additionally,
 measurement range is set to +/- 2G mode, bandwidth is set to 25Hz.
 @return If this function succeeds, the return value is #AKD_SUCCESS. Otherwise 
 the return value is #AKD_FAIL.
 */
int16_t Acc_InitDevice(void)
{
	/* This is just a abstract sample. */
	/*
	 if (fd_acc >= 0) {
		// Already initialized.
		return AKD_ERROR;
	 }
	 
     if((fd_acc = open(ASENSOR_NAME,O_RDWR)) == -1){
		return AKD_FAIL;
     }
	 */
	
	fd_acc = 0;
	return AKD_SUCCESS;
}

/*!
 Close device driver.
 This function closes device drivers of acceleration sensor.
 */
void Acc_DeinitDevice(void)
{
	/* This is just a abstract sample. */
	/*
     if(fd_acc != -1) {
		close(fd_acc);
		fd_acc = -1;
     }
	 */
	
	fd_acc = -1;
}

/*!
 Acquire acceleration data from acceleration sensor and convert it to Android 
 coordinate system.
 @return If this function succeeds, the return value is #AKD_SUCCESS. Otherwise 
 the return value is #AKD_FAIL.
 @param[out] fData A acceleration data array. The coordinate system of the 
 acquired data follows the definition of Android. 
 */
int16_t Acc_GetAccelerationData(float fData[3])
{
	if (fd_acc == -1) {
		DBGPRINT(DBG_LEVEL3, "Device file is not opened.\n");
		return AKD_FAIL;
	}
	
	/* This is just a abstract sample. */
	/*
     short Acc_temp[3];
     ioctl(fd_acc, READ_XYZ, Acc_temp);
	 */
	
	/* Horizontal */
	fData[0] = 0.0f;
	fData[1] = 0.0f;
	fData[2] = 9.8f;
	
	/* Vertical */
	/*
     fData[0] = 0.0f;
     fData[1] = 9.8f;
     fData[2] = 0.0f;
	 */
	
	return AKD_SUCCESS;
}
