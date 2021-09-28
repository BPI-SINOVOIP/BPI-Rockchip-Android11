/*  --------------------------------------------------------------------------------------------------------
 *  Copyright(C), 2010-2011, Fuzhou Rockchip Co. ,Ltd.  All Rights Reserved.
 *
 *  File:   Acc_mma8452.c
 *
 *  Desc:   ʵ�� akmd8975 Ҫ��� accelerometer sensor �Ĺ��ܽӿ� : ����, �ر�, ��ȡ���ݵ�. 
 *
 *          -----------------------------------------------------------------------------------
 *          < ϰ�� �� ������ > : 
 *
 *              acc : accelerometer sensor, "g sensor" �� ����. 
 *
 *          -----------------------------------------------------------------------------------
 *          < �ڲ�ģ�� or ����Ĳ�νṹ > :
 *
 *          -----------------------------------------------------------------------------------
 *          < ����ʵ�ֵĻ������� > :
 *
 *          -----------------------------------------------------------------------------------
 *          < �ؼ���ʶ�� > : 
 *
 *          -----------------------------------------------------------------------------------
 *          < ��ģ��ʵ���������ⲿģ�� > : 
 *              ... 
 *          -----------------------------------------------------------------------------------
 *  Note:
 *
 *  Author: ChenZhen
 *
 *  --------------------------------------------------------------------------------------------------------
 *  Version:
 *          v1.0 : init version
 *  --------------------------------------------------------------------------------------------------------
 *  Log:
	----Wed Jan 19 18:55:55 2011            v1.0
 *
 *  --------------------------------------------------------------------------------------------------------
 */


/* ---------------------------------------------------------------------------------------------------------
 * Include Files
 * ---------------------------------------------------------------------------------------------------------
 */
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "mma8452_kernel.h"

#include "AKCommon.h"
#include "AKMD_Driver.h"
#include "Acc_mma8452.h"

//#define ENABLE_DEBUG_LOG
#include "custom_log.h"

/* ---------------------------------------------------------------------------------------------------------
 * Local Macros 
 * ---------------------------------------------------------------------------------------------------------
 */

/** path to accelerometer control device. */
#define ASENSOR_PATH "/dev/mma8452_daemon"

/** ������ͬ�� ���ٶ���������ʱ, "Android �ϲ�ʹ�õ� ��ֵ" �� "sensor �����豸�ͳ��� ��ֵ" ֮��ı�ֵ. */
#define ACCELERATION_RATIO_ANDROID_TO_HW        (9.80665f / 1000000)

/** "sDisableAccTimer" �ڱ� app ģ�� scope �еı�ʶ ID. */
#define APP_TIME_ID__DISABLE_ACC    (1)

/* ---------------------------------------------------------------------------------------------------------
 * Local Typedefs 
 * ---------------------------------------------------------------------------------------------------------
 */

typedef char bool;

#undef TRUE
#define TRUE 1

#undef FALSE
#define FALSE 0

/* ---------------------------------------------------------------------------------------------------------
 * External Function Prototypes (referenced in other file)
 * ---------------------------------------------------------------------------------------------------------
 */


/* ---------------------------------------------------------------------------------------------------------
 * Local Function Prototypes
 * ---------------------------------------------------------------------------------------------------------
 */
void onTimeOut(sigval_t v);

/* ---------------------------------------------------------------------------------------------------------
 * Local Variables 
 * ---------------------------------------------------------------------------------------------------------
 */

/** acc(g sensor) �����豸(ASENSOR_PATH) �� FD. */
static int sAccFd = -1;

/** timer, ���ڶ�ʱ��ʱ֮�� disable acc. ������ "ϵͳ timer ID", ��̬����. */
static timer_t sDisableAccTimer = NULL;
/** ���� ENABLE_TIME_OUT ����, akmd8975 û���ٴζ�ȡ acc ����, �� disable acc �豸. */
static const int ENABLE_TIME_OUT = 5; 

/** ��ʶ��ǰģ���Ƿ� ʹ���� acc �豸(Ҫ����ɼ� g sensor ����). */
static bool sHasEnabledAcc = FALSE;

/* ---------------------------------------------------------------------------------------------------------
 * Global Variables
 * ---------------------------------------------------------------------------------------------------------
 */


/* ---------------------------------------------------------------------------------------------------------
 * Global Functions Implementation
 * ---------------------------------------------------------------------------------------------------------
 */

/**
 * Open device driver.
 * This function opens device driver of acceleration sensor.
 * Additionally, measurement range is set to +/- 2G mode, bandwidth is set to 25Hz.
 * @return 
 *      If this function succeeds, the return value is #AKD_SUCCESS.
 *      Otherwise the return value is #AKD_FAIL.
 */
int16_t Acc_InitDevice(void)
{
    D("Entered.");
    int16_t result = AKD_SUCCESS;
    struct sigevent se;     /* ����������� timer ��ʱ�¼��Ķ��ƴ���ʽ. */
    struct itimerspec ts;   /* �����ڶ��峬ʱʱ��. */

    /* ���Դ� acc �����豸�ļ�. */
    if ( 0 > (sAccFd = open(ASENSOR_PATH, O_RDONLY ) ) )
    {
        E("failed to open acc file '%s', error is '%s'", ASENSOR_PATH, strerror(errno));
        result = AKD_FAIL;
        goto EXIT;
    }
    
    /* < ���� sDisableAccTimer. > */
    memset(&se, 0, sizeof(se) );
    se.sigev_notify = SIGEV_THREAD; /* "A notification function will be called to perform notification". */
    se.sigev_notify_function = onTimeOut;   /* ֪ͨ�ص�����. */
    se.sigev_value.sival_int = APP_TIME_ID__DISABLE_ACC;    /* sigev_value : sigev_notify_function �Ļص�ʵ��. */
    if ( timer_create (CLOCK_REALTIME, &se, &sDisableAccTimer) < 0 ) 
    {
        E("failed create 'sDisableAccTimer'; error is '%s'.", strerror(errno));
        result = AKD_FAIL;
        goto EXIT;
    }
    // sDisableAccTimer ���� Acc_GetAccelerationData() ������. 

    sHasEnabledAcc = FALSE;     // ��ʽ��ʼ��. 
    
EXIT:
    if ( AKD_SUCCESS != result ) 
    {
        if ( NULL != sDisableAccTimer )
        {
            timer_delete(sDisableAccTimer);
            sDisableAccTimer = NULL;
        }
        if ( -1 != sAccFd )
        {
            close(sAccFd);
            sAccFd = -1;
        }
    }
    
    return result;
}

/** 
 * Close device driver.
 * This function closes device drivers of acceleration sensor.
 */
void Acc_DeinitDevice(void)
{
    D("Entered.");

    timer_delete(sDisableAccTimer);
    sDisableAccTimer = NULL;

    /* �� acc �豸�Ѿ�������, ��... */
    if ( sHasEnabledAcc ) 
    {
        D("to call 'GSENSOR_IOCTL_CLOSE'.");
        if ( ioctl(sAccFd, GSENSOR_IOCTL_CLOSE) < 0 )
        {
            E("failed to disable acc device.");
        }
        sHasEnabledAcc = FALSE;
    }

    close(sAccFd);
    sAccFd = -1;
}

/**
 * Acquire acceleration data from acceleration sensor and convert it to Android coordinate system.
 * .! : Ŀǰ���Ϊ ��������������ʽ. 
 * @param[out] fData 
 *          A acceleration data array.
 *          The coordinate system of the acquired data follows the definition of Android.
 * @return 
 *          If this function succeeds, the return value is #AKD_SUCCESS.
 *          Otherwise the return value is #AKD_FAIL.
 *
 */
int16_t Acc_GetAccelerationData(float fData[3])
{
    int16_t result = AKD_SUCCESS;
    struct itimerspec ts;   /* �����ڶ��峬ʱʱ��. */

    struct sensor_axis accData = {0, 0, 0};

    /* ����δʹ�� acc, ��... */
    if ( !sHasEnabledAcc )
    {
        /* ʹ�� acc. */
        if ( 0 > ioctl(sAccFd, GSENSOR_IOCTL_START) ) 
        {
            E("failed to START acc device; error is '%s'.", strerror(errno));
            result = AKD_FAIL;
            goto EXIT;
        }
        /* ���ò�����. */
		int sample_rate = MMA8452_RATE_12P5;        // .! : ��ʱ��ʹ�� 12.5 Hz.
        if ( 0 > ioctl(sAccFd, GSENSOR_IOCTL_APP_SET_RATE, &sample_rate) ) 
        {
            E("failed to set sample rete of acc device; error is '%s'.", strerror(errno));
            result = AKD_FAIL;
            goto EXIT;
        }
        /* ��λ��ʶ. */
        sHasEnabledAcc = TRUE;
    }
    
    /* ��ȡ acc sensor ����. */ // .! : ����������. 
    if ( 0 > ioctl(sAccFd, GSENSOR_IOCTL_GETDATA, &accData) )
    {
        E("failed to GET acc data, error is '%s.'", strerror(errno));
        result = AKD_FAIL;
        goto EXIT;
    }
    
    /* ���� sDisableAccTimer. */
    // D("to reset 'sDisableAccTimer'.");
    memset(&ts, 0, sizeof(ts) );
    ts.it_value.tv_sec = ENABLE_TIME_OUT;
    if ( timer_settime(sDisableAccTimer, 0, &ts, NULL) < 0 ) 
    {
        E("failed start 'sDisableAccTimer'; error is '%s'.", strerror(errno));
        result = AKD_FAIL;
        goto EXIT;
    }

    /* ת��Ϊ Android ����ĸ�ʽ, ������. */    // .! : �� HAL �� MmaSensor.cpp ��һ��, ʹ�� Ĭ�Ϻ������궨�� g sensor ����.
    fData[0] = ( (accData.x) * ACCELERATION_RATIO_ANDROID_TO_HW);
    fData[1] = ( (accData.y) * ACCELERATION_RATIO_ANDROID_TO_HW);
    fData[2] = ( (accData.z) * ACCELERATION_RATIO_ANDROID_TO_HW);
    D_WHEN_REPEAT(100, "got acc sensor data : x = %f, y = %f, z = %f.", fData[0], fData[1], fData[2] );
    
EXIT:
    return result;
}

int16_t Acc_SetEnable(const int8_t enabled)
{
	/* AOT cannot control device */
	return AKD_SUCCESS;
}

int16_t Acc_SetDelay(const int64_t ns)
{
	/* AOT cannot control device */
	return AKD_SUCCESS;
}


int16_t Acc_GetAccOffset(int16_t offset[3])
{
	offset[0] = 0;
	offset[1] = 0;
	offset[2] = 0;
	return AKD_SUCCESS;
}

void Acc_GetAccVector(const int16_t data[3], const int16_t offset[3], int16_t vec[3])
{
	vec[0] = (int16_t)(data[0] - offset[0]);
	vec[1] = (int16_t)(data[1] - offset[1]);
	vec[2] = (int16_t)(data[2] - offset[2]);
}


/* ---------------------------------------------------------------------------------------------------------
 * Local Functions Implementation
 * ---------------------------------------------------------------------------------------------------------
 */

/**
 * sDisableAccTimer ��ʱ��֪ͨ�ص�.
 */
void onTimeOut(sigval_t v)
{
    int appTimerId = v.sival_int;
    I("'sDisableAccTimer' timers out, appTimerId = %d.", appTimerId);
    switch ( appTimerId )
    {
        case APP_TIME_ID__DISABLE_ACC:
            D("to disable acc device.");
            if ( sHasEnabledAcc )
            {
                D("to call 'GSENSOR_IOCTL_CLOSE'.");
                if ( ioctl(sAccFd, GSENSOR_IOCTL_CLOSE) < 0 )
                {
                    E("failed to disable acc device.");
                }
                sHasEnabledAcc = FALSE; 
            }
            break;

        default:
            E("unknow app timer ID.");
            return;
    }
}

