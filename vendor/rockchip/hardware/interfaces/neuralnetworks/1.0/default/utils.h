#ifndef _RKNN_BRIDGE_UTILS_
#define _RKNN_BRIDGE_UTILS_

#include <log/log.h>
#include <cutils/properties.h>

#define LOG_TAG "RockchipNN"

#define UNUSED(x) (void)(x)

static bool g_debug_pro = 0;

#define RECORD_TAG() \
if (g_debug_pro) ALOGI("+++ %s +++", __func__); \
int ret = 0

#define CheckContext() \
if (context != ctx) { \
    return V1_0::ErrorStatus::RKNN_ERR_FAIL; \
}

#endif // _RKNN_BRIDGE_UTILS_
