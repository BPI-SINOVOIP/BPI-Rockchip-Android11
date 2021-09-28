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

#ifndef AAA_RKCTRLLOOP_H_
#define AAA_RKCTRLLOOP_H_

#include <CameraMetadata.h>
#include <vector>
#include <map>
#include <rkisp_control_loop.h>


NAMESPACE_DECLARATION {
/**
 * \class RKISP2CtrlLoop
 *
 */
class RKISP2CtrlLoop {
public:
    explicit RKISP2CtrlLoop(int camId);
    status_t init(const char* sensorName = nullptr,
                  const cl_result_callback_ops_t *cb = nullptr);
    void deinit();

    status_t start(const struct rkisp_cl_prepare_params_s& params);
    status_t stop();
    status_t setFrameParams(rkisp_cl_frame_metadata_s* frame_params);

private:
    // prevent copy constructor and assignment operator
    RKISP2CtrlLoop(const RKISP2CtrlLoop& other);
    RKISP2CtrlLoop& operator=(const RKISP2CtrlLoop& other);

// private members
private:
    int mCameraId;
    bool mIsStarted;
    void* mControlLoopCtx;
    cl_result_callback_ops_t *mCB;

}; //  class RKISP2CtrlLoop
} NAMESPACE_DECLARATION_END

#endif  //  AAA_RkCtrlLoop_H_
