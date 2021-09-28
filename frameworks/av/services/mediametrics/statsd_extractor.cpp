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

//#define LOG_NDEBUG 0
#define LOG_TAG "statsd_extractor"
#include <utils/Log.h>

#include <dirent.h>
#include <inttypes.h>
#include <pthread.h>
#include <pwd.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <statslog.h>

#include "MediaMetricsService.h"
#include "frameworks/base/core/proto/android/stats/mediametrics/mediametrics.pb.h"
#include "iface_statsd.h"

namespace android {

bool statsd_extractor(const mediametrics::Item *item)
{
    if (item == nullptr) return false;

    // these go into the statsd wrapper
    const nsecs_t timestamp = MediaMetricsService::roundTime(item->getTimestamp());
    std::string pkgName = item->getPkgName();
    int64_t pkgVersionCode = item->getPkgVersionCode();
    int64_t mediaApexVersion = 0;


    // the rest into our own proto
    //
    ::android::stats::mediametrics::ExtractorData metrics_proto;

    // flesh out the protobuf we'll hand off with our data
    //

    // android.media.mediaextractor.fmt         string
    std::string fmt;
    if (item->getString("android.media.mediaextractor.fmt", &fmt)) {
        metrics_proto.set_format(std::move(fmt));
    }
    // android.media.mediaextractor.mime        string
    std::string mime;
    if (item->getString("android.media.mediaextractor.mime", &mime)) {
        metrics_proto.set_mime(std::move(mime));
    }
    // android.media.mediaextractor.ntrk        int32
    int32_t ntrk = -1;
    if (item->getInt32("android.media.mediaextractor.ntrk", &ntrk)) {
        metrics_proto.set_tracks(ntrk);
    }

    // android.media.mediaextractor.entry       string
    std::string entry_point_string;
    if (item->getString("android.media.mediaextractor.entry", &entry_point_string)) {
      stats::mediametrics::ExtractorData::EntryPoint entry_point;
      if (entry_point_string == "sdk") {
        entry_point = stats::mediametrics::ExtractorData_EntryPoint_SDK;
      } else if (entry_point_string == "ndk-with-jvm") {
        entry_point = stats::mediametrics::ExtractorData_EntryPoint_NDK_WITH_JVM;
      } else if (entry_point_string == "ndk-no-jvm") {
        entry_point = stats::mediametrics::ExtractorData_EntryPoint_NDK_NO_JVM;
      } else {
        entry_point = stats::mediametrics::ExtractorData_EntryPoint_OTHER;
      }
      metrics_proto.set_entry_point(entry_point);
    }

    std::string serialized;
    if (!metrics_proto.SerializeToString(&serialized)) {
        ALOGE("Failed to serialize extractor metrics");
        return false;
    }

    if (enabled_statsd) {
        android::util::BytesField bf_serialized( serialized.c_str(), serialized.size());
        (void)android::util::stats_write(android::util::MEDIAMETRICS_EXTRACTOR_REPORTED,
                                   timestamp, pkgName.c_str(), pkgVersionCode,
                                   mediaApexVersion,
                                   bf_serialized);

    } else {
        ALOGV("NOT sending: private data (len=%zu)", strlen(serialized.c_str()));
    }

    return true;
}

} // namespace android
