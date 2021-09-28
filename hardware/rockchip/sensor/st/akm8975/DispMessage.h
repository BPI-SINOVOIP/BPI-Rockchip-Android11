/******************************************************************************
 *
 * $Id: DispMessage.h 177 2010-02-26 03:01:09Z rikita $
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
#ifndef AKMD_INC_DISPMESG_H
#define AKMD_INC_DISPMESG_H

// Include file for AK8975 library.
#include "AKCompass.h"

/*** Constant definition ******************************************************/

/*** Type declaration *********************************************************/

/*! These defined types represents the current mode. */
typedef enum _MODE {
	MODE_ERROR,					/*!< Error */
	MODE_FctShipmntTestBody,	/*!< On board function test */
	MODE_MeasureSNG,			/*!< Measurement */
	MODE_HCalibration,			/*!< Magnetic sensor calibration */
	MODE_Quit					/*!< Quit */
} MODE;

/*** Prototype of function ****************************************************/
// Disp_   : Display messages.
// Menu_   : Display menu (two or more selection) and wait for user input.

void Disp_StartMessage(void);

void Disp_EndMessage(void);

void Disp_MeasurementResult(AK8975PRMS * prms);

void Disp_MeasurementResultHook(AK8975PRMS * prms);     // Defined in main.c

MODE Menu_Main(void);

int16 TEST_DATA(const char testno[], 
				const char testname[],
                const int16 testdata, 
				const int16 lolimit,
                const int16 hilimit, 
				int16 * pf_total);

#endif

