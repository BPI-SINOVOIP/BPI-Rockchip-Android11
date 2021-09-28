/******************************************************************************
 *
 *  $Id: $
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
#include "AKCommon.h"
#include "AKMD_Driver.h"
#include "FST.h"
#include "misc.h"

#ifndef AKMD_FOR_AK09911
#error "AKMD parameter is not set"
#endif

#define TLIMIT_TN_REVISION_09911				""
#define TLIMIT_NO_RST_WIA1_09911				"1-3"
#define TLIMIT_TN_RST_WIA1_09911				"RST_WIA1"
#define TLIMIT_LO_RST_WIA1_09911				0x48
#define TLIMIT_HI_RST_WIA1_09911				0x48
#define TLIMIT_NO_RST_WIA2_09911				"1-4"
#define TLIMIT_TN_RST_WIA2_09911				"RST_WIA2"
#define TLIMIT_LO_RST_WIA2_09911				0x05
#define TLIMIT_HI_RST_WIA2_09911				0x05

#define TLIMIT_NO_ASAX_09911					"1-7"
#define TLIMIT_TN_ASAX_09911					"ASAX"
#define TLIMIT_LO_ASAX_09911					1
#define TLIMIT_HI_ASAX_09911					254
#define TLIMIT_NO_ASAY_09911					"1-8"
#define TLIMIT_TN_ASAY_09911					"ASAY"
#define TLIMIT_LO_ASAY_09911					1
#define TLIMIT_HI_ASAY_09911					254
#define TLIMIT_NO_ASAZ_09911					"1-9"
#define TLIMIT_TN_ASAZ_09911					"ASAZ"
#define TLIMIT_LO_ASAZ_09911					1
#define TLIMIT_HI_ASAZ_09911					254

#define TLIMIT_NO_SNG_ST1_09911				"2-3"
#define TLIMIT_TN_SNG_ST1_09911				"SNG_ST1"
#define TLIMIT_LO_SNG_ST1_09911				1
#define TLIMIT_HI_SNG_ST1_09911				1

#define TLIMIT_NO_SNG_HX_09911				"2-4"
#define TLIMIT_TN_SNG_HX_09911				"SNG_HX"
#define TLIMIT_LO_SNG_HX_09911				-8189
#define TLIMIT_HI_SNG_HX_09911				8189

#define TLIMIT_NO_SNG_HY_09911				"2-6"
#define TLIMIT_TN_SNG_HY_09911				"SNG_HY"
#define TLIMIT_LO_SNG_HY_09911				-8189
#define TLIMIT_HI_SNG_HY_09911				8189

#define TLIMIT_NO_SNG_HZ_09911				"2-8"
#define TLIMIT_TN_SNG_HZ_09911				"SNG_HZ"
#define TLIMIT_LO_SNG_HZ_09911				-8189
#define TLIMIT_HI_SNG_HZ_09911				8189

#define TLIMIT_NO_SNG_ST2_09911				"2-10"
#define TLIMIT_TN_SNG_ST2_09911				"SNG_ST2"
#define TLIMIT_LO_SNG_ST2_09911				0
#define TLIMIT_HI_SNG_ST2_09911				0

#define TLIMIT_NO_SLF_ST1_09911				"2-13"
#define TLIMIT_TN_SLF_ST1_09911				"SLF_ST1"
#define TLIMIT_LO_SLF_ST1_09911				1
#define TLIMIT_HI_SLF_ST1_09911				1

#define TLIMIT_NO_SLF_RVHX_09911				"2-14"
#define TLIMIT_TN_SLF_RVHX_09911				"SLF_REVSHX"
#define TLIMIT_LO_SLF_RVHX_09911				-30
#define TLIMIT_HI_SLF_RVHX_09911				30

#define TLIMIT_NO_SLF_RVHY_09911				"2-16"
#define TLIMIT_TN_SLF_RVHY_09911				"SLF_REVSHY"
#define TLIMIT_LO_SLF_RVHY_09911			-30
#define TLIMIT_HI_SLF_RVHY_09911				30

#define TLIMIT_NO_SLF_RVHZ_09911				"2-18"
#define TLIMIT_TN_SLF_RVHZ_09911				"SLF_REVSHZ"
#define TLIMIT_LO_SLF_RVHZ_09911				-400
#define TLIMIT_HI_SLF_RVHZ_09911				-50

#define TLIMIT_NO_SLF_ST2_09911				"2-20"
#define TLIMIT_TN_SLF_ST2_09911				"SLF_ST2"
#define TLIMIT_LO_SLF_ST2_09911				0
#define TLIMIT_HI_SLF_ST2_09911				0


////below is for AK8963C
#define TLIMIT_TN_REVISION				""
#define TLIMIT_NO_RST_WIA				"1-3"
#define TLIMIT_TN_RST_WIA				"RST_WIA"
#define TLIMIT_LO_RST_WIA				0x48
#define TLIMIT_HI_RST_WIA				0x48
#define TLIMIT_NO_RST_INFO				"1-4"
#define TLIMIT_TN_RST_INFO				"RST_INFO"
#define TLIMIT_LO_RST_INFO				0
#define TLIMIT_HI_RST_INFO				255
#define TLIMIT_NO_RST_ST1				"1-5"
#define TLIMIT_TN_RST_ST1				"RST_ST1"
#define TLIMIT_LO_RST_ST1				0
#define TLIMIT_HI_RST_ST1				0
#define TLIMIT_NO_RST_HXL				"1-6"
#define TLIMIT_TN_RST_HXL				"RST_HXL"
#define TLIMIT_LO_RST_HXL				0
#define TLIMIT_HI_RST_HXL				0
#define TLIMIT_NO_RST_HXH				"1-7"
#define TLIMIT_TN_RST_HXH				"RST_HXH"
#define TLIMIT_LO_RST_HXH				0
#define TLIMIT_HI_RST_HXH				0
#define TLIMIT_NO_RST_HYL				"1-8"
#define TLIMIT_TN_RST_HYL				"RST_HYL"
#define TLIMIT_LO_RST_HYL				0
#define TLIMIT_HI_RST_HYL				0
#define TLIMIT_NO_RST_HYH				"1-9"
#define TLIMIT_TN_RST_HYH				"RST_HYH"
#define TLIMIT_LO_RST_HYH				0
#define TLIMIT_HI_RST_HYH				0
#define TLIMIT_NO_RST_HZL				"1-10"
#define TLIMIT_TN_RST_HZL				"RST_HZL"
#define TLIMIT_LO_RST_HZL				0
#define TLIMIT_HI_RST_HZL				0
#define TLIMIT_NO_RST_HZH				"1-11"
#define TLIMIT_TN_RST_HZH				"RST_HZH"
#define TLIMIT_LO_RST_HZH				0
#define TLIMIT_HI_RST_HZH				0
#define TLIMIT_NO_RST_ST2				"1-12"
#define TLIMIT_TN_RST_ST2				"RST_ST2"
#define TLIMIT_LO_RST_ST2				0
#define TLIMIT_HI_RST_ST2				0
#define TLIMIT_NO_RST_CNTL1				"1-13"
#define TLIMIT_TN_RST_CNTL1				"RST_CNTL1"
#define TLIMIT_LO_RST_CNTL1				0
#define TLIMIT_HI_RST_CNTL1				0
#define TLIMIT_NO_RST_CNTL2				"1-14"
#define TLIMIT_TN_RST_CNTL2				"RST_CNTL2"
#define TLIMIT_LO_RST_CNTL2				0
#define TLIMIT_HI_RST_CNTL2				0

#define TLIMIT_NO_RST_ASTC				"1-15"
#define TLIMIT_TN_RST_ASTC				"RST_ASTC"
#define TLIMIT_LO_RST_ASTC				0
#define TLIMIT_HI_RST_ASTC				0
#define TLIMIT_NO_RST_I2CDIS			"1-16"
#define TLIMIT_TN_RST_I2CDIS			"RST_I2CDIS"
#define TLIMIT_LO_RST_I2CDIS_USEI2C		0
#define TLIMIT_HI_RST_I2CDIS_USEI2C		0
#define TLIMIT_LO_RST_I2CDIS_USESPI		1
#define TLIMIT_HI_RST_I2CDIS_USESPI		1
#define TLIMIT_NO_ASAX					"1-18"
#define TLIMIT_TN_ASAX					"ASAX"
#define TLIMIT_LO_ASAX					1
#define TLIMIT_HI_ASAX					254
#define TLIMIT_NO_ASAY					"1-19"
#define TLIMIT_TN_ASAY					"ASAY"
#define TLIMIT_LO_ASAY					1
#define TLIMIT_HI_ASAY					254
#define TLIMIT_NO_ASAZ					"1-20"
#define TLIMIT_TN_ASAZ					"ASAZ"
#define TLIMIT_LO_ASAZ					1
#define TLIMIT_HI_ASAZ					254
#define TLIMIT_NO_WR_CNTL1				"1-21"
#define TLIMIT_TN_WR_CNTL1				"WR_CNTL1"
#define TLIMIT_LO_WR_CNTL1				0x0F
#define TLIMIT_HI_WR_CNTL1				0x0F

#define TLIMIT_NO_SNG_ST1				"2-3"
#define TLIMIT_TN_SNG_ST1				"SNG_ST1"
#define TLIMIT_LO_SNG_ST1				1
#define TLIMIT_HI_SNG_ST1				1

#define TLIMIT_NO_SNG_HX				"2-4"
#define TLIMIT_TN_SNG_HX				"SNG_HX"
#define TLIMIT_LO_SNG_HX				-32759
#define TLIMIT_HI_SNG_HX				32759

#define TLIMIT_NO_SNG_HY				"2-6"
#define TLIMIT_TN_SNG_HY				"SNG_HY"
#define TLIMIT_LO_SNG_HY				-32759
#define TLIMIT_HI_SNG_HY				32759

#define TLIMIT_NO_SNG_HZ				"2-8"
#define TLIMIT_TN_SNG_HZ				"SNG_HZ"
#define TLIMIT_LO_SNG_HZ				-32759
#define TLIMIT_HI_SNG_HZ				32759

#define TLIMIT_NO_SNG_ST2				"2-10"
#define TLIMIT_TN_SNG_ST2				"SNG_ST2"
#define TLIMIT_LO_SNG_ST2_14BIT			0
#define TLIMIT_HI_SNG_ST2_14BIT			0
#define TLIMIT_LO_SNG_ST2_16BIT			16
#define TLIMIT_HI_SNG_ST2_16BIT			16

#define TLIMIT_NO_SLF_ST1				"2-14"
#define TLIMIT_TN_SLF_ST1				"SLF_ST1"
#define TLIMIT_LO_SLF_ST1				1
#define TLIMIT_HI_SLF_ST1				1

#define TLIMIT_NO_SLF_RVHX				"2-15"
#define TLIMIT_TN_SLF_RVHX				"SLF_REVSHX"
#define TLIMIT_LO_SLF_RVHX				-200
#define TLIMIT_HI_SLF_RVHX				200

#define TLIMIT_NO_SLF_RVHY				"2-17"
#define TLIMIT_TN_SLF_RVHY				"SLF_REVSHY"
#define TLIMIT_LO_SLF_RVHY				-200
#define TLIMIT_HI_SLF_RVHY				200

#define TLIMIT_NO_SLF_RVHZ				"2-19"
#define TLIMIT_TN_SLF_RVHZ				"SLF_REVSHZ"
#define TLIMIT_LO_SLF_RVHZ				-3200
#define TLIMIT_HI_SLF_RVHZ				-800

#define TLIMIT_NO_SLF_ST2				"2-21"
#define TLIMIT_TN_SLF_ST2				"SLF_ST2"
#define TLIMIT_LO_SLF_ST2_14BIT			0
#define TLIMIT_HI_SLF_ST2_14BIT			0
#define TLIMIT_LO_SLF_ST2_16BIT			16
#define TLIMIT_HI_SLF_ST2_16BIT			16


/*!
 @return If @a testdata is in the range of between @a lolimit and @a hilimit,
 the return value is 1, otherwise -1.
 @param[in] testno   A pointer to a text string.
 @param[in] testname A pointer to a text string.
 @param[in] testdata A data to be tested.
 @param[in] lolimit  The maximum allowable value of @a testdata.
 @param[in] hilimit  The minimum allowable value of @a testdata.
 @param[in,out] pf_total
 */
int16
TEST_DATA(const char testno[],
		  const char testname[],
          const int16 testdata,
		  const int16 lolimit,
		  const int16 hilimit,
          int16 * pf_total)
{
	int16 pf;                     //Pass;1, Fail;-1

	if ((testno == NULL) && (strncmp(testname, "START", 5) == 0)) {
		// Display header
		AKMDEBUG(AKMDBG_DISP1, "--------------------------------------------------------------------\n");
		AKMDEBUG(AKMDBG_DISP1, " Test No. Test Name    Fail    Test Data    [      Low         High]\n");
		AKMDEBUG(AKMDBG_DISP1, "--------------------------------------------------------------------\n");

		pf = 1;
	} else if ((testno == NULL) && (strncmp(testname, "END", 3) == 0)) {
		// Display result
		AKMDEBUG(AKMDBG_DISP1, "--------------------------------------------------------------------\n");
		if (*pf_total == 1) {
			AKMDEBUG(AKMDBG_DISP1, "Factory shipment test was passed.\n\n");
		} else {
			AKMDEBUG(AKMDBG_DISP1, "Factory shipment test was failed.\n\n");
		}

		pf = 1;
	} else {
		if ((lolimit <= testdata) && (testdata <= hilimit)) {
			//Pass
			pf = 1;
		} else {
			//Fail
			pf = -1;
		}

		//display result
		AKMDEBUG(AKMDBG_DISP1, " %7s  %-10s      %c    %9d    [%9d    %9d]\n",
				 testno, testname, ((pf == 1) ? ('.') : ('F')), testdata,
				 lolimit, hilimit);
	}

	//Pass/Fail check
	if (*pf_total != 0) {
		if ((*pf_total == 1) && (pf == 1)) {
			*pf_total = 1;            //Pass
		} else {
			*pf_total = -1;           //Fail
		}
	}
	return pf;
}
/*!
 Execute "Onboard Function Test" (NOT includes "START" and "END" command).
 @retval 1 The test is passed successfully.
 @retval -1 The test is failed.
 @retval 0 The test is aborted by kind of system error.
 */
int16 FST_AK8963(void)
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
	if (AKD_Reset() != AKD_SUCCESS) {
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

		// Read values from WIA to HZL.
	if (AKD_RxData(AK8963_REG_WIA, i2cData, 8) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	
		// Read values from HZH to ASTC.
	if (AKD_RxData(AK8963_REG_HZH, &i2cData[8], 5) != AKD_SUCCESS) {
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
	if (AKD_SetMode(AK8963_MODE_SNG_MEASURE) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// Wait for DRDY pin changes to HIGH.
	//usleep(AKM_MEASURE_TIME_US);
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

	if((i2cData[8] & 0x10) == 0){ // 14bit mode  //format changed at kernel part as AK09911
		hdata[0] <<= 2;
		hdata[1] <<= 2;
		hdata[2] <<= 2;
	}	

	// TEST
	TEST_DATA(TLIMIT_NO_SNG_ST1, TLIMIT_TN_SNG_ST1, (int16)i2cData[0], TLIMIT_LO_SNG_ST1, TLIMIT_HI_SNG_ST1, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HX, TLIMIT_TN_SNG_HX, hdata[0], TLIMIT_LO_SNG_HX, TLIMIT_HI_SNG_HX, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HY, TLIMIT_TN_SNG_HY, hdata[1], TLIMIT_LO_SNG_HY, TLIMIT_HI_SNG_HY, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HZ, TLIMIT_TN_SNG_HZ, hdata[2], TLIMIT_LO_SNG_HZ, TLIMIT_HI_SNG_HZ, &pf_total);
	if((i2cData[8] & 0x10) == 0){ // 14bit mode
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
	if (AKD_SetMode(AK8963_MODE_SELF_TEST) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// Wait for DRDY pin changes to HIGH.
	//usleep(AKM_MEASURE_TIME_US);
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

	if((i2cData[8] & 0x10) == 0){ // 14bit mode
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
	if((i2cData[8] & 0x10) == 0){ // 14bit mode
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
 Execute "Onboard Function Test" (NOT includes "START" and "END" command).
 @retval 1 The test is passed successfully.
 @retval -1 The test is failed.
 @retval 0 The test is aborted by kind of system error.
 */
int16 FST_AK09911(void)
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
	if (AKD_Reset() != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// Read values from WIA.
	if (AKD_RxData(AK09911_REG_WIA1, i2cData, 2) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// TEST
	TEST_DATA(TLIMIT_NO_RST_WIA1_09911,   TLIMIT_TN_RST_WIA1_09911,   (int16)i2cData[0],  TLIMIT_LO_RST_WIA1_09911,   TLIMIT_HI_RST_WIA1_09911,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_WIA2_09911,   TLIMIT_TN_RST_WIA2_09911,   (int16)i2cData[1],  TLIMIT_LO_RST_WIA2_09911,   TLIMIT_HI_RST_WIA2_09911,   &pf_total);

	// Set to FUSE ROM access mode
	if (AKD_SetMode(AK09911_MODE_FUSE_ACCESS) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// Read values from ASAX to ASAZ
	if (AKD_RxData(AK09911_FUSE_ASAX, i2cData, 3) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	asax = (int16)i2cData[0];
	asay = (int16)i2cData[1];
	asaz = (int16)i2cData[2];

	// TEST
	TEST_DATA(TLIMIT_NO_ASAX_09911, TLIMIT_TN_ASAX_09911, asax, TLIMIT_LO_ASAX_09911, TLIMIT_HI_ASAX_09911, &pf_total);
	TEST_DATA(TLIMIT_NO_ASAY_09911, TLIMIT_TN_ASAY_09911, asay, TLIMIT_LO_ASAY_09911, TLIMIT_HI_ASAY_09911, &pf_total);
	TEST_DATA(TLIMIT_NO_ASAZ_09911, TLIMIT_TN_ASAZ_09911, asaz, TLIMIT_LO_ASAZ_09911, TLIMIT_HI_ASAZ_09911, &pf_total);

	// Set to PowerDown mode
	if (AKD_SetMode(AK09911_MODE_POWERDOWN) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	//***********************************************
	//  Step2
	//***********************************************

	// Set to SNG measurement pattern (Set CNTL register)
	if (AKD_SetMode(AK09911_MODE_SNG_MEASURE) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// Wait for DRDY pin changes to HIGH.
	//usleep(AKM_MEASURE_TIME_US);
	// Get measurement data from AK09911
	// ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + TEMP + ST2
	// = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 + 1 = 9yte
	if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	hdata[0] = (int16)((((uint16)(i2cData[2]))<<8)+(uint16)(i2cData[1]));
	hdata[1] = (int16)((((uint16)(i2cData[4]))<<8)+(uint16)(i2cData[3]));
	hdata[2] = (int16)((((uint16)(i2cData[6]))<<8)+(uint16)(i2cData[5]));

	// TEST
	i2cData[0] &= 0x7F;
	TEST_DATA(TLIMIT_NO_SNG_ST1_09911,  TLIMIT_TN_SNG_ST1_09911,  (int16)i2cData[0], TLIMIT_LO_SNG_ST1_09911,  TLIMIT_HI_SNG_ST1_09911,  &pf_total);

	// TEST
	TEST_DATA(TLIMIT_NO_SNG_HX_09911,   TLIMIT_TN_SNG_HX_09911,   hdata[0],          TLIMIT_LO_SNG_HX_09911,   TLIMIT_HI_SNG_HX_09911,   &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HY_09911,   TLIMIT_TN_SNG_HY_09911,   hdata[1],          TLIMIT_LO_SNG_HY_09911,   TLIMIT_HI_SNG_HY_09911,   &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HZ_09911,   TLIMIT_TN_SNG_HZ_09911,   hdata[2],          TLIMIT_LO_SNG_HZ_09911,   TLIMIT_HI_SNG_HZ_09911,   &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_ST2_09911,  TLIMIT_TN_SNG_ST2_09911,  (int16)i2cData[8], TLIMIT_LO_SNG_ST2_09911,  TLIMIT_HI_SNG_ST2_09911,  &pf_total);

	// Set to Self-test mode (Set CNTL register)
	if (AKD_SetMode(AK09911_MODE_SELF_TEST) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// Wait for DRDY pin changes to HIGH.
	//usleep(AKM_MEASURE_TIME_US);
	// Get measurement data from AK09911
	// ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + TEMP + ST2
	// = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 + 1 = 9byte
	if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// TEST
	i2cData[0] &= 0x7F;
	TEST_DATA(TLIMIT_NO_SLF_ST1_09911, TLIMIT_TN_SLF_ST1_09911, (int16)i2cData[0], TLIMIT_LO_SLF_ST1_09911, TLIMIT_HI_SLF_ST1_09911, &pf_total);

	hdata[0] = (int16)((((uint16)(i2cData[2]))<<8)+(uint16)(i2cData[1]));
	hdata[1] = (int16)((((uint16)(i2cData[4]))<<8)+(uint16)(i2cData[3]));
	hdata[2] = (int16)((((uint16)(i2cData[6]))<<8)+(uint16)(i2cData[5]));

	// TEST
	TEST_DATA(
			  TLIMIT_NO_SLF_RVHX_09911,
			  TLIMIT_TN_SLF_RVHX_09911,
			  (hdata[0])*(asax/128.0f + 1),
			  TLIMIT_LO_SLF_RVHX_09911,
			  TLIMIT_HI_SLF_RVHX_09911,
			  &pf_total
			  );

	TEST_DATA(
			  TLIMIT_NO_SLF_RVHY_09911,
			  TLIMIT_TN_SLF_RVHY_09911,
			  (hdata[1])*(asay/128.0f + 1),
			  TLIMIT_LO_SLF_RVHY_09911,
			  TLIMIT_HI_SLF_RVHY_09911,
			  &pf_total
			  );

	TEST_DATA(
			  TLIMIT_NO_SLF_RVHZ_09911,
			  TLIMIT_TN_SLF_RVHZ_09911,
			  (hdata[2])*(asaz/128.0f + 1),
			  TLIMIT_LO_SLF_RVHZ_09911,
			  TLIMIT_HI_SLF_RVHZ_09911,
			  &pf_total
			  );

		TEST_DATA(
			TLIMIT_NO_SLF_ST2_09911,
			TLIMIT_TN_SLF_ST2_09911,
			(int16)i2cData[8],
			TLIMIT_LO_SLF_ST2_09911,
			TLIMIT_HI_SLF_ST2_09911,
			&pf_total
			);

	return pf_total;
}

/*!
 Execute "Onboard Function Test" (includes "START" and "END" command).
 @retval 1 The test is passed successfully.
 @retval -1 The test is failed.
 @retval 0 The test is aborted by kind of system error.
 */
int16 FST_Body(AKSCPRMS* prms)
{
	int16 pf_total = 1;

	//***********************************************
	//    Reset Test Result
	//***********************************************
	TEST_DATA(NULL, "START", 0, 0, 0, &pf_total);

	//***********************************************
	//    Step 1 to 2
	//***********************************************
	if(prms->akm_device==0)
		pf_total = FST_AK8963();
	else
		pf_total = FST_AK09911();

	//***********************************************
	//    Judge Test Result
	//***********************************************
	TEST_DATA(NULL, "END", 0, 0, 0, &pf_total);

	return pf_total;
}

