/******************************************************************************
 *
 *  Workfile: AKManualCal.h 
 *
 *  Author: Rikita 
 *  Date: 09/09/10 17:11 
 *  Revision: 15 
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
#ifndef AKSC_INC_AKMANUALCAL_H
#define AKSC_INC_AKMANUALCAL_H

#include "AKMDevice.h"

//========================= Type declaration  ===========================//

//========================= Constant definition =========================//

//========================= Prototype of Function =======================//
AKLIB_C_API_START
int16 AKSC_HOffsetCal(				//(o)   : Calibration success(1), failure(0)
	const	int16vec	hdata[],	//(i)   : Geomagnetic vectors(the size must be 3)
			int16vec*	ho			//(o)   : Offset(5Q11)
);

AKLIB_C_API_END

#endif

