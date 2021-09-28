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

#define LOG_TAG "hwc-drm-debug"
#include <log/log.h>
#include <cutils/properties.h>

#include "rockchip/utils/drmdebug.h"

namespace android {

unsigned int g_log_level;
unsigned int g_frame;

void InitDebugModule()
{
  g_log_level = 0;
  g_frame = 0;
  UpdateLogLevel();
  InitHwcVersion();
}

void InitHwcVersion()
{
  char acVersion[50] = {0};
  strcpy(acVersion,GHWC_VERSION);
  property_set("vendor.ghwc.version", acVersion);
  ALOGD("DrmHwcTwo version : %s", acVersion);
  return;
}
int UpdateLogLevel()
{
  char value[PROPERTY_VALUE_MAX];
  property_get("vendor.hwc.log", value, "0");
  if(!strcmp(value,"info"))
    g_log_level = DBG_FETAL | DBG_ERROR | DBG_WARN | DBG_INFO;
  else if(!strcmp(value,"debug"))
    g_log_level = DBG_FETAL | DBG_ERROR | DBG_WARN | DBG_INFO | DBG_DEBUG;
  else if(!strcmp(value,"verbose"))
    g_log_level = DBG_FETAL | DBG_ERROR | DBG_WARN | DBG_INFO | DBG_DEBUG | DBG_VERBOSE;
  else if(!strcmp(value,"all"))
    g_log_level = DBG_MARSK;
  else
    g_log_level = atoi(value);
  return 0;
}
bool LogLevel(LOG_LEVEL log_level){
  return (g_log_level & log_level) > 0;
}

void IncFrameCnt() { g_frame++; }
int GetFrameCnt(){ return g_frame;}

}

