/******************************************************************************
 *
 * $Id: misc.h 303 2011-08-12 04:22:45Z kihara.gb $
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
#ifndef AKMD_INC_MISC_H
#define AKMD_INC_MISC_H

#include "AKCompass.h"

/*** Constant definition ******************************************************/
#define AKMD_FORM0	0
#define AKMD_FORM1	1

/*** Type declaration *********************************************************/
typedef enum _AKMD_CNTLCODE {
	AKKEY_NONE = 0, AKKEY_STOP_MEASURE,
} AKMD_CNTLCODE;

typedef struct _AKMD_LOOP_TIME {
	int64_t interval; /*!< Interval of each event */
	int64_t duration; /*!< duration to the next event */
} AKMD_LOOP_TIME;

/*** Global variables *********************************************************/

/*** Prototype of Function  ***************************************************/
void msleep(signed int msec);

int16 misc_openForm(void);
void misc_closeForm(void);
int16 misc_checkForm(void);

struct timespec int64_to_timespec(int64_t val);
int64_t timespec_to_int64(struct timespec* val);
int64_t CalcDuration(struct timespec* begin, struct timespec* end);

int openInputDevice(const char* name);
int16 GetHDOEDecimator(int64_t* time, int16* hdoe_interval);

int16 ConvertCoordinate(
	const	AKMD_PATNO	pat,/*!< [in]  Convert Pattern Number */
			int16vec*	vec	/*!< [out] Coordinate system after converted */
);

#endif /*AKMD_INC_MISC_H*/

