/******************************************************************************
 *
 *  $Id: Acc_aot.h 303 2011-08-12 04:22:45Z kihara.gb $
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
#ifndef AKMD_INC_ACCAOT_H
#define AKMD_INC_ACCAOT_H

#include "AK8963Driver.h"

/*** Constant definition ******************************************************/

/*** Type declaration *********************************************************/

/*** Global variables *********************************************************/

/*** Prototype of function ****************************************************/
int16_t AOT_InitDevice(void);
void	AOT_DeinitDevice(void);
int16_t AOT_GetAccelerationData(int16_t data[3]);

#endif //AKSC_INC_ACCADXL34X_H

