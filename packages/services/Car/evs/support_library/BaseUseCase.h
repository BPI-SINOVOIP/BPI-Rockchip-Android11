/*
 * Copyright (C) 2019 The Android Open Source Project
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
#ifndef EVS_SUPPORT_LIBRARY_BASEUSECASE_H_
#define EVS_SUPPORT_LIBRARY_BASEUSECASE_H_

#include <string>
#include <vector>

namespace android {
namespace automotive {
namespace evs {
namespace support {

using ::std::string;
using ::std::vector;

/**
 * Base class for all the use cases in the EVS support library.
 */
class BaseUseCase {
public:
    /**
     * Requests delivery of camera frames from the desired EVS camera(s). The
     * use case begins receiving periodic calls from EVS camera with new image
     * frames until stopVideoStream is called.
     *
     * If the same EVS camera has already been started by other use cases,
     * the frame delivery to this use case starts without affecting the status
     * of the EVS camera.
     *
     * @return Returns true if the video stream is started successfully.
     * Otherwise returns false.
     *
     * @see stopVideoStream()
     */
    virtual bool startVideoStream() = 0;

    /**
     * Stops the delivery of EVS camera frames, and tries to close the EVS
     * camera. Because delivery is asynchronous, frames may continue to
     * arrive for some time after this call returns.
     *
     * If other use cases are using the camera at the same time, the EVS
     * camera will not be closed, until all the other use cases using the
     * camera are stopped.
     *
     * @see startVideoStream()
     */
    virtual void stopVideoStream() = 0;

    /**
     * Default constructor for BaseUseCase.
     *
     * @param The ids for the desired EVS cameras.
     */
    BaseUseCase(vector<string> cameraIds) : mCameraIds(cameraIds) {};

    virtual ~BaseUseCase() {}

protected:
    vector<string> mCameraIds;
};

}  // namespace support
}  // namespace evs
}  // namespace automotive
}  // namespace android

#endif  // EVS_SUPPORT_LIBRARY_BASEUSECASE_H_
