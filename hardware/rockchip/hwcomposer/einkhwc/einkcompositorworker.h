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

#ifndef ANDROID_EINK_COMPOSITOR_WORKER_H_
#define ANDROID_EINK_COMPOSITOR_WORKER_H_

#include "drmhwcomposer.h"
#include "worker.h"
#include "hwc_rockchip.h"
#include "hwc_debug.h"
#include "drmframebuffer.h"

//open header
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//map header
#include <map>

//gui
#include <ui/Rect.h>
#include <ui/Region.h>
#include <ui/GraphicBufferMapper.h>

//rga
#include <RockchipRga.h>

//Trace
#include <Trace.h>

#include <queue>

#define MaxRgaBuffers 2

namespace android {

#define EINK_FB_SIZE		0x400000 /* 4M */

/*
 * IMPORTANT: Those values is corresponding to android hardware program,
 * so *FORBID* to changes bellow values, unless you know what you're doing.
 * And if you want to add new refresh modes, please appended to the tail.
 */
enum panel_refresh_mode {
	EPD_NULL			= -1,
	EPD_AUTO			= 0,
	EPD_OVERLAY		= 1,
	EPD_FULL_GC16		= 2,
	EPD_FULL_GL16		= 3,
	EPD_FULL_GLR16		= 4,
	EPD_FULL_GLD16		= 5,
	EPD_FULL_GCC16	= 6,
	EPD_PART_GC16		= 7,
	EPD_PART_GL16		= 8,
	EPD_PART_GLR16	= 9,
	EPD_PART_GLD16	= 10,
	EPD_PART_GCC16	= 11,
	EPD_A2				= 12,
	EPD_A2_DITHER        = 13,
	EPD_DU				= 14,
	EPD_DU4			= 15,
	EPD_A2_ENTER		= 16,
	EPD_RESET			= 17,
	EPD_SUSPEND		= 18,
	EPD_RESUME		= 19,
	EPD_POWER_OFF	= 20,
	EPD_FORCE_FULL	= 21,
	EPD_AUTO_DU		= 22,
	EPD_AUTO_DU4		= 23,
};

/*
 * IMPORTANT: android hardware use struct, so *FORBID* to changes this, unless you know what you're doing.
 */
struct ebc_buf_info_t {
	int offset;
	int epd_mode;
	int height;
	int width;
	int panel_color;
	int win_x1;
	int win_y1;
	int win_x2;
	int win_y2;
	int width_mm;
	int height_mm;
	int needpic; // 1: buf can not be drop by ebc, 0: buf can drop by ebc
	char tid_name[16];
};

struct win_coordinate{
	int x1;
	int x2;
	int y1;
	int y2;
};

#define USE_RGA 1
/*
 * ebc system ioctl command
 */
#define EBC_GET_BUFFER			(0x7000)
#define EBC_SEND_BUFFER			(0x7001)
#define EBC_GET_BUFFER_INFO		(0x7002)
#define EBC_SET_FULL_MODE_NUM	(0x7003)
#define EBC_ENABLE_OVERLAY		(0x7004)
#define EBC_DISABLE_OVERLAY		(0x7005)
#define EBC_GET_OSD_BUFFER	(0x7006)
#define EBC_SEND_OSD_BUFFER	(0x7007)
#define EBC_NEW_BUF_PREPARE	(0x7008)
#define EBC_SET_DIFF_PERCENT	(0x7009)
#define EBC_WAIT_NEW_BUF_TIME (0x700a)

class EinkCompositorWorker : public Worker {
 public:
  EinkCompositorWorker();
  ~EinkCompositorWorker() override;

  int Init(struct hwc_context_t *ctx);
  void QueueComposite(hwc_display_contents_1_t *dc,int gCurrentEpdMode,int gResetEpdMode);
   void SignalComposite();

 protected:
  void Routine() override;

 private:
  struct EinkComposition {
    UniqueFd outbuf_acquire_fence;
    std::vector<UniqueFd> layer_acquire_fences;
    int release_timeline;
    buffer_handle_t fb_handle = NULL;
    int einkMode;
  };

  int CreateNextTimelineFence();
  int FinishComposition(int timeline);
  int Rgba888ToGray256ByRga(DrmRgaBuffer &rgaBuffer,const buffer_handle_t          &fb_handle);
  int Rgba8888ClipRgba(DrmRgaBuffer &rgaBuffer,const buffer_handle_t          &fb_handle);
  int Rgba888ToGray16ByRga(int *output_buffer,const buffer_handle_t          &fb_handle, int epd_mode);
  int RgaClipGrayRect(DrmRgaBuffer &rgaBuffer,const buffer_handle_t &fb_handle);
  int ConvertToColorEink1(const buffer_handle_t &fb_handle);
  int ConvertToColorEink2(const buffer_handle_t &fb_handle);
  int InToOrOutY8Regal(const buffer_handle_t &fb_handle);
  int ConvertToY8Regal(const buffer_handle_t &fb_handle);
  int ConvertToY4Dither(const buffer_handle_t &fb_handle, int epd_mode);
  int ConvertToY1Dither(const buffer_handle_t &fb_handle);
  int ColorCommit(int epd_mode);
  int EinkCommit(int epd_mode);
  int Y4Commit(int epd_mode);
  int A2Commit(int epd_mode);
  int update_fullmode_num();
  int update_diff_percent_num();
  int update_waiting_time();
  int DumpEinkSurface(int *buffer);
  int PostEink(int *buffer, Rect rect, int mode);
  int PostEinkY8(int *buffer, Rect rect, int mode);
  int SetEinkMode(EinkComposition *composition);
  int SetColorEinkMode(EinkComposition *composition);
  void Compose(std::unique_ptr<EinkComposition> composition);

  bool isSupportRkRga() {
    RockchipRga& rkRga(RockchipRga::get());
    return rkRga.RkRgaIsReady();
  }

  std::queue<std::unique_ptr<EinkComposition>> composite_queue_;
  int timeline_fd_;
  int timeline_;
  int timeline_current_;

  // mutable since we need to acquire in HaveQueuedComposites
  mutable pthread_mutex_t eink_lock_;

  pthread_cond_t eink_queue_cond_;
  //Eink support
  struct hwc_context_t *hwc_context_;

  int ebc_fd = -1;
  void *ebc_buffer_base = NULL;
  int waveform_fd = -1;
  void *waveform_base = NULL;
  struct ebc_buf_info_t ebc_buf_info;
  struct ebc_buf_info_t commit_buf_info;
  int gLastEpdMode = EPD_PART_GC16;

  Region gLastA2Region;
  Region gSavedUpdateRegion;

  int rgaBuffer_index = 0;
  DrmRgaBuffer rgaBuffers[MaxRgaBuffers];
  int *gray16_buffer = NULL;
  int *gray256_old_buffer = NULL;
  int *gray256_new_buffer = NULL;
  char* rga_output_addr = NULL;
  bool rgba_to_y4_by_rga = false;
  buffer_handle_t last_fb_handle = NULL;
  int last_fb_handle_mode = EPD_PART_GC16;
};
}

#endif
