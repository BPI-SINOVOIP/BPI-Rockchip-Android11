/*
 * Copyright (C) 2013 The Android Open Source Project
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
//#define LOG_NDEBUG 0
#define LOG_TAG "EmulatedCamera_HotplugThread"
#include <log/log.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/inotify.h>

#include "EmulatedCameraHotplugThread.h"
#include "EmulatedCameraFactory.h"
#include <qemu_pipe_bp.h>

#define FAKE_HOTPLUG_FILE "/data/misc/media/emulator.camera.hotplug"

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024*(EVENT_SIZE+16))

#define SubscriberInfo EmulatedCameraHotplugThread::SubscriberInfo

namespace android {

EmulatedCameraHotplugThread::EmulatedCameraHotplugThread(
    std::vector<int> subscribedCameraIds) :
        Thread(/*canCallJava*/false),
        mSubscribedCameraIds(std::move(subscribedCameraIds)) {

    mRunning = true;
    mInotifyFd = 0;
}

EmulatedCameraHotplugThread::~EmulatedCameraHotplugThread() {
}

status_t EmulatedCameraHotplugThread::requestExitAndWait() {
    ALOGE("%s: Not implemented. Use requestExit + join instead",
          __FUNCTION__);
    return INVALID_OPERATION;
}

void EmulatedCameraHotplugThread::requestExit() {
    Mutex::Autolock al(mMutex);

    ALOGV("%s: Requesting thread exit", __FUNCTION__);
    mRunning = false;

    bool rmWatchFailed = false;
    Vector<SubscriberInfo>::iterator it;
    for (it = mSubscribers.begin(); it != mSubscribers.end(); ++it) {

        if (inotify_rm_watch(mInotifyFd, it->WatchID) == -1) {

            ALOGE("%s: Could not remove watch for camID '%d',"
                  " error: '%s' (%d)",
                 __FUNCTION__, it->CameraID, strerror(errno),
                 errno);

            rmWatchFailed = true ;
        } else {
            ALOGV("%s: Removed watch for camID '%d'",
                __FUNCTION__, it->CameraID);
        }
    }

    if (rmWatchFailed) { // unlikely
        // Give the thread a fighting chance to error out on the next
        // read
        if (close(mInotifyFd) == -1) {
            ALOGE("%s: close failure error: '%s' (%d)",
                 __FUNCTION__, strerror(errno), errno);
        }
    }

    ALOGV("%s: Request exit complete.", __FUNCTION__);
}

status_t EmulatedCameraHotplugThread::readyToRun() {
    Mutex::Autolock al(mMutex);

    mInotifyFd = -1;

    do {
        ALOGV("%s: Initializing inotify", __FUNCTION__);

        mInotifyFd = inotify_init();
        if (mInotifyFd == -1) {
            ALOGE("%s: inotify_init failure error: '%s' (%d)",
                 __FUNCTION__, strerror(errno), errno);
            mRunning = false;
            break;
        }

        /**
         * For each fake camera file, add a watch for when
         * the file is closed (if it was written to)
         */
        for (int cameraId: mSubscribedCameraIds) {
            if (!addWatch(cameraId)) {
                mRunning = false;
                break;
            }
        }
    } while(false);

    if (!mRunning) {
        status_t err = -errno;

        if (mInotifyFd != -1) {
            close(mInotifyFd);
        }

        return err;
    }

    return OK;
}

bool EmulatedCameraHotplugThread::threadLoop() {

    // If requestExit was already called, mRunning will be false
    while (mRunning) {
        char buffer[EVENT_BUF_LEN];
        int length = QEMU_PIPE_RETRY(
                        read(mInotifyFd, buffer, EVENT_BUF_LEN));

        if (length < 0) {
            ALOGE("%s: Error reading from inotify FD, error: '%s' (%d)",
                 __FUNCTION__, strerror(errno),
                 errno);
            mRunning = false;
            break;
        }

        ALOGV("%s: Read %d bytes from inotify FD", __FUNCTION__, length);

        int i = 0;
        while (i < length) {
            inotify_event* event = (inotify_event*) &buffer[i];

            if (event->mask & IN_IGNORED) {
                Mutex::Autolock al(mMutex);
                if (!mRunning) {
                    ALOGV("%s: Shutting down thread", __FUNCTION__);
                    break;
                } else {
                    ALOGE("%s: File was deleted, aborting",
                          __FUNCTION__);
                    mRunning = false;
                    break;
                }
            } else if (event->mask & IN_CLOSE_WRITE) {
                int cameraId = getCameraId(event->wd);

                if (cameraId < 0) {
                    ALOGE("%s: Got bad camera ID from WD '%d",
                          __FUNCTION__, event->wd);
                } else {
                    // Check the file for the new hotplug event
                    String8 filePath = getFilePath(cameraId);
                    /**
                     * NOTE: we carefully avoid getting an inotify
                     * for the same exact file because it's opened for
                     * read-only, but our inotify is for write-only
                     */
                    int newStatus = readFile(filePath);

                    if (newStatus < 0) {
                        mRunning = false;
                        break;
                    }

                    int halStatus = newStatus ?
                        CAMERA_DEVICE_STATUS_PRESENT :
                        CAMERA_DEVICE_STATUS_NOT_PRESENT;
                    gEmulatedCameraFactory.onStatusChanged(cameraId,
                                                           halStatus);
                }

            } else {
                ALOGW("%s: Unknown mask 0x%x",
                      __FUNCTION__, event->mask);
            }

            i += EVENT_SIZE + event->len;
        }
    }

    if (!mRunning) {
        close(mInotifyFd);
        return false;
    }

    return true;
}

String8 EmulatedCameraHotplugThread::getFilePath(int cameraId) const {
    return String8::format(FAKE_HOTPLUG_FILE ".%d", cameraId);
}

int EmulatedCameraHotplugThread::getCameraId(const String8& filePath) const {
    for (int cameraId: mSubscribedCameraIds) {
        String8 camPath = getFilePath(cameraId);

        if (camPath == filePath) {
            return cameraId;
        }
    }

    return NAME_NOT_FOUND;
}

int EmulatedCameraHotplugThread::getCameraId(int wd) const {
    for (size_t i = 0; i < mSubscribers.size(); ++i) {
        if (mSubscribers[i].WatchID == wd) {
            return mSubscribers[i].CameraID;
        }
    }

    return NAME_NOT_FOUND;
}

SubscriberInfo* EmulatedCameraHotplugThread::getSubscriberInfo(int cameraId)
{
    for (size_t i = 0; i < mSubscribers.size(); ++i) {
        if (mSubscribers[i].CameraID == cameraId) {
            return (SubscriberInfo*)&mSubscribers[i];
        }
    }

    return NULL;
}

bool EmulatedCameraHotplugThread::addWatch(int cameraId) {
    String8 camPath = getFilePath(cameraId);
    int wd = inotify_add_watch(mInotifyFd,
                               camPath.string(),
                               IN_CLOSE_WRITE);

    if (wd == -1) {
        ALOGE("%s: Could not add watch for '%s', error: '%s' (%d)",
             __FUNCTION__, camPath.string(), strerror(errno),
             errno);

        mRunning = false;
        return false;
    }

    ALOGV("%s: Watch added for camID='%d', wd='%d'",
          __FUNCTION__, cameraId, wd);

    SubscriberInfo si = { cameraId, wd };
    mSubscribers.push_back(si);

    return true;
}

bool EmulatedCameraHotplugThread::removeWatch(int cameraId) {
    SubscriberInfo* si = getSubscriberInfo(cameraId);

    if (!si) return false;

    if (inotify_rm_watch(mInotifyFd, si->WatchID) == -1) {

        ALOGE("%s: Could not remove watch for camID '%d', error: '%s' (%d)",
             __FUNCTION__, cameraId, strerror(errno),
             errno);

        return false;
    }

    Vector<SubscriberInfo>::iterator it;
    for (it = mSubscribers.begin(); it != mSubscribers.end(); ++it) {
        if (it->CameraID == cameraId) {
            break;
        }
    }

    if (it != mSubscribers.end()) {
        mSubscribers.erase(it);
    }

    return true;
}

int EmulatedCameraHotplugThread::readFile(const String8& filePath) const {

    int fd = QEMU_PIPE_RETRY(
                open(filePath.string(), O_RDONLY, /*mode*/0));
    if (fd == -1) {
        ALOGE("%s: Could not open file '%s', error: '%s' (%d)",
             __FUNCTION__, filePath.string(), strerror(errno), errno);
        return -1;
    }

    char buffer[1];
    int length;

    length = QEMU_PIPE_RETRY(
                    read(fd, buffer, sizeof(buffer)));

    int retval;

    ALOGV("%s: Read file '%s', length='%d', buffer='%c'",
         __FUNCTION__, filePath.string(), length, buffer[0]);

    if (length == 0) { // EOF
        retval = 0; // empty file is the same thing as 0
    } else if (buffer[0] == '0') {
        retval = 0;
    } else { // anything non-empty that's not beginning with '0'
        retval = 1;
    }

    close(fd);

    return retval;
}

} //namespace android
