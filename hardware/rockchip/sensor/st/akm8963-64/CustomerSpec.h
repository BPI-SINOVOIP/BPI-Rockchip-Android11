/******************************************************************************
 *
 *  $Id: $
 *
 * -- Copyright Notice --
 *
 * Copyright (c) 2004 Asahi Kasei Microdevices Corporation, Japan
 * All Rights Reserved.
 *
 * This software program is the proprietary program of Asahi Kasei Microdevices
 * Corporation("AKM") licensed to authorized Licensee under the respective
 * agreement between the Licensee and AKM only for use with AKM's electronic
 * compass IC.
 *
 * THIS SOFTWARE IS PROVIDED TO YOU "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABLITY, FITNESS FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT OF
 * THIRD PARTY RIGHTS, AND WE SHALL NOT BE LIABLE FOR ANY LOSSES AND DAMAGES
 * WHICH MAY OCCUR THROUGH USE OF THIS SOFTWARE.
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
#define CSPEC_CI_LICENSER	"ASAHIKASEI"
#ifdef AKMD_FOR_AK09911
#define CSPEC_CI_LICENSEE	"RC_11_V7_AR_64"
#else
#define CSPEC_CI_LICENSEE	"RC_63_V7_AR_64"
#endif

// Parameters for Average
//  The number of magnetic data to be averaged.
//  CSPEC_HNAVE must be 1, 2, 4, 8, 16 or 32.
#define CSPEC_HNAVE		32

// Parameters for Direction Calculation
#define CSPEC_DVEC_X		0
#define CSPEC_DVEC_Y		0
#define CSPEC_DVEC_Z		0

// The number of formation
#define CSPEC_NUM_FORMATION		2

// the counter of Suspend
#define CSPEC_CNTSUSPEND_SNG	8

// Parameters for FST
//  1 : USE SPI
//  0 : NOT USE SPI(I2C)
#define CSPEC_SPI_USE			0

//Parameters for Temperature sensor
//	0x00 : disable
//	0x80 : enable
#define CSPEC_TEMPERATURE		0x80

//Parameters for Noise Suppression Filter
//	0x00 : disable
//	0x20 : Low
//	0x40 : Middle
//	0x60 : High
#define CSPEC_NSF				0x40

// Setting file
#define CSPEC_SETTING_FILE	"/data/vendor/akmd/akmd_set.txt"
#define CSPEC_PDC_FILE		"/data/vendor/akmd/pdc.txt"

// DOEPlus(software softiron distortion compensation) Enable(1)/Disable(0)
#define CSPEC_ENABLE_DOEPLUS	1
#endif //AKMD_INC_CUSTOMERSPEC_H

