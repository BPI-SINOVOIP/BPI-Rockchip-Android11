/*
 * Copyright (C) 2020 Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ANDROID_DRM_DEBUG_H_
#define ANDROID_DRM_DEBUG_H_
#include <stdlib.h>
#include <utils/CallStack.h>


/*hwc version*/
#define GHWC_VERSION                    "HWC2-1.2.16"

//Print call statck when you call ALOGD_CALLSTACK.
#define ALOGD_CALLSTACK(...)                             \
    do {                                                 \
        ALOGD(__VA_ARGS__);                              \
        android::CallStack callstack;                    \
        callstack.update();                              \
        callstack.log(LOG_TAG, ANDROID_LOG_DEBUG, "  "); \
    }while (false)

namespace android {

#define UN_USED(arg)     (arg=arg)

#define HWC2_ALOGD_IF_VERBOSE(x, ...)  \
    ALOGD_IF(LogLevel(DBG_VERBOSE),"%s,line=%d " x ,__FUNCTION__,__LINE__, ##__VA_ARGS__)

#define HWC2_ALOGD_IF_DEBUG(x, ...)  \
    ALOGD_IF(LogLevel(DBG_DEBUG),"%s,line=%d " x ,__FUNCTION__,__LINE__, ##__VA_ARGS__)

#define HWC2_ALOGE(x, ...)  \
    ALOGE("%s,line=%d " x ,__FUNCTION__,__LINE__, ##__VA_ARGS__)

#define HWC2_ALOGI(x, ...)  \
    ALOGI("%s,line=%d " x ,__FUNCTION__,__LINE__, ##__VA_ARGS__)


enum LOG_LEVEL
{
    //Log level flag
    /*1*/
    DBG_FETAL = 1 << 0,
    /*2*/
    DBG_ERROR = 1 << 1,
    /*4*/
    DBG_WARN  = 1 << 2,
    /*8*/
    DBG_INFO  = 1 << 3,
    /*16*/
    DBG_DEBUG = 1 << 4,
    /*32*/
    DBG_VERBOSE = 1 << 5,
    /*Mask*/
    DBG_MARSK = 0xFF,
};

/* print time macros. */
#define PRINT_TIME_START        \
    struct timeval tpend1, tpend2;\
    long usec1 = 0;\
    gettimeofday(&tpend1,NULL);\

#define PRINT_TIME_END(tag)        \
    gettimeofday(&tpend2,NULL);\
    usec1 = 1000*(tpend2.tv_sec - tpend1.tv_sec) + (tpend2.tv_usec- tpend1.tv_usec)/1000;\
    ALOGD("%s use time=%ld ms",tag,usec1);

void InitDebugModule();
void InitHwcVersion();
int UpdateLogLevel();
bool LogLevel(LOG_LEVEL log_level);
void IncFrameCnt();
int GetFrameCnt();
int hwc_get_int_property(const char* pcProperty,const char* default_value);
bool hwc_get_bool_property(const char* pcProperty,const char* default_value);
int hwc_get_string_property(const char* pcProperty,const char* default_value,char* retult);

bool isRK356x(uint32_t soc_id);
bool isRK3566(uint32_t soc_id);
bool isRK3399(uint32_t soc_id);
bool isRK3588(uint32_t soc_id);

bool isDrmVerison44(uint32_t drm_version);
bool isDrmVerison419(uint32_t drm_version);
bool isDrmVerison510(uint32_t drm_version);
}
#endif
