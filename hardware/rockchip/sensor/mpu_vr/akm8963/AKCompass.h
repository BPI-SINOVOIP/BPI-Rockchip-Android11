/******************************************************************************
 *
 * $Id: AKCompass.h 304 2011-08-12 07:52:29Z kihara.gb $
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
#ifndef AKMD_INC_AKCOMPASS_H
#define AKMD_INC_AKCOMPASS_H

#include "AKCommon.h"
#include "CustomerSpec.h"
#include "AK8963Driver.h" // for using BYTE

//**************************************
// Include files for AK8963  library.
//**************************************
#include "AK8963.h"
#include "AKConfigure.h"
#include "AKMDevice.h"
#include "AKCertification.h"
#include "AKDirection6D.h"
#include "AKHDOE.h"
#include "AKHFlucCheck.h"
#include "AKManualCal.h"
#include "AKVersion.h"

/*** Constant definition ******************************************************/
#define	THETAFILTER_SCALE	4128
#define	HFLUCV_TH		2500

#define	OUTBIT_14		0
#define	OUTBIT_16		1
#define	OUTBIT_INVALID		-1

/*** Type declaration *********************************************************/
typedef enum _AKMD_PATNO {
	PAT_INVALID = 0,
	PAT1,
	PAT2,
	PAT3,
	PAT4,
	PAT5,
	PAT6,
	PAT7,
	PAT8
} AKMD_PATNO;

/*! A parameter structure which is needed for HDOE and Direction calculation. */
typedef struct _AK8963PRMS{

	// Variables for magnetic sensor.
	int16vec	m_ho;
	int16vec	HSUC_HO[CSPEC_NUM_FORMATION];
	int32vec	m_ho32;
	int16vec	m_hs;
	int16vec	HFLUCV_HREF[CSPEC_NUM_FORMATION];
	AKSC_HFLUCVAR	m_hflucv;

	// Variables for Decomp8963.
	int16vec	m_hdata[AKSC_HDATA_SIZE];
	int16		m_hn;		// Number of acquired data
	int16vec	m_hvec;		// Averaged value
	int16vec	m_asa;

	// Variables for HDOE.
	AKSC_HDOEVAR	m_hdoev;
	AKSC_HDST	m_hdst;
	AKSC_HDST	HSUC_HDST[CSPEC_NUM_FORMATION];

	// Variables for formation change
	int16		m_form;
	int16		m_cntSuspend;

	// Variables for Direction6D.
	int16		m_ds3Ret;
	int16		m_hnave;
	int16vec	m_dvec;
	int16		m_theta;
	int16		m_delta;
	int16		m_hr;
	int16		m_hrhoriz;
	int16		m_ar;
	int16		m_phi180;
	int16		m_phi90;
	int16		m_eta180;
	int16		m_eta90;
	I16MATRIX	m_mat;
	I16QUAT		m_quat;

	// Variables for acceleration sensor.
	int16vec	m_avec;

	I16MATRIX	m_hlayout[CSPEC_NUM_FORMATION];
	I16MATRIX	m_alayout[CSPEC_NUM_FORMATION];
	AKMD_PATNO	m_layout;

	// Variables for decimation.
	int16		m_callcnt;

        // Variables for outbit.
	int16		m_outbit;

	// Ceritication
	uint8		m_licenser[AKSC_CI_MAX_CHARSIZE+1];	//end with '\0'
	uint8		m_licensee[AKSC_CI_MAX_CHARSIZE+1];	//end with '\0'
	int16		m_key[AKSC_CI_MAX_KEYSIZE];

	// base
	int32vec	m_hbase;
	int32vec	HSUC_HBASE[CSPEC_NUM_FORMATION];

} AK8963PRMS;


/*** Global variables *********************************************************/

/*** Prototype of function ****************************************************/

#endif //AKMD_INC_AKCOMPASS_H

