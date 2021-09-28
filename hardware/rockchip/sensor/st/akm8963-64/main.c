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
#include <sched.h>
#include <pthread.h>
#include <linux/input.h>

#include "AKCommon.h"
#include "AKMD_Driver.h"
#include "DispMessage.h"
#include "FileIO.h"
#include "FST.h"
#include "Measure.h"
#include "misc.h"

#define ERROR_OPTPARSE			(-1)
#define ERROR_INITDEVICE		(-2)
#define ERROR_HLAYOUT			(-3)
#define ERROR_FUSEROM			(-4)
#define ERROR_GET_SIZE_DOEP		(-5)
#define ERROR_MALLOC_DOEP		(-6)
#define ERROR_GETOPEN_STAT		(-7)
#define ERROR_STARTCLONE		(-8)
#define ERROR_GETCLOSE_STAT		(-9)

#define CONVERT_Q14_TO_Q16(x)  ((int32_t)((x) * 4))
#define CONVERT_FLOAT_Q16(x)  ((int32_t)((x) * 65536))
#define CONVERT_AKSC_Q16(x) ((int32_t)((x) * 65535 * 9.80665f / 720.0))
#define CONVERT_Q6_DEG_Q16_RAD(x) ((int32_t)((x) * 1024 * 3.141592f / 180.0f))

/* Global variable. See AKCommon.h file. */
int g_stopRequest = 0;
int g_opmode = 0;
int g_dbgzone = 0;
int g_mainQuit = AKD_FALSE;

/* Static variable. */
static pthread_t s_thread;	/*!< Thread handle */
static FORM_CLASS s_formClass = {
	.open  = misc_openForm,
	.close = misc_closeForm,
	.check = misc_checkForm,
};

/*!
 A thread function which is raised when measurement is started.
 @param[in] args A pointer to #AKSCPRMS structure.
 */
static void* thread_main(void* args)
{
	MeasureSNGLoop((AKSCPRMS *)args);
	return ((void*)0);
}

static void signal_handler(int sig)
{
	if (sig == SIGINT) {
		AKMERROR;
		g_stopRequest = 1;
		g_mainQuit = AKD_TRUE;
	}
}

/*!
 Starts new thread.
 @return If this function succeeds, the return value is 1. Otherwise,
 the return value is 0.
 @param[in] prms A pointer to #AKSCPRMS structure. This pointer is passed
 to the new thread argument.
 */
static int startClone(AKSCPRMS* prms)
{
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	g_stopRequest = 0;
	if (pthread_create(&s_thread, &attr, thread_main, prms) == 0) {
		return 1;
	} else {
		return 0;
	}
}

/*!
 Output measurement result. If this application is run as ConsoleMode,
 the measurement result is output to console. If this application is run as
 DaemonMode, the measurement result is written to device driver.
 @param[in] prms pointer to #AKSCPRMS structure.
 @param[in] flag This flag shows which data contains the valid data.
 The device driver will report only the valid data to HAL layer.
 */
 #if 0 
void Disp_MeasurementResultHook(AKSCPRMS* prms, const uint16 flag)
 {
	 int32_t rbuf[AKM_YPR_DATA_SIZE] = { 0 };
	 int16_t totalHDST = 0;
	 int16_t i = 0;

	 /* Adjust magnetic reliability based on the level of each algorithm */
	 if (prms->m_en_doeplus == 1) {
		 if ((prms->m_hdst == 3) && (prms->m_doep_lv <= 2)){
			 totalHDST = 2;
		 } else if ((prms->m_hdst == 2) && (prms->m_doep_lv <= 1)){
			 totalHDST = 1;
		 } else {
			 totalHDST = prms->m_hdst;
		 }
		 AKMDEBUG(AKMDBG_DOEPLUS, "DOE level: %2d, %2d, %2d\n",
			 prms->m_hdst, prms->m_doep_lv, totalHDST);
	 } else {
		 totalHDST = prms->m_hdst;
	 }

	 /* Coordinate system is already converted to Android */
	 rbuf[0] = flag;				/* Data flag */
	 rbuf[1] = prms->m_avec.u.x;	/* Ax */
	 rbuf[2] = prms->m_avec.u.y;	/* Ay */
	 rbuf[3] = prms->m_avec.u.z;	/* Az */
	 rbuf[4] = 3;
	 /* Acc status */
	 rbuf[5] = prms->m_hvec.u.x;	/* Mx */
	 rbuf[6] = prms->m_hvec.u.y;	/* My */
	 rbuf[7] = prms->m_hvec.u.z;	/* Mz */
	 rbuf[8] = totalHDST;		/* Mag status */
	 /* Gyr: Q16 Format (rad/sec) */
	 rbuf[9]  = CONVERT_Q6_DEG_Q16_RAD(prms->m_pgout.u.x);	/* Gy_x */
	 rbuf[10] = CONVERT_Q6_DEG_Q16_RAD(prms->m_pgout.u.y);	/* Gy_y */
	 rbuf[11] = CONVERT_Q6_DEG_Q16_RAD(prms->m_pgout.u.z);	/* Gy_z */
 /*	if(rbuf[9] == 0)
	 {
	     i++;
	     rbuf[9] = i + 1;
	     ALOGE("AKMD2 gyro.x = %d,i = %d\n",rbuf[9],i);
	 }
	 else
	 {
	     i = 0;
	 }*/
	 rbuf[12] = totalHDST;		/* Gyro status */
	 /* Orientation Q6 format (degree)*/
	 rbuf[13] = prms->m_theta;	/* yaw   */
	 rbuf[14] = prms->m_phi180;	/* pitch */
	 rbuf[15] = prms->m_eta90;	/* roll  */
	 ALOGI("--------------rbuf[]1 2 3, 5 6 7, 13 14 15 =%d %d %d,%d %d %d, %d %d %d\n",rbuf[1],rbuf[2],rbuf[3],rbuf[5],rbuf[6],rbuf[7],rbuf[13]/64,rbuf[14]/64,rbuf[15]/64);
	 /* Gravity (from AKSC format to Q16 format) */
	 rbuf[16] = CONVERT_AKSC_Q16(prms->m_pgGravity.u.x);
	 rbuf[17] = CONVERT_AKSC_Q16(prms->m_pgGravity.u.y);
	 rbuf[18] = CONVERT_AKSC_Q16(prms->m_pgGravity.u.z);
	 /* LinAcc (from Float to Q16 format) */
	 rbuf[19] = CONVERT_AKSC_Q16(prms->m_pgLinAcc.u.x);
	 rbuf[20] = CONVERT_AKSC_Q16(prms->m_pgLinAcc.u.y);
	 rbuf[21] = CONVERT_AKSC_Q16(prms->m_pgLinAcc.u.z);
	 /* RotVec Q14 format */
	 rbuf[22] = CONVERT_Q14_TO_Q16(prms->m_pgquat.u.x);
	 rbuf[23] = CONVERT_Q14_TO_Q16(prms->m_pgquat.u.y);
	 rbuf[24] = CONVERT_Q14_TO_Q16(prms->m_pgquat.u.z);
	 rbuf[25] = CONVERT_Q14_TO_Q16(prms->m_pgquat.u.w);
	 AKD_SetYPR(rbuf);

	 if (g_opmode & OPMODE_CONSOLE) {
		 Disp_MeasurementResult(prms);
	 }
 }
#endif

void Disp_MeasurementResultHook(AKSCPRMS* prms, const uint16 flag)
{
	if (!g_opmode) {
		int rbuf[AKM_YPR_DATA_SIZE] = { 0 };
	int16_t totalHDST = 0;
	/* Adjust magnetic reliability based on the level of each algorithm */
	if (prms->m_en_doeplus == 1) {
		if ((prms->m_hdst == 3) && (prms->m_doep_lv <= 2)){
			totalHDST = 2;
		} else if ((prms->m_hdst == 2) && (prms->m_doep_lv <= 1)){
			totalHDST = 1;
		} else {
			totalHDST = prms->m_hdst;
		}
		AKMDEBUG(AKMDBG_DOEPLUS, "DOE level: %2d, %2d, %2d\n",
			prms->m_hdst, prms->m_doep_lv, totalHDST);
	} else {
		totalHDST = prms->m_hdst;
	}
	
	//ALOGE("%s: totalHDST=%d, prms->m_hdst=%d\n", __FUNCTION__, 
	//	totalHDST, prms->m_hdst);
	
	rbuf[0] = flag;				/* Data flag */
	rbuf[1] = prms->m_avec.u.x;	/* Ax */
	rbuf[2] = prms->m_avec.u.y;	/* Ay */
	rbuf[3] = prms->m_avec.u.z;	/* Az */
	rbuf[4] = 3;					/* Acc status */
	rbuf[5] = prms->m_hvec.u.x;	/* Mx */
	rbuf[6] = prms->m_hvec.u.y;	/* My */
	rbuf[7] = prms->m_hvec.u.z;	/* Mz */
	rbuf[8] = totalHDST;		/* Mag status */
	rbuf[9] = prms->m_theta;		/* yaw	(deprecate) x*/
	rbuf[10] = prms->m_phi180;	/* pitch (deprecate) y*/
	rbuf[11] = prms->m_eta90;		/* roll  (deprecate) z*/
		AKD_SetYPR(rbuf);
	} else {
		Disp_MeasurementResult(prms);
	}
}

/*!
 This function parse the option.
 @retval 1 Parse succeeds.
 @retval 0 Parse failed.
 @param[in] argc Argument count
 @param[in] argv Argument vector
 @param[out] layout_patno
 */
int OptParse(
	int	argc,
	char*	argv[],
	AKMD_PATNO*	hlayout_patno,
	int16*	en_doeplus,
	int16*	pg_filter)
{
	int	opt;
	char optVal;

	/* Initial value */
	*hlayout_patno = PAT_INVALID;
	*en_doeplus = CSPEC_ENABLE_DOEPLUS;
	*pg_filter = 0;  //default

	while ((opt = getopt(argc, argv, "dsm:z:p:f:")) != -1) {
		switch(opt){
			case 'm':
				optVal = (char)(optarg[0] - '0');
				if ((PAT1 <= optVal) && (optVal <= PAT8)) {
					*hlayout_patno = (AKMD_PATNO)optVal;
				}
				break;
			case 'z':
				/* If error detected, hopefully 0 is returned. */
				g_dbgzone = (int)strtol(optarg, (char**)NULL, 0);
				break;
			case 's':
				g_opmode |= OPMODE_CONSOLE;
				break;

			case 'p':
				/* DOEPlus enable/disable */
				optVal = (char)(optarg[0] - '0');
				if ((0 == optVal) || (1 == optVal)) {
					*en_doeplus = optVal;
				}
				break;
			case 'f':
				/* DOEPlus enable/disable */
				optVal = (char)(optarg[0] - '0');
				if ((0 <= optVal) && (optVal<8) ){
					*pg_filter = optVal;
				}
			break;
			case 'd': //Enable AKM RAW data LOG
				g_akmlog_enable = 1;
				break;
				
			default:
				ALOGE("%s: Invalid argument", argv[0]);
				return 0;
		}
	}

	AKMDEBUG(AKMDBG_DEBUG, "%s: Mode=0x%04X\n", __FUNCTION__, g_opmode);
	AKMDEBUG(AKMDBG_DEBUG, "%s: Layout=%d\n", __FUNCTION__, *hlayout_patno);
	AKMDEBUG(AKMDBG_DEBUG, "%s: Dbg Zone=0x%04X\n", __FUNCTION__, g_dbgzone);

	return 1;
}

/*!
 This is main function.
 */
int main(int argc, char **argv)
{
	AKSCPRMS prms;
	int retValue = 0;
	int16_t size;

	ALOGI("start in akmd\n");
	/* Show the version info of this software. */
	Disp_StartMessage();

	g_akmlog_enable=0;

#if ENABLE_AKMDEBUG
	/* Register signal handler */
	signal(SIGINT, signal_handler);
#endif

#if ENABLE_FORMATION
	RegisterFormClass(&s_formClass);
#endif

	/* Initialize parameters structure. */
	InitAKSCPRMS(&prms);

	/* Parse command-line options */
	if (OptParse(argc, argv, &prms.m_hlayout, &prms.m_en_doeplus,&prms.PG_filter) == 0) {
		retValue = ERROR_OPTPARSE;
		goto THE_END_OF_MAIN_FUNCTION;
	}

	/* Open device driver. */
	if (AKD_InitDevice() != AKD_SUCCESS) {
		retValue = ERROR_INITDEVICE;
		goto THE_END_OF_MAIN_FUNCTION;
	}

	/* If layout is not specified with argument, get parameter from driver */
	if (prms.m_hlayout == PAT_INVALID) {
		int16_t n;
		if (AKD_GetLayout(&n) == AKD_SUCCESS) {
			if ((PAT1 <= n) && (n <= PAT8)) {
				prms.m_hlayout = (AKMD_PATNO)n;
			}
		}
		/* Error */
		if (prms.m_hlayout == PAT_INVALID) {
			ALOGE("Magnetic sensor's layout is not specified.");
			retValue = ERROR_HLAYOUT;
			goto THE_END_OF_MAIN_FUNCTION;
		}
	}

	/* Read Fuse ROM */
	if (ReadFUSEROM(&prms) != AKRET_PROC_SUCCEED) {
		retValue = ERROR_FUSEROM;
		goto THE_END_OF_MAIN_FUNCTION;
	}

	/* PDC */
	LoadPDC(&prms);

	/* malloc DOEPlus prms*/
	size = AKSC_GetSizeDOEPVar();
	if(size <= 0){
		retValue = ERROR_GET_SIZE_DOEP;
		goto THE_END_OF_MAIN_FUNCTION;
	}
	prms.m_doep_var = (AKSC_DOEPVAR *)malloc(size*sizeof(int32));
	if(prms.m_doep_var == NULL){
		retValue = ERROR_MALLOC_DOEP;
		goto THE_END_OF_MAIN_FUNCTION;
	}

	/* Here is the Main Loop */
	if (g_opmode & OPMODE_CONSOLE) {
		/*** Console Mode *********************************************/
		while (AKD_TRUE) {
			/* Select operation */
			switch (Menu_Main()) {
				case MODE_FST:
					FST_Body(&prms);
					break;

				case MODE_MeasureSNG:
					/* Read Parameters from file. */
					if (LoadParameters(&prms) == 0) {
						SetDefaultPRMS(&prms);
					}
					/* Reset flag */
					g_stopRequest = 0;
					/* Measurement routine */
					MeasureSNGLoop(&prms);

					/* Write Parameters to file. */
					SaveParameters(&prms);
					break;

				case MODE_OffsetCalibration:
					/* Read Parameters from file. */
					if (LoadParameters(&prms) == 0) {
						SetDefaultPRMS(&prms);
					}
					/* measure offset (NOT sensitivity) */
					if (SimpleCalibration(&prms) == AKRET_PROC_SUCCEED) {
						SaveParameters(&prms);
					}
					break;

				case MODE_Quit:
					goto THE_END_OF_MAIN_FUNCTION;
					break;

				default:
					AKMDEBUG(AKMDBG_DEBUG, "Unknown operation mode.\n");
					break;
			}
		}
	} else {
		/*** Daemon Mode *********************************************/
		while (g_mainQuit == AKD_FALSE) {
			int st = 0;
			/* Wait until device driver is opened. */
			if (AKD_GetOpenStatus(&st) != AKD_SUCCESS) {
				retValue = ERROR_GETOPEN_STAT;
				goto THE_END_OF_MAIN_FUNCTION;
			}
			if (st == 0) {
				AKMDEBUG(AKMDBG_DEBUG, "Suspended.");
			} else {
				AKMDEBUG(AKMDBG_DEBUG, "Compass Opened.");
				/* Read Parameters from file. */
				if (LoadParameters(&prms) == 0) {
					SetDefaultPRMS(&prms);
				}
				/* Reset flag */
				g_stopRequest = 0;
				/* Start measurement thread. */
				if (startClone(&prms) == 0) {
					retValue = ERROR_STARTCLONE;
					goto THE_END_OF_MAIN_FUNCTION;
				}

				/* Wait until device driver is closed. */
				if (AKD_GetCloseStatus(&st) != AKD_SUCCESS) {
					retValue = ERROR_GETCLOSE_STAT;
					g_mainQuit = AKD_TRUE;
				}
				/* Wait thread completion. */
				g_stopRequest = 1;
				pthread_join(s_thread, NULL);
				AKMDEBUG(AKMDBG_DEBUG, "Compass Closed.");

				/* Write Parameters to file. */
				SaveParameters(&prms);
			}
		}
	}

	/* free */
	free(prms.m_doep_var);
	prms.m_doep_var = NULL;

THE_END_OF_MAIN_FUNCTION:

	/* Close device driver. */
	AKD_DeinitDevice();

	/* Show the last message. */
	Disp_EndMessage(retValue);

	return retValue;
}


