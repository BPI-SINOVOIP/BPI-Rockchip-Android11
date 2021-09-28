#ifndef __LOG_H__
#define __LOG_H__

#define LOG_TAG "MPIMMZ"

#ifdef ANDROID

#include <android/log.h>

#define ALOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#define ALOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define ALOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__))
#define ALOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))
//#define ALOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#define ALOGV(...)

#else

#define ALOGD(...)
#define ALOGI(...)
#define ALOGW(...)
#define ALOGE(...)
#define ALOGV(...)

#endif

#endif