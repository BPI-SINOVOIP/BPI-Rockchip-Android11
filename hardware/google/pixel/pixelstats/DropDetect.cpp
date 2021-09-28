/*
 * Copyright (C) 2018 The Android Open Source Project
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
#include <pixelstats/DropDetect.h>

#include <chre/util/nanoapp/app_id.h>
#include <chre_host/host_protocol_host.h>
#include <chre_host/socket_client.h>

#include <android/frameworks/stats/1.0/IStats.h>
#define LOG_TAG "pixelstats-vendor"
#include <log/log.h>

#include <inttypes.h>
#include <math.h>

using android::sp;
using android::chre::HostProtocolHost;
using android::chre::IChreMessageHandlers;
using android::chre::SocketClient;
using android::frameworks::stats::V1_0::IStats;
using android::frameworks::stats::V1_0::PhysicalDropDetected;

// following convention of CHRE code.
namespace fbs = ::chre::fbs;

namespace android {
namespace hardware {
namespace google {
namespace pixel {

namespace {  // anonymous namespace for file-local definitions

// The following two structs are defined in nanoapps/drop/messaging.h
// by the DropDetect nanoapp.
struct __attribute__((__packed__)) DropEventPayload {
    float confidence;
    float accel_magnitude_peak;
    int32_t free_fall_duration_ns;
};

struct __attribute__((__packed__)) DropEventPayloadV2 {
    uint64_t free_fall_duration_ns;
    float impact_accel_x;
    float impact_accel_y;
    float impact_accel_z;
};

// This enum is defined in nanoapps/drop/messaging.h
// by the DropDetect nanoapp.
enum DropConstants {
    kDropEnableRequest = 1,
    kDropEnableNotification = 2,
    kDropDisableRequest = 3,
    kDropDisableNotification = 4,
    kDropEventDetection = 5,
    kDropEventDetectionV2 = 6,
};

void requestNanoappList(SocketClient &client) {
    flatbuffers::FlatBufferBuilder builder(64);
    HostProtocolHost::encodeNanoappListRequest(builder);

    if (!client.sendMessage(builder.GetBufferPointer(), builder.GetSize())) {
        ALOGE("Failed to send NanoappList request");
    }
}

}  // namespace

DropDetect::DropDetect(const uint64_t drop_detect_app_id) : kDropDetectAppId(drop_detect_app_id) {}

sp<DropDetect> DropDetect::start(const uint64_t drop_detect_app_id, const char *const chre_socket) {
    sp<DropDetect> dropDetect = new DropDetect(drop_detect_app_id);
    if (!dropDetect->connectInBackground(chre_socket, dropDetect)) {
        ALOGE("Couldn't connect to CHRE socket");
        return nullptr;
    }
    return dropDetect;
}

void DropDetect::onConnected() {
    requestNanoappList(*this);
}

/**
 * Decode unix socket msgs to CHRE messages, and call the appropriate
 * callback depending on the CHRE message.
 */
void DropDetect::onMessageReceived(const void *data, size_t length) {
    if (!HostProtocolHost::decodeMessageFromChre(data, length, *this)) {
        ALOGE("Failed to decode message");
    }
}

/**
 * Handle the response of a NanoappList request.
 * Ensure that the Drop Detect nanoapp is running.
 */
void DropDetect::handleNanoappListResponse(const fbs::NanoappListResponseT &response) {
    for (const std::unique_ptr<fbs::NanoappListEntryT> &nanoapp : response.nanoapps) {
        if (nanoapp->app_id == kDropDetectAppId) {
            if (!nanoapp->enabled)
                ALOGE("Drop Detect app not enabled");
            else
                ALOGI("Drop Detect enabled");
            return;
        }
    }
    ALOGE("Drop Detect app not found");
}

static PhysicalDropDetected dropEventFromNanoappPayload(const struct DropEventPayload *p) {
    ALOGI("Received drop detect message! Confidence %f Peak %f Duration %g",
          p->confidence, p->accel_magnitude_peak, p->free_fall_duration_ns / 1e9);

    uint8_t confidence = p->confidence * 100;
    confidence = std::min<int>(confidence, 100);
    confidence = std::max<int>(0, confidence);
    int32_t accel_magnitude_peak_1000ths_g = p->accel_magnitude_peak * 1000.0;
    int32_t free_fall_duration_ms = p->free_fall_duration_ns / 1000000;

    return PhysicalDropDetected{ confidence,
                                 accel_magnitude_peak_1000ths_g,
                                 free_fall_duration_ms };
}

static PhysicalDropDetected dropEventFromNanoappPayload(const struct DropEventPayloadV2 *p) {
    ALOGI("Received drop detect message: "
          "duration %g ms, impact acceleration: x = %f, y = %f, z = %f",
          p->free_fall_duration_ns / 1e6,
          p->impact_accel_x,
          p->impact_accel_y,
          p->impact_accel_z);

    float impact_magnitude = sqrt(p->impact_accel_x * p->impact_accel_x +
                                  p->impact_accel_y * p->impact_accel_y +
                                  p->impact_accel_z * p->impact_accel_z);
    /* Scale impact magnitude as percentage between [50, 100] m/s2. */
    constexpr float min_confidence_magnitude = 50;
    constexpr float max_confidence_magnitude = 100;
    uint8_t confidence_percentage =
        impact_magnitude < min_confidence_magnitude ? 0 :
        impact_magnitude > max_confidence_magnitude ? 100 :
            (impact_magnitude - min_confidence_magnitude) /
            (max_confidence_magnitude - min_confidence_magnitude) * 100;

    int32_t free_fall_duration_ms = static_cast<int32_t>(p->free_fall_duration_ns / 1000000);

    return PhysicalDropDetected{
        .confidencePctg = confidence_percentage,
        .accelPeak = static_cast<int32_t>(impact_magnitude * 1000),
        .freefallDuration = free_fall_duration_ms,
    };
}

static void reportDropEventToStatsd(const PhysicalDropDetected& drop) {
    sp<IStats> stats_client = IStats::tryGetService();
    if (!stats_client) {
        ALOGE("Unable to connect to Stats service");
    } else {
        Return<void> ret = stats_client->reportPhysicalDropDetected(drop);
        if (!ret.isOk())
            ALOGE("Unable to report physical drop to Stats service");
    }
}

/**
 * listen for messages from the DropDetect nanoapp and report them to
 * PixelStats.
 */
void DropDetect::handleNanoappMessage(const fbs::NanoappMessageT &message) {
    if (message.app_id != kDropDetectAppId)
        return;

    if (message.message_type == kDropEventDetection &&
        message.message.size() >= sizeof(struct DropEventPayload)) {
        reportDropEventToStatsd(dropEventFromNanoappPayload(
            reinterpret_cast<const struct DropEventPayload *>(&message.message[0])));
    } else if (message.message_type == kDropEventDetectionV2 &&
               message.message.size() >= sizeof(struct DropEventPayloadV2)) {
        reportDropEventToStatsd(dropEventFromNanoappPayload(
            reinterpret_cast<const struct DropEventPayloadV2 *>(&message.message[0])));
    }
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
