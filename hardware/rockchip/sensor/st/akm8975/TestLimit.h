/******************************************************************************
 *
 * $Id: TestLimit.h 177 2010-02-26 03:01:09Z rikita $
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
#ifndef AKMD_INC_TESTLIMIT_H
#define AKMD_INC_TESTLIMIT_H

/*** Limit of factory shipment test *******************************************/

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
#define TLIMIT_NO_RST_CNTL				"1-13"
#define TLIMIT_TN_RST_CNTL				"RST_CNTL"
#define TLIMIT_LO_RST_CNTL				0
#define TLIMIT_HI_RST_CNTL				0
#define TLIMIT_NO_RST_ASTC				"1-14"
#define TLIMIT_TN_RST_ASTC				"RST_ASTC"
#define TLIMIT_LO_RST_ASTC				0
#define TLIMIT_HI_RST_ASTC				0
#define TLIMIT_NO_RST_I2CDIS			"1-15"
#define TLIMIT_TN_RST_I2CDIS			"RST_I2CDIS"
#define TLIMIT_LO_RST_I2CDIS_USEI2C		0
#define TLIMIT_HI_RST_I2CDIS_USEI2C		0
#define TLIMIT_LO_RST_I2CDIS_USESPI		1
#define TLIMIT_HI_RST_I2CDIS_USESPI		1
#define TLIMIT_NO_ASAX					"1-17"
#define TLIMIT_TN_ASAX					"ASAX"
#define TLIMIT_LO_ASAX					1
#define TLIMIT_HI_ASAX					254
#define TLIMIT_NO_ASAY					"1-18"
#define TLIMIT_TN_ASAY					"ASAY"
#define TLIMIT_LO_ASAY					1
#define TLIMIT_HI_ASAY					254
#define TLIMIT_NO_ASAZ					"1-19"
#define TLIMIT_TN_ASAZ					"ASAZ"
#define TLIMIT_LO_ASAZ					1
#define TLIMIT_HI_ASAZ					254
#define TLIMIT_NO_WR_CNTL				"1-20"
#define TLIMIT_TN_WR_CNTL				"WR_CNTL"
#define TLIMIT_LO_WR_CNTL				0x0F
#define TLIMIT_HI_WR_CNTL				0x0F

#define TLIMIT_NO_SNG_ST1				"2-3"
#define TLIMIT_TN_SNG_ST1				"SNG_ST1"
#define TLIMIT_LO_SNG_ST1				1
#define TLIMIT_HI_SNG_ST1				1

#define TLIMIT_NO_SNG_HX				"2-4"
#define TLIMIT_TN_SNG_HX				"SNG_HX"
#define TLIMIT_LO_SNG_HX				-4096
#define TLIMIT_HI_SNG_HX				4095

#define TLIMIT_NO_SNG_HY				"2-6"
#define TLIMIT_TN_SNG_HY				"SNG_HY"
#define TLIMIT_LO_SNG_HY				-4096
#define TLIMIT_HI_SNG_HY				4095

#define TLIMIT_NO_SNG_HZ				"2-8"
#define TLIMIT_TN_SNG_HZ				"SNG_HZ"
#define TLIMIT_LO_SNG_HZ				-4096
#define TLIMIT_HI_SNG_HZ				4095

#define TLIMIT_NO_SNG_ST2				"2-10"
#define TLIMIT_TN_SNG_ST2				"SNG_ST2"
#define TLIMIT_LO_SNG_ST2				0
#define TLIMIT_HI_SNG_ST2				0

#define TLIMIT_NO_SLF_ST1				"2-14"
#define TLIMIT_TN_SLF_ST1				"SLF_ST1"
#define TLIMIT_LO_SLF_ST1				1
#define TLIMIT_HI_SLF_ST1				1

#define TLIMIT_NO_SLF_RVHX				"2-15"
#define TLIMIT_TN_SLF_RVHX				"SLF_REVSHX"
#define TLIMIT_LO_SLF_RVHX				-100
#define TLIMIT_HI_SLF_RVHX				100

#define TLIMIT_NO_SLF_RVHY				"2-17"
#define TLIMIT_TN_SLF_RVHY				"SLF_REVSHY"
#define TLIMIT_LO_SLF_RVHY				-100
#define TLIMIT_HI_SLF_RVHY				100

#define TLIMIT_NO_SLF_RVHZ				"2-19"
#define TLIMIT_TN_SLF_RVHZ				"SLF_REVSHZ"
#define TLIMIT_LO_SLF_RVHZ				-1000
#define TLIMIT_HI_SLF_RVHZ				-300

#define TLIMIT_NO_SLF_ST2				"2-21"
#define TLIMIT_TN_SLF_ST2				"SLF_ST2"
#define TLIMIT_LO_SLF_ST2				0
#define TLIMIT_HI_SLF_ST2				0

#endif
