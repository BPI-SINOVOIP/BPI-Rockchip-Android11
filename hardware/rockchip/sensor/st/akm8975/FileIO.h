/******************************************************************************
 *
 * $Id: FileIO.h 177 2010-02-26 03:01:09Z rikita $
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
#ifndef AKMD_INC_FILEIO_H
#define AKMD_INC_FILEIO_H

// Common include files.
#include "AKCommon.h"

// Include file for AK8975 library.
#include "AKCompass.h"

/*** Constant definition ******************************************************/
#define SETTING_FILE_NAME	"/data/misc/akmd_set.txt"
#define DELIMITER           " = "

/*** Type declaration *********************************************************/

/*** Global variables *********************************************************/

/*** Prototype of function ****************************************************/
int16 LoadParameters(AK8975PRMS * prms);

int16 LoadInt16(FILE * lpFile,
				const char *lpKeyName,
				int16 * val);

int16 LoadInt16vec(FILE * lpFile, 
				   const char *lpKeyName,
				   int16vec * vec);

int16 SaveParameters(AK8975PRMS * prms);

int16 SaveInt16(FILE * lpFile, 
				const char *lpKeyName,
				const int16 val);

int16 SaveInt16vec(FILE * lpFile,
				   const char *lpKeyName,
				   const int16vec * vec);

#endif
