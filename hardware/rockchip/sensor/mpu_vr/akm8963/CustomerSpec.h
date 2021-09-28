/******************************************************************************
 *
 * $Id: CustomerSpec.h 303 2011-08-12 04:22:45Z kihara.gb $
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
#ifndef AKMD_INC_CUSTOMERSPEC_H
#define AKMD_INC_CUSTOMERSPEC_H

/*******************************************************************************
 User defines parameters.
 ******************************************************************************/

// Certification information
#define CSPEC_CI_AK_DEVICE	8963
#define CSPEC_CI_LICENSER	"ASAHIKASEI"
#define CSPEC_CI_LICENSEE	"ROCK_63_ICS"

// Parameters for Average
//	The number of magnetic data block to be averaged.
//	 NBaveh*(*nh) must be 1, 2, 4, 8 or 16.
#define CSPEC_HNAVE		8

// Parameters for Direction Calculation
#define CSPEC_DVEC_X		0
#define CSPEC_DVEC_Y		0
#define CSPEC_DVEC_Z		0


////////////////////////////////////////
// AK8975
// Formation 0
#define CSPEC_FORM0_HLAYOUT_11	1 
#define CSPEC_FORM0_HLAYOUT_12	0 
#define CSPEC_FORM0_HLAYOUT_13	0
#define CSPEC_FORM0_HLAYOUT_21	0 
#define CSPEC_FORM0_HLAYOUT_22	1 
#define CSPEC_FORM0_HLAYOUT_23	0
#define CSPEC_FORM0_HLAYOUT_31	0
#define CSPEC_FORM0_HLAYOUT_32	0
#define CSPEC_FORM0_HLAYOUT_33	1

// Formation 1
#define CSPEC_FORM1_HLAYOUT_11	1
#define CSPEC_FORM1_HLAYOUT_12	0
#define CSPEC_FORM1_HLAYOUT_13	0
#define CSPEC_FORM1_HLAYOUT_21	0
#define CSPEC_FORM1_HLAYOUT_22	1
#define CSPEC_FORM1_HLAYOUT_23	0
#define CSPEC_FORM1_HLAYOUT_31	0
#define CSPEC_FORM1_HLAYOUT_32	0
#define CSPEC_FORM1_HLAYOUT_33	1

// Parameters for Acceleration sensor
// Formation 0
#define CSPEC_FORM0_ALAYOUT_11	1
#define CSPEC_FORM0_ALAYOUT_12	0
#define CSPEC_FORM0_ALAYOUT_13	0
#define CSPEC_FORM0_ALAYOUT_21	0
#define CSPEC_FORM0_ALAYOUT_22	1
#define CSPEC_FORM0_ALAYOUT_23	0
#define CSPEC_FORM0_ALAYOUT_31	0
#define CSPEC_FORM0_ALAYOUT_32	0
#define CSPEC_FORM0_ALAYOUT_33	1

// Formation 1
#define CSPEC_FORM1_ALAYOUT_11	1
#define CSPEC_FORM1_ALAYOUT_12	0
#define CSPEC_FORM1_ALAYOUT_13	0
#define CSPEC_FORM1_ALAYOUT_21	0
#define CSPEC_FORM1_ALAYOUT_22	1
#define CSPEC_FORM1_ALAYOUT_23	0
#define CSPEC_FORM1_ALAYOUT_31	0
#define CSPEC_FORM1_ALAYOUT_32	0
#define CSPEC_FORM1_ALAYOUT_33	1


// The number of formation
#define CSPEC_NUM_FORMATION		2

// the counter of Suspend
#define CSPEC_CNTSUSPEND_SNG	8

// Parameters for FctShipmntTest
//  1 : USE SPI
//  0 : NOT USE SPI(I2C)
#define CSPEC_SPI_USE			0

// Setting file
#define CSPEC_SETTING_FILE	"/data/misc/akmd_set.txt"

#endif //AKMD_INC_CUSTOMERSPEC_H

