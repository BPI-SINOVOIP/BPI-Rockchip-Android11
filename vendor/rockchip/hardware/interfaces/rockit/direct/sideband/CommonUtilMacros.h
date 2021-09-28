/*
 * Copyright (C) 2014-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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
 */

#ifndef _COMMON_UTIL_MACROS_H_
#define _COMMON_UTIL_MACROS_H_

#include <algorithm>
#include <string>

/**
 * Use to check input parameter and if failed, return err_code and print error message
 */
#define VOID_VALUE
#define CheckError(condition, err_code, err_msg, args...) \
            do { \
                if (condition) { \
                    ALOGE(err_msg, ##args);\
                    return err_code;\
                }\
            } while (0)

/**
 * Use to check input parameter and if failed, return err_code and print warning message,
 * this should be used for non-vital error checking.
 */
#define CheckWarning(condition, err_code, err_msg, args...) \
            do { \
                if (condition) { \
                    LOGW(err_msg, ##args);\
                    return err_code;\
                }\
            } while (0)

// macro for memcpy
#ifndef MEMCPY_S
#define MEMCPY_S(dest, dmax, src, smax) memcpy((dest), (src), std::min((size_t)(dmax), (size_t)(smax)))
#endif

#define STDCOPY(dst, src, size) std::copy((src), ((src) + (size)), (dst))

#define CLEAR(x) memset (&(x), 0, sizeof (x))

#define CLEAR_N(x, n) memset (&(x), 0, sizeof(x) * n)

#define STRLEN_S(x) std::char_traits<char>::length(x)


/**
 * \macro PRINT_BACKTRACE_LINUX
 * Debug macro for printing backtraces in Linux (GCC compatible) environments
 */
#define PRINT_BACKTRACE_LINUX() \
{ \
    /* TODO */ \
}


/*
 * For Android, from N release (7.0) onwards the folder where the HAL has
 * permissions to dump files is /data/misc/cameraserver/
 * before that was /data/misc/media/
 * This is due to the introduction of the new cameraserver process.
 * For Linux, the same folder is used.
 */
/* #define CAMERA_OPERATION_FOLDER "/tmp/" */
#define CAMERA_OPERATION_FOLDER "/data/dump/"

#define V4L2_TYPE_IS_VALID(type) \
    ((type) == V4L2_BUF_TYPE_VIDEO_CAPTURE \
     || (type) == V4L2_BUF_TYPE_VIDEO_OUTPUT \
     || (type) == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE \
     || (type) == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
/*
    || (type) == V4L2_BUF_TYPE_META_OUTPUT \
    || (type) == V4L2_BUF_TYPE_META_CAPTURE
*/
#define V4L2_TYPE_IS_META(type) false\

/**
 * \macro UNUSED
 *  applied to parameters not used in a method in order to avoid the compiler
 *  warning
 */
#define UNUSED(x) (void)(x)

#define UNUSED1(a)                                                  (void)(a)
#define UNUSED2(a,b)                                                (void)(a),UNUSED1(b)

#endif /* _COMMON_UTIL_MACROS_H_ */
