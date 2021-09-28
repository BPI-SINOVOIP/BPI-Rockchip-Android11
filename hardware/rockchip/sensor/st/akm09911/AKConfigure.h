/******************************************************************************
 *
 *  $Id: AKConfigure.h 92 2012-11-30 11:54:41Z yamada.rj $
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
#ifndef AKSC_INC_AKCONFIG_H
#define AKSC_INC_AKCONFIG_H

//========================= Language configuration ===================//
#if defined(__cplusplus)
#define AKLIB_C_API_START	extern "C" {
#define AKLIB_C_API_END		}
#else
#define AKLIB_C_API_START
#define AKLIB_C_API_END
#endif

//========================= Debug configuration ======================//
#if defined(_DEBUG) || defined(DEBUG)
#define AKSC_TEST_MODE
#endif
#if defined(AKSC_TEST_MODE)
#define STATIC
#else
#define STATIC	static
#endif

//========================= Arithmetic Cast ==========================//
#define AKSC_ARITHMETIC_CAST


#endif

