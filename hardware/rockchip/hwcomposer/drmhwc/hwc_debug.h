/*
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co.Ltd.
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

#ifndef _HWC_DEBUG_H_
#define _HWC_DEBUG_H_

#include <stdlib.h>

#ifdef ANDROID_P
#include <log/log.h>
#else
#include <cutils/log.h>
#endif

#include <cutils/properties.h>
#include <hardware/hwcomposer.h>
#if RK_DRM_GRALLOC
#include "gralloc_drm_handle.h"
#endif
#include <utils/CallStack.h>

#define HWC_GET_HANDLE_LAYERNAME(gralloc, sf_layer, sf_handle, layername, size) \
    if(sf_layer && sf_layer->compositionType != HWC_FRAMEBUFFER_TARGET && ((sf_layer->flags & HWC_SKIP_LAYER) == 0)) \
        hwc_get_handle_layername(gralloc,sf_handle,layername,size)\


//Print call statck when you call ALOGD_CALLSTACK.
#define ALOGD_CALLSTACK(...)                             \
    do {                                                 \
        ALOGD(__VA_ARGS__);                              \
        android::CallStack callstack;                    \
        callstack.update();                              \
        callstack.log(LOG_TAG, ANDROID_LOG_DEBUG, "  "); \
    }while (false)

namespace android {

enum LOG_LEVEL
{
    //Log level flag
    /*1*/
    DBG_VERBOSE = 1 << 0,
    /*2*/
    DBG_DEBUG = 1 << 1,
    /*4*/
    DBG_INFO = 1 << 2,
    /*8*/
    DBG_WARN = 1 << 3,
    /*16*/
    DBG_ERROR = 1 << 4,
    /*32*/
    DBG_FETAL = 1 << 5,
    /*64*/
    DBG_SILENT = 1 << 6,
};

bool log_level(LOG_LEVEL log_level);

/* interval ms of print fps.*/
#define HWC_DEBUG_FPS_INTERVAL_MS 1

/* print time macros. */
#define PRINT_TIME_START        \
    struct timeval tpend1, tpend2;\
    long usec1 = 0;\
    gettimeofday(&tpend1,NULL);\

#define PRINT_TIME_END(tag)        \
    gettimeofday(&tpend2,NULL);\
    usec1 = 1000*(tpend2.tv_sec - tpend1.tv_sec) + (tpend2.tv_usec- tpend1.tv_usec)/1000;\
    if (property_get_bool( PROPERTY_TYPE ".hwc.time", 0)) \
    ALOGD_IF(1,"%s use time=%ld ms",tag,usec1);


void inc_frame();
void dec_frame();
int get_frame();
int init_log_level();
bool log_level(LOG_LEVEL log_level);
void init_rk_debug();
int DumpLayer(const char* layer_name,buffer_handle_t handle);
int DumpLayerList(hwc_display_contents_1_t *dc, const gralloc_module_t *gralloc);
void hwc_dump_fps(void);
void dump_layer(const gralloc_module_t *gralloc, bool bDump, hwc_layer_1_t *layer, int index);
}

#endif

