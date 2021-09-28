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


#define LOG_TAG "hwc_debug"

#include "hwc_debug.h"
#include "hwc_rockchip.h"
#include <sstream>

namespace android {

unsigned int g_log_level;
unsigned int g_frame;

void inc_frame()
{
    g_frame++;
}

void dec_frame()
{
    g_frame--;
}

int get_frame()
{
    return g_frame;
}

int init_log_level()
{
    char value[PROPERTY_VALUE_MAX];
    int iValue;
    property_get("sys.hwc.log", value, "0");
    g_log_level = atoi(value);
    return 0;
}

bool log_level(LOG_LEVEL log_level)
{
    return g_log_level & log_level;
}

void init_rk_debug()
{
  g_log_level = 0;
  g_frame = 0;
  init_log_level();
}

/**
 * @brief Dump Layer data.
 *
 * @param layer_index   layer index
 * @param layer 		layer data
 * @return 				Errno no
 */
#define DUMP_LAYER_CNT (20)
static int DumpSurfaceCount = 0;
int DumpLayer(const char* layer_name,buffer_handle_t handle)
{
    char pro_value[PROPERTY_VALUE_MAX];

    property_get("sys.dump",pro_value,0);

    if(handle && !strcmp(pro_value,"true"))
    {
        FILE * pfile = NULL;
        char data_name[100] ;
        const gralloc_module_t *gralloc;
        void* cpu_addr;
        int i;
        int width,height,stride,byte_stride,format,size;

#if (!RK_PER_MODE && RK_DRM_GRALLOC)
        width = hwc_get_handle_attibute(handle,ATT_WIDTH);
        height = hwc_get_handle_attibute(handle,ATT_HEIGHT);
        stride = hwc_get_handle_attibute(handle,ATT_STRIDE);
        byte_stride = hwc_get_handle_attibute(handle,ATT_BYTE_STRIDE);
        format = hwc_get_handle_attibute(handle,ATT_FORMAT);
        size = hwc_get_handle_attibute(handle,ATT_SIZE);
#else
        width = hwc_get_handle_width(handle);
        height = hwc_get_handle_height(handle);
        stride = hwc_get_handle_stride(handle);
        byte_stride = hwc_get_handle_byte_stride(handle);
        format = hwc_get_handle_format(handle);
        size = hwc_get_handle_size(handle);
#endif
        system("mkdir /data/dump/ && chmod /data/dump/ 777 ");
        DumpSurfaceCount++;
        sprintf(data_name,"/data/dump/dmlayer%s_%d_%d_%d.bin",layer_name, DumpSurfaceCount,
                stride,height);
        hwc_lock(handle, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                        0, 0, width, height, (void **)&cpu_addr);
        pfile = fopen(data_name,"wb");
        if(pfile)
        {
            fwrite((const void *)cpu_addr,(size_t)(size),1,pfile);
            fflush(pfile);
            fclose(pfile);
            ALOGD(" dump surface layer_name: %s,data_name %s,w:%d,h:%d,stride :%d,size=%d,cpu_addr=%p",
                layer_name,data_name,width,height,byte_stride,size,cpu_addr);
        }
        else
        {
            ALOGE("Open %s fail", data_name);
            ALOGD(" dump surface layer_name: %s,data_name %s,w:%d,h:%d,stride :%d,size=%d,cpu_addr=%p",
                layer_name,data_name,width,height,byte_stride,size,cpu_addr);
        }
        hwc_unlock(handle);
        //only dump once time.
        if(DumpSurfaceCount > DUMP_LAYER_CNT)
        {
            DumpSurfaceCount = 0;
            property_set("sys.dump","0");
        }
    }
    return 0;
}

static unsigned int HWC_Clockms(void)
{
	struct timespec t = { .tv_sec = 0, .tv_nsec = 0 };
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (unsigned int)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

void hwc_dump_fps(void)
{
	static unsigned int n_frames = 0;
	static unsigned int lastTime = 0;

	++n_frames;

	if (property_get_bool("sys.hwc.fps", 0))
	{
		unsigned int time = HWC_Clockms();
		unsigned int intv = time - lastTime;
		if (intv >= HWC_DEBUG_FPS_INTERVAL_MS)
		{
			unsigned int fps = n_frames * 1000 / intv;
			ALOGD_IF(log_level(DBG_DEBUG),"fps %u", fps);

			n_frames = 0;
			lastTime = time;
		}
	}
}

void dump_layer(const gralloc_module_t *gralloc, bool bDump, hwc_layer_1_t *layer, int index) {
    size_t i;
  std::ostringstream out;
  int format;
  char layername[100];

  if (!bDump && !log_level(DBG_VERBOSE))
    return;

    if(layer->flags & HWC_SKIP_LAYER)
    {
        ALOGD_IF(log_level(DBG_VERBOSE) || bDump, "layer %p skipped", layer);
    }
    else
    {
        if(layer->handle)
        {
#if RK_DRM_GRALLOC
            format = hwc_get_handle_attibute(layer->handle, ATT_FORMAT);
#else
            format = hwc_get_handle_format(layer->handle);
#endif

#if RK_PRINT_LAYER_NAME
#ifdef USE_HWC2
                hwc_get_handle_layername(gralloc, layer->handle, layername, 100);
                out << "layer[" << index << "]=" << layername
#else
                out << "layer[" << index << "]=" << layer->LayerName
#endif

#else
                out << "layer[" << index << "]"
#endif

                << "\n\tlayer=" << layer
                << ",type=" << layer->compositionType
                << ",hints=" << layer->compositionType
                << ",flags=" << layer->flags
                << ",handle=" << layer->handle
                << ",format=0x" << std::hex << format
                << ",fd =" << std::dec << hwc_get_handle_primefd(layer->handle)
                << ",transform=0x" <<  std::hex << layer->transform
                << ",blend=0x" << layer->blending
                << ",sourceCropf{" << std::dec
                    << layer->sourceCropf.left << "," << layer->sourceCropf.top << ","
                    << layer->sourceCropf.right << "," << layer->sourceCropf.bottom
                << "},sourceCrop{"
                    << layer->sourceCrop.left << ","
                    << layer->sourceCrop.top << ","
                    << layer->sourceCrop.right << ","
                    << layer->sourceCrop.bottom
                << "},displayFrame{"
                    << layer->displayFrame.left << ","
                    << layer->displayFrame.top << ","
                    << layer->displayFrame.right << ","
                    << layer->displayFrame.bottom << "},";
        }
        else
        {
#if RK_PRINT_LAYER_NAME
#ifdef USE_HWC2
                hwc_get_handle_layername(layer->handle, layername, 100);
                out << "layer[" << index << "]=" << layername
#else
                out << "layer[" << index << "]=" << layer->LayerName
#endif

#else
                out << "layer[" << index << "]"
#endif
                << "\n\tlayer=" << layer
                << ",type=" << layer->compositionType
                << ",hints=" << layer->compositionType
                << ",flags=" << layer->flags
                << ",handle=" << layer->handle
                << ",transform=0x" <<  std::hex << layer->transform
                << ",blend=0x" << layer->blending
                << ",sourceCropf{" << std::dec
                    << layer->sourceCropf.left << "," << layer->sourceCropf.top << ","
                    << layer->sourceCropf.right << "," << layer->sourceCropf.bottom
                << "},sourceCrop{"
                    << layer->sourceCrop.left << ","
                    << layer->sourceCrop.top << ","
                    << layer->sourceCrop.right << ","
                    << layer->sourceCrop.bottom
                << "},displayFrame{"
                    << layer->displayFrame.left << ","
                    << layer->displayFrame.top << ","
                    << layer->displayFrame.right << ","
                    << layer->displayFrame.bottom << "},";
        }
        for (i = 0; i < layer->visibleRegionScreen.numRects; i++)
        {
            out << "rect[" << i << "]={"
                << layer->visibleRegionScreen.rects[i].left << ","
                << layer->visibleRegionScreen.rects[i].top << ","
                << layer->visibleRegionScreen.rects[i].right << ","
                << layer->visibleRegionScreen.rects[i].bottom << "},";
        }
        out << "\n";
        ALOGD_IF(log_level(DBG_VERBOSE) || bDump,"%s",out.str().c_str());
    }
}

}
