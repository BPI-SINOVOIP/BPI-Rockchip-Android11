/******************************************************************************
 *
 *  $Id: AKMDevice.h 92 2012-11-30 11:54:41Z yamada.rj $
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

typedef union _I16QUAT{
	struct {
		int16		w;
		int16		x;
		int16		y;
		int16		z;
	}u;
	int16	q[4];
} I16QUAT;

#endif

