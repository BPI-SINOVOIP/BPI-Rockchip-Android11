/******************************************************************************
 *
 * $Id: main.c 325 2011-08-24 09:34:36Z yamada.rj $
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

#include <sched.h>
#include <pthread.h>
#include <linux/input.h>

#include "FileIO.h"
#include "DispMessage.h"
#include "Measure.h"
#include "misc.h"

#define ERROR_OPTPARSE			(-1)
#define ERROR_INITDEVICE		(-2)
#define ERROR_FUSEROM			(-3)
#define ERROR_GETOPEN_STAT		(-4)
#define ERROR_STARTCLONE		(-5)
#define ERROR_GETCLOSE_STAT		(-6)

#define CONVERT_Q16(x)	((int)((x) * 65536))

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
 @param[in] args A pointer to #AK8963PRMS structure.
 */
static void* thread_main(void* args)
{
	MeasureSNGLoop((AK8963PRMS *) args);
	return ((void*)0);
}

static void signal_handler(int sig)
{
	if (sig == SIGINT) {
		ALOGE("SIGINT signal");
		g_stopRequest = 1;
		g_mainQuit = AKD_TRUE;
	}
}

/*!
 Starts new thread.
 @return If this function succeeds, the return value is 1. Otherwise,
 the return value is 0.
 @param[in] prms A pointer to #AK8963PRMS structure. This pointer is passed
 to the new thread argument.
 */
static int startClone(AK8963PRMS* prms)
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
 @param[in] prms pointer to #AK8963PRMS structure.
 @param[in] flag This flag shows which data contains the valid data.
 The device driver will report only the valid data to HAL layer.
 */
void Disp_MeasurementResultHook(AK8963PRMS* prms, const uint16 flag)
{
	if (!g_opmode) {
		int rbuf[YPR_DATA_SIZE] = { 0 };
		rbuf[0] = flag;				/* Data flag */
		rbuf[1] = prms->m_avec.u.x;	/* Ax */
		rbuf[2] = prms->m_avec.u.y;	/* Ay */
		rbuf[3] = prms->m_avec.u.z;	/* Az */
		rbuf[4] = 3;					/* Acc status */
		rbuf[5] = prms->m_hvec.u.x;	/* Mx */
		rbuf[6] = prms->m_hvec.u.y;	/* My */
		rbuf[7] = prms->m_hvec.u.z;	/* Mz */
		rbuf[8] = prms->m_hdst;		/* Mag status */
		rbuf[9] = prms->m_theta;		/* yaw	(deprecate) */
		rbuf[10] = prms->m_phi180;	/* pitch (deprecate) */
		rbuf[11] = prms->m_eta90;		/* roll  (deprecate) */
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
	AKMD_PATNO*	layout_patno,
	int16*	outbit)
{
	int	opt;
	char	optVal;

	*layout_patno = PAT_INVALID;
	*outbit = OUTBIT_INVALID;

	while ((opt = getopt(argc, argv, "sm:o:z:")) != -1) {
		switch(opt){
			case 'm':
				optVal = (char)(optarg[0] - '0');
				if ((PAT1 <= optVal) && (optVal <= PAT8)) {
					*layout_patno = (AKMD_PATNO)optVal;
					AKMDEBUG(DBG_LEVEL2, "%s: Layout=%d\n", __FUNCTION__, optVal);
				}
				break;
			case 'o':
				optVal = (char)(optarg[0] - '0');
				if ((optVal == OUTBIT_14) || (optVal == OUTBIT_16)) {
					*outbit = optVal;
					AKMDEBUG(DBG_LEVEL2, "%s: outbit=%d\n", __FUNCTION__, *outbit);
				}
				break;
			case 'z':
				/* If error detected, hopefully 0 is returned. */
				errno = 0;
				g_dbgzone = (int)strtol(optarg, (char**)NULL, 0);
				AKMDEBUG(DBG_LEVEL2, "%s: Dbg Zone=%d\n", __FUNCTION__, g_dbgzone);
				break;
			case 's':
				g_opmode = 1;
				break;
			default:
				ALOGE("%s: Invalid argument", argv[0]);
				return 0;
		}
	}

	/* If layout is not specified with argument, get parameter from driver */
	if (*layout_patno == PAT_INVALID) {
		int16_t n;
		if (AKD_GetLayout(&n) == AKD_SUCCESS) {
			if ((PAT1 <= n) && (n <= PAT8)) {
				*layout_patno = (AKMD_PATNO)n;
			}
		}
	}
	/* Error */
	if (*layout_patno == PAT_INVALID) {
		ALOGE("No layout is specified.");
		return 0;
	}
	
	/* If outbit is not specified with argument, get parameter from driver */
	if (*outbit == OUTBIT_INVALID) {
		int16_t b;
		if (AKD_GetOutbit(&b) == AKD_SUCCESS) {
			if ((b == OUTBIT_14) || (b == OUTBIT_16)) {
				*outbit = b;
			}
		}
	}
	/* Error */
	if (*outbit == OUTBIT_INVALID) {
		ALOGE("No outbit is specified.");
		return 0;
	}

	return 1;
}

/*!
 This is main function.
 */
int main(int argc, char **argv)
{
	AK8963PRMS prms;
	int retValue = 0;
	AKMD_PATNO pat;
	int16 outbit;

	/* Show the version info of this software. */
	Disp_StartMessage();

#if ENABLE_AKMDEBUG
	/* Register signal handler */
	signal(SIGINT, signal_handler);
#endif

#if ENABLE_FORMATION
	RegisterFormClass(&s_formClass);
#endif

	/* Open device driver. */
	if (AKD_InitDevice() != AKD_SUCCESS) {
		retValue = ERROR_INITDEVICE;
		goto THE_END_OF_MAIN_FUNCTION;
	}

	/* Parse command-line options */
	if (OptParse(argc, argv, &pat, &outbit) == 0) {
		retValue = ERROR_OPTPARSE;
		goto THE_END_OF_MAIN_FUNCTION;
	}

	/* Initialize parameters structure. */
	InitAK8963PRMS(&prms);

	/* Put argument to PRMS. */
	prms.m_layout = pat;
	prms.m_outbit = outbit;

	/* Read Fuse ROM */
	if (ReadAK8963FUSEROM(&prms) != AKRET_PROC_SUCCEED) {
		retValue = ERROR_FUSEROM;
		goto THE_END_OF_MAIN_FUNCTION;
	}

	/* Here is the Main Loop */
	if (g_opmode) {
		/*** Console Mode *********************************************/
		while (AKD_TRUE) {
			/* Select operation */
			switch (Menu_Main()) {
				case MODE_FctShipmntTestBody:
					FctShipmntTest_Body(&prms);
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

				case MODE_Quit:
					goto THE_END_OF_MAIN_FUNCTION;
					break;

				default:
					AKMDEBUG(DBG_LEVEL0, "Unknown operation mode.\n");
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
				ALOGI("Suspended.");
			} else {
				ALOGI("Compass Opened.");
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
				ALOGI("Compass Closed.");

				/* Write Parameters to file. */
				SaveParameters(&prms);
			}
		}
	}

THE_END_OF_MAIN_FUNCTION:

	/* Close device driver. */
	AKD_DeinitDevice();

	/* Show the last message. */
	Disp_EndMessage(retValue);

	return retValue;
}


