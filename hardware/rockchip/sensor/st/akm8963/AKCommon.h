/******************************************************************************
 *
 * $Id: AKCommon.h 448 2011-12-15 11:19:33Z yamada.rj $
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
#ifndef AKMD_INC_AKCOMMON_H
#define AKMD_INC_AKCOMMON_H

#include <stdio.h>     //frpintf
#include <stdlib.h>    //atoi
#include <string.h>    //memset
#include <unistd.h>
#include <stdarg.h>    //va_list
#include <utils/Log.h> //LOGV
#include <errno.h>     //errno
#include "custom_log.h"


/*** Constant definition ******************************************************/
#undef LOG_TAG
#define LOG_TAG "AKMD2"

#define DBG_LEVEL0	0	// Critical
#define DBG_LEVEL1	1	// Notice
#define DBG_LEVEL2	2	// Information
#define DBG_LEVEL3	3	// Debug
#define DBG_LEVEL4	4	// Verbose

#ifndef DBG_LEVEL
#define DBG_LEVEL	DBG_LEVEL0
#endif

#define DATA_AREA01	0x0001
#define DATA_AREA02	0x0002
#define DATA_AREA03	0x0004
#define DATA_AREA04	0x0008
#define DATA_AREA05	0x0010
#define DATA_AREA06	0x0020
#define DATA_AREA07	0x0040
#define DATA_AREA08	0x0080
#define DATA_AREA09	0x0100
#define DATA_AREA10	0x0200
#define DATA_AREA11	0x0400
#define DATA_AREA12	0x0800
#define DATA_AREA13	0x1000
#define DATA_AREA14	0x2000
#define DATA_AREA15	0x4000
#define DATA_AREA16	0x8000


/* Debug area definition */
#define AKMDATA_BDATA		DATA_AREA01	/*<! AK8963's BDATA */
#define AKMDATA_AVEC		DATA_AREA02	/*<! Acceleration data */
#define AKMDATA_EXECTIME	DATA_AREA03	/*<! Time of each loop cycle */
#define AKMDATA_EXECFLAG	DATA_AREA04	/*<! Execution flags */
#define AKMDATA_MAGDRV		DATA_AREA05	/*<! AK8963 driver's data */
#define AKMDATA_ACCDRV		DATA_AREA06	/*<! Acceleration driver's data */
#define AKMDATA_GETINTERVAL	DATA_AREA07	/*<! Interval */
#define AKMDATA_D6D			DATA_AREA08 /*<! Direction6D */

#ifndef ENABLE_AKMDEBUG
#define ENABLE_AKMDEBUG		0	/* Eanble debug output when it is 1. */
#endif

#ifndef OUTPUT_STDOUT
#define OUTPUT_STDOUT		0	/* Output to stdout when it is 1. */
#endif

/***** Debug output ******************************************/
#if ENABLE_AKMDEBUG
#if OUTPUT_STDOUT
#define AKMDEBUG(level, format, ...) \
    (((level) <= DBG_LEVEL) \
	  ? (fprintf(stdout, (format), ##__VA_ARGS__)) \
	  : ((void)0))
#else
#define AKMDEBUG(level, format, ...) \
	ALOGD_IF(((level) <= DBG_LEVEL), (format), ##__VA_ARGS__)
#endif
#else
#define AKMDEBUG(level, format, ...)
#endif

/***** Dbg Area Output ***************************************/
#if ENABLE_AKMDEBUG
#define AKMDATA(flag, format, ...)  \
	((((int)flag) & g_dbgzone) \
	  ? (fprintf(stdout, (format), ##__VA_ARGS__)) \
	  : ((void)0))
#else
#define AKMDATA(flag, format, ...)
#endif

/***** Data output *******************************************/
#if OUTPUT_STDOUT
#define AKMDUMP(format, ...) \
	fprintf(stderr, (format), ##__VA_ARGS__)
#else
#define AKMDUMP(format, ...) \
	ALOGD((format), ##__VA_ARGS__)
#endif

/***** Log output ********************************************/
#ifdef AKM_LOG_ENABLE
#define AKM_LOG(format, ...)	ALOGD((format), ##__VA_ARGS__)
#else
#define AKM_LOG(format, ...)
#endif

/***** Error output *******************************************/
#define AKMERROR \
	((g_opmode == 0) \
	  ? (ALOGE("%s:%d Error.", __FUNCTION__, __LINE__)) \
	  : (fprintf(stderr, "%s:%d Error.\n", __FUNCTION__, __LINE__)))

#define AKMERROR_STR(api) \
	((g_opmode == 0) \
	  ? (ALOGE("%s:%d %s Error (%s).", \
	  		  __FUNCTION__, __LINE__, (api), strerror(errno))) \
	  : (fprintf(stderr, "%s:%d %s Error (%s).\n", \
	  		  __FUNCTION__, __LINE__, (api), strerror(errno))))

/*** Type declaration *********************************************************/

/*** Global variables *********************************************************/
extern int g_stopRequest;	/*!< 0:Not stop,  1:Stop */
extern int g_opmode;		/*!< 0:Daemon mode, 1:Console mode. */
extern int g_dbgzone;		/*!< Debug zone. */

/*** Prototype of function ****************************************************/

#endif //AKMD_INC_AKCOMMON_H

