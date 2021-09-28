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

#ifndef CAR_LIB_EVS_SUPPORT_RESOURCEMANAGER_H
#define CAR_LIB_EVS_SUPPORT_RESOURCEMANAGER_H

#include <utils/RefBase.h>
#include <unordered_map>
#include <mutex>

#include <android/hardware/automotive/evs/1.0/IEvsEnumerator.h>

#include "StreamHandler.h"

namespace android {
namespace automotive {
namespace evs {
namespace support {

using ::android::sp;
using ::std::string;
using ::std::mutex;
using ::std::unordered_map;

/*
 * Manages EVS related resources. E.g. evs camera, stream handler, and display.
 *
 * The methods in the class are guaranteed to be thread-safe.
 */
class ResourceManager : public android::RefBase {
public:
    /*
     * Gets the singleton instance of the class.
     */
    static sp<ResourceManager> getInstance();

    /*
     * Obtains a StreamHandler instance to receive evs camera imagery from the
     * given camera.
     *
     * When this function is called with a new camera id the first time, an evs
     * camera instance will be opened. An internal reference count will be
     * incremented by one every time when this method is called with the same
     * camera id. The count will be decreased by one when releaseStreamHandler
     * method is called, and when the reference count for the camera is
     * decreased to zero, the stream handler will be shut down and the evs
     * camera instance will be closed.
     *
     * The method will block other stream handler related calls. For example,
     * method releaseStreamHandler.
     *
     * @see releaseStreamHandler()
     */
    sp<StreamHandler> obtainStreamHandler(string pCameraId);

    /*
     * Releases the StreamHandler associated with the given camera.
     *
     * An internal reference count will be decreased when this method is
     * called. When the count is down to zero, the stream handler will be shut
     * down and the evs camera instance will be closed.
     *
     * The method will block other stream handler related calls. For example,
     * method obtainStreamHandler.
     *
     * @see obtainStreamHandler()
     */
    void releaseStreamHandler(string pCameraId);

    /*
     * Obtains an interface object used to exclusively interact with the
     * system's evs display.
     *
     * @see closeDisplay()
     */
    sp<IEvsDisplay> openDisplay();

    /*
     * Releases the evs display interface.
     *
     * @see openDisplay()
     */
    void closeDisplay(sp<IEvsDisplay>);

    /**
     * Returns true if display is opened by openDisplay method; returns false
     * if display is never opened, or closed by closeDisplay method.
     *
     * @see openDisplay()
     * @see closeDisplay()
     */
    bool isDisplayOpened();

private:
    static sp<IEvsEnumerator> getEvsEnumerator(string serviceName = kDefaultServiceName);

    static const string kDefaultServiceName;

    static sp<ResourceManager> sInstance;
    static sp<IEvsEnumerator> sEvs;
    static mutex sLockSingleton, sLockEvs;

    class CameraInstance : public RefBase {
    public:
        int useCaseCount = 0;
        string cameraId;
        sp<IEvsCamera> camera;
        sp<StreamHandler> handler;

    private:
        void onLastStrongRef(const void* /*id*/) {
            ALOGD("StreamHandler::onLastStrongRef");

            handler->shutdown();
            ALOGD("Stream handler for camera id (%s) has been shutdown",
                  cameraId.c_str());

            getEvsEnumerator()->closeCamera(camera);
            ALOGD("Camera with id (%s) has been closed", cameraId.c_str());
        }
    };

    sp<IEvsDisplay> mDisplay;
    unordered_map<string, sp<CameraInstance>> mCameraInstances;
    mutex mLockStreamHandler, mLockDisplay;
};

}  // namespace support
}  // namespace evs
}  // namespace automotive
}  // namespace android

#endif //CAR_LIB_EVS_SUPPORT_RESOURCEMANAGER_H
