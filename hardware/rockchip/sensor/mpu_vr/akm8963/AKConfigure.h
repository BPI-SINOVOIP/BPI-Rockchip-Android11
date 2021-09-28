/******************************************************************************
 *
 *  $Id: AKConfigure.h 361 2011-07-27 09:27:24Z yamada.rj $
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

