/******************************************************************************
 *
 * $Id: misc.c 196 2010-03-17 10:33:00Z rikita $
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
#include "misc.h"
#include <fcntl.h>
#include <linux/input.h>

int s_stopRequest;	/*!< 0:Not stop,  1:Stop */
int s_opmode;		/*!< 0:Daemon mode, 1:Console mode */

static int s_keyFile = -1;	/*!< FD to console control device */
static int s_formFile = -1;	/*!< FD to formation detect device */

#define NUMOF_AKMD_INTERVAL 8
static AKMD_INTERVAL s_interval[NUMOF_AKMD_INTERVAL] = {
	{ 10000, 10},	/* 100Hz SENSOR_DELAY_FASTEST */
	{ 12500,  8},	/*  80Hz */
	{ 20000,  5},	/*  50Hz SENSOR_DELAY_GAME */
	{ 25000,  4},	/*  40Hz */
	{ 50000,  2},	/*  20Hz */
	{ 60000,  2},   /*  16Hz SENSOR_DELAY_UI */
	{100000,  1},	/*  10Hz */
	{125000,  1}	/*   8Hz SENSOR_DELAY_NORMAL */
};

/*!
 Sleep function in millisecond.
 */
inline void msleep(signed int msec)
{
	usleep(1000 * msec);
}

/*!
 Check if measurement stop event is occurred. This function should not be
 called except the application is run as ConsoleMode.
 @return If event is not occurred, the return value is 0. When this function
 fails, the return value is -1. When this function succeeds, the return 
 value is positive value.
 */
int16 checkKeyConsole(void)
{
	int ch;
	int len;
	struct input_event ev;
	
	if (s_keyFile < 0) {
		return -1;
	}
	
	len = read(s_keyFile, &ev, sizeof(ev));
	
	while (len > 0) {
		if ((ev.type == EV_KEY) && (ev.value == 0)) {
			ch = (ev.code == KEY_ENTER) ? AKKEY_STOP_MEASURE : ev.code;
			return ch;
		}
		len = read(s_keyFile, &ev, sizeof(ev));
	}
	
	return 0;
}

/*!
 Open device driver for checking measurement stop condition.
 @return If function succeeds, the return value is 1. Otherwise, the return
 value is 0.
 */
int16 openKey(void)
{
	if (s_opmode) {
		if (s_keyFile < 0) {
			s_keyFile = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
			if (s_keyFile < 0) {
				s_keyFile = -1;
				return 0;
			}
		}
	}
	return 1;
}

/*!
 Close device driver for checking measurement stop condition.
 */
void closeKey(void)
{
	if (s_keyFile != -1) {
		close(s_keyFile);
		s_keyFile = -1;
	}
}

/*!
 Check if measurement stop event is occurred.
 @return If event is not occurred, the return value is 0. When this function
 fails, the return value is -1. When this function succeeds, the return 
 value is positive value.
 */
int16 checkKey(void)
{
	if (s_opmode) {
		return checkKeyConsole();
	} else {
		return s_stopRequest ? AKKEY_STOP_MEASURE : AKKEY_NONE;
	}
}

/*!
 Open device driver which detects current formation 
 @return If function succeeds, the return value is 1. Otherwise, the return
 value is 0.
 */
int16 openFormation(void)
{
	if (s_formFile < 0) {
		s_formFile = 0;
	}
	return 1;
}

/*!
 Close device driver 
 */
void  closeFormation(void)
{
	if (s_formFile != -1) {
		s_formFile = -1;
	}
}

/*!
 Get current formation 
 @return The number represents current form.
 */
int16 getFormation(void)
{
	return AKMD_FORM0;
}

/*!
 Get valid measurement interval and HDOE decimator.
 @return If this function succeeds, the return value is 1. Otherwise 0.
 */
int16 GetValidInterval(const int32 request,		/*!< Request of interval in
												 microsecond */
					   AKMD_INTERVAL* interval	/*!< Actual interval */
					   )
{
	int i;
	
	for (i = 0; i < NUMOF_AKMD_INTERVAL; i++) {
		*interval = s_interval[i];
		if (request <= s_interval[i].interval) {
			break;
		}
	}

	return 1;
}




