/******************************************************************************
 *
 * $Id: AKCommon.h 185 2010-03-02 08:40:36Z rikita $
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
#ifndef AKMD_INC_AKCOMMON_H
#define AKMD_INC_AKCOMMON_H

#include <stdio.h>
#include <string.h>    //memset
#include <unistd.h>
#include <stdarg.h>    //va_list
/*** Constant definition ******************************************************/
#undef LOG_TAG
#define LOG_TAG "AKMD2"
//#define LOGD(format, ...)  printf(format, ##__VA_ARGS__);

//#define ENABLE_DEBUG_LOG
#include "custom_log.h"

//#define ENABLE_VERBOSE_LOG
#ifdef ENABLE_VERBOSE_LOG
#define V(x...) D(x)
#else
#define V(x...)
#endif

#define DBG_LEVEL0   0x0001	// Critical
#define DBG_LEVEL1   0x0002	// Notice
#define DBG_LEVEL2   0x0003	// Information
#define DBG_LEVEL3   0x0004	// Debug
#define DBGFLAG      DBG_LEVEL2
// #define DBGFLAG      DBG_LEVEL3


/*
 * Debug print macro.
 */
#if 0
// #ifndef DBGPRINT
#undef DBGPRINT
#define DBGPRINT(level, format, ...) \
    ((((level) != 0) && ((level) <= DBGFLAG))  \
     ? (fprintf(stdout, (format), ##__VA_ARGS__)) \
     : (void)0)

/*
#define DBGPRINT(level, format, ...)                                  \
    LOGV_IF(((level) != 0) && ((level) <= DBGFLAG), (format), ##__VA_ARGS__)
*/
#else

#undef DBGPRINT
#define DBGPRINT(level, format, ...)                                  \
    ((((level) != 0) && ((level) <= DBGFLAG))  \
     ? (D(format, ##__VA_ARGS__) ) \
     : (void)0)
#endif


#endif //INC_AKCOMMON_H

