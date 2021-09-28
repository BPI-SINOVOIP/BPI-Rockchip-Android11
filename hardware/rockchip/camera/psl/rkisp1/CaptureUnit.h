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

#include <memory>
#include <mutex>
#include "CaptureUnitSettings.h"
//#include <linux/rkisp1-config_v12.h>

#ifndef __CAPTURE_UNIT_H__
#define __CAPTURE_UNIT_H__

namespace android {
namespace camera2 {

/**
 * \class ICaptureEventListener
 *
 * Abstract interface implemented by entities interested on receiving notifications
 * from the input system.
 *
 * Notifications are sent for 2A statistics, histogram and RAW frames.
 */
class ICaptureEventListener {
public:

    enum CaptureMessageId {
        CAPTURE_MESSAGE_ID_EVENT = 0,
        CAPTURE_MESSAGE_ID_ERROR
    };

    enum CaptureEventType {
        CAPTURE_EVENT_MIPI_COMPRESSED = 0,
        CAPTURE_EVENT_MIPI_UNCOMPRESSED,
        CAPTURE_EVENT_RAW_BAYER,
        CAPTURE_EVENT_RAW_BAYER_SCALED,
        CAPTURE_EVENT_2A_STATISTICS,
        CAPTURE_EVENT_AE_HISTOGRAM,
        CAPTURE_EVENT_NEW_SENSOR_DESCRIPTOR,
        CAPTURE_EVENT_NEW_SOF,
        CAPTURE_EVENT_SHUTTER,
        CAPTURE_EVENT_YUV,
        CAPTURE_REQUEST_DONE,
        CAPTURE_EVENT_MAX
    };

    // For MESSAGE_ID_EVENT
    struct CaptureMessageEvent {
        CaptureEventType type;
        struct timeval   timestamp;
        unsigned int     sequence;
        unsigned int     reqId;
        CaptureMessageEvent() : type(CAPTURE_EVENT_MAX),
            sequence(0),
            reqId(0)
        {
            CLEAR(timestamp);
        }
    };

    // For MESSAGE_ID_ERROR
    struct CaptureMessageError {
        status_t code;
        CaptureMessageError() : code(CAPTURE_MESSAGE_ID_ERROR) {}
    };

    struct CaptureMessageData {
        CaptureMessageEvent event;
        CaptureMessageError error;
    };

    struct CaptureMessage {
        CaptureMessageId   id;
        CaptureMessageData data;
        CaptureMessage() : id(CAPTURE_MESSAGE_ID_ERROR) {}
    };

    virtual bool notifyCaptureEvent(CaptureMessage *msg) = 0;
    virtual ~ICaptureEventListener() {}

}; // ICaptureEventListener

} // namespace camera2
} // namespace android

#endif // __CAPTURE_UNIT_H__
