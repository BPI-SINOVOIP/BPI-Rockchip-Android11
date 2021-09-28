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
#define LOG_TAG "HwStreamBase"

#include "LogHelper.h"
#include "HwStreamBase.h"
#include "CameraBuffer.h"

#include <utils/Errors.h>

namespace android {
namespace camera2 {

HwStreamBase::HwStreamBase(CameraStreamNode &stream)
{
    LOGI("@%s", __FUNCTION__);
    stream.query(&mInfo);
}


HwStreamBase::~HwStreamBase()
{
    LOGI("@%s", __FUNCTION__);
}

status_t HwStreamBase::query(FrameInfo *info)
{
    if (info!=nullptr)
        *info = mInfo;
    return NO_ERROR;
}

status_t HwStreamBase::registerBuffers(std::vector<std::shared_ptr<CameraBuffer> > &theBuffers)
{
    UNUSED(theBuffers);
    return NO_ERROR;
}

status_t HwStreamBase::capture(std::shared_ptr<CameraBuffer> aBuffer,
                                Camera3Request* request)
{
    UNUSED(aBuffer);
    UNUSED(request);
    return NO_ERROR;
}

status_t HwStreamBase::captureDone(std::shared_ptr<CameraBuffer> buffer,
                                    Camera3Request* request)
{
    UNUSED(buffer);
    UNUSED(request);
    return NO_ERROR;
}

status_t HwStreamBase::reprocess(std::shared_ptr<CameraBuffer> buffer,
                                  Camera3Request* request)
{
    LOGI("@%s capture stream", __FUNCTION__);
    UNUSED(buffer);
    UNUSED(request);
    return OK;
}

void HwStreamBase::dump(int fd) const
{
    UNUSED(fd);
}

status_t HwStreamBase::configure(void)
{
    return NO_ERROR;
}

} /* namespace camera2 */
} /* namespace android */
