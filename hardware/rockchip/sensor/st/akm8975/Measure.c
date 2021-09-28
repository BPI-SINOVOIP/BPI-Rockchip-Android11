/******************************************************************************
 *
 * $Id$
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
#include "AKCommon.h"
#include "AK8975Driver.h"
#include "Measure.h"
#include "TestLimit.h"
#include "FileIO.h"
#include "DispMessage.h"
#include "misc.h"
#include <errno.h>
extern int g_file;			/*!< FD to AK8975 device file. @see : "MSENSOR_NAME" */

//#define NOASA

/*!
 Initialize #AK8975PRMS structure. At first, 0 is set to all parameters. 
 After that, some parameters, which should not be 0, are set to specific
 value. Some of initial values can be customized by editing the file
 \c "CustomerSpec.h".
 @param[out] prms A pointer to #AK8975PRMS structure.
 */
void InitAK8975PRMS(AK8975PRMS* prms)
{
	struct akm_platform_data pdata;
	int i,j,k;
	// Set 0 to the AK8975PRMS structure.
	memset(prms, 0, sizeof(AK8975PRMS));
	
	// Sensitivity
	prms->m_hs.u.x = AKSC_HSENSE_TARGET;
	prms->m_hs.u.y = AKSC_HSENSE_TARGET;
	prms->m_hs.u.z = AKSC_HSENSE_TARGET;
	
	// HDOE
	prms->m_hdst = AKSC_HDST_UNSOLVED;
	
	// Set counter
	prms->m_cntSuspend = 0;
	
	// (m_hdata is initialized with AKSC_InitDecomp8975)
	prms->m_hnave = CSPEC_HNAVE;
	prms->m_dvec.u.x = CSPEC_DVEC_X;
	prms->m_dvec.u.y = CSPEC_DVEC_Y;
	prms->m_dvec.u.z = CSPEC_DVEC_Z;

	if (ioctl(g_file, ECS_IOCTL_GET_PLATFORM_DATA, &pdata) >= 0) 
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
			//DBGPRINT(DBG_LEVEL2, "%s:m_layout[%d][%d][%d]=%d\n",__FUNCTION__,i,j,k,pdata.m_layout[i][j][k]);
		}
			
	}
	else
	{
		//***********************************************
		// Set HLAYOUT
		//***********************************************
		DBGPRINT(DBG_LEVEL2, "%s:%s\n",__FUNCTION__,strerror(errno));
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
 Fill #AK8975PRMS structure with default value.
 @param[out] prms A pointer to #AK8975PRMS structure.
 */
void SetDefaultPRMS(AK8975PRMS* prms)
{
	// Set parameter to HDST, HO, HREF, THRE
	// Form 0
	prms->HSUC_HDST[0] = AKSC_HDST_UNSOLVED;
	prms->HSUC_HO[0].u.x = 0;
	prms->HSUC_HO[0].u.y = 0;
	prms->HSUC_HO[0].u.z = 0;
	prms->HFLUCV_HREF[0].u.x = 0;
	prms->HFLUCV_HREF[0].u.y = 0;
	prms->HFLUCV_HREF[0].u.z = 0;
	
	// Form 1
	prms->HSUC_HDST[1] = AKSC_HDST_UNSOLVED;
	prms->HSUC_HO[1].u.x = 0;
	prms->HSUC_HO[1].u.y = 0;
	prms->HSUC_HO[1].u.z = 0;
	prms->HFLUCV_HREF[1].u.x = 0;
	prms->HFLUCV_HREF[1].u.y = 0;
	prms->HFLUCV_HREF[1].u.z = 0;
}

/*!
 Read hard coded value (Fuse ROM) from AK8975. Then set the read value to 
 calculation parameter.
 @return If parameters are read successfully, the return value is 
 #AKRET_PROC_SUCCEED. Otherwise the return value is #AKRET_PROC_FAIL. No 
 error code is reserved to show which operation has failed.
 @param[out] prms A pointer to #AK8975PRMS structure.
 */
int16 ReadAK8975FUSEROM(AK8975PRMS* prms)
{
	BYTE    i2cData[6];
	
	// Set to PowerDown mode
	if (AKD_SetMode(AK8975_MODE_POWERDOWN) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return AKRET_PROC_FAIL;
	}
	
	// Set to FUSE ROM access mode
	if (AKD_SetMode(AK8975_MODE_FUSE_ACCESS) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return AKRET_PROC_FAIL;
	}
	
	// Read values. ASAX, ASAY, ASAZ
	if (AKD_RxData(AK8975_FUSE_ASAX, i2cData, 3) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return AKRET_PROC_FAIL;
	}
	prms->m_asa.u.x = (int16)i2cData[0];
	prms->m_asa.u.y = (int16)i2cData[1];
	prms->m_asa.u.z = (int16)i2cData[2];

	DBGPRINT(DBG_LEVEL3, "%s: asa(dec)=%d,%d,%d\n", __FUNCTION__,
			 prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z);

#ifdef NOASA
	if((prms->m_asa.u.x==0)||(prms->m_asa.u.y==0)||(prms->m_asa.u.z==0)){
		prms->m_asa.u.x = i2cData[0] = 128;
		prms->m_asa.u.y = i2cData[1] = 128;
		prms->m_asa.u.z = i2cData[2] = 128;
	}
#endif
	
	// Set keywords for SmartCompassLibrary certification
	prms->m_key[2] = (int16)i2cData[0];
	prms->m_key[3] = (int16)i2cData[1];
	prms->m_key[4] = (int16)i2cData[2];
	
	// Set to PowerDown mode 
	if (AKD_SetMode(AK8975_MODE_POWERDOWN) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return AKRET_PROC_FAIL;
	}
	
	// Set keywords for SmartCompassLibrary certification
	if (AKD_RxData(AK8975_REG_WIA, i2cData, 1) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return AKRET_PROC_FAIL;
	}
	prms->m_key[0] = CSPEC_CI_AK_DEVICE;
	prms->m_key[1] = (int16)i2cData[0];
	strncpy((char *)prms->m_licenser, CSPEC_CI_LICENSER, AKSC_CI_MAX_CHARSIZE);
	strncpy((char *)prms->m_licensee, CSPEC_CI_LICENSEE, AKSC_CI_MAX_CHARSIZE);
	
	return AKRET_PROC_SUCCEED;
}


/*!
 Set initial values to registers of AK8975. Then initialize algorithm 
 parameters.
 @return If parameters are read successfully, the return value is 
 #AKRET_PROC_SUCCEED. Otherwise the return value is #AKRET_PROC_FAIL. No 
 error code is reserved to show which operation has failed.
 @param[in,out] prms A pointer to a #AK8975PRMS structure.
 */
int16 InitAK8975_Measure(AK8975PRMS* prms)
{
	BYTE  i2cData[AKSC_BDATA_SIZE];
	
	// Set to PowerDown mode
	if (AKD_SetMode(AK8975_MODE_POWERDOWN) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return AKRET_PROC_FAIL;
	}
	
	prms->m_form = getFormation();
	
	// Restore the value when succeeding in estimating of HOffset. 
	prms->m_ho   = prms->HSUC_HO[prms->m_form]; 
	prms->m_hdst = prms->HSUC_HDST[prms->m_form];
	
	// Initialize the decompose parameters
	AKSC_InitDecomp8975(prms->m_hdata);
	
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
	
	return AKRET_PROC_SUCCEED;
}

/*!
 Set initial values to registers of AK8975. Then initialize algorithm 
 parameters.
 @return Currently, this function always return #AKRET_PROC_SUCCEED. 
 @param[in,out] prms A pointer to a #AK8975PRMS structure.
 */
int16 SwitchFormation(AK8975PRMS* prms)
{
	prms->m_form = getFormation();
	
	// Restore the value when succeeding in estimating of HOffset. 
	prms->m_ho   = prms->HSUC_HO[prms->m_form]; 
	prms->m_hdst = prms->HSUC_HDST[prms->m_form];
	
	// Initialize the decompose parameters
	AKSC_InitDecomp8975(prms->m_hdata);
	
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
	prms->m_callcnt = 0;
	
	return AKRET_PROC_SUCCEED;
}


/*!
 Execute "Onboard Function Test" (includes "START" and "END" command).
 @retval 1 The test is passed successfully.
 @retval -1 The test is failed.
 @retval 0 The test is aborted by kind of system error.
 @param[in] prms A pointer to a #AK8975PRMS structure.
 */
int16 FctShipmntTest_Body(AK8975PRMS* prms)
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
 @param[in] prms A pointer to a #AK8975PRMS structure.
 */
int16 FctShipmntTestProcess_Body(AK8975PRMS* prms)
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
	
	// Set to PowerDown mode 
	if (AKD_SetMode(AK8975_MODE_POWERDOWN) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return 0;
	}
	
	// When the serial interface is SPI,
	// write "00011011" to I2CDIS register(to disable I2C,).
	if(CSPEC_SPI_USE == 1){
		i2cData[0] = 0x1B;
		if (AKD_TxData(AK8975_REG_I2CDIS, i2cData, 1) != AKD_SUCCESS) {
			DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
			return 0;
		}
	}
	
	// Read values from WIA to ASTC.
	if (AKD_RxData(AK8975_REG_WIA, i2cData, 13) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return 0;
	}
	
	// TEST
	TEST_DATA(TLIMIT_NO_RST_WIA,  TLIMIT_TN_RST_WIA,  (int16)i2cData[0],  TLIMIT_LO_RST_WIA,  TLIMIT_HI_RST_WIA,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_INFO, TLIMIT_TN_RST_INFO, (int16)i2cData[1],  TLIMIT_LO_RST_INFO, TLIMIT_HI_RST_INFO, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_ST1,  TLIMIT_TN_RST_ST1,  (int16)i2cData[2],  TLIMIT_LO_RST_ST1,  TLIMIT_HI_RST_ST1,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HXL,  TLIMIT_TN_RST_HXL,  (int16)i2cData[3],  TLIMIT_LO_RST_HXL,  TLIMIT_HI_RST_HXL,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HXH,  TLIMIT_TN_RST_HXH,  (int16)i2cData[4],  TLIMIT_LO_RST_HXH,  TLIMIT_HI_RST_HXH,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HYL,  TLIMIT_TN_RST_HYL,  (int16)i2cData[5],  TLIMIT_LO_RST_HYL,  TLIMIT_HI_RST_HYL,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HYH,  TLIMIT_TN_RST_HYH,  (int16)i2cData[6],  TLIMIT_LO_RST_HYH,  TLIMIT_HI_RST_HYH,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HZL,  TLIMIT_TN_RST_HZL,  (int16)i2cData[7],  TLIMIT_LO_RST_HZL,  TLIMIT_HI_RST_HZL,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HZH,  TLIMIT_TN_RST_HZH,  (int16)i2cData[8],  TLIMIT_LO_RST_HZH,  TLIMIT_HI_RST_HZH,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_ST2,  TLIMIT_TN_RST_ST2,  (int16)i2cData[9],  TLIMIT_LO_RST_ST2,  TLIMIT_HI_RST_ST2,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_CNTL, TLIMIT_TN_RST_CNTL, (int16)i2cData[10], TLIMIT_LO_RST_CNTL, TLIMIT_HI_RST_CNTL, &pf_total);
	// i2cData[11] is BLANK.
	TEST_DATA(TLIMIT_NO_RST_ASTC, TLIMIT_TN_RST_ASTC, (int16)i2cData[12], TLIMIT_LO_RST_ASTC, TLIMIT_HI_RST_ASTC, &pf_total);
	
	// Read values from I2CDIS.
	if (AKD_RxData(AK8975_REG_I2CDIS, i2cData, 1) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return 0;
	}
	if(CSPEC_SPI_USE == 1){
		TEST_DATA(TLIMIT_NO_RST_I2CDIS, TLIMIT_TN_RST_I2CDIS, (int16)i2cData[0], TLIMIT_LO_RST_I2CDIS_USESPI, TLIMIT_HI_RST_I2CDIS_USESPI, &pf_total);
	}else{
		TEST_DATA(TLIMIT_NO_RST_I2CDIS, TLIMIT_TN_RST_I2CDIS, (int16)i2cData[0], TLIMIT_LO_RST_I2CDIS_USEI2C, TLIMIT_HI_RST_I2CDIS_USEI2C, &pf_total);
	}
	
	// Set to FUSE ROM access mode
	if (AKD_SetMode(AK8975_MODE_FUSE_ACCESS) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return 0;
	}
	
	// Read values from ASAX to ASAZ
	if (AKD_RxData(AK8975_FUSE_ASAX, i2cData, 3) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return 0;
	}
	asax = (int16)i2cData[0];
	asay = (int16)i2cData[1];
	asaz = (int16)i2cData[2];
#ifdef NOASA
	if((asax==0)||(asay==0)||(asaz==0)){
		asax = 128;
		asay = 128;
		asaz = 128;
	}
#endif
	
	// TEST
	TEST_DATA(TLIMIT_NO_ASAX, TLIMIT_TN_ASAX, asax, TLIMIT_LO_ASAX, TLIMIT_HI_ASAX, &pf_total);
	TEST_DATA(TLIMIT_NO_ASAY, TLIMIT_TN_ASAY, asay, TLIMIT_LO_ASAY, TLIMIT_HI_ASAY, &pf_total);
	TEST_DATA(TLIMIT_NO_ASAZ, TLIMIT_TN_ASAZ, asaz, TLIMIT_LO_ASAZ, TLIMIT_HI_ASAZ, &pf_total);
	
	// Read values. CNTL
	if (AKD_RxData(AK8975_REG_CNTL, i2cData, 1) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return 0;
	}
	
	// Set to PowerDown mode 
	if (AKD_SetMode(AK8975_MODE_POWERDOWN) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return 0;
	}
	
	// TEST
	TEST_DATA(TLIMIT_NO_WR_CNTL, TLIMIT_TN_WR_CNTL, (int16)i2cData[0], TLIMIT_LO_WR_CNTL, TLIMIT_HI_WR_CNTL, &pf_total);

	
	//***********************************************
	//  Step2
	//***********************************************
	
	// Set to SNG measurement pattern (Set CNTL register) 
	if (AKD_SetMode(AK8975_MODE_SNG_MEASURE) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return 0;
	}
	
	// Wait for DRDY pin changes to HIGH.
	// Get measurement data from AK8975
	// ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + ST2
	// = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 = 8 bytes
	if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return 0;
	}
	
	hdata[0] = (int16)((((uint16)(i2cData[2]))<<8)+(uint16)(i2cData[1]));
	hdata[1] = (int16)((((uint16)(i2cData[4]))<<8)+(uint16)(i2cData[3]));
	hdata[2] = (int16)((((uint16)(i2cData[6]))<<8)+(uint16)(i2cData[5]));
	
	// TEST
	TEST_DATA(TLIMIT_NO_SNG_ST1, TLIMIT_TN_SNG_ST1, (int16)i2cData[0], TLIMIT_LO_SNG_ST1, TLIMIT_HI_SNG_ST1, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HX, TLIMIT_TN_SNG_HX, hdata[0], TLIMIT_LO_SNG_HX, TLIMIT_HI_SNG_HX, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HY, TLIMIT_TN_SNG_HY, hdata[1], TLIMIT_LO_SNG_HY, TLIMIT_HI_SNG_HY, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HZ, TLIMIT_TN_SNG_HZ, hdata[2], TLIMIT_LO_SNG_HZ, TLIMIT_HI_SNG_HZ, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_ST2, TLIMIT_TN_SNG_ST2, (int16)i2cData[7], TLIMIT_LO_SNG_ST2, TLIMIT_HI_SNG_ST2, &pf_total);
	
	// Generate magnetic field for self-test (Set ASTC register)
	i2cData[0] = 0x40;
	if (AKD_TxData(AK8975_REG_ASTC, i2cData, 1) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return 0;
	}
	
	// Set to Self-test mode (Set CNTL register)
	if (AKD_SetMode(AK8975_MODE_SELF_TEST) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return 0;
	}
	
	// Wait for DRDY pin changes to HIGH.
	// Get measurement data from AK8975
	// ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + ST2
	// = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 = 8Byte
	if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return 0;
	}
		
	// TEST
	TEST_DATA(TLIMIT_NO_SLF_ST1, TLIMIT_TN_SLF_ST1, (int16)i2cData[0], TLIMIT_LO_SLF_ST1, TLIMIT_HI_SLF_ST1, &pf_total);
	
	hdata[0] = (int16)((((uint16)(i2cData[2]))<<8)+(uint16)(i2cData[1]));
	hdata[1] = (int16)((((uint16)(i2cData[4]))<<8)+(uint16)(i2cData[3]));
	hdata[2] = (int16)((((uint16)(i2cData[6]))<<8)+(uint16)(i2cData[5]));
	
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
	TEST_DATA(TLIMIT_NO_SLF_ST2, TLIMIT_TN_SLF_ST2, (int16)i2cData[7], TLIMIT_LO_SLF_ST2, TLIMIT_HI_SLF_ST2, &pf_total);
	
	// Set to Normal mode for self-test.
	i2cData[0] = 0x00;
	if (AKD_TxData(AK8975_REG_ASTC, i2cData, 1) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return 0;
	}
	
	return pf_total;
}

/*!
 This is the main routine of measurement.
 @param[in,out] prms A pointer to a #AK8975PRMS structure.
 */
void MeasureSNGLoop(AK8975PRMS* prms)
{
	BYTE    i2cData[AKSC_BDATA_SIZE];
	int16   i;
	int16   bData[AKSC_BDATA_SIZE];  // Measuring block data
	int16   ret;
	int32   ch;
	int32   doze;
	int32_t delay;
	AKMD_INTERVAL interval;
	struct timespec tsstart, tsend;
	

	if (openKey() < 0) {
		DBGPRINT(DBG_LEVEL1, 
				 "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return;
	}
	
	if (openFormation() < 0) {
		DBGPRINT(DBG_LEVEL1, 
				 "%s:%d Error.\n", __FUNCTION__, __LINE__);
		return;
	}

	// Get initial interval
	GetValidInterval(CSPEC_INTERVAL_SNG, &interval);

	// Initialize
	if(InitAK8975_Measure(prms) != AKD_SUCCESS){
		return;
	}
	
	while(TRUE){
		// Get start time
		if (clock_gettime(CLOCK_REALTIME, &tsstart) < 0) {
			DBGPRINT(DBG_LEVEL1, 
					 "%s:%d Error.\n", __FUNCTION__, __LINE__);
			return;
		}
		// Set to SNG measurement pattern (Set CNTL register) 
		if (AKD_SetMode(AK8975_MODE_SNG_MEASURE) != AKD_SUCCESS) {
			DBGPRINT(DBG_LEVEL1, 
					 "%s:%d Error.\n", __FUNCTION__, __LINE__);
			return;
		}
		
        // .! : 获取 M snesor 的原始数据. 这里可能阻塞.  
		// Get measurement data from AK8975
		// ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + ST2
		// = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 = 8 bytes
		if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) {
			DBGPRINT(DBG_LEVEL1, 
					 "%s:%d Error.\n", __FUNCTION__, __LINE__);
			return;
		}

		// Copy to local variable
		// DBGPRINT(DBG_LEVEL3, "%s: bData(Hex)=", __FUNCTION__);
		for(i=0; i<AKSC_BDATA_SIZE; i++){
			bData[i] = i2cData[i];
			// DBGPRINT(DBG_LEVEL3, "%02x,", bData[i]);
		}
		// DBGPRINT(DBG_LEVEL3, "\n");
        D_WHEN_REPEAT(100, 
                      "raw mag x : %d, raw mag y : %d, raw mag z : %d.", 
                      (signed short)(bData[1] + (bData[2] << 8) ), 
                      (signed short)(bData[3] + (bData[4] << 8) ), 
                      (signed short)(bData[5] + (bData[6] << 8) ) );
		
        // .! : 
		//  Get acceelration sensor's measurement data.
		if (GetAccVec(prms) != AKRET_PROC_SUCCEED) {
			return;
		}
        /*
		DBGPRINT(DBG_LEVEL3, 
				 "%s: acc(Hex)=%02x,%02x,%02x\n", __FUNCTION__,
				 prms->m_avec.u.x, prms->m_avec.u.y, prms->m_avec.u.z);
        */
		
		ret = MeasuringEventProcess(
									bData,
									prms,
									getFormation(),
									interval.decimator,
									CSPEC_CNTSUSPEND_SNG
									);
		// Check the return value
		if(ret == AKRET_PROC_SUCCEED){
			if(prms->m_cntSuspend > 0){
				// Show message
				DBGPRINT(DBG_LEVEL2, 
						 "Suspend cycle count = %d\n", prms->m_cntSuspend);
			} 
			else if (prms->m_callcnt <= 1){
				// Check interval
				if (AKD_GetDelay(&delay) != AKD_SUCCESS) {
					DBGPRINT(DBG_LEVEL1, 
							 "%s:%d Error.\n", __FUNCTION__, __LINE__);
				} else {
					GetValidInterval(delay, &interval);
				}
			}
			
			// Display(or dispatch) the result.
			Disp_MeasurementResultHook(prms);
		}
		else if(ret == AKRET_FORMATION_CHANGED){
			// Switch formation.
			SwitchFormation(prms);
		}
		else if(ret == AKRET_DATA_READERROR){
			DBGPRINT(DBG_LEVEL2, 
					 "Data read error occurred.\n\n");
		}
		else if(ret == AKRET_DATA_OVERFLOW){
			DBGPRINT(DBG_LEVEL2, 
					 "Data overflow occurred.\n\n");
		}
		else if(ret == AKRET_HFLUC_OCCURRED){
			DBGPRINT(DBG_LEVEL2, 
					 "AKSC_HFlucCheck did not return 1.\n\n");
		}
		else{
			// Does not reach here
			LOGE("MeasuringEventProcess has failed.\n");
			break;
		}
		
		// Check user order
		ch = checkKey();
		
		if (ch == AKKEY_STOP_MEASURE) {
			break;
		}
		else if(ch < 0){
			LOGD("Bad key code.\n");
			break;
		}

		// Get end time
		if (clock_gettime(CLOCK_REALTIME, &tsend) < 0) {
			DBGPRINT(DBG_LEVEL1, 
					 "%s:%d Error.\n", __FUNCTION__, __LINE__);
			return;
		}
		// calculate wait time
		doze = interval.interval - ((tsend.tv_sec - tsstart.tv_sec)*1000000 +
									(tsend.tv_nsec - tsstart.tv_nsec)/1000);
		if (doze < 0) {
			doze = 0;
		}

		// Adjust sampling frequency
		// DBGPRINT(DBG_LEVEL3, "Sleep %d usec.\n", doze);
		usleep(doze);
	}
	// Set to PowerDown mode 
	if (AKD_SetMode(AK8975_MODE_POWERDOWN) != AKD_SUCCESS) {
		DBGPRINT(DBG_LEVEL1, 
				 "%s:%d Error.\n", __FUNCTION__, __LINE__);
	}

	closeFormation();
	closeKey();
}



/*!
 SmartCompass main calculation routine. This function will be processed 
 when INT pin event is occurred.
 @retval AKRET_
 @param[in] bData An array of register values which holds TMPS, H1X, H1Y 
 and H1Z value respectively.
 @param[in,out] prms A pointer to a #AK8975PRMS structure.
 @param[in] curForm 当前设备的 formation 状态标识. 
 @param[in] hDecimator
 @param[in] cntSuspend
 // @param[in] pre_form The index of hardware position which represents the 
 // index when this function is called at the last time.
 */
int16 MeasuringEventProcess(
							const int16	bData[],
							AK8975PRMS*	prms,
							const int16	curForm,
							const int16	hDecimator,
							const int16	cntSuspend
							)
{
	int16vec have;
	int16    dor, derr, hofl;
	int16    isOF;
	int16    aksc_ret;
	int16    hdSucc;
	int16    preThe;
	
	dor = 0;
	derr = 0;
	hofl = 0;
	//DBGPRINT(DBG_LEVEL2, "%s:prms->m_form1111=%d\n",
		//		 __FUNCTION__,prms->m_form );
	// Decompose one block data into each Magnetic sensor's data
	aksc_ret = AKSC_Decomp8975(
							   bData,   // 输入 raw 的 mag 数据.
							   prms->m_hnave,   // 指定 参与平均计算的 mag 数据(向量) 的个数. 
							   &prms->m_asa,    // 输入 sensor 的 敏感度调整参数, 该参数的源头是 AK8975 的 fuse rom.
							   prms->m_hdata,   // 输入/输出 : raw mag 数据的 AKSC 形态. 数组实例的元素个数是 AKSC_HDATA_SIZE.
							   &prms->m_hn,     // 返回的 mag 数据的个数. 对 AK8975 将是 1. 
							   &have,   // 返回平均计算之后得到的 mag 数据.
							   &dor,    // 返回是否发生 "data overrun".
							   &derr,   // 返回是否发生 "data error".
							   &hofl    // 返回是否发生 "data overflow".
							   );
	
	if (aksc_ret == 0) {
		DBGPRINT(DBG_LEVEL1, 
				 "AKSC_Decomp8975 failed. asa(dec)=%d,%d,%d hn=%d\n", 
				 prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z, prms->m_hn);
		return AKRET_PROC_FAIL;
	}
	
	// Check the formation change
	if(prms->m_form != curForm){
		// Set counter
		prms->m_cntSuspend = cntSuspend;
		
		prms->m_form = curForm;
		
		return AKRET_FORMATION_CHANGED;
	}
	
	if(derr == 1){
		return AKRET_DATA_READERROR;
	}
	
	if(prms->m_cntSuspend > 0){
		prms->m_cntSuspend--;
	}
	else {
		// Detect a fluctuation of magnetic field.
		isOF = AKSC_HFlucCheck(&(prms->m_hflucv), &(prms->m_hdata[0]));
		
		if(hofl == 1){
			// Set a HDOE level as "HDST_UNSOLVED" 
			AKSC_SetHDOELevel(
							  &prms->m_hdoev,
							  &prms->m_ho,
							  AKSC_HDST_UNSOLVED,
							  1
							  );
			prms->m_hdst = AKSC_HDST_UNSOLVED;
			return AKRET_DATA_OVERFLOW;
		}
		else if(isOF == 1){
			// Set a HDOE level as "HDST_UNSOLVED" 
			AKSC_SetHDOELevel(
							  &prms->m_hdoev,
							  &prms->m_ho,
							  AKSC_HDST_UNSOLVED,
							  1
							  );
			prms->m_hdst = AKSC_HDST_UNSOLVED;
			return AKRET_HFLUC_OCCURRED;
		}
		else {
			prms->m_callcnt--;
			if(prms->m_callcnt <= 0){
				//Calculate Magnetic sensor's offset by DOE     .Q : DOE ?
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
				
				if(hdSucc > 0){
					prms->HSUC_HO[prms->m_form] = prms->m_ho;
					prms->HSUC_HDST[prms->m_form] = prms->m_hdst;
					prms->HFLUCV_HREF[prms->m_form] = prms->m_hflucv.href;
				}
				
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
		DBGPRINT(DBG_LEVEL1, "AKSC_VNorm failed.\n");
		return AKRET_PROC_FAIL;
	}
	
	preThe = prms->m_theta;

    D_WHEN_REPEAT(100, "befor calling AKSC_DirectionS3() : \n"
            "\t prms->m_form = %d", prms->m_form);
	
	prms->m_ds3Ret = AKSC_DirectionS3(
									  prms->m_licenser,
									  prms->m_licensee,
									  prms->m_key,
									  &prms->m_hvec,    /* 输入的 mag 数据矢量. */
									  &prms->m_avec,    /* 输入的 acc 数据矢量. */
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
									  &prms->m_mat
									  );
	
	prms->m_theta =	AKSC_ThetaFilter(
									 prms->m_theta,
									 preThe,
									 THETAFILTER_SCALE
									 );
	if(prms->m_ds3Ret != 3) {
        /*
		DBGPRINT(DBG_LEVEL2, "AKSC_Direction6D failed (0x%02x). \n",
				 prms->m_ds3Ret);
        */
		DBGPRINT(DBG_LEVEL2, "AKSC_Direction3S failed (0x%x). \n",
				 (unsigned short)(prms->m_ds3Ret) );
		DBGPRINT(DBG_LEVEL2, "hvec=%d,%d,%d  avec=%d,%d,%d  dvec=%d,%d,%d\n",
				 prms->m_hvec.u.x, prms->m_hvec.u.y, prms->m_hvec.u.z,
				 prms->m_avec.u.x, prms->m_avec.u.y, prms->m_avec.u.z,
				 prms->m_dvec.u.x, prms->m_dvec.u.y, prms->m_dvec.u.z);
	}
	return AKRET_PROC_SUCCEED;
}

/*!
 Acquire acceleration data from acceleration sensor.
 @return If this function succeeds, the return value is AKRET_PROC_SUCCEED.
 Otherwise the return value is AKRET_PROC_FAIL.
 @param[out] prms A pointer to #AK8975PRMS structure.
 */
int16 GetAccVec(AK8975PRMS * prms)
{
	int i;
	int16 acc[3];   /* 将缓存 acc sensor 返回的数据. */
	
	if (AKD_GetAccelerationData(acc) != AKD_SUCCESS) {
		return AKRET_PROC_FAIL;
	}
	// Convert format
	for (i = 0; i < 3; i++) {
		prms->m_avec.v[i] = acc[i];
	}
	
	return AKRET_PROC_SUCCEED;
}
