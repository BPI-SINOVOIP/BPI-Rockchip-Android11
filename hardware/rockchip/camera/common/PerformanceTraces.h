/*
 * Copyright (C) 2012-2017 The Android Open Source Project
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

#ifndef ANDROID_LIBCAMERA_PERFORMANCE_TRACES
#define ANDROID_LIBCAMERA_PERFORMANCE_TRACES
#include <Utils.h>
#include <string.h>
#include <stdio.h>
#include <mutex>
#include "LogHelper.h"
#include <utils/Trace.h>
#include "IaAtrace.h"

#undef ATRACE_TAG
#define ATRACE_TAG ATRACE_TAG_CAMERA
/**
 * \class PerformanceTraces
 *
 * Interface for managing R&D traces used for performance
 * analysis and testing.
 *
 * This interface is designed to minimize call overhead and it can
 * be disabled altogether in product builds. Calling the functions
 * from different threads is safe (no crashes), but may lead to
 * at least transient incorrect results, so the output values need
 * to be postprocessed for analysis.
 *
 * This code should be disabled in product builds.
 */
NAMESPACE_DECLARATION {
namespace PerformanceTraces {

/**
 * \class HalAtrace
 *
 * This class allows tracing the execution of a method by writing some magic
 * data to trace_marker.
 * By declaring object of this class at the beginning of a method the
 * constructor code which writes method name and some extra information to
 * trace_marker is executed then.
 * When the method finishes the object is automatically destroyed. The code
 * in the destructor which writes "E" to trace_marker is executed then.
 * The tool, like Camtune, can offline visualizes those traces recorded
 * from trace_marker, and thus greatly improve the efficiency of performance
 * profiling.
 */
  class HalAtrace {
      public:
          HalAtrace(const char* func, const char* tag, const char* note = nullptr, int value = -1);
          ~HalAtrace();
          static void reset(void);
      private:
          static int mTraceLevel;
  };

  /**
   * Helper function to disable all the performance traces
   */
  void reset(void);

  /**
   * Helper macro to use HalAtrace.
   */
#ifdef CAMERA_HAL_DEBUG
  // Use hal define trace, not implement
  #define PERFORMANCE_HAL_ATRACE() \
      PerformanceTraces::HalAtrace atrace(__FUNCTION__, LOG_TAG);
  #define PERFORMANCE_HAL_ATRACE_PARAM1(note, value) \
      PerformanceTraces::HalAtrace atrace(__FUNCTION__, LOG_TAG, note, value);
#else
  #define PERFORMANCE_HAL_ATRACE()
  #define PERFORMANCE_HAL_ATRACE_PARAM1(note, value)
#endif

// Use Android ATRACE
// How to get a trace ?
// 1. get the systrace.py file
//    path: android-sdk/platform-tools/systrace
//          or external/chromium-trace/systrace.py
// 2. $ python external/chromium-trace/systrace.py --time=10 -o trace.html camera
//    this will start to trace 10s and get and the trace.html in current path,
//    your process need to trace should be done in this 10 second.
// 3. open the trace.html in google chrome web browser to analyzer the trace
#define CAMERA_TRACE_BUF 64

#define PERFORMANCE_ATRACE_CALL() ATRACE_CALL()
#define PERFORMANCE_ATRACE_NAME(name) ATRACE_NAME(name)
#define PERFORMANCE_ATRACE_BEGIN(name) ATRACE_BEGIN(name)
#define PERFORMANCE_ATRACE_END() ATRACE_END()

#define PERFORMANCE_ATRACE_ASYNC_BEGIN(name, cookie) ATRACE_ASYNC_BEGIN(name, cookie)
#define PERFORMANCE_ATRACE_ASYNC_END(name, cookie) ATRACE_ASYNC_END(name, cookie)

#define PERFORMANCE_ATRACE_NAME_SNPRINTF(fmt_str, ...) \
  char atrace_name[CAMERA_TRACE_BUF]; \
   snprintf(atrace_name, CAMERA_TRACE_BUF, fmt_str, ##__VA_ARGS__); \
  ATRACE_NAME(atrace_name)

#define PERFORMANCE_ATRACE_BEGIN_SNPRINTF(fmt_str, ...) \
  char atrace_begin[CAMERA_TRACE_BUF]; \
   snprintf(atrace_begin, CAMERA_TRACE_BUF, fmt_str, ##__VA_ARGS__); \
  ATRACE_BEGIN(atrace_begin)

} // ns PerformanceTraces

/**
 * \class ScopedPerfTrace
 *
 * This class allows tracing the execution of a method. By declaring object
 * of this class at the beginning of a method/function the constructor code is
 * executed then.
 * When the method finishes the object is automatically destroyed.
 * The code in the destructor is useful to trace how long it took to execute
 * a method.
 * If a maxExecTime is provided an error message will be printed in case the
 * execution time took longer than expected
 */
class ScopedPerfTrace {
public:
inline ScopedPerfTrace(int level, const char* name, nsecs_t maxExecTime = 0) :
       mLevel(level),
       mName(name),
       mMaxExecTime(maxExecTime)
{
    mStartTime = systemTime();
}

inline ~ScopedPerfTrace()
{
    nsecs_t actualExecTime = systemTime()- mStartTime;
    if (LogHelper::isPerfDumpTypeEnable(mLevel)) {
        LOGD("%s took %" PRId64 " ns", mName, actualExecTime);
    }

    if (CC_UNLIKELY(mMaxExecTime > 0 && actualExecTime > mMaxExecTime)){
        LOGW("KPI:%s took longer than expected. Actual %" PRId64 " us expected %" PRId64 " us",
                mName, actualExecTime/1000, mMaxExecTime/1000);
    }
}

private:
    nsecs_t mStartTime;     /*!> systemTime when this object was created */
    int mLevel;             /*!> Trace level used */
    const char* mName;      /*!> Name of this trace object */
    nsecs_t mMaxExecTime;   /*!> Maximum time this object is expected to live */
};

/**
 * HAL_KPI_TRACE_CALL
 * Prints traces of the execution time of the method and checks if it took
 * longer than maxTime. In that case it prints an warning trace
 */
#define HAL_KPI_TRACE_CALL(level, maxTime)  ScopedPerfTrace __kpiTracer(level, __FUNCTION__, maxTime)
#define HAL_PER_TRACE_NAME(level, name) ScopedPerfTrace  __perftracer(level, name )
#define HAL_PER_TRACE_CALL(level)  HAL_PER_TRACE_NAME(level, __FUNCTION__)

} NAMESPACE_DECLARATION_END
#endif // ANDROID_LIBCAMERA_PERFORMANCE_TRACES
