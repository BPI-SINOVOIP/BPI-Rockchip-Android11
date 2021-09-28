/*  --------------------------------------------------------------------------------------------------------
 *  Copyright(C), 2009-2010, Fuzhou Rockchip Co., Ltd.  All Rights Reserved.
 *
 *  File:   custom_log.h 
 *
 *  Desc:   ChenZhen 偏好的, 对 Android log 机构的定制配置. 
 *
 *          -----------------------------------------------------------------------------------
 *          < 习语 和 缩略语 > : 
 *
 *          -----------------------------------------------------------------------------------
 *
 *  Usage:	调用本 log 机构的 .c or .h 文件, 若要使能这里的 log 机制, 
 *          则必须在 inclue 本文件之前, "#define ENABLE_DEBUG_LOG" 先. 
 *          
 *          同 log.h 一样, client 文件在 inclue 本文件之前, "最好" #define LOG_TAG <module_tag>
 *
 *  Note:
 *
 *  Author: ChenZhen
 *
 *  --------------------------------------------------------------------------------------------------------
 *  Version:
 *          v1.0
 *  --------------------------------------------------------------------------------------------------------
 *  Log:
	----Tue Mar 02 21:30:33 2010            v.10
 *        
 *  --------------------------------------------------------------------------------------------------------
 */


#ifndef __CUSTOM_LOG_H__
#define __CUSTOM_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------------------------------------
 *  Include Files
 * ---------------------------------------------------------------------------------------------------------
 */
#include <log/log.h>
    
/* ---------------------------------------------------------------------------------------------------------
 *  Macros Definition 
 * ---------------------------------------------------------------------------------------------------------
 */

/** 若下列 macro 有被定义, 才 使能 log 输出 : 参见 "Usage". */ 
// #define ENABLE_DEBUG_LOG

/** 是否 log 源文件 路径信息. */
#define LOG_FILE_PATH

/*-----------------------------------*/

#if  PLATFORM_SDK_VERSION >= 16
#define LOGV(fmt,args...) ALOGV(fmt,##args)
#define LOGD(fmt,args...) ALOGD(fmt,##args)
#define LOGI(fmt,args...) ALOGI(fmt,##args)
#define LOGW(fmt,args...) ALOGW(fmt,##args)
#define LOGE(fmt,args...) ALOGE(fmt,##args)
#define LOGE_IF(cond,fmt,args...)	  ALOGE_IF(cond,fmt,##args)
#endif

#ifdef ENABLE_DEBUG_LOG

#ifdef LOG_FILE_PATH
#define D(fmt, args...) \
    { LOGD("[File] : %s; [Line] : %d; [Func] : %s() ; " fmt, __FILE__, __LINE__, __FUNCTION__, ## args); }
#else
#define D(fmt, args...) \
    { LOGD("[Line] : %d; [Func] : %s() ; " fmt, __LINE__, __FUNCTION__, ## args); }
#endif

#else
#define D(...)  ((void)0)
#endif


/*-----------------------------------*/

#ifdef ENABLE_DEBUG_LOG

#ifdef LOG_FILE_PATH
#define I(fmt, args...) \
    { LOGI("[File] : %s; [Line] : %d; [Func] : %s() ; ! Info : " fmt, __FILE__, __LINE__, __FUNCTION__, ## args); }
#else
#define I(fmt, args...) \
    { LOGI("[Line] : %d; [Func] : %s() ; ! Info : " fmt, __LINE__, __FUNCTION__, ## args); }
#endif
#else
#define I(...)  ((void)0)
#endif

/*-----------------------------------*/
#ifdef ENABLE_DEBUG_LOG
#ifdef LOG_FILE_PATH
#define W(fmt, args...) \
    { LOGW("[File] : %s; [Line] : %d; [Func] : %s() ; !! Warning : " fmt, __FILE__, __LINE__, __FUNCTION__, ## args); }
#else
#define W(fmt, args...) \
    { LOGW("[Line] : %d; [Func] : %s() ; !! Warning : " fmt, __LINE__, __FUNCTION__, ## args); }
#endif
#else
#define W(...)  ((void)0)
#endif

/*-----------------------------------*/
#ifdef ENABLE_DEBUG_LOG
#ifdef LOG_FILE_PATH
#define E(fmt, args...) \
    { LOGE("[File] : %s; [Line] : %d; [Func] : %s() ; !!! Error : " fmt, __FILE__, __LINE__, __FUNCTION__, ## args); }
#else
#define E(fmt, args...) \
    { LOGE("[Line] : %d; [Func] : %s() ; !!! Error : " fmt, __LINE__, __FUNCTION__, ## args); }
#endif
#else
#define E(...)  ((void)0)
#endif

/*-----------------------------------*/
/** 
 * 若程序重复运行到当前位置的次数等于 threshold 或者第一次到达, 则打印指定的 log 信息. 
 */
#ifdef ENABLE_DEBUG_LOG
#define D_WHEN_REPEAT(threshold, fmt, args...) \
    do { \
        static int count = 0; \
        if ( 0 == count || (count++) == threshold ) { \
            D(fmt, ##args); \
            count = 1; \
        } \
    } while (0)
#else
#define D_WHEN_REPEAT(...)  ((void)0)
#endif
    
/* ---------------------------------------------------------------------------------------------------------
 *  Types and Structures Definition
 * ---------------------------------------------------------------------------------------------------------
 */


/* ---------------------------------------------------------------------------------------------------------
 *  Global Functions' Prototype
 * ---------------------------------------------------------------------------------------------------------
 */


/* ---------------------------------------------------------------------------------------------------------
 *  Inline Functions Implementation 
 * ---------------------------------------------------------------------------------------------------------
 */

#ifdef __cplusplus
}
#endif

#endif /* __CUSTOM_LOG_H__ */

