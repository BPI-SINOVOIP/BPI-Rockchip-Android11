/*
 * Copyright (C) 2017 Intel Corporation.
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

#ifndef HAL_ROCKCHIP_PSL_RKISP2_RKISP2DevImpl_H_
#define HAL_ROCKCHIP_PSL_RKISP2_RKISP2DevImpl_H_

#include <memory>
#include <Utils.h>
#include <vector>
#include <deque>
#include <mutex>
#include <array>
#include <dlfcn.h>
#include "utils/Mutex.h"
#include "utils/Thread.h"
#include <condition_variable>
#include <ui/GraphicBuffer.h>
#include "RgaCropScale.h"
#include "LogHelper.h"
#include "rockx/rockx.h"
#include "eptz/eptz_algorithm.h"

namespace android {
namespace camera2 {
namespace rkisp2 {

struct DetectData {
  int left;
  int top;
  int right;
  int bottom;
  float score;
};

class EptzThread : public android::Thread {
    public:
        EptzThread();
        virtual ~EptzThread();
        virtual bool threadLoop() override;
        void setPreviewCfg(int width, int height);
        void calculateRect(RgaCropScale::Params *p);
        void converData(RgaCropScale::Params p);
        void setMode(int mode);
        void setOcclusionMode(int mode);
        int getMode();

        bool runnable;
        bool isInit;
        bool has_img_data;
        std::vector<DetectData> mDetectDatas;
        std::vector<sp<GraphicBuffer>> nnBufVecs;
        int mLastXY[4];
    private:
        void EptzInitCfg(int width, int height);
        int RockxInit(char *model_path, char *licence_path);
        int RockxDetectFace(void *in_data, int inWidth, int inHeight, int inPixelFmt);
        int RockxDetectOcclusion(void *in_data, int inWidth, int inHeight, int inPixelFmt);
        void EptzInit(int mSrcWidth, int mSrcHeight, int mClipWidth, int mClipHeight);

        int eptz_mode;
        int occlusion_mode;
        int src_width;
        int src_height;
        int npu_width;
        int npu_height;
        int mTexUsage;
        rockx_handle_t rockx_handle;
        EptzInitInfo mEptzInfo;
        mutable std::mutex mtx_;
        mutable std::mutex face_mtx_;
};

} /* namespace rkisp2 */
} /* namespace camera2 */
} /* namespace android */

#endif
