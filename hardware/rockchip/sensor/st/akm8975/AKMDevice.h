/******************************************************************************
 *
 *  Workfile: AKMDevice.h 
 *
 *  Author: Rikita 
 *  Date: 09/12/16 19:47 
 *  Revision: 40 
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
#ifndef AKSC_INC_AKMDEVICE_H
#define AKSC_INC_AKMDEVICE_H

#include "AKConfigure.h"

//========================= Compiler configuration ======================//
#ifdef AKSC_ARITHMETIC_CAST
#define I32MUL(x,y)	((int32)(x)*(int32)(y))
#define UI32MUL(x,y) ((uint32)(x)*(uint32)(y))
#else
#define I32MUL(x,y)	((x)*(y))
#define UI32MUL(x,y) ((x)*(y))
#endif

//========================= Constant definition =========================//
#define AKSC_HDATA_SIZE		32

//========================= Type declaration  ===========================//
typedef	unsigned char	uint8;	
typedef	short           int16;
typedef long            int32;
typedef	unsigned short	uint16;
typedef unsigned long	uint32;

//========================= Vectors =====================================//
typedef union _int16vec{ // Three-dimensional vector constructed of signed 16 bits fixed point numbers
	struct {
		int16	x;
		int16	y;
		int16	z;
	}u;
	int16	v[3];
} int16vec;

typedef union _int32vec{ // Three-dimensional vector constructed of signed 32 bits fixed point numbers
	struct{
		int32	x;
		int32	y;
		int32	z;
	} u;
	int32 v[3];
} int32vec;

//========================= Matrix ======================================//
typedef union _I16MATRIX{
	struct {
		int16	_11, _12, _13;
		int16	_21, _22, _23;
		int16	_31, _32, _33;
	}u;
	int16	m[3][3];
} I16MATRIX;

#endif

