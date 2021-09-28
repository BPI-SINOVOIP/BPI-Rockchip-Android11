/******************************************************************************
 *
 *  Workfile: AKHFlucCheck.h 
 *
 *  Author: Rikita 
 *  Date: 09/09/10 17:11 
 *  Revision: 6 
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
#ifndef AKSC_INC_AKHFLUCCHECK_H
#define AKSC_INC_AKHFLUCCHECK_H

#include "AKMDevice.h"

//========================= Constant definition =========================//

//========================= Type declaration  ===========================//
typedef struct _AKSC_HFLUCVAR {
	int16vec	href; // Basis of magnetic field
	int16		th;   // The range of fluctuation
}AKSC_HFLUCVAR;

//========================= Prototype of Function =======================//
AKLIB_C_API_START

int16 AKSC_InitHFlucCheck(
			AKSC_HFLUCVAR*	hflucv,	//(o)	: A set of criteria to be initialized
	const	int16vec*		href,	//(i)	: Initial value of basis
	const	int16			th		//(i)	: The range of fluctuation
);

int16 AKSC_HFlucCheck(
			AKSC_HFLUCVAR*	hflucv,	//(i/o)	: A set of criteria
	const	int16vec*		hdata	//(i)	: Current magnetic vector
);

AKLIB_C_API_END

#endif
