/*  --------------------------------------------------------------------------------------------------------
 *  Copyright(C), 2010-2011, Fuzhou Rockchip Co. ,Ltd.  All Rights Reserved.
 *
 *  File:   Acc_mma8452.c
 *
 *  Desc:   实现 akmd8975 要求的 accelerometer sensor 的功能接口 : 开启, 关闭, 获取数据等. 
 *
 *          -----------------------------------------------------------------------------------
 *          < 习语 和 缩略语 > : 
 *
 *              acc : accelerometer sensor, "g sensor" 的 别名. 
 *
 *          -----------------------------------------------------------------------------------
 *          < 内部模块 or 对象的层次结构 > :
 *
 *          -----------------------------------------------------------------------------------
 *          < 功能实现的基本流程 > :
 *
 *          -----------------------------------------------------------------------------------
 *          < 关键标识符 > : 
 *
 *          -----------------------------------------------------------------------------------
 *          < 本模块实现依赖的外部模块 > : 
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

/** 表征相同的 加速度物理量的时, "Android 上层使用的 数值" 和 "sensor 数据设备送出的 数值" 之间的比值. */
#define ACCELERATION_RATIO_ANDROID_TO_HW        (9.80665f / 1000000)

/** "sDisableAccTimer" 在本 app 模块 scope 中的标识 ID. */
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

/** acc(g sensor) 控制设备(ASENSOR_PATH) 的 FD. */
static int sAccFd = -1;

/** timer, 用于定时超时之后 disable acc. 这里是 "系统 timer ID", 动态创建. */
static timer_t sDisableAccTimer = NULL;
/** 若在 ENABLE_TIME_OUT 秒内, akmd8975 没有再次读取 acc 数据, 则 disable acc 设备. */
static const int ENABLE_TIME_OUT = 5; 

/** 标识当前模块是否 使能了 acc 设备(要求其采集 g sensor 数据). */
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
    struct sigevent se;     /* 将用来定义对 timer 超时事件的定制处理方式. */
    struct itimerspec ts;   /* 将用于定义超时时间. */

    /* 尝试打开 acc 控制设备文件. */
    if ( 0 > (sAccFd = open(ASENSOR_PATH, O_RDONLY ) ) )
    {
        E("failed to open acc file '%s', error is '%s'", ASENSOR_PATH, strerror(errno));
        result = AKD_FAIL;
        goto EXIT;
    }
    
    /* < 创建 sDisableAccTimer. > */
    memset(&se, 0, sizeof(se) );
    se.sigev_notify = SIGEV_THREAD; /* "A notification function will be called to perform notification". */
    se.sigev_notify_function = onTimeOut;   /* 通知回调函数. */
    se.sigev_value.sival_int = APP_TIME_ID__DISABLE_ACC;    /* sigev_value : sigev_notify_function 的回调实参. */
    if ( timer_create (CLOCK_REALTIME, &se, &sDisableAccTimer) < 0 ) 
    {
        E("failed create 'sDisableAccTimer'; error is '%s'.", strerror(errno));
        result = AKD_FAIL;
        goto EXIT;
    }
    // sDisableAccTimer 将在 Acc_GetAccelerationData() 中启动. 

    sHasEnabledAcc = FALSE;     // 显式初始化. 
    
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

    /* 若 acc 设备已经被启动, 则... */
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
 * .! : 目前设计为 尽量不阻塞的形式. 
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
    struct itimerspec ts;   /* 将用于定义超时时间. */

    struct sensor_axis accData = {0, 0, 0};

    /* 若尚未使能 acc, 则... */
    if ( !sHasEnabledAcc )
    {
        /* 使能 acc. */
        if ( 0 > ioctl(sAccFd, GSENSOR_IOCTL_START) ) 
        {
            E("failed to START acc device; error is '%s'.", strerror(errno));
            result = AKD_FAIL;
            goto EXIT;
        }
        /* 设置采样率. */
		int sample_rate = MMA8452_RATE_12P5;        // .! : 暂时先使用 12.5 Hz.
        if ( 0 > ioctl(sAccFd, GSENSOR_IOCTL_APP_SET_RATE, &sample_rate) ) 
        {
            E("failed to set sample rete of acc device; error is '%s'.", strerror(errno));
            result = AKD_FAIL;
            goto EXIT;
        }
        /* 置位标识. */
        sHasEnabledAcc = TRUE;
    }
    
    /* 获取 acc sensor 数据. */ // .! : 尽量不阻塞. 
    if ( 0 > ioctl(sAccFd, GSENSOR_IOCTL_GETDATA, &accData) )
    {
        E("failed to GET acc data, error is '%s.'", strerror(errno));
        result = AKD_FAIL;
        goto EXIT;
    }
    
    /* 重置 sDisableAccTimer. */
    // D("to reset 'sDisableAccTimer'.");
    memset(&ts, 0, sizeof(ts) );
    ts.it_value.tv_sec = ENABLE_TIME_OUT;
    if ( timer_settime(sDisableAccTimer, 0, &ts, NULL) < 0 ) 
    {
        E("failed start 'sDisableAccTimer'; error is '%s'.", strerror(errno));
        result = AKD_FAIL;
        goto EXIT;
    }

    /* 转化为 Android 定义的格式, 待返回. */    // .! : 和 HAL 的 MmaSensor.cpp 中一致, 使用 默认横屏坐标定义 g sensor 数据.
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
 * sDisableAccTimer 超时的通知回调.
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

