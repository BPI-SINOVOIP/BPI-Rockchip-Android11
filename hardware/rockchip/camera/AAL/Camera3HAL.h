/*
 * Copyright (C) 2013-2017 Intel Corporation
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

#ifndef _CAMERA3_HAL_H_
#define _CAMERA3_HAL_H_

#include <hardware/camera3.h>

#include "PlatformData.h"
#include "RequestThread.h"

NAMESPACE_DECLARATION {

/**
 * \class Camera3HAL
 *
 * This class represents a single HAL device instance. It has the following
 * roles:
 * - It implements the camera3_device_ops_t  API  defined by Android.
 * - It instantiates all the other objects that make the HAL:
 *      - PSL layer
 *      - RequestThread
 *      - StreamManager
 *      - Request manager
 */
class Camera3HAL : public camera3_device_ops_t {

public:
    Camera3HAL(int cameraId, const hw_module_t* module);
    virtual ~Camera3HAL();
    status_t init(void);
    status_t deinit(void);
    camera3_device_t* getDeviceStruct() { return &mDevice; }
    int getCameraId() { return mCameraId; };

    /**********************************************************************
     * camera3_device_ops_t implementation
     */
    status_t initialize(const camera3_callback_ops_t *callback_ops);

    int configure_streams(camera3_stream_configuration_t *stream_list);

    camera_metadata_t* construct_default_request_settings(int type);

    int process_capture_request(camera3_capture_request_t *request);

    void get_metadata_vendor_tag_ops(vendor_tag_query_ops_t *ops);

    void dump(int fd);
    void dumpTemplateMeta(CameraMetadata& metadata, int type);

    int flush();

private:
    int mCameraId;
    ICameraHw *mCameraHw;
    std::unique_ptr<RequestThread> mRequestThread;
    camera3_device_t   mDevice;
};

} NAMESPACE_DECLARATION_END
#endif // _CAMERA3_HAL_H_
