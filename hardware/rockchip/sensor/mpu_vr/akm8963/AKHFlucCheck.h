/******************************************************************************
 *
 *  $Id: AKHFlucCheck.h 361 2011-07-27 09:27:24Z yamada.rj $
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

void AKSC_TransByHbase(
	const	int32vec*	prevHbase,	//(i)	: Previous hbase
	const	int32vec*	hbase,		//(i)	: Current hbase
			int16vec*	ho,			//(i/o)	: Offset value based on current hbase(16bit)
			int32vec*	ho32,		//(i/o)	: Offset value based on current hbase(32bit)
			int16*		overflow	//(o)	: 0;success, 1;ho overflow.
);

AKLIB_C_API_END

#endif
