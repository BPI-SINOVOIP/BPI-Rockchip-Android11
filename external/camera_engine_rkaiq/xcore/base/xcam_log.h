/*
 * xcam_log.h - xcam log
 *
 *  Copyright (c) 2014-2015 Intel Corporation
 *  Copyright (c) 2019, Fuzhou Rockchip Electronics Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef __XCAM_UTILS_LOG_H
#define __XCAM_UTILS_LOG_H

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    XCORE_LOG_LEVEL_ERR,
    XCORE_LOG_LEVEL_WARNING,
    XCORE_LOG_LEVEL_INFO,
    XCORE_LOG_LEVEL_DEBUG,
    XCORE_LOG_LEVEL_VERBOSE,
    XCORE_LOG_LEVEL_LOW1,
} xcore_log_level_t;

typedef enum {
    XCORE_LOG_MODULE_AEC,
    XCORE_LOG_MODULE_AWB,
    XCORE_LOG_MODULE_AF,
    XCORE_LOG_MODULE_ABLC,
    XCORE_LOG_MODULE_ADPCC,
    XCORE_LOG_MODULE_ATMO,
    XCORE_LOG_MODULE_ANR,
    XCORE_LOG_MODULE_ALSC,
    XCORE_LOG_MODULE_AGIC,
    XCORE_LOG_MODULE_ADEBAYER,
    XCORE_LOG_MODULE_ACCM,
    XCORE_LOG_MODULE_AGAMMA,
    XCORE_LOG_MODULE_AWDR,
    XCORE_LOG_MODULE_ADEHAZE,
    XCORE_LOG_MODULE_A3DLUT,
    XCORE_LOG_MODULE_ALDCH,
    XCORE_LOG_MODULE_AR2Y, // including CSM,YUV422
    XCORE_LOG_MODULE_ACP, // color processing
    XCORE_LOG_MODULE_AIE,
    XCORE_LOG_MODULE_ASHARP,
    XCORE_LOG_MODULE_AORB,
    XCORE_LOG_MODULE_AFEC,
    XCORE_LOG_MODULE_ACGC,
    XCORE_LOG_MODULE_ASD, // secen detection
    XCORE_LOG_MODULE_XCORE,
    XCORE_LOG_MODULE_ANALYZER,
    XCORE_LOG_MODULE_CAMHW,
    XCORE_LOG_MODULE_ADEGAMMA,
    XCORE_LOG_MODULE_AMERGE,
	XCORE_LOG_MODULE_AMD,
    XCORE_LOG_MODULE_MAX,
} xcore_log_modules_t;

#ifdef  __cplusplus
extern "C" {
#endif
void xcam_set_log (const char* file_name);
void xcam_print_log (int module, int sub_modules, int level, const char* format, ...);
int xcam_get_log_level();
void xcam_get_runtime_log_level();
bool xcam_get_enviroment_value(const char* variable, unsigned long long* value);
void xcam_get_awb_log_level(unsigned char *log_level, unsigned char *sub_modules);
char* timeString();

#ifdef  __cplusplus
}
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "rkisp"

#define __BI_FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

typedef struct xcore_cam_log_module_info_s {
    const char* module_name;
    int log_level;
    int sub_modules;
} xcore_cam_log_module_info_t;

extern xcore_cam_log_module_info_t g_xcore_log_infos[XCORE_LOG_MODULE_MAX];

// module debug
#define XCAM_MODULE_LOG_ERROR(module, submodules, format, ...)    \
    do { \
        if (XCORE_LOG_LEVEL_ERR <= g_xcore_log_infos[module].log_level && \
                (submodules & g_xcore_log_infos[module].sub_modules)) \
            xcam_print_log (module, submodules, XCORE_LOG_LEVEL_ERR, "XCAM ERROR %s:%d: " format "\n", __BI_FILENAME__, __LINE__, ## __VA_ARGS__); \
    } while(0) \

#define XCAM_MODULE_LOG_WARNING(module, submodules, format, ...)   \
    do { \
        if (XCORE_LOG_LEVEL_WARNING <= g_xcore_log_infos[module].log_level && \
                (submodules & g_xcore_log_infos[module].sub_modules)) \
            xcam_print_log (module, submodules, XCORE_LOG_LEVEL_WARNING, "XCAM WARNING %s:%d: " format "\n", __BI_FILENAME__, __LINE__, ## __VA_ARGS__); \
    } while(0) \

#define XCAM_MODULE_LOG_INFO(module, submodules, format, ...)   \
    do { \
        if (XCORE_LOG_LEVEL_INFO <= g_xcore_log_infos[module].log_level && \
                (submodules & g_xcore_log_infos[module].sub_modules)) \
            xcam_print_log (module, submodules, XCORE_LOG_LEVEL_INFO, "XCAM INFO %s:%d: " format "\n", __BI_FILENAME__ , __LINE__, ## __VA_ARGS__); \
    } while(0) \

#define XCAM_MODULE_LOG_VERBOSE(module, submodules, format, ...)   \
    do { \
        if (XCORE_LOG_LEVEL_VERBOSE <= g_xcore_log_infos[module].log_level && \
                (submodules & g_xcore_log_infos[module].sub_modules)) \
            xcam_print_log (module, submodules, XCORE_LOG_LEVEL_VERBOSE, "XCAM VERBOSE %s:%d: " format "\n", __BI_FILENAME__ , __LINE__, ## __VA_ARGS__); \
    } while(0) \

#define XCAM_MODULE_LOG_DEBUG(module, submodules, format, ...)   \
    do { \
        if (XCORE_LOG_LEVEL_DEBUG <= g_xcore_log_infos[module].log_level && \
                (submodules & g_xcore_log_infos[module].sub_modules)) \
          xcam_print_log (module, submodules, XCORE_LOG_LEVEL_DEBUG, "XCAM DEBUG %s:%d: " format "\n", __BI_FILENAME__ , __LINE__, ## __VA_ARGS__); \
    } while(0) \

#define XCAM_MODULE_LOG_LOW1(module, submodules, format, ...)   \
    do { \
        if (XCORE_LOG_LEVEL_LOW1 <= g_xcore_log_infos[module].log_level && \
                (submodules & g_xcore_log_infos[module].sub_modules)) \
          xcam_print_log (module, submodules, XCORE_LOG_LEVEL_LOW1, "XCAM LOW1 %s:%d: " format "\n", __BI_FILENAME__, __LINE__, ## __VA_ARGS__); \
    } while(0) \

// generic/xcore
#define XCAM_LOG_ERROR(format, ...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_XCORE, 0xff, format, ##__VA_ARGS__)
#define XCAM_LOG_WARNING(format, ...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_XCORE, 0xff, format, ##__VA_ARGS__)
#define XCAM_LOG_INFO(format, ...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_XCORE, 0xff, format, ##__VA_ARGS__)
#define XCAM_LOG_VERBOSE(format, ...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_XCORE, 0xff, format, ##__VA_ARGS__)
#define XCAM_LOG_DEBUG(format, ...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_XCORE, 0xff, format, ##__VA_ARGS__)
#define XCAM_LOG_LOW1(format, ...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_XCORE, 0xff, format, ##__VA_ARGS__)

// define generic module logs
#define LOGD(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_XCORE, 0xff, ##__VA_ARGS__)
#define LOGE(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_XCORE, 0xff, ##__VA_ARGS__)
#define LOGW(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_XCORE, 0xff, ##__VA_ARGS__)
#define LOGV(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_XCORE, 0xff, ##__VA_ARGS__)
#define LOGI(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_XCORE, 0xff, ##__VA_ARGS__)
#define LOG1(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_XCORE, 0xff, ##__VA_ARGS__)

// define aec module logs
#define LOGD_AEC_SUBM(sub_modules, ...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_AEC, sub_modules, ##__VA_ARGS__)
#define LOGE_AEC_SUBM(sub_modules, ...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_AEC, sub_modules, ##__VA_ARGS__)
#define LOGW_AEC_SUBM(sub_modules, ...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_AEC, sub_modules, ##__VA_ARGS__)
#define LOGV_AEC_SUBM(sub_modules, ...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_AEC, sub_modules, ##__VA_ARGS__)
#define LOGI_AEC_SUBM(sub_modules, ...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_AEC, sub_modules, ##__VA_ARGS__)
#define LOG1_AEC_SUBM(sub_modules, ...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_AEC, sub_modules, ##__VA_ARGS__)

#define LOGD_AEC(...) LOGD_AEC_SUBM(0xff, ##__VA_ARGS__)
#define LOGE_AEC(...) LOGE_AEC_SUBM(0xff, ##__VA_ARGS__)
#define LOGW_AEC(...) LOGW_AEC_SUBM(0xff, ##__VA_ARGS__)
#define LOGV_AEC(...) LOGV_AEC_SUBM(0xff, ##__VA_ARGS__)
#define LOGI_AEC(...) LOGI_AEC_SUBM(0xff, ##__VA_ARGS__)
#define LOG1_AEC(...) LOG1_AEC_SUBM(0xff, ##__VA_ARGS__)

// define awb module logs
#define LOGD_AWB_SUBM(sub_modules, ...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_AWB, sub_modules, ##__VA_ARGS__)
#define LOGE_AWB_SUBM(sub_modules, ...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_AWB, sub_modules, ##__VA_ARGS__)
#define LOGW_AWB_SUBM(sub_modules, ...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_AWB, sub_modules, ##__VA_ARGS__)
#define LOGV_AWB_SUBM(sub_modules, ...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_AWB, sub_modules, ##__VA_ARGS__)
#define LOGI_AWB_SUBM(sub_modules, ...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_AWB, sub_modules, ##__VA_ARGS__)
#define LOG1_AWB_SUBM(sub_modules, ...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_AWB, sub_modules, ##__VA_ARGS__)

#define LOGD_AWB(...) LOGD_AWB_SUBM(0xff, ##__VA_ARGS__)
#define LOGE_AWB(...) LOGE_AWB_SUBM(0xff, ##__VA_ARGS__)
#define LOGW_AWB(...) LOGW_AWB_SUBM(0xff, ##__VA_ARGS__)
#define LOGV_AWB(...) LOGV_AWB_SUBM(0xff, ##__VA_ARGS__)
#define LOGI_AWB(...) LOGI_AWB_SUBM(0xff, ##__VA_ARGS__)
#define LOG1_AWB(...) LOG1_AWB_SUBM(0xff, ##__VA_ARGS__)

// define af module logs
#define LOGD_AF_SUBM(sub_modules, ...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_AF, sub_modules, ##__VA_ARGS__)
#define LOGE_AF_SUBM(sub_modules, ...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_AF, sub_modules, ##__VA_ARGS__)
#define LOGW_AF_SUBM(sub_modules, ...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_AF, sub_modules, ##__VA_ARGS__)
#define LOGV_AF_SUBM(sub_modules, ...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_AF, sub_modules, ##__VA_ARGS__)
#define LOGI_AF_SUBM(sub_modules, ...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_AF, sub_modules, ##__VA_ARGS__)
#define LOG1_AF_SUBM(sub_modules, ...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_AF, sub_modules, ##__VA_ARGS__)

#define LOGD_AF(...) LOGD_AF_SUBM(0xff, ##__VA_ARGS__)
#define LOGE_AF(...) LOGE_AF_SUBM(0xff, ##__VA_ARGS__)
#define LOGW_AF(...) LOGW_AF_SUBM(0xff, ##__VA_ARGS__)
#define LOGV_AF(...) LOGV_AF_SUBM(0xff, ##__VA_ARGS__)
#define LOGI_AF(...) LOGI_AF_SUBM(0xff, ##__VA_ARGS__)
#define LOG1_AF(...) LOG1_AF_SUBM(0xff, ##__VA_ARGS__)

// define acp module logs
#define LOGD_ACP(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_ACP, 0xff, ##__VA_ARGS__)
#define LOGE_ACP(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_ACP, 0xff, ##__VA_ARGS__)
#define LOGW_ACP(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_ACP, 0xff, ##__VA_ARGS__)
#define LOGV_ACP(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_ACP, 0xff, ##__VA_ARGS__)
#define LOGI_ACP(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_ACP, 0xff, ##__VA_ARGS__)
#define LOG1_ACP(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_ACP, 0xff, ##__VA_ARGS__)

// define orb module logs
#define LOGD_ORB(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_AORB, 0xff, ##__VA_ARGS__)
#define LOGE_ORB(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_AORB, 0xff, ##__VA_ARGS__)
#define LOGW_ORB(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_AORB, 0xff, ##__VA_ARGS__)
#define LOGV_ORB(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_AORB, 0xff, ##__VA_ARGS__)
#define LOGI_ORB(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_AORB, 0xff, ##__VA_ARGS__)
#define LOG1_ORB(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_AORB, 0xff, ##__VA_ARGS__)

// define atmo module logs
#define LOGD_ATMO(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_ATMO, 0xff, ##__VA_ARGS__)//ATMO algo para
#define LOGE_ATMO(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_ATMO, 0xff, ##__VA_ARGS__)
#define LOGW_ATMO(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_ATMO, 0xff, ##__VA_ARGS__)
#define LOGV_ATMO(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_ATMO, 0xff, ##__VA_ARGS__)//ATMO register data
#define LOGI_ATMO(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_ATMO, 0xff, ##__VA_ARGS__)
#define LOG1_ATMO(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_ATMO, 0xff, ##__VA_ARGS__)//ATMO calc data

// define amerge module logs
#define LOGD_AMERGE(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_AMERGE, 0xff, ##__VA_ARGS__)//AMERGE algo para
#define LOGE_AMERGE(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_AMERGE, 0xff, ##__VA_ARGS__)
#define LOGW_AMERGE(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_AMERGE, 0xff, ##__VA_ARGS__)
#define LOGV_AMERGE(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_AMERGE, 0xff, ##__VA_ARGS__)//AMERGE register data
#define LOGI_AMERGE(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_AMERGE, 0xff, ##__VA_ARGS__)
#define LOG1_AMERGE(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_AMERGE, 0xff, ##__VA_ARGS__)//AMERGE calc data

// define anr module logs
#define LOGD_ANR(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_ANR, 0xff, ##__VA_ARGS__)
#define LOGE_ANR(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_ANR, 0xff, ##__VA_ARGS__)
#define LOGW_ANR(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_ANR, 0xff, ##__VA_ARGS__)
#define LOGV_ANR(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_ANR, 0xff, ##__VA_ARGS__)
#define LOGI_ANR(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_ANR, 0xff, ##__VA_ARGS__)
#define LOG1_ANR(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_ANR, 0xff, ##__VA_ARGS__)

// define ablc module logs
#define LOGD_ABLC(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_ABLC, 0xff, ##__VA_ARGS__)
#define LOGE_ABLC(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_ABLC, 0xff, ##__VA_ARGS__)
#define LOGW_ABLC(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_ABLC, 0xff, ##__VA_ARGS__)
#define LOGV_ABLC(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_ABLC, 0xff, ##__VA_ARGS__)
#define LOGI_ABLC(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_ABLC, 0xff, ##__VA_ARGS__)
#define LOG1_ABLC(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_ABLC, 0xff, ##__VA_ARGS__)

// define ADPCC module logs
#define LOGD_ADPCC(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_ADPCC, 0xff, ##__VA_ARGS__)
#define LOGE_ADPCC(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_ADPCC, 0xff, ##__VA_ARGS__)
#define LOGW_ADPCC(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_ADPCC, 0xff, ##__VA_ARGS__)
#define LOGV_ADPCC(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_ADPCC, 0xff, ##__VA_ARGS__)
#define LOGI_ADPCC(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_ADPCC, 0xff, ##__VA_ARGS__)
#define LOG1_ADPCC(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_ADPCC, 0xff, ##__VA_ARGS__)

// define AGIC module logs
#define LOGD_AGIC(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_AGIC, 0xff, ##__VA_ARGS__)
#define LOGE_AGIC(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_AGIC, 0xff, ##__VA_ARGS__)
#define LOGW_AGIC(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_AGIC, 0xff, ##__VA_ARGS__)
#define LOGV_AGIC(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_AGIC, 0xff, ##__VA_ARGS__)
#define LOGI_AGIC(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_AGIC, 0xff, ##__VA_ARGS__)
#define LOG1_AGIC(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_AGIC, 0xff, ##__VA_ARGS__)

// define ALSC module logs
#define LOGD_ALSC(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_ALSC, 0xff, ##__VA_ARGS__)
#define LOGE_ALSC(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_ALSC, 0xff, ##__VA_ARGS__)
#define LOGW_ALSC(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_ALSC, 0xff, ##__VA_ARGS__)
#define LOGV_ALSC(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_ALSC, 0xff, ##__VA_ARGS__)
#define LOGI_ALSC(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_ALSC, 0xff, ##__VA_ARGS__)
#define LOG1_ALSC(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_ALSC, 0xff, ##__VA_ARGS__)

// define asd module logs
#define LOGD_ASD(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_ASD, 0xff, ##__VA_ARGS__)
#define LOGE_ASD(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_ASD, 0xff, ##__VA_ARGS__)
#define LOGW_ASD(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_ASD, 0xff, ##__VA_ARGS__)
#define LOGV_ASD(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_ASD, 0xff, ##__VA_ARGS__)
#define LOGI_ASD(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_ASD, 0xff, ##__VA_ARGS__)
#define LOG1_ASD(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_ASD, 0xff, ##__VA_ARGS__)

// define ACCM module logs
#define LOGD_ACCM(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_ACCM, 0xff, ##__VA_ARGS__)
#define LOGE_ACCM(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_ACCM, 0xff, ##__VA_ARGS__)
#define LOGW_ACCM(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_ACCM, 0xff, ##__VA_ARGS__)
#define LOGV_ACCM(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_ACCM, 0xff, ##__VA_ARGS__)
#define LOGI_ACCM(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_ACCM, 0xff, ##__VA_ARGS__)
#define LOG1_ACCM(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_ACCM, 0xff, ##__VA_ARGS__)

// define A3DLUT module logs
#define LOGD_A3DLUT(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_A3DLUT, 0xff, ##__VA_ARGS__)
#define LOGE_A3DLUT(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_A3DLUT, 0xff, ##__VA_ARGS__)
#define LOGW_A3DLUT(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_A3DLUT, 0xff, ##__VA_ARGS__)
#define LOGV_A3DLUT(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_A3DLUT, 0xff, ##__VA_ARGS__)
#define LOGI_A3DLUT(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_A3DLUT, 0xff, ##__VA_ARGS__)
#define LOG1_A3DLUT(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_A3DLUT, 0xff, ##__VA_ARGS__)

// define ADEHAZE module logs
#define LOGD_ADEHAZE(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_ADEHAZE, 0xff, ##__VA_ARGS__)
#define LOGE_ADEHAZE(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_ADEHAZE, 0xff, ##__VA_ARGS__)
#define LOGW_ADEHAZE(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_ADEHAZE, 0xff, ##__VA_ARGS__)
#define LOGV_ADEHAZE(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_ADEHAZE, 0xff, ##__VA_ARGS__)
#define LOGI_ADEHAZE(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_ADEHAZE, 0xff, ##__VA_ARGS__)
#define LOG1_ADEHAZE(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_ADEHAZE, 0xff, ##__VA_ARGS__)

// define ADEGAMMA module logs
#define LOGD_ADEGAMMA(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_ADEGAMMA, 0xff, ##__VA_ARGS__)
#define LOGE_ADEGAMMA(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_ADEGAMMA, 0xff, ##__VA_ARGS__)
#define LOGW_ADEGAMMA(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_ADEGAMMA, 0xff, ##__VA_ARGS__)
#define LOGV_ADEGAMMA(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_ADEGAMMA, 0xff, ##__VA_ARGS__)
#define LOGI_ADEGAMMA(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_ADEGAMMA, 0xff, ##__VA_ARGS__)
#define LOG1_ADEGAMMA(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_ADEGAMMA, 0xff, ##__VA_ARGS__)

// define ADEBAYER module logs
#define LOGD_ADEBAYER(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_ADEBAYER, 0xff, ##__VA_ARGS__)
#define LOGE_ADEBAYER(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_ADEBAYER, 0xff, ##__VA_ARGS__)
#define LOGW_ADEBAYER(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_ADEBAYER, 0xff, ##__VA_ARGS__)
#define LOGV_ADEBAYER(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_ADEBAYER, 0xff, ##__VA_ARGS__)
#define LOGI_ADEBAYER(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_ADEBAYER, 0xff, ##__VA_ARGS__)
#define LOG1_ADEBAYER(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_ADEBAYER, 0xff, ##__VA_ARGS__)

// define AGAMMA module logs
#define LOGD_AGAMMA(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_AGAMMA, 0xff, ##__VA_ARGS__)
#define LOGE_AGAMMA(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_AGAMMA, 0xff, ##__VA_ARGS__)
#define LOGW_AGAMMA(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_AGAMMA, 0xff, ##__VA_ARGS__)
#define LOGV_AGAMMA(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_AGAMMA, 0xff, ##__VA_ARGS__)
#define LOGI_AGAMMA(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_AGAMMA, 0xff, ##__VA_ARGS__)
#define LOG1_AGAMMA(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_AGAMMA, 0xff, ##__VA_ARGS__)

// define AWDR module logs
#define LOGD_AWDR(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_AWDR, 0xff, ##__VA_ARGS__)
#define LOGE_AWDR(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_AWDR, 0xff, ##__VA_ARGS__)
#define LOGW_AWDR(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_AWDR, 0xff, ##__VA_ARGS__)
#define LOGV_AWDR(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_AWDR, 0xff, ##__VA_ARGS__)
#define LOGI_AWDR(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_AWDR, 0xff, ##__VA_ARGS__)
#define LOG1_AWDR(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_AWDR, 0xff, ##__VA_ARGS__)

// define AR2Y module logs
#define LOGD_AR2Y(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_AR2Y, 0xff, ##__VA_ARGS__)
#define LOGE_AR2Y(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_AR2Y, 0xff, ##__VA_ARGS__)
#define LOGW_AR2Y(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_AR2Y, 0xff, ##__VA_ARGS__)
#define LOGV_AR2Y(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_AR2Y, 0xff, ##__VA_ARGS__)
#define LOGI_AR2Y(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_AR2Y, 0xff, ##__VA_ARGS__)
#define LOG1_AR2Y(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_AR2Y, 0xff, ##__VA_ARGS__)

// define AORB module logs
#define LOGD_AORB(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_AORB, 0xff, ##__VA_ARGS__)
#define LOGE_AORB(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_AORB, 0xff, ##__VA_ARGS__)
#define LOGW_AORB(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_AORB, 0xff, ##__VA_ARGS__)
#define LOGV_AORB(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_AORB, 0xff, ##__VA_ARGS__)
#define LOGI_AORB(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_AORB, 0xff, ##__VA_ARGS__)
#define LOG1_AORB(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_AORB, 0xff, ##__VA_ARGS__)

// define AFEC module logs
#define LOGD_AFEC(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_AFEC, 0xff, ##__VA_ARGS__)
#define LOGE_AFEC(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_AFEC, 0xff, ##__VA_ARGS__)
#define LOGW_AFEC(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_AFEC, 0xff, ##__VA_ARGS__)
#define LOGV_AFEC(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_AFEC, 0xff, ##__VA_ARGS__)
#define LOGI_AFEC(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_AFEC, 0xff, ##__VA_ARGS__)
#define LOG1_AFEC(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_AFEC, 0xff, ##__VA_ARGS__)

// define ALDCH module logs
#define LOGD_ALDCH(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_ALDCH, 0xff, ##__VA_ARGS__)
#define LOGE_ALDCH(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_ALDCH, 0xff, ##__VA_ARGS__)
#define LOGW_ALDCH(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_ALDCH, 0xff, ##__VA_ARGS__)
#define LOGV_ALDCH(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_ALDCH, 0xff, ##__VA_ARGS__)
#define LOGI_ALDCH(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_ALDCH, 0xff, ##__VA_ARGS__)
#define LOG1_ALDCH(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_ALDCH, 0xff, ##__VA_ARGS__)

// define ASHARP module logs
#define LOGD_ASHARP(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_ASHARP, 0xff, ##__VA_ARGS__)
#define LOGE_ASHARP(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_ASHARP, 0xff, ##__VA_ARGS__)
#define LOGW_ASHARP(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_ASHARP, 0xff, ##__VA_ARGS__)
#define LOGV_ASHARP(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_ASHARP, 0xff, ##__VA_ARGS__)
#define LOGI_ASHARP(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_ASHARP, 0xff, ##__VA_ARGS__)
#define LOG1_ASHARP(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_ASHARP, 0xff, ##__VA_ARGS__)

// define AIE module logs
#define LOGD_AIE(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_AIE, 0xff, ##__VA_ARGS__)
#define LOGE_AIE(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_AIE, 0xff, ##__VA_ARGS__)
#define LOGW_AIE(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_AIE, 0xff, ##__VA_ARGS__)
#define LOGV_AIE(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_AIE, 0xff, ##__VA_ARGS__)
#define LOGI_AIE(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_AIE, 0xff, ##__VA_ARGS__)
#define LOG1_AIE(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_AIE, 0xff, ##__VA_ARGS__)

// define ACGC module logs
#define LOGD_ACGC(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_ACGC, 0xff, ##__VA_ARGS__)
#define LOGE_ACGC(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_ACGC, 0xff, ##__VA_ARGS__)
#define LOGW_ACGC(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_ACGC, 0xff, ##__VA_ARGS__)
#define LOGV_ACGC(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_ACGC, 0xff, ##__VA_ARGS__)
#define LOGI_ACGC(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_ACGC, 0xff, ##__VA_ARGS__)
#define LOG1_ACGC(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_ACGC, 0xff, ##__VA_ARGS__)

// define AEIS module logs
#define LOGD_AEIS(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_AFEC, 0x02, ##__VA_ARGS__)
#define LOGE_AEIS(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_AFEC, 0x02, ##__VA_ARGS__)
#define LOGW_AEIS(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_AFEC, 0x02, ##__VA_ARGS__)
#define LOGV_AEIS(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_AFEC, 0x02, ##__VA_ARGS__)
#define LOGI_AEIS(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_AFEC, 0x02, ##__VA_ARGS__)
#define LOG1_AEIS(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_AFEC, 0x02, ##__VA_ARGS__)

// define AMD module logs
#define LOGD_AMD(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_AMD, 0xff, ##__VA_ARGS__)
#define LOGE_AMD(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_AMD, 0xff, ##__VA_ARGS__)
#define LOGW_AMD(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_AMD, 0xff, ##__VA_ARGS__)
#define LOGV_AMD(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_AMD, 0xff, ##__VA_ARGS__)
#define LOGI_AMD(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_AMD, 0xff, ##__VA_ARGS__)
#define LOG1_AMD(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_AMD, 0xff, ##__VA_ARGS__)

// define analyzer module logs
#define LOGD_ANALYZER(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_ANALYZER, 0xff, ##__VA_ARGS__)
#define LOGE_ANALYZER(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_ANALYZER, 0xff, ##__VA_ARGS__)
#define LOGW_ANALYZER(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_ANALYZER, 0xff, ##__VA_ARGS__)
#define LOGV_ANALYZER(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_ANALYZER, 0xff, ##__VA_ARGS__)
#define LOGI_ANALYZER(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_ANALYZER, 0xff, ##__VA_ARGS__)
#define LOG1_ANALYZER(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_ANALYZER, 0xff, ##__VA_ARGS__)

#define LOGD_ANALYZER_SUBM(sub_modules, ...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_ANALYZER, sub_modules, ##__VA_ARGS__)
#define LOGE_ANALYZER_SUBM(sub_modules, ...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_ANALYZER, sub_modules, ##__VA_ARGS__)
#define LOGW_ANALYZER_SUBM(sub_modules, ...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_ANALYZER, sub_modules, ##__VA_ARGS__)
#define LOGV_ANALYZER_SUBM(sub_modules, ...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_ANALYZER, sub_modules, ##__VA_ARGS__)
#define LOGI_ANALYZER_SUBM(sub_modules, ...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_ANALYZER, sub_modules, ##__VA_ARGS__)
#define LOG1_ANALYZER_SUBM(sub_modules, ...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_ANALYZER, sub_modules, ##__VA_ARGS__)

// define camhw module logs
#define LOGD_CAMHW(...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_CAMHW, 0xff, ##__VA_ARGS__)
#define LOGE_CAMHW(...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_CAMHW, 0xff, ##__VA_ARGS__)
#define LOGW_CAMHW(...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_CAMHW, 0xff, ##__VA_ARGS__)
#define LOGV_CAMHW(...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_CAMHW, 0xff, ##__VA_ARGS__)
#define LOGI_CAMHW(...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_CAMHW, 0xff, ##__VA_ARGS__)
#define LOG1_CAMHW(...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_CAMHW, 0xff, ##__VA_ARGS__)

#define LOGD_CAMHW_SUBM(sub_modules, ...) XCAM_MODULE_LOG_DEBUG(XCORE_LOG_MODULE_CAMHW, sub_modules, ##__VA_ARGS__)
#define LOGE_CAMHW_SUBM(sub_modules, ...) XCAM_MODULE_LOG_ERROR(XCORE_LOG_MODULE_CAMHW, sub_modules, ##__VA_ARGS__)
#define LOGW_CAMHW_SUBM(sub_modules, ...) XCAM_MODULE_LOG_WARNING(XCORE_LOG_MODULE_CAMHW, sub_modules, ##__VA_ARGS__)
#define LOGV_CAMHW_SUBM(sub_modules, ...) XCAM_MODULE_LOG_VERBOSE(XCORE_LOG_MODULE_CAMHW, sub_modules, ##__VA_ARGS__)
#define LOGI_CAMHW_SUBM(sub_modules, ...) XCAM_MODULE_LOG_INFO(XCORE_LOG_MODULE_CAMHW, sub_modules, ##__VA_ARGS__)
#define LOG1_CAMHW_SUBM(sub_modules, ...) XCAM_MODULE_LOG_LOW1(XCORE_LOG_MODULE_CAMHW, sub_modules, ##__VA_ARGS__)

#define XCAM_LOG_MODULE_ENTER(module) XCAM_MODULE_LOG_LOW1(module, 0xff, "ENTER %s", __func__)
#define XCAM_LOG_MODULE_EXIT(module) XCAM_MODULE_LOG_LOW1(module, 0xff, "EXIT %s", __func__)

// convinient macros
#define ENTER_XCORE_FUNCTION() XCAM_LOG_MODULE_ENTER(XCORE_LOG_MODULE_XCORE)
#define EXIT_XCORE_FUNCTION() XCAM_LOG_MODULE_EXIT(XCORE_LOG_MODULE_XCORE)
#define ENTER_CAMHW_FUNCTION() XCAM_LOG_MODULE_ENTER(XCORE_LOG_MODULE_CAMHW)
#define EXIT_CAMHW_FUNCTION() XCAM_LOG_MODULE_EXIT(XCORE_LOG_MODULE_CAMHW)
#define ENTER_ANALYZER_FUNCTION() XCAM_LOG_MODULE_ENTER(XCORE_LOG_MODULE_ANALYZER)
#define EXIT_ANALYZER_FUNCTION() XCAM_LOG_MODULE_EXIT(XCORE_LOG_MODULE_ANALYZER)

#define NULL_RETURN(ptr) do { if (ptr == NULL) {LOGE("%s is NULL!\n", #ptr); return;} } while(0)
#define NULL_RETURN_RET(ptr, ret) do { if (ptr == NULL) {LOGE("%s is NULL!\n", #ptr); return ret;} } while(0)

#ifndef ANDROID_OS
#define ALOGD LOGD
#define ALOGE LOGE
#define ALOGW LOGW
#define ALOGV LOGV
#define ALOGI LOGI
#define ALOG1 LOG1
#define ALOG2 LOG2
#else
#include <utils/Log.h>
#define TRACE_D ALOGIF
#endif // ANDROID_OS
#endif
