/*
 * Copyright (C) 2012, 2014-2017 The Android Open Source Project
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

#define LOG_TAG "PerformanceTraces"

#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sstream>
#include "PerformanceTraces.h"

NAMESPACE_DECLARATION {
namespace PerformanceTraces {

int HalAtrace::mTraceLevel = 0;

HalAtrace::HalAtrace(const char* func, const char* tag, const char* note, int value)
{
    std::ostringstream stringStream;
    if (value < 0 || note == nullptr) {
        stringStream << "< " << func << "," << tag << ">";
        ia_trace_begin(mTraceLevel, stringStream.str().c_str());
    } else {
        stringStream << "< " << func << "," << tag << ">:" << note << "(" << value << ")";
        ia_trace_begin(mTraceLevel, stringStream.str().c_str());
    }
}

HalAtrace::~HalAtrace()
{
    ia_trace_end(mTraceLevel);
}

void HalAtrace::reset(void)
{
    if (LogHelper::isPerfDumpTypeEnable(CAMERA_DEBUG_LOG_ATRACE_LEVEL))
      mTraceLevel = 1;
    else
      mTraceLevel = 0;
}

/**
 * \class PerformanceTimer
 *
 * Private class for managing R&D traces used for performance
 * analysis and testing.
 *
 * This code should be disabled in product builds.
 */
class PerformanceTimer {

public:
    nsecs_t mStartAt;
    nsecs_t mLastRead;
    bool mFilled;            //!< timestamp has been taken
    bool mRequested;         //!< trace is requested/enabled

    PerformanceTimer(void) :
        mStartAt(0),
        mLastRead(0),
        mFilled(false),
        mRequested(false) {
    }

    bool isRunning(void) { return mFilled && mRequested; }

    bool isRequested(void) { return mRequested; }

    int64_t timeUs(void) {
        uint64_t now = systemTime();
        mLastRead = now;
        return (now - mStartAt) / 1000;
    }

    int64_t lastTimeUs(void) {
        uint64_t now = systemTime();
        return (now - mLastRead) / 1000;
    }

   /**
     * Enforce a standard format on timestamp traces parsed
     * by offline PnP tools.
     *
     * \see system/core/include/android/log.h
     */
    void formattedTrace(const char* p, const char *f) {
        LOGD("%s:%s, Time: %" PRId64 " us, Diff: %" PRId64 " us",
             p, f, timeUs(), mFilled ? lastTimeUs() : -1);
    }

    void start(void) {
        mStartAt = mLastRead = systemTime();
        mFilled = true;
    }

    void stop(void) { mFilled = false; }

};

void reset(void) {}

} // namespace PerformanceTraces
} NAMESPACE_DECLARATION_END
