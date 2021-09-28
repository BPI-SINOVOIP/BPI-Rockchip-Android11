/******************************************************************************
 *
 * $Id: misc.h 185 2010-03-02 08:40:36Z rikita $
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
#ifndef AKMD_INC_MISC_H
#define AKMD_INC_MISC_H

#include "AKCompass.h"

/*** Constant definition ******************************************************/
#define AKMD_FORM0	0
#define AKMD_FORM1	1

/*** Type declaration *********************************************************/
typedef enum _AKMD_CNTLCODE {
	AKKEY_NONE = 0, 
	AKKEY_STOP_MEASURE,
} AKMD_CNTLCODE;

typedef struct _AKMD_INTERVAL {
	int32 interval;				/*!< Measurement interval */
	int16 decimator;			/*!< HDOE decimator */  // .Q :
} AKMD_INTERVAL;

/*** Global variables *********************************************************/

/*** Prototype of Function  ***************************************************/
void msleep(signed int msec);	/*! sleep function in millisecond */

int16 openKey(void);			/*!< Open keyboard (used with Console mode) */
void closeKey(void);			/*!< Close keyboard */
int16 checkKey(void);			/*!< Cehck current key */

int16 openFormation(void);		/*!< Open device driver which detects current 
								 formation */
void closeFormation(void);		/*!< Close device driver */
int16 getFormation(void);		/*!< Get current formation */

int16 GetValidInterval(const int32 request,		/*!< Request of interval in
												 microsecond */
					   AKMD_INTERVAL* interval	/*!< Actual interval */
					   );

#endif //AKMD_INC_MISC_H


