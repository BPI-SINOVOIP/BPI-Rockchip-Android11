/******************************************************************************
 *
 * $Id: DispMessage.h 303 2011-08-12 04:22:45Z kihara.gb $
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
#ifndef AKMD_INC_DISPMESG_H
#define AKMD_INC_DISPMESG_H

// Include file for AK8963 library.
#include "AKCompass.h"

/*** Constant definition ******************************************************/
#define DISP_CONV_AKSCF(val)	((val)*0.06f)
#define DISP_CONV_Q6F(val)		((val)*0.015625f)

/*** Type declaration *********************************************************/

/*! These defined types represents the current mode. */
typedef enum _MODE {
	MODE_ERROR,					/*!< Error */
	MODE_FctShipmntTestBody,	/*!< On board function test */
	MODE_MeasureSNG,			/*!< Measurement */
	MODE_Quit					/*!< Quit */
} MODE;

/*** Prototype of function ****************************************************/
// Disp_   : Display messages.
// Menu_   : Display menu (two or more selection) and wait for user input.

void Disp_StartMessage(void);

void Disp_EndMessage(int ret);

void Disp_MeasurementResult(AK8963PRMS* prms);

// Defined in main.c
void Disp_MeasurementResultHook(AK8963PRMS* prms, const uint16 flag);

MODE Menu_Main(void);

int16 TEST_DATA(
	const char testno[],
	const char testname[],
	const int16 testdata,
	const int16 lolimit,
	const int16 hilimit,
	int16* pf_total
);

#endif

