/******************************************************************************
 *
 * $Id: CustomerSpec.h 200 2010-03-19 10:25:52Z rikita $
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
#ifndef AKMD_INC_CUSTOMERSPEC_H
#define AKMD_INC_CUSTOMERSPEC_H

/*******************************************************************************
 User defines parameters.
 ******************************************************************************/

// Certification information
#define CSPEC_CI_AK_DEVICE	8975
#define CSPEC_CI_LICENSER	"ASAHIKASEI"
#define CSPEC_CI_LICENSEE	"ROCKCHIP-FUZHOU"

// Parameters for Average
//	The number of magnetic data block to be averaged.
//	 NBaveh*(*nh) must be 1, 2, 4, 8 or 16.
#define CSPEC_HNAVE		4

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

// measurement time + extra time
#define	CSPEC_TIME_MEASUREMENTDRDY	20

// The number of formation
#define CSPEC_NUM_FORMATION		2   // .! : 虽然文档说明本 macro 是配置接口, 但是实现代码(如 类型 AK8975PRMS, LoadParameters() )中有对该 macro 的硬耦合,
                                    //      所以, "不要" 修改本 macro 的配置. 

// the counter of Suspend
#define CSPEC_CNTSUSPEND_SNG	8

// Parameters for FctShipmntTest
//  1 : USE SPI
//  0 : NOT USE SPI(I2C)
#define CSPEC_SPI_USE			0     


/*** Deprecate ****************************************************************/
// Set Decimator for HDOEProcess( ) 
// 8-16Hz : 1
//   20Hz : 2
//   30Hz : 3
//   50Hz : 5
//  100Hz : 10
#define CSPEC_HDECIMATOR_SNG				1

// Default interval
#define CSPEC_INTERVAL_SNG				125000

#endif

