/******************************************************************************
 *
 * $Id: Measure.h 304 2011-08-12 07:52:29Z kihara.gb $
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
#ifndef AKMD_INC_MEASURE_H
#define AKMD_INC_MEASURE_H

// Include files for AK8963 library.
#include "AKCompass.h"
#include "misc.h"

/*** Constant definition ******************************************************/
#define AKRET_PROC_SUCCEED      0x00	/*!< The process has been successfully done. */
#define AKRET_FORMATION_CHANGED 0x01	/*!< The formation is changed */
#define AKRET_DATA_READERROR    0x02	/*!< Data read error occurred. */
#define AKRET_DATA_OVERFLOW     0x04	/*!< Data overflow occurred. */
#define AKRET_OFFSET_OVERFLOW	0x08	/*!< Offset values overflow. */
#define AKRET_HBASE_CHANGED	0x10	/*!< hbase was changed. */
#define AKRET_HFLUC_OCCURRED    0x20	/*!< A magnetic field fluctuation occurred. */
#define AKRET_VNORM_ERROR	0x40	/*!< AKSC_VNorm error. */
#define AKRET_PROC_FAIL         0x80	/*!< The process failes. */

#define AKMD_MAG_MIN_INTERVAL	 10000000	/*!< Minimum magnetometer interval */
#define AKMD_ACC_MIN_INTERVAL	 10000000	/*!< Minimum acceleration interval */
#define AKMD_ORI_MIN_INTERVAL	 10000000	/*!< Minimum orientation interval */
#define AKMD_LOOP_MARGIN	  3000000	/*!< Minimum sleep time */
#define AKMD_SETTING_INTERVAL	500000000	/*!< Setting event interval */

/*** Type declaration *********************************************************/
typedef int16(*OPEN_FORM)(void);
typedef void(*CLOSE_FORM)(void);
typedef int16(*CHECK_FORM)(void);

typedef struct _FORM_CLASS {
	OPEN_FORM	open;
	CLOSE_FORM	close;
	CHECK_FORM	check;
} FORM_CLASS;

/*** Global variables *********************************************************/

/*** Prototype of function ****************************************************/
void RegisterFormClass(
	FORM_CLASS* pt
);

void InitAK8963PRMS(
	AK8963PRMS*	prms
);

void SetDefaultPRMS(
	AK8963PRMS*	prms
);

int16 GetInterval(
	AKMD_LOOP_TIME* acc_acq,
	AKMD_LOOP_TIME* mag_acq,
	AKMD_LOOP_TIME* ori_acq,
	AKMD_LOOP_TIME* mag_mes,
	AKMD_LOOP_TIME* acc_mes,
	int16* hdoe_dec
);

int SetLoopTime(
	AKMD_LOOP_TIME* tm,
	int64_t execTime,
	int64_t* minDuration
);

int16 ReadAK8963FUSEROM(
	AK8963PRMS*	prms
);

int16 InitAK8963_Measure(
	AK8963PRMS*	prms
);

int16 FctShipmntTest_Body(
	AK8963PRMS*	prms
);

int16 FctShipmntTestProcess_Body(
	AK8963PRMS*	prms
);

void MeasureSNGLoop(
	AK8963PRMS*	prms
);

int16 GetMagneticVector(
	const int16	bData[],
	AK8963PRMS*	prms,
	const int16	curForm,
	const int16	hDecimator
);

int16 CalcDirection(
	AK8963PRMS* prms
);

#endif

