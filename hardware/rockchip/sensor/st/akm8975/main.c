/******************************************************************************
 *
 * $Id: main.c 181 2010-03-01 06:27:13Z rikita $
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

#include <sched.h>
#include <pthread.h>
#include <linux/input.h>

#include "FileIO.h"
#include "DispMessage.h"
#include "Measure.h"

//#define ENABLE_DEBUG_LOG
#include "custom_log.h"


extern int g_file;			/*!< FD to AK8975 device file. @see : "MSENSOR_NAME" */
extern int s_stopRequest;	/*!< 0:Not stop,  1:Stop */
extern int s_opmode;		/*!< 0:Daemon mode, 1:Console mode */
static pthread_t s_thread;	/*!< Thread handle : 对 measure thread 的 引用. */

/*!
 A thread function which is raised when measurement is started.
 @param[in] args A pointer to #AK8975PRMS structure.
 */
static void *thread_main(void *args)
{
	MeasureSNGLoop((AK8975PRMS *) args);
	return 0;
}

/*!
 Starts new thread.
 @return If this function succeeds, the return value is 1. Otherwise,
 the return value is 0.
 @param[in] prms A pointer to #AK8975PRMS structure. This pointer is passed
 to the new thread argument.
 */
static int startClone(AK8975PRMS * prms)
{
	pthread_attr_t attr;
	
	pthread_attr_init(&attr);
	s_stopRequest = 0;
    V("m_hs : [%d, %d, %d].", (prms->m_hs).v[0], (prms->m_hs).v[1], (prms->m_hs).v[2] );
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
 @param[in] prms pointer to #AK8975PRMS structure.
 */
void Disp_MeasurementResultHook(AK8975PRMS * prms)
{
    /* 若 "不是" console mode, 则... */
	if (!s_opmode) {
		short rbuf[12] = { 0 };
		rbuf[0] = prms->m_theta;     // yaw
		rbuf[1] = prms->m_phi180;    // pitch
		rbuf[2] = prms->m_eta90;     // roll
		rbuf[3] = 25;                // tmp (AK8975 doesn't have temperature sensor)
		rbuf[4] = prms->m_hdst;      // m_stat
		rbuf[5] = 3;                 // g_stat
		rbuf[6] = prms->m_avec.u.x;  // G_Sensor x
		rbuf[7] = prms->m_avec.u.y;  // G_Sensor y
		rbuf[8] = prms->m_avec.u.z;  // G_Sensor z
		rbuf[9] = prms->m_hvec.u.x;  // M_x
		rbuf[10] = prms->m_hvec.u.y; // M_y
		rbuf[11] = prms->m_hvec.u.z; // M_z
        D_WHEN_REPEAT(100, "yaw = %d, pitch = %d, roll = %d; M_x = %d, M_y = %d, M_z = %d.", rbuf[0], rbuf[1], rbuf[2], rbuf[9], rbuf[10], rbuf[11] );
        /* .! : 将计算得到的结果回写到驱动. */
		ioctl(g_file, ECS_IOCTL_SET_YPR, &rbuf);        // 之后, 驱动会将该数据上报 sensor HAL. 
	} 
    /* 否则, ... */
    else {
		Disp_MeasurementResult(prms);
	}
}

/* .! :
 This is main function.
 */
int main(int argc, char **argv)
{
	AK8975PRMS prms;
	int retValue = 0;
	int mainQuit = FALSE;
	int i;
	
	for (i = 1; i < argc; i++) {
		if (0 == strncmp("-s", argv[i], 2)) {
			s_opmode = 1;
		}
	}
	
#ifdef COMIF
	//// Initialize Library
	//// AKSC_Version functions are called in Disp_StartMessage().
	//// Therefore, AKSC_ActivateLibrary function has to be called previously.
	//retValue = AKSC_ActivateLibrary(GUID_AKSC);
	//if (retValue < 0) {
	//	LOGE("akmd2 : Failed to activate library (%d).\n", retValue);
	//	goto THE_END_OF_MAIN_FUNCTION;
	//}
	//LOGI("akmd2 : Activation succeeded (%d).\n", retValue);
#endif
	
	// Show the version info of this software.
	Disp_StartMessage();
	
	// Open device driver.
	if (AKD_InitDevice() != AKD_SUCCESS) {
		LOGE("akmd2 : Device initialization failed.\n");
		retValue = -1;
		goto THE_END_OF_MAIN_FUNCTION;
	}
	
	// Initialize parameters structure.
	InitAK8975PRMS(&prms);
	
	// Read Fuse ROM
	if (ReadAK8975FUSEROM(&prms) == 0) {
		LOGE("akmd2 : Fuse ROM read failed.\n");
		retValue = -2;
		goto THE_END_OF_MAIN_FUNCTION;
	}

	// Here is the Main Loop
	if (s_opmode) {
		//*** Console Mode *********************************************
		while (mainQuit == FALSE) {
			// Select operation
			switch (Menu_Main()) {
				case MODE_FctShipmntTestBody:
					FctShipmntTest_Body(&prms);
					break;
					
				case MODE_MeasureSNG:
					// Read Parameters from file.
					if (LoadParameters(&prms) == 0) {
						LOGE("akmd2 : Setting file can't be read.\n");
						SetDefaultPRMS(&prms);
					}
					
#ifdef COMIF
					//// Activation
					//retValue = AKSC_ActivateLibrary(GUID_AKSC);
					//if (retValue < 0) {
					//	LOGE("akmd2 : Failed to activate library (%d).\n", retValue);
					//	goto THE_END_OF_MAIN_FUNCTION;
					//}
					//LOGI("akmd2 : Activation succeeded (%d).\n", retValue);
#endif
					// Measurement routine
					MeasureSNGLoop(&prms);
					
					// Write Parameters to file.
					if (SaveParameters(&prms) == 0) {
						LOGE("akmd2 : Setting file can't be saved.\n");
					}
					break;
					
				case MODE_Quit:
					mainQuit = TRUE;
					break;
					
				default:
					DBGPRINT(DBG_LEVEL1, "Unknown operation mode.\n");
					break;
			}
		}
	} else {
		//*** Daemon Mode *********************************************
        I("AKMD runs in daemon mode.");
		while (mainQuit == FALSE) {
			int st = 0;
            // .! : 
			// Wait until device driver is opened.
			if (ioctl(g_file, ECS_IOCTL_GET_OPEN_STATUS, &st) < 0) {
				retValue = -3;
				goto THE_END_OF_MAIN_FUNCTION;
			}
			if (st == 0) {
				LOGI("akmd2 : Suspended.\n");
			} else {
				LOGI("akmd2 : Compass Opened.\n");
                V("m_hs : [%d, %d, %d].", (prms.m_hs).v[0], (prms.m_hs).v[1], (prms.m_hs).v[2] );
				// Read Parameters from file.
				if (LoadParameters(&prms) == 0) {
					LOGE("akmd2 : Setting file can't be read.\n");
					SetDefaultPRMS(&prms);
				}
                V("m_hs : [%d, %d, %d].", (prms.m_hs).v[0], (prms.m_hs).v[1], (prms.m_hs).v[2] );
				
#ifdef COMIF
				//// Activation
				//retValue = AKSC_ActivateLibrary(GUID_AKSC);
				//if (retValue < 0) {
				//	LOGE("akmd2 : Failed to activate library (%d).\n", retValue);
				//	retValue = -4;
				//	goto THE_END_OF_MAIN_FUNCTION;
				//}
				//LOGI("akmd2 : Activation succeeded (%d).\n", retValue);
#endif
				
                // .! : 
				// Start measurement thread.
				if (startClone(&prms) == 0) {
					retValue = -5;
					goto THE_END_OF_MAIN_FUNCTION;
				}
				
				// Wait until device driver is closed.
				if (ioctl(g_file, ECS_IOCTL_GET_CLOSE_STATUS, &st) < 0) {
					retValue = -6;
					goto THE_END_OF_MAIN_FUNCTION;
				}
				// Wait thread completion.
				s_stopRequest = 1;
				pthread_join(s_thread, NULL);
				LOGI("akmd2 : Compass Closed.\n");
				// Write Parameters to file.
				if (SaveParameters(&prms) == 0) {
					LOGE("akmd2 : Setting file can't be saved.\n");
				}
			}
		}
	}
	
THE_END_OF_MAIN_FUNCTION:
	
#ifdef COMIF
	//// Close library
	//AKSC_ReleaseLibrary(); 
	//LOGI("akmd2 : Library released.\n");
#endif
	
	// Close device driver.
	AKD_DeinitDevice();
	
	// Show the last message.
	Disp_EndMessage();
	
	return retValue;
}
