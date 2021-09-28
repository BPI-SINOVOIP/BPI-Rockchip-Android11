/******************************************************************************
 *
 * $Id: Measure.c 327 2011-08-25 06:00:17Z yamada.rj $
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
#include "AKCommon.h"
#include "AK8963Driver.h"
#include "Measure.h"
#include "TestLimit.h"
#include "FileIO.h"
#include "DispMessage.h"
#include "misc.h"

extern int s_fdDev;



#define ACC_ACQ_FLAG_POS	ACC_DATA_FLAG
#define MAG_ACQ_FLAG_POS	MAG_DATA_FLAG
#define ORI_ACQ_FLAG_POS	ORI_DATA_FLAG
#define ACC_MES_FLAG_POS	8
#define ACC_INT_FLAG_POS	9
#define MAG_MES_FLAG_POS	10
#define MAG_INT_FLAG_POS	11
#define SETTING_FLAG_POS	12

/* deg x (pi/180.0) */
#define DEG2RAD(deg)		((deg) * 0.017453292)

static FORM_CLASS* g_form = NULL;

/*!
 This function open formation status device.
 @return Return 0 on success. Negative value on fail.
 */
static int16 openForm(void)
{
	if (g_form != NULL) {
		if (g_form->open != NULL) {
			return g_form->open();
		}
	}
	// If function is not set, return success.
	return 0;
}

/*!
 This function close formation status device.
 @return None.
 */
static void closeForm(void)
{
	if (g_form != NULL) {
		if (g_form->close != NULL) {
			g_form->close();
		}
	}
}

/*!
 This function check formation status
 @return The index of formation.
 */
static int16 checkForm(void)
{
	if (g_form != NULL) {
		if (g_form->check != NULL) {
			return g_form->check();
		}
	}
	// If function is not set, return default value.
	return 0;
}

/*!
 This function registers the callback function.
 @param[in]
 */
void RegisterFormClass(FORM_CLASS* pt)
{
	g_form = pt;
}

/*!
 Initialize #AK8963PRMS structure. At first, 0 is set to all parameters.
 After that, some parameters, which should not be 0, are set to specific
 value. Some of initial values can be customized by editing the file
 \c "CustomerSpec.h".
 @param[out] prms A pointer to #AK8963PRMS structure.
 */
void InitAK8963PRMS(AK8963PRMS* prms)
{
	int i,j,k;
	struct akm_platform_data pdata;
	// Set 0 to the AK8963PRMS structure.
	memset(prms, 0, sizeof(AK8963PRMS));

	// Sensitivity
	prms->m_hs.u.x = AKSC_HSENSE_TARGET;
	prms->m_hs.u.y = AKSC_HSENSE_TARGET;
	prms->m_hs.u.z = AKSC_HSENSE_TARGET;

	// HDOE
	prms->m_hdst = AKSC_HDST_UNSOLVED;

	// (m_hdata is initialized with AKSC_InitDecomp8963)
	prms->m_hnave = CSPEC_HNAVE;
	prms->m_dvec.u.x = CSPEC_DVEC_X;
	prms->m_dvec.u.y = CSPEC_DVEC_Y;
	prms->m_dvec.u.z = CSPEC_DVEC_Z;	
	
	if (ioctl(s_fdDev, ECS_IOCTL_GET_PLATFORM_DATA, &pdata) >= 0) 
	{
		// Form 0
		prms->m_hlayout[0].u._11 = pdata.m_layout[0][0][0];
		prms->m_hlayout[0].u._12 = pdata.m_layout[0][0][1];
		prms->m_hlayout[0].u._13 = pdata.m_layout[0][0][2];
		prms->m_hlayout[0].u._21 = pdata.m_layout[0][1][0];
		prms->m_hlayout[0].u._22 = pdata.m_layout[0][1][1];
		prms->m_hlayout[0].u._23 = pdata.m_layout[0][1][2];
		prms->m_hlayout[0].u._31 = pdata.m_layout[0][2][0];
		prms->m_hlayout[0].u._32 = pdata.m_layout[0][2][1];
		prms->m_hlayout[0].u._33 = pdata.m_layout[0][2][2];
		
		// Form 1
		prms->m_hlayout[1].u._11 = pdata.m_layout[1][0][0];
		prms->m_hlayout[1].u._12 = pdata.m_layout[1][0][1];
		prms->m_hlayout[1].u._13 = pdata.m_layout[1][0][2];
		prms->m_hlayout[1].u._21 = pdata.m_layout[1][1][0];
		prms->m_hlayout[1].u._22 = pdata.m_layout[1][1][1];
		prms->m_hlayout[1].u._23 = pdata.m_layout[1][1][2];
		prms->m_hlayout[1].u._31 = pdata.m_layout[1][2][0];
		prms->m_hlayout[1].u._32 = pdata.m_layout[1][2][1];
		prms->m_hlayout[1].u._33 = pdata.m_layout[1][2][2];
		
		
		//***********************************************
		// Set ALAYOUT
		//***********************************************
		
		// Form 0
		prms->m_alayout[0].u._11 = pdata.m_layout[2][0][0];
		prms->m_alayout[0].u._12 = pdata.m_layout[2][0][1];
		prms->m_alayout[0].u._13 = pdata.m_layout[2][0][2];
		prms->m_alayout[0].u._21 = pdata.m_layout[2][1][0];
		prms->m_alayout[0].u._22 = pdata.m_layout[2][1][1];
		prms->m_alayout[0].u._23 = pdata.m_layout[2][1][2];
		prms->m_alayout[0].u._31 = pdata.m_layout[2][2][0];
		prms->m_alayout[0].u._32 = pdata.m_layout[2][2][1];
		prms->m_alayout[0].u._33 = pdata.m_layout[2][2][2];

		// Form 1
		prms->m_alayout[1].u._11 = pdata.m_layout[3][0][0];
		prms->m_alayout[1].u._12 = pdata.m_layout[3][0][1];
		prms->m_alayout[1].u._13 = pdata.m_layout[3][0][2];
		prms->m_alayout[1].u._21 = pdata.m_layout[3][1][0];
		prms->m_alayout[1].u._22 = pdata.m_layout[3][1][1];
		prms->m_alayout[1].u._23 = pdata.m_layout[3][1][2];
		prms->m_alayout[1].u._31 = pdata.m_layout[3][2][0];
		prms->m_alayout[1].u._32 = pdata.m_layout[3][2][1];
		prms->m_alayout[1].u._33 = pdata.m_layout[3][2][2];
		
		for(i=0; i<4; i++)
		for(j=0; j<3; j++)
		for(k=0; k<3; k++)
		{
			AKM_LOG("%s:m_layout[%d][%d][%d]=%d\n",__FUNCTION__,i,j,k,pdata.m_layout[i][j][k]);
		}
			
	}
	else
	{
		//***********************************************
		// Set HLAYOUT
		//***********************************************
		LOGD("%s:%s\n",__FUNCTION__,strerror(errno));
		// Form 0
		prms->m_hlayout[0].u._11 = CSPEC_FORM0_HLAYOUT_11;
		prms->m_hlayout[0].u._12 = CSPEC_FORM0_HLAYOUT_12;
		prms->m_hlayout[0].u._13 = CSPEC_FORM0_HLAYOUT_13;
		prms->m_hlayout[0].u._21 = CSPEC_FORM0_HLAYOUT_21;
		prms->m_hlayout[0].u._22 = CSPEC_FORM0_HLAYOUT_22;
		prms->m_hlayout[0].u._23 = CSPEC_FORM0_HLAYOUT_23;
		prms->m_hlayout[0].u._31 = CSPEC_FORM0_HLAYOUT_31;
		prms->m_hlayout[0].u._32 = CSPEC_FORM0_HLAYOUT_32;
		prms->m_hlayout[0].u._33 = CSPEC_FORM0_HLAYOUT_33;
		
		// Form 1
		prms->m_hlayout[1].u._11 = CSPEC_FORM1_HLAYOUT_11;
		prms->m_hlayout[1].u._12 = CSPEC_FORM1_HLAYOUT_12;
		prms->m_hlayout[1].u._13 = CSPEC_FORM1_HLAYOUT_13;
		prms->m_hlayout[1].u._21 = CSPEC_FORM1_HLAYOUT_21;
		prms->m_hlayout[1].u._22 = CSPEC_FORM1_HLAYOUT_22;
		prms->m_hlayout[1].u._23 = CSPEC_FORM1_HLAYOUT_23;
		prms->m_hlayout[1].u._31 = CSPEC_FORM1_HLAYOUT_31;
		prms->m_hlayout[1].u._32 = CSPEC_FORM1_HLAYOUT_32;
		prms->m_hlayout[1].u._33 = CSPEC_FORM1_HLAYOUT_33;
		
		
		//***********************************************
		// Set ALAYOUT
		//***********************************************
		
		// Form 0
		prms->m_alayout[0].u._11 = CSPEC_FORM0_ALAYOUT_11;
		prms->m_alayout[0].u._12 = CSPEC_FORM0_ALAYOUT_12;
		prms->m_alayout[0].u._13 = CSPEC_FORM0_ALAYOUT_13;
		prms->m_alayout[0].u._21 = CSPEC_FORM0_ALAYOUT_21;
		prms->m_alayout[0].u._22 = CSPEC_FORM0_ALAYOUT_22;
		prms->m_alayout[0].u._23 = CSPEC_FORM0_ALAYOUT_23;
		prms->m_alayout[0].u._31 = CSPEC_FORM0_ALAYOUT_31;
		prms->m_alayout[0].u._32 = CSPEC_FORM0_ALAYOUT_32;
		prms->m_alayout[0].u._33 = CSPEC_FORM0_ALAYOUT_33;

		// Form 1
		prms->m_alayout[1].u._11 = CSPEC_FORM1_ALAYOUT_11;
		prms->m_alayout[1].u._12 = CSPEC_FORM1_ALAYOUT_12;
		prms->m_alayout[1].u._13 = CSPEC_FORM1_ALAYOUT_13;
		prms->m_alayout[1].u._21 = CSPEC_FORM1_ALAYOUT_21;
		prms->m_alayout[1].u._22 = CSPEC_FORM1_ALAYOUT_22;
		prms->m_alayout[1].u._23 = CSPEC_FORM1_ALAYOUT_23;
		prms->m_alayout[1].u._31 = CSPEC_FORM1_ALAYOUT_31;
		prms->m_alayout[1].u._32 = CSPEC_FORM1_ALAYOUT_32;
		prms->m_alayout[1].u._33 = CSPEC_FORM1_ALAYOUT_33;
	}
	
	
}

/*!
 Fill #AK8963PRMS structure with default value.
 @param[out] prms A pointer to #AK8963PRMS structure.
 */
void SetDefaultPRMS(AK8963PRMS* prms)
{
	int16 i;
	// Set parameter to HDST, HO, HREF
	for (i = 0; i < CSPEC_NUM_FORMATION; i++) {
		prms->HSUC_HDST[i] = AKSC_HDST_UNSOLVED;
		prms->HSUC_HO[i].u.x = 0;
		prms->HSUC_HO[i].u.y = 0;
		prms->HSUC_HO[i].u.z = 0;
		prms->HFLUCV_HREF[i].u.x = 0;
		prms->HFLUCV_HREF[i].u.y = 0;
		prms->HFLUCV_HREF[i].u.z = 0;
		prms->HSUC_HBASE[i].u.x = 0;
		prms->HSUC_HBASE[i].u.y = 0;
		prms->HSUC_HBASE[i].u.z = 0;
	}
}

/*!
 Get interval from device driver. This function will not resolve dependencies.
 Dependencies will be resolved in Sensor HAL.
 @param[out] mag_acq Magnetometer acquisition timing.
 @param[out] acc_acq Accelerometer acquisition timing.
 @param[out] ori_acq Orientation sensor acquisition timing.
 @param[out] mag_mes Magnetometer measurement timing.
 @param[out] acc_mes Accelerometer measurement timing.
 @param[out] hdoe_interval HDOE decimator.
 */
int16 GetInterval(
	AKMD_LOOP_TIME* acc_acq,
	AKMD_LOOP_TIME* mag_acq,
	AKMD_LOOP_TIME* ori_acq,
	AKMD_LOOP_TIME* mag_mes,
	AKMD_LOOP_TIME* acc_mes,
	int16* hdoe_dec)
{
	/* Accelerometer, Magnetometer, Orientation */
	/* Delay is in nano second unit. */
	/* Negative value means the sensor is disabled.*/
	int64_t delay[AKM_NUM_SENSORS];

	if (AKD_GetDelay(delay) < 0) {
		return AKRET_PROC_FAIL;
	}
	AKMDATA(AKMDATA_GETINTERVAL,"delay=%lld,%lld,%lld\n",
		delay[0], delay[1], delay[2]);

#ifdef AKMD_ACC_COMBINED
	/* Accelerometer's interval limit */
	if ((0 <= delay[0]) && (delay[0] <= AKMD_ACC_MIN_INTERVAL)) {
		delay[0] = AKMD_ACC_MIN_INTERVAL;
	}
#else
	/* Always disabled */
	delay[0] = -1;
#endif
	/* Magnetmeter's frequency should be discrete value */
	if ((0 <= delay[1]) && (delay[1] <= AKMD_MAG_MIN_INTERVAL)) {
		delay[1] = AKMD_MAG_MIN_INTERVAL;
	}
	/* Orientation sensor's interval limit */
	if ((0 <= delay[2]) && (delay[2] <= AKMD_ORI_MIN_INTERVAL)) {
		delay[2] = AKMD_ORI_MIN_INTERVAL;
	}
	/* update */
	if ((delay[0] != acc_acq->interval) ||
		(delay[1] != mag_acq->interval) ||
		(delay[2] != ori_acq->interval)) {
		acc_acq->duration = acc_acq->interval = delay[0];
		mag_acq->duration = mag_acq->interval = delay[1];
		ori_acq->duration = ori_acq->interval = delay[2];

		mag_mes->interval = mag_acq->interval;
		mag_mes->duration = 0;

		// Adjust frequency for HDOE
		if (0 <= (mag_mes->interval)) {
			GetHDOEDecimator(&(mag_mes->interval), hdoe_dec);
		}
#ifndef AKMD_ACC_COMBINED
		// Solve dependencies
		if (ori_acq->interval >= 0) {
			// Orientation is enabled
			acc_mes->interval = ori_acq->interval;
			acc_mes->duration = 0;
		} else {
			// Both are disabled
			acc_mes->interval = -1;
			acc_mes->duration = 0;
		}
#endif

		AKMDATA(AKMDATA_GETINTERVAL,
				 "%s:\n"
				 "  AcqInterval(M,A,O,G)=%lld,%lld,%lld\n"
				 "  MesInterval(M,A)=%lld,%lld\n",
				 __FUNCTION__,
				 mag_acq->interval, acc_acq->interval, ori_acq->interval,
				 mag_mes->interval, acc_mes->interval);
	}

	return AKRET_PROC_SUCCEED;
}

/*!
 Calculate loop duration
 @return If it is time to fire the event, the return value is 1, otherwise 0.
 @param[in,out] tm An event.
 @param[in] execTime The time to execute main loop for one time.
 @param[in,out] minDuration The minimum sleep time in all events.
 */
int SetLoopTime(
	AKMD_LOOP_TIME* tm,
	int64_t execTime,
	int64_t* minDuration)
{
	int ret = 0;
	if (tm->interval >= 0) {
		tm->duration -= execTime;
		if (tm->duration <= AKMD_LOOP_MARGIN) {
			tm->duration = tm->interval;
			ret = 1;
		} else if (tm->duration < *minDuration) {
			*minDuration = tm->duration;
		}
	}
	return ret;
}

/*!
 Read hard coded value (Fuse ROM) from AK8963. Then set the read value to
 calculation parameter.
 @return If parameters are read successfully, the return value is
 #AKRET_PROC_SUCCEED. Otherwise the return value is #AKRET_PROC_FAIL. No
 error code is reserved to show which operation has failed.
 @param[out] prms A pointer to #AK8963PRMS structure.
 */
int16 ReadAK8963FUSEROM(AK8963PRMS* prms)
{
	BYTE    i2cData[6];

	// Set to PowerDown mode
	if (AKD_SetMode(AK8963_MODE_POWERDOWN) != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}

	// Set to FUSE ROM access mode
	if (AKD_SetMode(AK8963_MODE_FUSE_ACCESS) != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}

	// Read values. ASAX, ASAY, ASAZ
	if (AKD_RxData(AK8963_FUSE_ASAX, i2cData, 3) != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}
	prms->m_asa.u.x = (int16)i2cData[0];
	prms->m_asa.u.y = (int16)i2cData[1];
	prms->m_asa.u.z = (int16)i2cData[2];

	AKMDEBUG(DBG_LEVEL2, "%s: asa(dec)=%d,%d,%d\n", __FUNCTION__,
			 prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z);

	// Set keywords for SmartCompassLibrary certification
	prms->m_key[2] = (int16)i2cData[0];
	prms->m_key[3] = (int16)i2cData[1];
	prms->m_key[4] = (int16)i2cData[2];

	// Set to PowerDown mode
	if (AKD_SetMode(AK8963_MODE_POWERDOWN) != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}

	// Set keywords for SmartCompassLibrary certification
	if (AKD_RxData(AK8963_REG_WIA, i2cData, 1) != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}
	prms->m_key[0] = CSPEC_CI_AK_DEVICE;
	prms->m_key[1] = (int16)i2cData[0];
	strncpy((char *)prms->m_licenser, CSPEC_CI_LICENSER, AKSC_CI_MAX_CHARSIZE);
	strncpy((char *)prms->m_licensee, CSPEC_CI_LICENSEE, AKSC_CI_MAX_CHARSIZE);

	AKMDEBUG(DBG_LEVEL2, "%s: key=%d, licenser=%s, licensee=%s\n",
			 __FUNCTION__, prms->m_key[1], prms->m_licenser, prms->m_licensee);

	return AKRET_PROC_SUCCEED;
}


/*!
 Set initial values to registers of AK8963. Then initialize algorithm
 parameters.
 @return If parameters are read successfully, the return value is
 #AKRET_PROC_SUCCEED. Otherwise the return value is #AKRET_PROC_FAIL. No
 error code is reserved to show which operation has failed.
 @param[in,out] prms A pointer to a #AK8963PRMS structure.
 */
int16 InitAK8963_Measure(AK8963PRMS* prms)
{
	// Reset device.
	if (AKD_ResetAK8963() != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}

	prms->m_form = checkForm();

	// Restore the value when succeeding in estimating of HOffset.
	prms->m_ho   = prms->HSUC_HO[prms->m_form];
	prms->m_ho32.u.x = (int32)prms->HSUC_HO[prms->m_form].u.x;
	prms->m_ho32.u.y = (int32)prms->HSUC_HO[prms->m_form].u.y;
	prms->m_ho32.u.z = (int32)prms->HSUC_HO[prms->m_form].u.z;

	prms->m_hdst = prms->HSUC_HDST[prms->m_form];
	prms->m_hbase = prms->HSUC_HBASE[prms->m_form];

	// Initialize the decompose parameters
	AKSC_InitDecomp8963(prms->m_hdata);

	// Initialize HDOE parameters
	AKSC_InitHDOEProcPrmsS3(
							&prms->m_hdoev,
							1,
							&prms->m_ho,
							prms->m_hdst
							);

	AKSC_InitHFlucCheck(
						&(prms->m_hflucv),
						&(prms->HFLUCV_HREF[prms->m_form]),
						HFLUCV_TH
						);

	// Reset counter
	prms->m_cntSuspend = 0;
	prms->m_callcnt = 0;

	LOGD("%s: m_form=%d", __FUNCTION__, prms->m_form);

	return AKRET_PROC_SUCCEED;
}


/*!
 Execute "Onboard Function Test" (includes "START" and "END" command).
 @retval 1 The test is passed successfully.
 @retval -1 The test is failed.
 @retval 0 The test is aborted by kind of system error.
 @param[in] prms A pointer to a #AK8963PRMS structure.
 */
int16 FctShipmntTest_Body(AK8963PRMS* prms)
{
	int16 pf_total = 1;

	//***********************************************
	//    Reset Test Result
	//***********************************************
	TEST_DATA(NULL, "START", 0, 0, 0, &pf_total);

	//***********************************************
	//    Step 1 to 2
	//***********************************************
	pf_total = FctShipmntTestProcess_Body(prms);

	//***********************************************
	//    Judge Test Result
	//***********************************************
	TEST_DATA(NULL, "END", 0, 0, 0, &pf_total);

	return pf_total;
}

/*!
 Execute "Onboard Function Test" (NOT includes "START" and "END" command).
 @retval 1 The test is passed successfully.
 @retval -1 The test is failed.
 @retval 0 The test is aborted by kind of system error.
 @param[in] prms A pointer to a #AK8963PRMS structure.
 */
int16 FctShipmntTestProcess_Body(AK8963PRMS* prms)
{
	int16   pf_total;  //p/f flag for this subtest
	BYTE    i2cData[16];
	int16   hdata[3];
	int16   asax;
	int16   asay;
	int16   asaz;

	//***********************************************
	//  Reset Test Result
	//***********************************************
	pf_total = 1;

	//***********************************************
	//  Step1
	//***********************************************

	// Reset device.
	if (AKD_ResetAK8963() != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// When the serial interface is SPI,
	// write "00011011" to I2CDIS register(to disable I2C,).
	if (CSPEC_SPI_USE == 1) {
		i2cData[0] = 0x1B;
		if (AKD_TxData(AK8963_REG_I2CDIS, i2cData, 1) != AKD_SUCCESS) {
			AKMERROR;
			return 0;
		}
	}

	// Read values from WIA to ASTC.
	if (AKD_RxData(AK8963_REG_WIA, i2cData, 13) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// TEST
	TEST_DATA(TLIMIT_NO_RST_WIA,   TLIMIT_TN_RST_WIA,   (int16)i2cData[0],  TLIMIT_LO_RST_WIA,   TLIMIT_HI_RST_WIA,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_INFO,  TLIMIT_TN_RST_INFO,  (int16)i2cData[1],  TLIMIT_LO_RST_INFO,  TLIMIT_HI_RST_INFO,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_ST1,   TLIMIT_TN_RST_ST1,   (int16)i2cData[2],  TLIMIT_LO_RST_ST1,   TLIMIT_HI_RST_ST1,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HXL,   TLIMIT_TN_RST_HXL,   (int16)i2cData[3],  TLIMIT_LO_RST_HXL,   TLIMIT_HI_RST_HXL,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HXH,   TLIMIT_TN_RST_HXH,   (int16)i2cData[4],  TLIMIT_LO_RST_HXH,   TLIMIT_HI_RST_HXH,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HYL,   TLIMIT_TN_RST_HYL,   (int16)i2cData[5],  TLIMIT_LO_RST_HYL,   TLIMIT_HI_RST_HYL,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HYH,   TLIMIT_TN_RST_HYH,   (int16)i2cData[6],  TLIMIT_LO_RST_HYH,   TLIMIT_HI_RST_HYH,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HZL,   TLIMIT_TN_RST_HZL,   (int16)i2cData[7],  TLIMIT_LO_RST_HZL,   TLIMIT_HI_RST_HZL,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HZH,   TLIMIT_TN_RST_HZH,   (int16)i2cData[8],  TLIMIT_LO_RST_HZH,   TLIMIT_HI_RST_HZH,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_ST2,   TLIMIT_TN_RST_ST2,   (int16)i2cData[9],  TLIMIT_LO_RST_ST2,   TLIMIT_HI_RST_ST2,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_CNTL1, TLIMIT_TN_RST_CNTL1, (int16)i2cData[10], TLIMIT_LO_RST_CNTL1, TLIMIT_HI_RST_CNTL1, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_CNTL2, TLIMIT_TN_RST_CNTL2, (int16)i2cData[11], TLIMIT_LO_RST_CNTL2, TLIMIT_HI_RST_CNTL2, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_ASTC,  TLIMIT_TN_RST_ASTC,  (int16)i2cData[12], TLIMIT_LO_RST_ASTC,  TLIMIT_HI_RST_ASTC,  &pf_total);

	// Read values from I2CDIS.
	if (AKD_RxData(AK8963_REG_I2CDIS, i2cData, 1) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	if (CSPEC_SPI_USE == 1) {
		TEST_DATA(TLIMIT_NO_RST_I2CDIS, TLIMIT_TN_RST_I2CDIS, (int16)i2cData[0], TLIMIT_LO_RST_I2CDIS_USESPI, TLIMIT_HI_RST_I2CDIS_USESPI, &pf_total);
	}else{
		TEST_DATA(TLIMIT_NO_RST_I2CDIS, TLIMIT_TN_RST_I2CDIS, (int16)i2cData[0], TLIMIT_LO_RST_I2CDIS_USEI2C, TLIMIT_HI_RST_I2CDIS_USEI2C, &pf_total);
	}

	// Set to FUSE ROM access mode
	if (AKD_SetMode(AK8963_MODE_FUSE_ACCESS) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// Read values from ASAX to ASAZ
	if (AKD_RxData(AK8963_FUSE_ASAX, i2cData, 3) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	asax = (int16)i2cData[0];
	asay = (int16)i2cData[1];
	asaz = (int16)i2cData[2];

	// TEST
	TEST_DATA(TLIMIT_NO_ASAX, TLIMIT_TN_ASAX, asax, TLIMIT_LO_ASAX, TLIMIT_HI_ASAX, &pf_total);
	TEST_DATA(TLIMIT_NO_ASAY, TLIMIT_TN_ASAY, asay, TLIMIT_LO_ASAY, TLIMIT_HI_ASAY, &pf_total);
	TEST_DATA(TLIMIT_NO_ASAZ, TLIMIT_TN_ASAZ, asaz, TLIMIT_LO_ASAZ, TLIMIT_HI_ASAZ, &pf_total);

	// Read values. CNTL
	if (AKD_RxData(AK8963_REG_CNTL1, i2cData, 1) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// Set to PowerDown mode
	if (AKD_SetMode(AK8963_MODE_POWERDOWN) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// TEST
	TEST_DATA(TLIMIT_NO_WR_CNTL1, TLIMIT_TN_WR_CNTL1, (int16)i2cData[0], TLIMIT_LO_WR_CNTL1, TLIMIT_HI_WR_CNTL1, &pf_total);

	//***********************************************
	//  Step2
	//***********************************************

	// Set to SNG measurement pattern (Set CNTL register)
	if (AKD_SetMode(AK8963_MODE_SNG_MEASURE | (prms->m_outbit << 4)) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// Wait for DRDY pin changes to HIGH.
	// Get measurement data from AK8963
	// ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + ST2
	// = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 = 8 bytes
	if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	hdata[0] = (int16)((((uint16)(i2cData[2]))<<8)+(uint16)(i2cData[1]));
	hdata[1] = (int16)((((uint16)(i2cData[4]))<<8)+(uint16)(i2cData[3]));
	hdata[2] = (int16)((((uint16)(i2cData[6]))<<8)+(uint16)(i2cData[5]));

	if((i2cData[7] & 0x10) == 0){ // 14bit mode
		hdata[0] <<= 2;
		hdata[1] <<= 2;
		hdata[2] <<= 2;
	}	

	// TEST
	TEST_DATA(TLIMIT_NO_SNG_ST1, TLIMIT_TN_SNG_ST1, (int16)i2cData[0], TLIMIT_LO_SNG_ST1, TLIMIT_HI_SNG_ST1, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HX, TLIMIT_TN_SNG_HX, hdata[0], TLIMIT_LO_SNG_HX, TLIMIT_HI_SNG_HX, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HY, TLIMIT_TN_SNG_HY, hdata[1], TLIMIT_LO_SNG_HY, TLIMIT_HI_SNG_HY, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HZ, TLIMIT_TN_SNG_HZ, hdata[2], TLIMIT_LO_SNG_HZ, TLIMIT_HI_SNG_HZ, &pf_total);
	if((i2cData[7] & 0x10) == 0){ // 14bit mode
		TEST_DATA(
			TLIMIT_NO_SNG_ST2,
			TLIMIT_TN_SNG_ST2,
			(int16)i2cData[7],
			TLIMIT_LO_SNG_ST2_14BIT,
			TLIMIT_HI_SNG_ST2_14BIT,
			&pf_total
			);
	} else { // 16bit mode
		TEST_DATA(
			TLIMIT_NO_SNG_ST2,
			TLIMIT_TN_SNG_ST2,
			(int16)i2cData[7],
			TLIMIT_LO_SNG_ST2_16BIT,
			TLIMIT_HI_SNG_ST2_16BIT,
			&pf_total
			);
	}

	// Generate magnetic field for self-test (Set ASTC register)
	i2cData[0] = 0x40;
	if (AKD_TxData(AK8963_REG_ASTC, i2cData, 1) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// Set to Self-test mode (Set CNTL register)
	if (AKD_SetMode(AK8963_MODE_SELF_TEST | (prms->m_outbit << 4)) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// Wait for DRDY pin changes to HIGH.
	// Get measurement data from AK8963
	// ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + ST2
	// = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 = 8Byte
	if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// TEST
	TEST_DATA(TLIMIT_NO_SLF_ST1, TLIMIT_TN_SLF_ST1, (int16)i2cData[0], TLIMIT_LO_SLF_ST1, TLIMIT_HI_SLF_ST1, &pf_total);

	hdata[0] = (int16)((((uint16)(i2cData[2]))<<8)+(uint16)(i2cData[1]));
	hdata[1] = (int16)((((uint16)(i2cData[4]))<<8)+(uint16)(i2cData[3]));
	hdata[2] = (int16)((((uint16)(i2cData[6]))<<8)+(uint16)(i2cData[5]));

	if((i2cData[7] & 0x10) == 0){ // 14bit mode
		hdata[0] <<= 2;
		hdata[1] <<= 2;
		hdata[2] <<= 2;
	}

	// TEST
	TEST_DATA(
			  TLIMIT_NO_SLF_RVHX,
			  TLIMIT_TN_SLF_RVHX,
			  (hdata[0])*((asax - 128)*0.5f/128.0f + 1),
			  TLIMIT_LO_SLF_RVHX,
			  TLIMIT_HI_SLF_RVHX,
			  &pf_total
			  );

	TEST_DATA(
			  TLIMIT_NO_SLF_RVHY,
			  TLIMIT_TN_SLF_RVHY,
			  (hdata[1])*((asay - 128)*0.5f/128.0f + 1),
			  TLIMIT_LO_SLF_RVHY,
			  TLIMIT_HI_SLF_RVHY,
			  &pf_total
			  );

	TEST_DATA(
			  TLIMIT_NO_SLF_RVHZ,
			  TLIMIT_TN_SLF_RVHZ,
			  (hdata[2])*((asaz - 128)*0.5f/128.0f + 1),
			  TLIMIT_LO_SLF_RVHZ,
			  TLIMIT_HI_SLF_RVHZ,
			  &pf_total
			  );

	// TEST
	if((i2cData[7] & 0x10) == 0){ // 14bit mode
		TEST_DATA(
			TLIMIT_NO_SLF_ST2,
			TLIMIT_TN_SLF_ST2,
			(int16)i2cData[7],
			TLIMIT_LO_SLF_ST2_14BIT,
			TLIMIT_HI_SLF_ST2_14BIT,
			&pf_total
			);
	}else{ // 16bit mode
		TEST_DATA(
			TLIMIT_NO_SLF_ST2,
			TLIMIT_TN_SLF_ST2,
			(int16)i2cData[7],
			TLIMIT_LO_SLF_ST2_16BIT,
			TLIMIT_HI_SLF_ST2_16BIT,
			&pf_total
			);
	}

	// Set to Normal mode for self-test.
	i2cData[0] = 0x00;
	if (AKD_TxData(AK8963_REG_ASTC, i2cData, 1) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	return pf_total;
}

/*!
 This is the main routine of measurement.
 @param[in,out] prms A pointer to a #AK8963PRMS structure.
 */
void MeasureSNGLoop(AK8963PRMS* prms)
{
	BYTE    i2cData[AKSC_BDATA_SIZE];
	int16   bData[AKSC_BDATA_SIZE];  // Measuring block data
	int16   ret;
	int16   i;
	int16	hdoe_interval = 1;

	/* Acceleration interval */
	AKMD_LOOP_TIME acc_acq = { -1, 0 };
	/* Magnetic field interval */
	AKMD_LOOP_TIME mag_acq = { -1, 0 };
	/* Orientation interval */
	AKMD_LOOP_TIME ori_acq = { -1, 0 };
	/* Magnetic acquisition interval */
	AKMD_LOOP_TIME mag_mes = { -1, 0 };
	/* Acceleration acquisition interval */
	AKMD_LOOP_TIME acc_mes = { -1, 0 };
	/* Magnetic measurement interval */
	AKMD_LOOP_TIME mag_int = { AK8963_MEASUREMENT_TIME_NS, 0 };
	/* Setting interval */
	AKMD_LOOP_TIME setting = { AKMD_SETTING_INTERVAL, 0 };

	/* 0x0001: Acceleration execute flag (data output) */
	/* 0x0002: Magnetic execute flag (data output) */
	/* 0x0004: Orientation execute flag (data output) */
	/* 0x0100: Magnetic measurement flag */
	/* 0x0200: Magnetic interrupt flag */
	/* 0x0400: Acceleration measurement flag */
	/* 0x0800: Acceleration interrupt flag */
	/* 0x1000: Setting execute flag */
	uint16 exec_flags;

	struct timespec currTime = { 0, 0 }; /* Current time */
	struct timespec lastTime = { 0, 0 }; /* Previous time */
	int64_t execTime; /* Time between two points */
	int64_t minVal; /* The minimum duration to the next event */
	int measuring = 0; /* The value is 1, if while measuring. */

	if (openForm() < 0) {
		AKMERROR;
		return;
	}

	/* Get initial interva */
	if (GetInterval(&acc_acq, &mag_acq, &ori_acq,
					&mag_mes, &acc_mes,
					&hdoe_interval) != AKRET_PROC_SUCCEED) {
		AKMERROR;
		goto MEASURE_SNG_END;
	}

	/* Initialize */
	if (InitAK8963_Measure(prms) != AKRET_PROC_SUCCEED) {
		goto MEASURE_SNG_END;
	}

	/* Beginning time */
	if (clock_gettime(CLOCK_MONOTONIC, &currTime) < 0) {
		AKMERROR;
		goto MEASURE_SNG_END;
	}

	while (g_stopRequest != AKKEY_STOP_MEASURE) {
		exec_flags = 0;
		minVal = 1000000000; /*1sec*/

		/* Copy the last time */
		lastTime = currTime;

		/* Get current time */
		if (clock_gettime(CLOCK_MONOTONIC, &currTime) < 0) {
			AKMERROR;
			break;
		}

		/* Calculate the difference */
		execTime = CalcDuration(&currTime, &lastTime);

		AKMDATA(AKMDATA_EXECTIME,
				"Executing(%6.2f)\n", (double)execTime / 1000000.0);

		/* Subtract the differential time from each event.
		 If subtracted value is negative turn event flag on. */
		exec_flags |= (SetLoopTime(&setting, execTime, &minVal)
					   << (SETTING_FLAG_POS));

		exec_flags |= (SetLoopTime(&mag_acq, execTime, &minVal)
					   << (MAG_ACQ_FLAG_POS));

		exec_flags |= (SetLoopTime(&acc_acq, execTime, &minVal)
					   << (ACC_ACQ_FLAG_POS));

		exec_flags |= (SetLoopTime(&ori_acq, execTime, &minVal)
					   << (ORI_ACQ_FLAG_POS));

		exec_flags |= (SetLoopTime(&acc_mes, execTime, &minVal)
					   << (ACC_MES_FLAG_POS));

		/* Magnetometer needs special care. While the device is
		 under measuring, measurement start flag should not be turned on.*/
		if (mag_mes.interval >= 0) {
			mag_mes.duration -= execTime;
			if (!measuring) {
				/* Not measuring */
				if (mag_mes.duration <= AKMD_LOOP_MARGIN) {
					exec_flags |= (1 << (MAG_MES_FLAG_POS));
				} else if (mag_mes.duration < minVal) {
					minVal = mag_mes.duration;
				}
			} else {
				/* While measuring */
				mag_int.duration -= execTime;
				/* NO_MARGIN! */
				if (mag_int.duration <= 0) {
					exec_flags |= (1 << (MAG_INT_FLAG_POS));
				} else if (mag_int.duration < minVal) {
					minVal = mag_int.duration;
				}
			}
		}

		/* If all flag is off, go sleep */
		if (exec_flags == 0) {
			AKMDATA(AKMDATA_EXECTIME, "Sleeping(%6.2f)...\n",
					(double)minVal / 1000000.0);
			if (minVal > 0) {
				struct timespec doze = { 0, 0 };
				doze = int64_to_timespec(minVal);
				nanosleep(&doze, NULL);
			}
		} else {
			AKMDATA(AKMDATA_EXECFLAG, "ExecFlags=0x%04X\n", exec_flags);

			if (exec_flags & (1 << (MAG_MES_FLAG_POS))) {
				/* Set to SNG measurement pattern (Set CNTL register) */
				if (AKD_SetMode(AK8963_MODE_SNG_MEASURE | (prms->m_outbit << 4)) != AKD_SUCCESS) {
					AKMERROR;
					break;
				}
				mag_mes.duration = mag_mes.interval;
				mag_int.duration = mag_int.interval;
				measuring = 1;
			}

			if (exec_flags & (1 << (MAG_INT_FLAG_POS))) {
				/* Get magnetometer measurement data */
				/* ST1 + HXL,HXH + HYL,HYH + HZL,HZH + ST2 = 8 Byte */
				if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) {
					AKMERROR;
					break;
				}
				// Copy to local variable
				AKMDATA(AKMDATA_BDATA, "bData(Hex)=");
				for (i=0; i<AKSC_BDATA_SIZE; i++) {
					bData[i] = i2cData[i];
					AKMDATA(AKMDATA_BDATA, "%02x,", bData[i]);
				}
				AKMDATA(AKMDATA_BDATA, "\n");

				ret = GetMagneticVector(
											bData,
											prms,
											checkForm(),
											hdoe_interval);

				// Check the return value
				if (ret == AKRET_PROC_SUCCEED) {
					;/* Do nothing */
				} else if (ret == AKRET_FORMATION_CHANGED) {
					;/* Do nothing */
				/*} else if (ret == AKRET_DATA_READERROR) {
					AKMDEBUG(DBG_LEVEL3, "Data read error occurred.\n");
				} else if (ret == AKRET_DATA_OVERFLOW) {
					AKMDEBUG(DBG_LEVEL3, "Data overflow occurred.\n");
				} else if (ret == AKRET_OFFSET_OVERFLOW) {
					AKMDEBUG(DBG_LEVEL3, "Offset overflow occurred.\n");
				} else if (ret == AKRET_HBASE_CHANGED) {
					AKMDEBUG(DBG_LEVEL3, "Huge magnetic force.\n");
				} else if (ret == AKRET_HFLUC_OCCURRED) {
					AKMDEBUG(DBG_LEVEL3, "A fluctuation of magnetic field.\n");
				} else if (ret == AKRET_VNORM_ERROR) {
					AKMDEBUG(DBG_LEVEL3, "Data normalizeation was failed.\n");*/
				} else {
					ALOGE("GetMagneticVector has failed (0x%04X).\n", ret);
				}
				measuring = 0;
			}

			if (exec_flags & (1 << (ACC_MES_FLAG_POS))) {
				/* Get accelerometer data */
				if (AKD_GetAccelerationData(prms->m_avec.v) != AKD_SUCCESS) {
					AKMERROR;
					break;
				}

#ifdef AKMD_ACC_COMBINED
				ConvertCoordinate(prms->m_layout, &prms->m_avec);
#endif
				AKMDATA(AKMDATA_AVEC, "acc(dec)=%d,%d,%d\n",
						prms->m_avec.u.x, prms->m_avec.u.y, prms->m_avec.u.z);
			}

			if (exec_flags & (1 << (ORI_ACQ_FLAG_POS))) {
				/* Calculate direction angle */
				if (CalcDirection(prms) != AKRET_PROC_SUCCEED) {
					AKMERROR;
				}
			}
		}
		if (exec_flags & 0x0F) {
			/* If any ACQ flag is on, report the data to device driver */
			Disp_MeasurementResultHook(prms, (uint16)(exec_flags & 0x0F));
		}

		if (exec_flags & (1 << (SETTING_FLAG_POS))) {
			/* Get measurement interval from device driver */
			GetInterval(&acc_acq, &mag_acq, &ori_acq,
						&mag_mes, &acc_mes, &hdoe_interval);
		}
	}

MEASURE_SNG_END:
	// Set to PowerDown mode
	if (AKD_SetMode(AK8963_MODE_POWERDOWN) != AKD_SUCCESS) {
		AKMERROR;
	}

	closeForm();
}



/*!
 SmartCompass main calculation routine. This function will be processed
 when INT pin event is occurred.
 @retval AKRET_
 @param[in] bData An array of register values which holds,
 ST1, HXL, HXH, HYL, HYH, HZL, HZH and ST2 value respectively.
 @param[in,out] prms A pointer to a #AK8963PRMS structure.
 @param[in] curForm The index of hardware position which represents the
 index when this function is called.
 @param[in] hDecimator HDOE will execute once while this function is called
 this number of times.
 */
int16 GetMagneticVector(
	const int16	bData[],
	AK8963PRMS*	prms,
	const int16	curForm,
	const int16	hDecimator)
{
	const int16vec hrefZero = {{0, 0, 0}};
	int16vec	have;
	int16		dor, derr, hofl, cb;
	int32vec	preHbase;
	int16		overflow;
	int16		hfluc;
	int16		hdSucc;
	int16		aksc_ret;
	int16		ret;

	have.u.x = 0;
	have.u.y = 0;
	have.u.z = 0;
	dor = 0;
	derr = 0;
	hofl = 0;
	cb = 0;
	preHbase = prms->m_hbase;
	overflow = 0;
	ret = AKRET_PROC_SUCCEED;

	// Subtract the formation suspend counter
	if (prms->m_cntSuspend > 0) {
		prms->m_cntSuspend--;

		// Check the suspend counter
		if (prms->m_cntSuspend == 0) {
			// Restore the value when succeeding in estimating of HOffset.
			prms->m_ho   = prms->HSUC_HO[prms->m_form];
			prms->m_ho32.u.x = (int32)prms->HSUC_HO[prms->m_form].u.x;
			prms->m_ho32.u.y = (int32)prms->HSUC_HO[prms->m_form].u.y;
			prms->m_ho32.u.z = (int32)prms->HSUC_HO[prms->m_form].u.z;

			prms->m_hdst = prms->HSUC_HDST[prms->m_form];
			prms->m_hbase = prms->HSUC_HBASE[prms->m_form];

			// Initialize the decompose parameters
			AKSC_InitDecomp8963(prms->m_hdata);

			// Initialize HDOE parameters
			AKSC_InitHDOEProcPrmsS3(
				&prms->m_hdoev,
				1,
				&prms->m_ho,
				prms->m_hdst
			);

			// Initialize HFlucCheck parameters
			AKSC_InitHFlucCheck(
				&(prms->m_hflucv),
				&(prms->HFLUCV_HREF[prms->m_form]),
				HFLUCV_TH
			);
		}
	}

	// Decompose one block data into each Magnetic sensor's data
	aksc_ret = AKSC_Decomp8963(
					bData,
					prms->m_hnave,
					&prms->m_asa,
					prms->m_hdata,
					&prms->m_hbase,
					&prms->m_hn,
					&have,
					&dor,
					&derr,
					&hofl,
					&cb
				);
	AKM_LOG("%s: ST1, HXH&HXL, HYH&HYL, HZH&HZL, ST2,"
			" hdata[0].u.x, hdata[0].u.y, hdata[0].u.z,"
			" asax, asay, asaz ="
			" %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
			__FUNCTION__,
			bData[0],
			(int16)(((uint16)bData[2])<<8|bData[1]),
			(int16)(((uint16)bData[4])<<8|bData[3]),
			(int16)(((uint16)bData[6])<<8|bData[5]), bData[7],
			prms->m_hdata[0].u.x, prms->m_hdata[0].u.y, prms->m_hdata[0].u.z,
			prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z);

	if (aksc_ret == 0) {
		AKMDUMP("AKSC_Decomp8963 failed.\n"
				"  ST1=0x%02X, ST2=0x%02X\n"
				"  XYZ(HEX)=%02X,%02X,%02X,%02X,%02X,%02X\n"
				"  asa(dec)=%d,%d,%d\n"
				"  hbase(dec)=%ld,%ld,%ld\n",
				bData[0], bData[7],
				bData[1], bData[2], bData[3], bData[4], bData[5], bData[6],
				prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z,
				prms->m_hbase.u.x, prms->m_hbase.u.y, prms->m_hbase.u.z);
		return AKRET_PROC_FAIL;
	}

	// Check the formation change
	if (prms->m_form != curForm) {
		prms->m_form = curForm;
		prms->m_cntSuspend = CSPEC_CNTSUSPEND_SNG;
		prms->m_callcnt = 0;
		ret |= AKRET_FORMATION_CHANGED;
		return ret;
	}

	// Check derr
	if (derr == 1) {
		ret |= AKRET_DATA_READERROR;
		return ret;
	}

	// Check hofl
	if (hofl == 1) {
		if (prms->m_cntSuspend <= 0) {
			// Set a HDOE level as "HDST_UNSOLVED"
			AKSC_SetHDOELevel(
							  &prms->m_hdoev,
							  &prms->m_ho,
							  AKSC_HDST_UNSOLVED,
							  1
							  );
			prms->m_hdst = AKSC_HDST_UNSOLVED;
		}
		ret |= AKRET_DATA_OVERFLOW;
		return ret;
	}

	// Check hbase
	if (cb == 1) {
		// Translate HOffset
		AKSC_TransByHbase(
			&(preHbase),
			&(prms->m_hbase),
			&(prms->m_ho),
			&(prms->m_ho32),
			&overflow
		);
		if (overflow == 1) {
			ret |= AKRET_OFFSET_OVERFLOW;
		}

		// Set hflucv.href to 0
		AKSC_InitHFlucCheck(
			&(prms->m_hflucv),
			&(hrefZero),
			HFLUCV_TH
		);

		if (prms->m_cntSuspend <= 0) {
			AKSC_SetHDOELevel(
				&prms->m_hdoev,
				&prms->m_ho,
				AKSC_HDST_UNSOLVED,
				1
			);
			prms->m_hdst = AKSC_HDST_UNSOLVED;
		}

		ret |= AKRET_HBASE_CHANGED;
		return ret;
	}

	if (prms->m_cntSuspend <= 0) {
		// Detect a fluctuation of magnetic field.
		hfluc = AKSC_HFlucCheck(&(prms->m_hflucv), &(prms->m_hdata[0]));

		if (hfluc == 1) {
			// Set a HDOE level as "HDST_UNSOLVED"
			AKSC_SetHDOELevel(
				&prms->m_hdoev,
				&prms->m_ho,
				AKSC_HDST_UNSOLVED,
				1
			);
			prms->m_hdst = AKSC_HDST_UNSOLVED;
			ret |= AKRET_HFLUC_OCCURRED;
			return ret;
		}
		else {
			prms->m_callcnt--;
			if (prms->m_callcnt <= 0) {
				//Calculate Magnetic sensor's offset by DOE
				hdSucc = AKSC_HDOEProcessS3(
							prms->m_licenser,
							prms->m_licensee,
							prms->m_key,
							&prms->m_hdoev,
							prms->m_hdata,
							prms->m_hn,
							&prms->m_ho,
							&prms->m_hdst
						 );

				if (hdSucc == AKSC_CERTIFICATION_DENIED) {
					AKMERROR;
					return AKRET_PROC_FAIL;
				}
				if (hdSucc > 0) {
					prms->HSUC_HO[prms->m_form] = prms->m_ho;
					prms->m_ho32.u.x = (int32)prms->m_ho.u.x;
					prms->m_ho32.u.y = (int32)prms->m_ho.u.y;
					prms->m_ho32.u.z = (int32)prms->m_ho.u.z;

					prms->HSUC_HDST[prms->m_form] = prms->m_hdst;
					prms->HFLUCV_HREF[prms->m_form] = prms->m_hflucv.href;
					prms->HSUC_HBASE[prms->m_form] = prms->m_hbase;
				}

				//Set decimator counter
				prms->m_callcnt = hDecimator;
			}
		}
	}

	// Subtract offset and normalize magnetic field vector.
	aksc_ret = AKSC_VNorm(
						  &have,
						  &prms->m_ho,
						  &prms->m_hs,
						  AKSC_HSENSE_TARGET,
						  &prms->m_hvec
						  );
	if (aksc_ret == 0) {
		AKMDUMP("AKSC_VNorm failed.\n"
				"  have=%6d,%6d,%6d  ho=%6d,%6d,%6d  hs=%6d,%6d,%6d\n",
				have.u.x, have.u.y, have.u.z,
				prms->m_ho.u.x, prms->m_ho.u.y, prms->m_ho.u.z,
				prms->m_hs.u.x, prms->m_hs.u.y, prms->m_hs.u.z);
		ret |= AKRET_VNORM_ERROR;
		return ret;
	}

	//Convert layout from sensor to Android by using PAT number.
	// Magnetometer
	ConvertCoordinate(prms->m_layout, &prms->m_hvec);

	return AKRET_PROC_SUCCEED;
}

/*!
 Calculate Yaw, Pitch, Roll angle.
 m_hvec and m_avec should be Android coordination.
 @return Always return #AKRET_PROC_SUCCEED.
 @param[in,out] prms A pointer to a #AK8963PRMS structure.
 */
int16 CalcDirection(AK8963PRMS* prms)
{
	/* Conversion matrix from Android to SmartCompass coordination */
/*
	const I16MATRIX hlayout = {{
								 0, 1, 0,
								-1, 0, 0,
								 0, 0, 1}};
	const I16MATRIX alayout = {{
								 0,-1, 0,
								 1, 0, 0,
								 0, 0,-1}};
*/
	int16    preThe;

	preThe = prms->m_theta;
	
	prms->m_ds3Ret = AKSC_DirectionS3(
									  prms->m_licenser,
									  prms->m_licensee,
									  prms->m_key,
									  &prms->m_hvec,
									  &prms->m_avec,
									  &prms->m_dvec,
									  &prms->m_hlayout[prms->m_form],
									  &prms->m_alayout[prms->m_form],
									  &prms->m_theta,
									  &prms->m_delta,
									  &prms->m_hr,
									  &prms->m_hrhoriz,
									  &prms->m_ar,
									  &prms->m_phi180,
									  &prms->m_phi90,
									  &prms->m_eta180,
									  &prms->m_eta90,
									  &prms->m_mat,
									  &prms->m_quat
									  );

	prms->m_theta =	AKSC_ThetaFilter(
									 prms->m_theta,
									 preThe,
									 THETAFILTER_SCALE
									 );

	if (prms->m_ds3Ret == AKSC_CERTIFICATION_DENIED) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}

	if (prms->m_ds3Ret != 3) {
/*
		AKMDUMP("AKSC_Direction6D failed (0x%02x).\n"
				"  hvec=%d,%d,%d  avec=%d,%d,%d  dvec=%d,%d,%d\n",
				prms->m_ds3Ret,
				prms->m_hvec.u.x, prms->m_hvec.u.y, prms->m_hvec.u.z,
				prms->m_avec.u.x, prms->m_avec.u.y, prms->m_avec.u.z,
				prms->m_dvec.u.x, prms->m_dvec.u.y, prms->m_dvec.u.z);
*/
	}

	/* Convert Yaw, Pitch, Roll angle to Android coordinate system */
	/* Actually, only Roll angle is opposite */
	if (prms->m_ds3Ret & 0x02) {
		prms->m_eta180 = -prms->m_eta180;
		prms->m_eta90  = -prms->m_eta90;

		AKMDATA(AKMDATA_D6D, "AKSC_Direction6D (0x%02x):\n"
				"  Yaw, Pitch, Roll=%6.1f,%6.1f,%6.1f\n",
				prms->m_ds3Ret,
				DISP_CONV_Q6F(prms->m_theta),
				DISP_CONV_Q6F(prms->m_phi180),
				DISP_CONV_Q6F(prms->m_eta90));
	}

	return AKRET_PROC_SUCCEED;
}



