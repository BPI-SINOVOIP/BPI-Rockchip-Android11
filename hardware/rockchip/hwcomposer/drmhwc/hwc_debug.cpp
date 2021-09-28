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
#include "drmgralloc4.h"

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
    property_get( PROPERTY_TYPE ".hwc.log", value, "0");
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
#define DUMP_LAYER_CNT (10)
static int DumpSurfaceCount = 0;
int DumpLayer(const char* layer_name,buffer_handle_t handle)
{
    char pro_value[PROPERTY_VALUE_MAX];

    property_get( PROPERTY_TYPE ".dump",pro_value,0);

    if(handle && !strcmp(pro_value,"true"))
    {
        FILE * pfile = NULL;
        char data_name[100] ;
        const gralloc_module_t *gralloc;
        void* cpu_addr;
        int width,height,stride,byte_stride,format,size;
#if USE_GRALLOC_4
        gralloc = NULL;
#else   // USE_GRALLOC_4

        int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                          (const hw_module_t **)&gralloc);
        if (ret) {
            ALOGE("Failed to open gralloc module");
            return ret;
        }
#endif  // USE_GRALLOC_4
#if (!RK_PER_MODE && RK_DRM_GRALLOC)
        width = hwc_get_handle_attibute(gralloc,handle,ATT_WIDTH);
        height = hwc_get_handle_attibute(gralloc,handle,ATT_HEIGHT);
        stride = hwc_get_handle_attibute(gralloc,handle,ATT_STRIDE);
        byte_stride = hwc_get_handle_attibute(gralloc,handle,ATT_BYTE_STRIDE);
        format = hwc_get_handle_attibute(gralloc,handle,ATT_FORMAT);
        size = hwc_get_handle_attibute(gralloc,handle,ATT_SIZE);
#else
        width = hwc_get_handle_width(gralloc,handle);
        height = hwc_get_handle_height(gralloc,handle);
        stride = hwc_get_handle_stride(gralloc,handle);
        byte_stride = hwc_get_handle_byte_stride(gralloc,handle);
        format = hwc_get_handle_format(gralloc,handle);
        size = hwc_get_handle_size(gralloc,handle);
#endif
        system("mkdir /data/dump/ && chmod /data/dump/ 777 ");
        DumpSurfaceCount++;
        sprintf(data_name,"/data/dump/dmlayer_%.20s_%d_%d_%d.bin",layer_name,DumpSurfaceCount,
                stride,height);
#if USE_GRALLOC_4
                gralloc4::lock(handle,
                               GRALLOC_USAGE_SW_READ_MASK, // 'usage'
                               0, // 'x'
                               0, // 'y'
                               width,
                               height,
                               (void **)&cpu_addr); // 'outData'
#else   // USE_GRALLOC_4
                gralloc->lock(gralloc,
                              handle,
                              GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                              0,
                              0,
                              width,
                              height,
                              (void **)&cpu_addr);
#endif  // USE_GRALLOC_4

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
#if USE_GRALLOC_4
        gralloc4::unlock(handle);
#else   // USE_GRALLOC_4
        gralloc->unlock(gralloc, handle);
#endif  // USE_GRALLOC_4

        //only dump once time.
        if(DumpSurfaceCount > DUMP_LAYER_CNT)
        {
            DumpSurfaceCount = 0;
            property_set( PROPERTY_TYPE ".dump","0");
        }
    }
    return 0;
}

int DumpLayerList(hwc_display_contents_1_t *dc, const gralloc_module_t *gralloc)
{
    char pro_value[PROPERTY_VALUE_MAX];
    property_get( PROPERTY_TYPE ".dump",pro_value,0);
    if(strcmp(pro_value,"true"))
        return 0;

    bool dumpFrame = false;
    bool dumpTargetOnly = false;
    size_t targetNameIndex = 1000;
    static int DumpSurfaceCount = 0;

    size_t num_dc_layers = dc->numHwLayers;
    for (size_t j = 0; j < num_dc_layers; ++j) {
        hwc_layer_1_t *sf_layer = &dc->hwLayers[j];
        char layername[100];
        if(sf_layer->compositionType != HWC_FRAMEBUFFER_TARGET){
#if RK_PRINT_LAYER_NAME
#ifdef USE_HWC2
            if(sf_layer->handle){
                hwc_get_handle_layername(gralloc, sf_layer->handle, layername, 100);
            }
#else
            UN_USED(gralloc);
            strcpy(layername, sf_layer->LayerName);
#endif
#endif
        }else{
            strcpy(layername,"FB-target");
        }

        char targetName[PROPERTY_VALUE_MAX] = {0};
        char targetOnly[PROPERTY_VALUE_MAX] = {0};
        char dumpFrameCnt[PROPERTY_VALUE_MAX] = {0};

        property_get( PROPERTY_TYPE ".dump.target_name",targetName,"");
        property_get( PROPERTY_TYPE ".dump.only_target",targetOnly,"false");
        property_get( PROPERTY_TYPE ".dump.frame_cnt",dumpFrameCnt,"10");

        size_t num_dc_layers = dc->numHwLayers;
        if(DumpSurfaceCount < atoi(dumpFrameCnt)){
            if(!strcmp(targetName,"")){
                dumpFrame = true;
                DumpSurfaceCount++;
            } else {
                if(strstr(layername,targetName)){
                    dumpFrame = true;
                    targetNameIndex = j;
                    if(!strcmp(targetOnly,"true"))
                        dumpTargetOnly = true;
                    DumpSurfaceCount++;
                }
            }
        } else {
            property_set( PROPERTY_TYPE ".dump","false");
            DumpSurfaceCount = 0;
            return 0;
        }
    }

    if(dumpFrame){
        for(size_t i = 0; i < num_dc_layers; i++){
            hwc_layer_1_t *sf_layer = &dc->hwLayers[i];
            if(!sf_layer->handle)
                continue;

            if(dumpTargetOnly && targetNameIndex != i)
                continue;

            if(sf_layer->acquireFenceFd > 0){
                sync_wait(sf_layer->acquireFenceFd, -1);
                close(sf_layer->acquireFenceFd);
                sf_layer->acquireFenceFd = -1;
            }

            FILE * pfile = NULL;
            char data_name[100];
            void* cpu_addr;
            int width,height,stride,byte_stride,format,size;
#if RK_DRM_GRALLOC
            width = hwc_get_handle_attibute(gralloc,sf_layer->handle,ATT_WIDTH);
            height = hwc_get_handle_attibute(gralloc,sf_layer->handle,ATT_HEIGHT);
            stride = hwc_get_handle_attibute(gralloc,sf_layer->handle,ATT_STRIDE);
            byte_stride = hwc_get_handle_attibute(gralloc,sf_layer->handle,ATT_BYTE_STRIDE);
            format = hwc_get_handle_attibute(gralloc,sf_layer->handle,ATT_FORMAT);
            size = hwc_get_handle_attibute(gralloc,sf_layer->handle,ATT_SIZE);
#else
            width = hwc_get_handle_width(gralloc,sf_layer->handle);
            height = hwc_get_handle_height(gralloc,sf_layer->handle);
            stride = hwc_get_handle_stride(gralloc,sf_layer->handle);
            byte_stride = hwc_get_handle_byte_stride(gralloc,sf_layer->handle);
            format = hwc_get_handle_format(gralloc,sf_layer->handle);
            size = hwc_get_handle_size(gralloc,sf_layer->handle);
#endif

            system("mkdir /data/dump/ && chmod /data/dump/ 777 ");
            if( targetNameIndex == i )
                sprintf(data_name,"/data/dump/layer-%d-%dx%d-%zu-target.yuv",DumpSurfaceCount, stride,height,i);
            else if(sf_layer->compositionType == HWC_FRAMEBUFFER_TARGET)
                sprintf(data_name,"/data/dump/layer-%d-%dx%d-%zu-fb.yuv",DumpSurfaceCount, stride,height,i);
            else
                sprintf(data_name,"/data/dump/layer-%d-%dx%d-%zu.yuv",DumpSurfaceCount, stride,height,i);

#if USE_GRALLOC_4
            gralloc4::lock(sf_layer->handle,
                           GRALLOC_USAGE_SW_READ_MASK, // 'usage'
                           0, // 'x'
                           0, // 'y'
                           width,
                           height,
                           (void **)&cpu_addr); // 'outData'
#else
            gralloc->lock(gralloc, sf_layer->handle, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                            0, 0, width, height, (void **)&cpu_addr);
#endif
            pfile = fopen(data_name,"wb");
            if(pfile) {
                fwrite((const void *)cpu_addr,(size_t)(size),1,pfile);
                fflush(pfile);
                fclose(pfile);
                ALOGD("dump surface layer_name: %s,data_name %s,w:%d,h:%d,stride :%d,size=%d,cpu_addr=%p",
                    sf_layer->LayerName,data_name,width,height,byte_stride,size,cpu_addr);
            } else {
                ALOGE("Open %s fail", data_name);
                ALOGD("dump surface layer_name: %s,data_name %s,w:%d,h:%d,stride :%d,size=%d,cpu_addr=%p",
                    sf_layer->LayerName,data_name,width,height,byte_stride,size,cpu_addr);
            }
#if USE_GRALLOC_4
            gralloc4::unlock(sf_layer->handle);
#else
            gralloc->unlock(gralloc, sf_layer->handle);
#endif
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

	if (property_get_bool( PROPERTY_TYPE ".hwc.fps", 0))
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
            format = hwc_get_handle_attibute(gralloc, layer->handle, ATT_FORMAT);
#else
            format = hwc_get_handle_format(gralloc, layer->handle);
#endif

#if RK_PRINT_LAYER_NAME
#ifdef USE_HWC2
                HWC_GET_HANDLE_LAYERNAME(gralloc,layer, layer->handle, layername, 100);
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
                << ",fd =" << std::dec << hwc_get_handle_primefd(gralloc, layer->handle)
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
                HWC_GET_HANDLE_LAYERNAME(gralloc,layer, layer->handle, layername, 100);
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
