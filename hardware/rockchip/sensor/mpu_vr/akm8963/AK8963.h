/******************************************************************************
 *
 *  $Id: AK8963.h 370 2011-08-03 04:57:28Z kihara.gb $
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
#ifndef AKSC_INC_AK8963_H
#define AKSC_INC_AK8963_H

#include "AKMDevice.h"

//========================= Type declaration  ===========================//

//========================= Constant definition =========================//
#define AKSC_HSENSE_TARGET	833	// Target sensitivity for magnetic sensor
#define AKSC_BDATA_SIZE		8

//========================= Prototype of Function =======================//
AKLIB_C_API_START

void AKSC_InitDecomp8963(
			int16vec			hdata[]	//(i/o)	: Magnetic data. index 0 is earlier data. The size of hdata must be HDATA_SIZE.
);

int16 AKSC_Decomp8963(					//(o)	: (0)abend, (1)normally calculated
	const	int16				bdata[],//(i)	: Block data
	const	int16				hNave,	//(i)	: The number of magnetic data to be averaged. hNave must be 0,1,2,4,8,16,32
	const	int16vec*			asa,	//(i)	: Sensitivity adjustment value(the value read from Fuse ROM)
			int16vec			hdata[],//(i/o)	: Magnetic data. index 0 is earlier data.
			int32vec*			hbase,	//(i/o)	: Magnetic data base
			int16*				hn,		//(o)	: The number of magnetic data, output 1.
			int16vec*			have,	//(o)	: Average of magnetic data
			int16*				dor,	//(o)	: 0;normal data, 1;data overrun is occurred.
			int16*				derr,	//(o)	: 0;normal data, 1;data read error occurred.
			int16*				hofl,	//(o)	: 0;normal data, 1;data overflow occurred.
			int16*				cb		//(o)	: 0;hbase not changed, 1;hbase is changed
);

AKLIB_C_API_END

#endif
