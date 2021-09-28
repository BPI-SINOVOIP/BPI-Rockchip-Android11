/*
 * Copyright (C) 2012 The Android Open Source Project
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

#define LOG_TAG "atrace"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#include <fstream>
#include <memory>

#include <binder/IBinder.h>
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>

#include <android/hardware/atrace/1.0/IAtraceDevice.h>
#include <android/hidl/manager/1.0/IServiceManager.h>
#include <hidl/ServiceManagement.h>

#include <pdx/default_transport/service_utility.h>
#include <utils/String8.h>
#include <utils/Timers.h>
#include <utils/Tokenizer.h>
#include <utils/Trace.h>
#include <android-base/file.h>
#include <android-base/macros.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>

using namespace android;
using pdx::default_transport::ServiceUtility;
using hardware::hidl_vec;
using hardware::hidl_string;
using hardware::Return;
using hardware::atrace::V1_0::IAtraceDevice;
using hardware::atrace::V1_0::Status;
using hardware::atrace::V1_0::toString;

using std::string;

#define MAX_SYS_FILES 11

const char* k_traceTagsProperty = "debug.atrace.tags.enableflags";
const char* k_userInitiatedTraceProperty = "debug.atrace.user_initiated";

const char* k_traceAppsNumberProperty = "debug.atrace.app_number";
const char* k_traceAppsPropertyTemplate = "debug.atrace.app_%d";
const char* k_coreServiceCategory = "core_services";
const char* k_pdxServiceCategory = "pdx";
const char* k_coreServicesProp = "ro.atrace.core.services";

typedef enum { OPT, REQ } requiredness  ;

struct TracingCategory {
    // The name identifying the category.
    const char* name;

    // A longer description of the category.
    const char* longname;

    // The userland tracing tags that the category enables.
    uint64_t tags;

    // The fname==NULL terminated list of /sys/ files that the category
    // enables.
    struct {
        // Whether the file must be writable in order to enable the tracing
        // category.
        requiredness required;

        // The path to the enable file.
        const char* path;
    } sysfiles[MAX_SYS_FILES];
};

/* Tracing categories */
static const TracingCategory k_categories[] = {
    { "gfx",        "Graphics",                 ATRACE_TAG_GRAPHICS, { } },
    { "input",      "Input",                    ATRACE_TAG_INPUT, { } },
    { "view",       "View System",              ATRACE_TAG_VIEW, { } },
    { "webview",    "WebView",                  ATRACE_TAG_WEBVIEW, { } },
    { "wm",         "Window Manager",           ATRACE_TAG_WINDOW_MANAGER, { } },
    { "am",         "Activity Manager",         ATRACE_TAG_ACTIVITY_MANAGER, { } },
    { "sm",         "Sync Manager",             ATRACE_TAG_SYNC_MANAGER, { } },
    { "audio",      "Audio",                    ATRACE_TAG_AUDIO, { } },
    { "video",      "Video",                    ATRACE_TAG_VIDEO, { } },
    { "camera",     "Camera",                   ATRACE_TAG_CAMERA, { } },
    { "hal",        "Hardware Modules",         ATRACE_TAG_HAL, { } },
    { "res",        "Resource Loading",         ATRACE_TAG_RESOURCES, { } },
    { "dalvik",     "Dalvik VM",                ATRACE_TAG_DALVIK, { } },
    { "rs",         "RenderScript",             ATRACE_TAG_RS, { } },
    { "bionic",     "Bionic C Library",         ATRACE_TAG_BIONIC, { } },
    { "power",      "Power Management",         ATRACE_TAG_POWER, { } },
    { "pm",         "Package Manager",          ATRACE_TAG_PACKAGE_MANAGER, { } },
    { "ss",         "System Server",            ATRACE_TAG_SYSTEM_SERVER, { } },
    { "database",   "Database",                 ATRACE_TAG_DATABASE, { } },
    { "network",    "Network",                  ATRACE_TAG_NETWORK, { } },
    { "adb",        "ADB",                      ATRACE_TAG_ADB, { } },
    { "vibrator",   "Vibrator",                 ATRACE_TAG_VIBRATOR, { } },
    { "aidl",       "AIDL calls",               ATRACE_TAG_AIDL, { } },
    { "nnapi",      "NNAPI",                    ATRACE_TAG_NNAPI, { } },
    { "rro",        "Runtime Resource Overlay", ATRACE_TAG_RRO, { } },
    { k_coreServiceCategory, "Core services", 0, { } },
    { k_pdxServiceCategory, "PDX services", 0, { } },
    { "sched",      "CPU Scheduling",   0, {
        { REQ,      "events/sched/sched_switch/enable" },
        { REQ,      "events/sched/sched_wakeup/enable" },
        { OPT,      "events/sched/sched_waking/enable" },
        { OPT,      "events/sched/sched_blocked_reason/enable" },
        { OPT,      "events/sched/sched_cpu_hotplug/enable" },
        { OPT,      "events/sched/sched_pi_setprio/enable" },
        { OPT,      "events/sched/sched_process_exit/enable" },
        { OPT,      "events/cgroup/enable" },
        { OPT,      "events/oom/oom_score_adj_update/enable" },
        { OPT,      "events/task/task_rename/enable" },
        { OPT,      "events/task/task_newtask/enable" },
    } },
    { "irq",        "IRQ Events",   0, {
        { REQ,      "events/irq/enable" },
        { OPT,      "events/ipi/enable" },
    } },
    { "irqoff",     "IRQ-disabled code section tracing", 0, {
        { REQ,      "events/preemptirq/irq_enable/enable" },
        { REQ,      "events/preemptirq/irq_disable/enable" },
    } },
    { "preemptoff", "Preempt-disabled code section tracing", 0, {
        { REQ,      "events/preemptirq/preempt_enable/enable" },
        { REQ,      "events/preemptirq/preempt_disable/enable" },
    } },
    { "i2c",        "I2C Events",   0, {
        { REQ,      "events/i2c/enable" },
        { REQ,      "events/i2c/i2c_read/enable" },
        { REQ,      "events/i2c/i2c_write/enable" },
        { REQ,      "events/i2c/i2c_result/enable" },
        { REQ,      "events/i2c/i2c_reply/enable" },
        { OPT,      "events/i2c/smbus_read/enable" },
        { OPT,      "events/i2c/smbus_write/enable" },
        { OPT,      "events/i2c/smbus_result/enable" },
        { OPT,      "events/i2c/smbus_reply/enable" },
    } },
    { "freq",       "CPU Frequency",    0, {
        { REQ,      "events/power/cpu_frequency/enable" },
        { OPT,      "events/power/clock_set_rate/enable" },
        { OPT,      "events/power/clock_disable/enable" },
        { OPT,      "events/power/clock_enable/enable" },
        { OPT,      "events/clk/clk_set_rate/enable" },
        { OPT,      "events/clk/clk_disable/enable" },
        { OPT,      "events/clk/clk_enable/enable" },
        { OPT,      "events/power/cpu_frequency_limits/enable" },
        { OPT,      "events/power/suspend_resume/enable" },
    } },
    { "membus",     "Memory Bus Utilization", 0, {
        { REQ,      "events/memory_bus/enable" },
    } },
    { "idle",       "CPU Idle",         0, {
        { REQ,      "events/power/cpu_idle/enable" },
    } },
    { "disk",       "Disk I/O",         0, {
        { OPT,      "events/f2fs/f2fs_sync_file_enter/enable" },
        { OPT,      "events/f2fs/f2fs_sync_file_exit/enable" },
        { OPT,      "events/f2fs/f2fs_write_begin/enable" },
        { OPT,      "events/f2fs/f2fs_write_end/enable" },
        { OPT,      "events/ext4/ext4_da_write_begin/enable" },
        { OPT,      "events/ext4/ext4_da_write_end/enable" },
        { OPT,      "events/ext4/ext4_sync_file_enter/enable" },
        { OPT,      "events/ext4/ext4_sync_file_exit/enable" },
        { REQ,      "events/block/block_rq_issue/enable" },
        { REQ,      "events/block/block_rq_complete/enable" },
    } },
    { "mmc",        "eMMC commands",    0, {
        { REQ,      "events/mmc/enable" },
    } },
    { "load",       "CPU Load",         0, {
        { REQ,      "events/cpufreq_interactive/enable" },
    } },
    { "sync",       "Synchronization",  0, {
        // linux kernel < 4.9
        { OPT,      "events/sync/enable" },
        // linux kernel == 4.9.x
        { OPT,      "events/fence/enable" },
        // linux kernel > 4.9
        { OPT,      "events/dma_fence/enable" },
    } },
    { "workq",      "Kernel Workqueues", 0, {
        { REQ,      "events/workqueue/enable" },
    } },
    { "memreclaim", "Kernel Memory Reclaim", 0, {
        { REQ,      "events/vmscan/mm_vmscan_direct_reclaim_begin/enable" },
        { REQ,      "events/vmscan/mm_vmscan_direct_reclaim_end/enable" },
        { REQ,      "events/vmscan/mm_vmscan_kswapd_wake/enable" },
        { REQ,      "events/vmscan/mm_vmscan_kswapd_sleep/enable" },
        { OPT,      "events/lowmemorykiller/enable" },
    } },
    { "regulators",  "Voltage and Current Regulators", 0, {
        { REQ,      "events/regulator/enable" },
    } },
    { "binder_driver", "Binder Kernel driver", 0, {
        { REQ,      "events/binder/binder_transaction/enable" },
        { REQ,      "events/binder/binder_transaction_received/enable" },
        { REQ,      "events/binder/binder_transaction_alloc_buf/enable" },
        { OPT,      "events/binder/binder_set_priority/enable" },
    } },
    { "binder_lock", "Binder global lock trace", 0, {
        { OPT,      "events/binder/binder_lock/enable" },
        { OPT,      "events/binder/binder_locked/enable" },
        { OPT,      "events/binder/binder_unlock/enable" },
    } },
    { "pagecache",  "Page cache", 0, {
        { REQ,      "events/filemap/enable" },
    } },
    { "memory",  "Memory", 0, {
        { OPT,      "events/mm_event/mm_event_record/enable" },
        { OPT,      "events/kmem/rss_stat/enable" },
        { OPT,      "events/kmem/ion_heap_grow/enable" },
        { OPT,      "events/kmem/ion_heap_shrink/enable" },
        { OPT,      "events/ion/ion_stat/enable" },
    } },
};

struct TracingVendorCategory {
    // The name identifying the category.
    std::string name;

    // A longer description of the category.
    std::string description;

    // If the category is enabled through command.
    bool enabled;

    TracingVendorCategory(string &&name, string &&description, bool enabled)
            : name(std::move(name))
            , description(std::move(description))
            , enabled(enabled)
    {}
};

/* Command line options */
static int g_traceDurationSeconds = 5;
static bool g_traceOverwrite = false;
static int g_traceBufferSizeKB = 2048;
static bool g_compress = false;
static bool g_nohup = false;
static int g_initialSleepSecs = 0;
static const char* g_categoriesFile = nullptr;
static const char* g_kernelTraceFuncs = nullptr;
static const char* g_debugAppCmdLine = "";
static const char* g_outputFile = nullptr;

/* Global state */
static bool g_tracePdx = false;
static bool g_traceAborted = false;
static bool g_categoryEnables[arraysize(k_categories)] = {};
static std::string g_traceFolder;
static sp<IAtraceDevice> g_atraceHal;
static std::vector<TracingVendorCategory> g_vendorCategories;

/* Sys file paths */
static const char* k_traceClockPath =
    "trace_clock";

static const char* k_traceBufferSizePath =
    "buffer_size_kb";

#if 0
// TODO: Re-enable after stabilization
static const char* k_traceCmdlineSizePath =
    "saved_cmdlines_size";
#endif

static const char* k_tracingOverwriteEnablePath =
    "options/overwrite";

static const char* k_currentTracerPath =
    "current_tracer";

static const char* k_printTgidPath =
    "options/print-tgid";

static const char* k_recordTgidPath =
    "options/record-tgid";

static const char* k_funcgraphAbsTimePath =
    "options/funcgraph-abstime";

static const char* k_funcgraphCpuPath =
    "options/funcgraph-cpu";

static const char* k_funcgraphProcPath =
    "options/funcgraph-proc";

static const char* k_funcgraphFlatPath =
    "options/funcgraph-flat";

static const char* k_ftraceFilterPath =
    "set_ftrace_filter";

static const char* k_tracingOnPath =
    "tracing_on";

static const char* k_tracePath =
    "trace";

static const char* k_traceStreamPath =
    "trace_pipe";

static const char* k_traceMarkerPath =
    "trace_marker";

// Check whether a file exists.
static bool fileExists(const char* filename) {
    return access((g_traceFolder + filename).c_str(), F_OK) != -1;
}

// Check whether a file is writable.
static bool fileIsWritable(const char* filename) {
    return access((g_traceFolder + filename).c_str(), W_OK) != -1;
}

// Truncate a file.
static bool truncateFile(const char* path)
{
    // This uses creat rather than truncate because some of the debug kernel
    // device nodes (e.g. k_ftraceFilterPath) currently aren't changed by
    // calls to truncate, but they are cleared by calls to creat.
    int traceFD = creat((g_traceFolder + path).c_str(), 0);
    if (traceFD == -1) {
        fprintf(stderr, "error truncating %s: %s (%d)\n", (g_traceFolder + path).c_str(),
            strerror(errno), errno);
        return false;
    }

    close(traceFD);

    return true;
}

static bool _writeStr(const char* filename, const char* str, int flags)
{
    std::string fullFilename = g_traceFolder + filename;
    int fd = open(fullFilename.c_str(), flags);
    if (fd == -1) {
        fprintf(stderr, "error opening %s: %s (%d)\n", fullFilename.c_str(),
                strerror(errno), errno);
        return false;
    }

    bool ok = true;
    ssize_t len = strlen(str);
    if (write(fd, str, len) != len) {
        fprintf(stderr, "error writing to %s: %s (%d)\n", fullFilename.c_str(),
                strerror(errno), errno);
        ok = false;
    }

    close(fd);

    return ok;
}

// Write a string to a file, returning true if the write was successful.
static bool writeStr(const char* filename, const char* str)
{
    return _writeStr(filename, str, O_WRONLY);
}

// Append a string to a file, returning true if the write was successful.
static bool appendStr(const char* filename, const char* str)
{
    return _writeStr(filename, str, O_APPEND|O_WRONLY);
}

static void writeClockSyncMarker()
{
  char buffer[128];
  int len = 0;
  int fd = open((g_traceFolder + k_traceMarkerPath).c_str(), O_WRONLY);
  if (fd == -1) {
      fprintf(stderr, "error opening %s: %s (%d)\n", k_traceMarkerPath,
              strerror(errno), errno);
      return;
  }
  float now_in_seconds = systemTime(CLOCK_MONOTONIC) / 1000000000.0f;

  len = snprintf(buffer, 128, "trace_event_clock_sync: parent_ts=%f\n", now_in_seconds);
  if (write(fd, buffer, len) != len) {
      fprintf(stderr, "error writing clock sync marker %s (%d)\n", strerror(errno), errno);
  }

  int64_t realtime_in_ms = systemTime(CLOCK_REALTIME) / 1000000;
  len = snprintf(buffer, 128, "trace_event_clock_sync: realtime_ts=%" PRId64 "\n", realtime_in_ms);
  if (write(fd, buffer, len) != len) {
      fprintf(stderr, "error writing clock sync marker %s (%d)\n", strerror(errno), errno);
  }

  close(fd);
}

// Enable or disable a kernel option by writing a "1" or a "0" into a /sys
// file.
static bool setKernelOptionEnable(const char* filename, bool enable)
{
    return writeStr(filename, enable ? "1" : "0");
}

// Check whether the category is supported on the device with the current
// rootness.  A category is supported only if all its required /sys/ files are
// writable and if enabling the category will enable one or more tracing tags
// or /sys/ files.
static bool isCategorySupported(const TracingCategory& category)
{
    if (strcmp(category.name, k_coreServiceCategory) == 0) {
        return !android::base::GetProperty(k_coreServicesProp, "").empty();
    }

    if (strcmp(category.name, k_pdxServiceCategory) == 0) {
        return true;
    }

    bool ok = category.tags != 0;
    for (int i = 0; i < MAX_SYS_FILES; i++) {
        const char* path = category.sysfiles[i].path;
        bool req = category.sysfiles[i].required == REQ;
        if (path != nullptr) {
            if (fileIsWritable(path)) {
                ok = true;
            } else if (req) {
                return false;
            }
        }
    }
    return ok;
}

// Check whether the category would be supported on the device if the user
// were root.  This function assumes that root is able to write to any file
// that exists.  It performs the same logic as isCategorySupported, but it
// uses file existence rather than writability in the /sys/ file checks.
static bool isCategorySupportedForRoot(const TracingCategory& category)
{
    bool ok = category.tags != 0;
    for (int i = 0; i < MAX_SYS_FILES; i++) {
        const char* path = category.sysfiles[i].path;
        bool req = category.sysfiles[i].required == REQ;
        if (path != nullptr) {
            if (req) {
                if (!fileExists(path)) {
                    return false;
                } else {
                    ok = true;
                }
            } else {
                ok |= fileExists(path);
            }
        }
    }
    return ok;
}

// Enable or disable overwriting of the kernel trace buffers.  Disabling this
// will cause tracing to stop once the trace buffers have filled up.
static bool setTraceOverwriteEnable(bool enable)
{
    return setKernelOptionEnable(k_tracingOverwriteEnablePath, enable);
}

// Set the user initiated trace property
static bool setUserInitiatedTraceProperty(bool enable)
{
    if (!android::base::SetProperty(k_userInitiatedTraceProperty, enable ? "1" : "")) {
        fprintf(stderr, "error setting user initiated strace system property\n");
        return false;
    }
    return true;
}

// Enable or disable kernel tracing.
static bool setTracingEnabled(bool enable)
{
    return setKernelOptionEnable(k_tracingOnPath, enable);
}

// Clear the contents of the kernel trace.
static bool clearTrace()
{
    return truncateFile(k_tracePath);
}

// Set the size of the kernel's trace buffer in kilobytes.
static bool setTraceBufferSizeKB(int size)
{
    char str[32] = "1";
    if (size < 1) {
        size = 1;
    }
    snprintf(str, 32, "%d", size);
    return writeStr(k_traceBufferSizePath, str);
}

#if 0
// TODO: Re-enable after stabilization
// Set the default size of cmdline hashtable
static bool setCmdlineSize()
{
    if (fileExists(k_traceCmdlineSizePath)) {
        return writeStr(k_traceCmdlineSizePath, "8192");
    }
    return true;
}
#endif

// Set the clock to the best available option while tracing. Use 'boot' if it's
// available; otherwise, use 'mono'. If neither are available use 'global'.
// Any write to the trace_clock sysfs file will reset the buffer, so only
// update it if the requested value is not the current value.
static bool setClock()
{
    std::ifstream clockFile((g_traceFolder + k_traceClockPath).c_str(), O_RDONLY);
    std::string clockStr((std::istreambuf_iterator<char>(clockFile)),
        std::istreambuf_iterator<char>());

    std::string newClock;
    if (clockStr.find("boot") != std::string::npos) {
        newClock = "boot";
    } else if (clockStr.find("mono") != std::string::npos) {
        newClock = "mono";
    } else {
        newClock = "global";
    }

    size_t begin = clockStr.find('[') + 1;
    size_t end = clockStr.find(']');
    if (newClock.compare(0, std::string::npos, clockStr, begin, end-begin) == 0) {
        return true;
    }
    return writeStr(k_traceClockPath, newClock.c_str());
}

static bool setPrintTgidEnableIfPresent(bool enable)
{
    // Pre-4.13 this was options/print-tgid as an android-specific option.
    // In 4.13+ this is an upstream option called options/record-tgid
    // Both options produce the same ftrace format change
    if (fileExists(k_printTgidPath)) {
        return setKernelOptionEnable(k_printTgidPath, enable);
    }
    if (fileExists(k_recordTgidPath)) {
        return setKernelOptionEnable(k_recordTgidPath, enable);
    }
    return true;
}

// Set the trace tags that userland tracing uses, and poke the running
// processes to pick up the new value.
static bool setTagsProperty(uint64_t tags)
{
    std::string value = android::base::StringPrintf("%#" PRIx64, tags);
    if (!android::base::SetProperty(k_traceTagsProperty, value)) {
        fprintf(stderr, "error setting trace tags system property\n");
        return false;
    }
    return true;
}

static void clearAppProperties()
{
    if (!android::base::SetProperty(k_traceAppsNumberProperty, "")) {
        fprintf(stderr, "failed to clear system property: %s",
              k_traceAppsNumberProperty);
    }
}

// Set the system property that indicates which apps should perform
// application-level tracing.
static bool setAppCmdlineProperty(char* cmdline)
{
    int i = 0;
    char* start = cmdline;
    while (start != nullptr) {
        char* end = strchr(start, ',');
        if (end != nullptr) {
            *end = '\0';
            end++;
        }
        std::string key = android::base::StringPrintf(k_traceAppsPropertyTemplate, i);
        if (!android::base::SetProperty(key, start)) {
            fprintf(stderr, "error setting trace app %d property to %s\n", i, key.c_str());
            clearAppProperties();
            return false;
        }
        start = end;
        i++;
    }

    std::string value = android::base::StringPrintf("%d", i);
    if (!android::base::SetProperty(k_traceAppsNumberProperty, value)) {
        fprintf(stderr, "error setting trace app number property to %s\n", value.c_str());
        clearAppProperties();
        return false;
    }
    return true;
}

// Disable all /sys/ enable files.
static bool disableKernelTraceEvents() {
    bool ok = true;
    for (size_t i = 0; i < arraysize(k_categories); i++) {
        const TracingCategory &c = k_categories[i];
        for (int j = 0; j < MAX_SYS_FILES; j++) {
            const char* path = c.sysfiles[j].path;
            if (path != nullptr && fileIsWritable(path)) {
                ok &= setKernelOptionEnable(path, false);
            }
        }
    }
    return ok;
}

// Verify that the comma separated list of functions are being traced by the
// kernel.
static bool verifyKernelTraceFuncs(const char* funcs)
{
    std::string buf;
    if (!android::base::ReadFileToString(g_traceFolder + k_ftraceFilterPath, &buf)) {
         fprintf(stderr, "error opening %s: %s (%d)\n", k_ftraceFilterPath,
            strerror(errno), errno);
         return false;
    }

    String8 funcList = String8::format("\n%s",buf.c_str());

    // Make sure that every function listed in funcs is in the list we just
    // read from the kernel, except for wildcard inputs.
    bool ok = true;
    char* myFuncs = strdup(funcs);
    char* func = strtok(myFuncs, ",");
    while (func) {
        if (!strchr(func, '*')) {
            String8 fancyFunc = String8::format("\n%s\n", func);
            bool found = funcList.find(fancyFunc.string(), 0) >= 0;
            if (!found || func[0] == '\0') {
                fprintf(stderr, "error: \"%s\" is not a valid kernel function "
                        "to trace.\n", func);
                ok = false;
            }
        }
        func = strtok(nullptr, ",");
    }
    free(myFuncs);
    return ok;
}

// Set the comma separated list of functions that the kernel is to trace.
static bool setKernelTraceFuncs(const char* funcs)
{
    bool ok = true;

    if (funcs == nullptr || funcs[0] == '\0') {
        // Disable kernel function tracing.
        if (fileIsWritable(k_currentTracerPath)) {
            ok &= writeStr(k_currentTracerPath, "nop");
        }
        if (fileIsWritable(k_ftraceFilterPath)) {
            ok &= truncateFile(k_ftraceFilterPath);
        }
    } else {
        // Enable kernel function tracing.
        ok &= writeStr(k_currentTracerPath, "function_graph");
        ok &= setKernelOptionEnable(k_funcgraphAbsTimePath, true);
        ok &= setKernelOptionEnable(k_funcgraphCpuPath, true);
        ok &= setKernelOptionEnable(k_funcgraphProcPath, true);
        ok &= setKernelOptionEnable(k_funcgraphFlatPath, true);

        // Set the requested filter functions.
        ok &= truncateFile(k_ftraceFilterPath);
        char* myFuncs = strdup(funcs);
        char* func = strtok(myFuncs, ",");
        while (func) {
            ok &= appendStr(k_ftraceFilterPath, func);
            func = strtok(nullptr, ",");
        }
        free(myFuncs);

        // Verify that the set functions are being traced.
        if (ok) {
            ok &= verifyKernelTraceFuncs(funcs);
        }
    }

    return ok;
}

static bool setCategoryEnable(const char* name)
{
    bool vendor_found = false;
    for (auto &c : g_vendorCategories) {
        if (strcmp(name, c.name.c_str()) == 0) {
            c.enabled = true;
            vendor_found = true;
        }
    }
    for (size_t i = 0; i < arraysize(k_categories); i++) {
        const TracingCategory& c = k_categories[i];
        if (strcmp(name, c.name) == 0) {
            if (isCategorySupported(c)) {
                g_categoryEnables[i] = true;
                return true;
            } else {
                if (isCategorySupportedForRoot(c)) {
                    fprintf(stderr, "error: category \"%s\" requires root "
                            "privileges.\n", name);
                } else {
                    fprintf(stderr, "error: category \"%s\" is not supported "
                            "on this device.\n", name);
                }
                return false;
            }
        }
    }
    if (vendor_found) {
        return true;
    }
    fprintf(stderr, "error: unknown tracing category \"%s\"\n", name);
    return false;
}

static bool setCategoriesEnableFromFile(const char* categories_file)
{
    if (!categories_file) {
        return true;
    }
    Tokenizer* tokenizer = nullptr;
    if (Tokenizer::open(String8(categories_file), &tokenizer) != NO_ERROR) {
        return false;
    }
    bool ok = true;
    while (!tokenizer->isEol()) {
        String8 token = tokenizer->nextToken(" ");
        if (token.isEmpty()) {
            tokenizer->skipDelimiters(" ");
            continue;
        }
        ok &= setCategoryEnable(token.string());
    }
    delete tokenizer;
    return ok;
}

static bool setUpUserspaceTracing()
{
    bool ok = true;

    // Set up the tags property.
    uint64_t tags = 0;
    for (size_t i = 0; i < arraysize(k_categories); i++) {
        if (g_categoryEnables[i]) {
            const TracingCategory &c = k_categories[i];
            tags |= c.tags;
        }
    }

    bool coreServicesTagEnabled = false;
    for (size_t i = 0; i < arraysize(k_categories); i++) {
        if (strcmp(k_categories[i].name, k_coreServiceCategory) == 0) {
            coreServicesTagEnabled = g_categoryEnables[i];
        }

        // Set whether to poke PDX services in this session.
        if (strcmp(k_categories[i].name, k_pdxServiceCategory) == 0) {
            g_tracePdx = g_categoryEnables[i];
        }
    }

    std::string packageList(g_debugAppCmdLine);
    if (coreServicesTagEnabled) {
        if (!packageList.empty()) {
            packageList += ",";
        }
        packageList += android::base::GetProperty(k_coreServicesProp, "");
    }
    ok &= setAppCmdlineProperty(&packageList[0]);
    ok &= setTagsProperty(tags);
    if (g_tracePdx) {
        ok &= ServiceUtility::PokeServices();
    }

    return ok;
}

static void cleanUpUserspaceTracing()
{
    setTagsProperty(0);
    clearAppProperties();

    if (g_tracePdx) {
        ServiceUtility::PokeServices();
    }
}


// Set all the kernel tracing settings to the desired state for this trace
// capture.
static bool setUpKernelTracing()
{
    bool ok = true;

    ok &= setUserInitiatedTraceProperty(true);

    // Set up the tracing options.
    ok &= setCategoriesEnableFromFile(g_categoriesFile);
    ok &= setTraceOverwriteEnable(g_traceOverwrite);
    ok &= setTraceBufferSizeKB(g_traceBufferSizeKB);
    // TODO: Re-enable after stabilization
    //ok &= setCmdlineSize();
    ok &= setClock();
    ok &= setPrintTgidEnableIfPresent(true);
    ok &= setKernelTraceFuncs(g_kernelTraceFuncs);

    // Disable all the sysfs enables.  This is done as a separate loop from
    // the enables to allow the same enable to exist in multiple categories.
    ok &= disableKernelTraceEvents();

    // Enable all the sysfs enables that are in an enabled category.
    for (size_t i = 0; i < arraysize(k_categories); i++) {
        if (g_categoryEnables[i]) {
            const TracingCategory &c = k_categories[i];
            for (int j = 0; j < MAX_SYS_FILES; j++) {
                const char* path = c.sysfiles[j].path;
                bool required = c.sysfiles[j].required == REQ;
                if (path != nullptr) {
                    if (fileIsWritable(path)) {
                        ok &= setKernelOptionEnable(path, true);
                    } else if (required) {
                        fprintf(stderr, "error writing file %s\n", path);
                        ok = false;
                    }
                }
            }
        }
    }

    return ok;
}

// Reset all the kernel tracing settings to their default state.
static void cleanUpKernelTracing()
{
    // Disable all tracing that we're able to.
    disableKernelTraceEvents();

    // Set the options back to their defaults.
    setTraceOverwriteEnable(true);
    setTraceBufferSizeKB(1);
    setPrintTgidEnableIfPresent(false);
    setKernelTraceFuncs(nullptr);
    setUserInitiatedTraceProperty(false);
}

// Enable tracing in the kernel.
static bool startTrace()
{
    return setTracingEnabled(true);
}

// Disable tracing in the kernel.
static void stopTrace()
{
    setTracingEnabled(false);
}

// Read data from the tracing pipe and forward to stdout
static void streamTrace()
{
    char trace_data[4096];
    int traceFD = open((g_traceFolder + k_traceStreamPath).c_str(), O_RDWR);
    if (traceFD == -1) {
        fprintf(stderr, "error opening %s: %s (%d)\n", k_traceStreamPath,
                strerror(errno), errno);
        return;
    }
    while (!g_traceAborted) {
        ssize_t bytes_read = read(traceFD, trace_data, 4096);
        if (bytes_read > 0) {
            write(STDOUT_FILENO, trace_data, bytes_read);
            fflush(stdout);
        } else {
            if (!g_traceAborted) {
                fprintf(stderr, "read returned %zd bytes err %d (%s)\n",
                        bytes_read, errno, strerror(errno));
            }
            break;
        }
    }
}

// Read the current kernel trace and write it to stdout.
static void dumpTrace(int outFd)
{
    ALOGI("Dumping trace");
    int traceFD = open((g_traceFolder + k_tracePath).c_str(), O_RDWR);
    if (traceFD == -1) {
        fprintf(stderr, "error opening %s: %s (%d)\n", k_tracePath,
                strerror(errno), errno);
        return;
    }

    if (g_compress) {
        z_stream zs;
        memset(&zs, 0, sizeof(zs));

        int result = deflateInit(&zs, Z_DEFAULT_COMPRESSION);
        if (result != Z_OK) {
            fprintf(stderr, "error initializing zlib: %d\n", result);
            close(traceFD);
            return;
        }

        constexpr size_t bufSize = 64*1024;
        std::unique_ptr<uint8_t> in(new uint8_t[bufSize]);
        std::unique_ptr<uint8_t> out(new uint8_t[bufSize]);
        if (!in || !out) {
            fprintf(stderr, "couldn't allocate buffers\n");
            close(traceFD);
            return;
        }

        int flush = Z_NO_FLUSH;

        zs.next_out = reinterpret_cast<Bytef*>(out.get());
        zs.avail_out = bufSize;

        do {

            if (zs.avail_in == 0) {
                // More input is needed.
                result = read(traceFD, in.get(), bufSize);
                if (result < 0) {
                    fprintf(stderr, "error reading trace: %s (%d)\n",
                            strerror(errno), errno);
                    result = Z_STREAM_END;
                    break;
                } else if (result == 0) {
                    flush = Z_FINISH;
                } else {
                    zs.next_in = reinterpret_cast<Bytef*>(in.get());
                    zs.avail_in = result;
                }
            }

            if (zs.avail_out == 0) {
                // Need to write the output.
                result = write(outFd, out.get(), bufSize);
                if ((size_t)result < bufSize) {
                    fprintf(stderr, "error writing deflated trace: %s (%d)\n",
                            strerror(errno), errno);
                    result = Z_STREAM_END; // skip deflate error message
                    zs.avail_out = bufSize; // skip the final write
                    break;
                }
                zs.next_out = reinterpret_cast<Bytef*>(out.get());
                zs.avail_out = bufSize;
            }

        } while ((result = deflate(&zs, flush)) == Z_OK);

        if (result != Z_STREAM_END) {
            fprintf(stderr, "error deflating trace: %s\n", zs.msg);
        }

        if (zs.avail_out < bufSize) {
            size_t bytes = bufSize - zs.avail_out;
            result = write(outFd, out.get(), bytes);
            if ((size_t)result < bytes) {
                fprintf(stderr, "error writing deflated trace: %s (%d)\n",
                        strerror(errno), errno);
            }
        }

        result = deflateEnd(&zs);
        if (result != Z_OK) {
            fprintf(stderr, "error cleaning up zlib: %d\n", result);
        }
    } else {
        char buf[4096];
        ssize_t rc;
        while ((rc = TEMP_FAILURE_RETRY(read(traceFD, buf, sizeof(buf)))) > 0) {
            if (!android::base::WriteFully(outFd, buf, rc)) {
                fprintf(stderr, "error writing trace: %s\n", strerror(errno));
                break;
            }
        }
        if (rc == -1) {
            fprintf(stderr, "error dumping trace: %s\n", strerror(errno));
        }
    }

    close(traceFD);
}

static void handleSignal(int /*signo*/)
{
    if (!g_nohup) {
        g_traceAborted = true;
    }
}

static void registerSigHandler()
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handleSignal;
    sigaction(SIGHUP, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGQUIT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

static void listSupportedCategories()
{
    for (size_t i = 0; i < arraysize(k_categories); i++) {
        const TracingCategory& c = k_categories[i];
        if (isCategorySupported(c)) {
            printf("  %10s - %s\n", c.name, c.longname);
        }
    }
    for (const auto &c : g_vendorCategories) {
        printf("  %10s - %s (HAL)\n", c.name.c_str(), c.description.c_str());
    }
}

// Print the command usage help to stderr.
static void showHelp(const char *cmd)
{
    fprintf(stderr, "usage: %s [options] [categories...]\n", cmd);
    fprintf(stderr, "options include:\n"
                    "  -a appname      enable app-level tracing for a comma "
                        "separated list of cmdlines; * is a wildcard matching any process\n"
                    "  -b N            use a trace buffer size of N KB\n"
                    "  -c              trace into a circular buffer\n"
                    "  -f filename     use the categories written in a file as space-separated\n"
                    "                    values in a line\n"
                    "  -k fname,...    trace the listed kernel functions\n"
                    "  -n              ignore signals\n"
                    "  -s N            sleep for N seconds before tracing [default 0]\n"
                    "  -t N            trace for N seconds [default 5]\n"
                    "  -z              compress the trace dump\n"
                    "  --async_start   start circular trace and return immediately\n"
                    "  --async_dump    dump the current contents of circular trace buffer\n"
                    "  --async_stop    stop tracing and dump the current contents of circular\n"
                    "                    trace buffer\n"
                    "  --stream        stream trace to stdout as it enters the trace buffer\n"
                    "                    Note: this can take significant CPU time, and is best\n"
                    "                    used for measuring things that are not affected by\n"
                    "                    CPU performance, like pagecache usage.\n"
                    "  --list_categories\n"
                    "                  list the available tracing categories\n"
                    " -o filename      write the trace to the specified file instead\n"
                    "                    of stdout.\n"
            );
}

bool findTraceFiles()
{
    static const std::string debugfs_path = "/sys/kernel/debug/tracing/";
    static const std::string tracefs_path = "/sys/kernel/tracing/";
    static const std::string trace_file = "trace_marker";

    bool tracefs = access((tracefs_path + trace_file).c_str(), F_OK) != -1;
    bool debugfs = access((debugfs_path + trace_file).c_str(), F_OK) != -1;

    if (!tracefs && !debugfs) {
        fprintf(stderr, "Error: Did not find trace folder\n");
        return false;
    }

    if (tracefs) {
        g_traceFolder = tracefs_path;
    } else {
        g_traceFolder = debugfs_path;
    }

    return true;
}

void initVendorCategories()
{
    g_atraceHal = IAtraceDevice::getService();

    if (g_atraceHal == nullptr) {
        // No atrace HAL
        return;
    }

    Return<void> ret = g_atraceHal->listCategories(
        [](const auto& list) {
            g_vendorCategories.reserve(list.size());
            for (const auto& category : list) {
                g_vendorCategories.emplace_back(category.name, category.description, false);
            }
        });
    if (!ret.isOk()) {
        fprintf(stderr, "calling atrace HAL failed: %s\n", ret.description().c_str());
    }
}

static bool setUpVendorTracing()
{
    if (g_atraceHal == nullptr) {
        // No atrace HAL
        return true;
    }

    std::vector<hidl_string> categories;
    for (const auto &c : g_vendorCategories) {
        if (c.enabled) {
            categories.emplace_back(c.name);
        }
    }

    if (!categories.size()) {
        return true;
    }

    auto ret = g_atraceHal->enableCategories(categories);
    if (!ret.isOk()) {
        fprintf(stderr, "calling atrace HAL failed: %s\n", ret.description().c_str());
        return false;
    } else if (ret != Status::SUCCESS) {
        fprintf(stderr, "calling atrace HAL failed: %s\n", toString(ret).c_str());
        return false;
    }
    return true;
}

static bool cleanUpVendorTracing()
{
    if (g_atraceHal == nullptr) {
        // No atrace HAL
        return true;
    }

    if (!g_vendorCategories.size()) {
        // No vendor categories
        return true;
    }

    auto ret = g_atraceHal->disableAllCategories();
    if (!ret.isOk()) {
        fprintf(stderr, "calling atrace HAL failed: %s\n", ret.description().c_str());
        return false;
    } else if (ret != Status::SUCCESS) {
        fprintf(stderr, "calling atrace HAL failed: %s\n", toString(ret).c_str());
        return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    bool async = false;
    bool traceStart = true;
    bool traceStop = true;
    bool traceDump = true;
    bool traceStream = false;
    bool onlyUserspace = false;

    if (argc == 2 && 0 == strcmp(argv[1], "--help")) {
        showHelp(argv[0]);
        exit(0);
    }

    if (!findTraceFiles()) {
        fprintf(stderr, "No trace folder found\n");
        exit(-1);
    }

    initVendorCategories();

    for (;;) {
        int ret;
        int option_index = 0;
        static struct option long_options[] = {
            {"async_start",       no_argument, nullptr,  0 },
            {"async_stop",        no_argument, nullptr,  0 },
            {"async_dump",        no_argument, nullptr,  0 },
            {"only_userspace",    no_argument, nullptr,  0 },
            {"list_categories",   no_argument, nullptr,  0 },
            {"stream",            no_argument, nullptr,  0 },
            {nullptr,                       0, nullptr,  0 }
        };

        ret = getopt_long(argc, argv, "a:b:cf:k:ns:t:zo:",
                          long_options, &option_index);

        if (ret < 0) {
            for (int i = optind; i < argc; i++) {
                if (!setCategoryEnable(argv[i])) {
                    fprintf(stderr, "error enabling tracing category \"%s\"\n", argv[i]);
                    exit(1);
                }
            }
            break;
        }

        switch(ret) {
            case 'a':
                g_debugAppCmdLine = optarg;
            break;

            case 'b':
                g_traceBufferSizeKB = atoi(optarg);
            break;

            case 'c':
                g_traceOverwrite = true;
            break;

            case 'f':
                g_categoriesFile = optarg;
            break;

            case 'k':
                g_kernelTraceFuncs = optarg;
            break;

            case 'n':
                g_nohup = true;
            break;

            case 's':
                g_initialSleepSecs = atoi(optarg);
            break;

            case 't':
                g_traceDurationSeconds = atoi(optarg);
            break;

            case 'z':
                g_compress = true;
            break;

            case 'o':
                g_outputFile = optarg;
            break;

            case 0:
                if (!strcmp(long_options[option_index].name, "async_start")) {
                    async = true;
                    traceStop = false;
                    traceDump = false;
                    g_traceOverwrite = true;
                } else if (!strcmp(long_options[option_index].name, "async_stop")) {
                    async = true;
                    traceStart = false;
                } else if (!strcmp(long_options[option_index].name, "async_dump")) {
                    async = true;
                    traceStart = false;
                    traceStop = false;
                } else if (!strcmp(long_options[option_index].name, "only_userspace")) {
                    onlyUserspace = true;
                } else if (!strcmp(long_options[option_index].name, "stream")) {
                    traceStream = true;
                    traceDump = false;
                } else if (!strcmp(long_options[option_index].name, "list_categories")) {
                    listSupportedCategories();
                    exit(0);
                }
            break;

            default:
                fprintf(stderr, "\n");
                showHelp(argv[0]);
                exit(-1);
            break;
        }
    }

    if (onlyUserspace) {
        if (!async || !(traceStart || traceStop)) {
            fprintf(stderr, "--only_userspace can only be used with "
                    "--async_start or --async_stop\n");
            exit(1);
        }
    }

    registerSigHandler();

    if (g_initialSleepSecs > 0) {
        sleep(g_initialSleepSecs);
    }

    bool ok = true;

    if (traceStart) {
        ok &= setUpUserspaceTracing();
    }

    if (ok && traceStart && !onlyUserspace) {
        ok &= setUpKernelTracing();
        ok &= setUpVendorTracing();
        ok &= startTrace();
    }

    if (ok && traceStart) {

        if (!traceStream && !onlyUserspace) {
            printf("capturing trace...");
            fflush(stdout);
        }

        // We clear the trace after starting it because tracing gets enabled for
        // each CPU individually in the kernel. Having the beginning of the trace
        // contain entries from only one CPU can cause "begin" entries without a
        // matching "end" entry to show up if a task gets migrated from one CPU to
        // another.
        if (!onlyUserspace)
            ok = clearTrace();

        writeClockSyncMarker();
        if (ok && !async && !traceStream) {
            // Sleep to allow the trace to be captured.
            struct timespec timeLeft;
            timeLeft.tv_sec = g_traceDurationSeconds;
            timeLeft.tv_nsec = 0;
            do {
                if (g_traceAborted) {
                    break;
                }
            } while (nanosleep(&timeLeft, &timeLeft) == -1 && errno == EINTR);
        }

        if (traceStream) {
            streamTrace();
        }
    }

    // Stop the trace and restore the default settings.
    if (traceStop && !onlyUserspace)
        stopTrace();

    if (ok && traceDump && !onlyUserspace) {
        if (!g_traceAborted) {
            printf(" done\n");
            fflush(stdout);
            int outFd = STDOUT_FILENO;
            if (g_outputFile) {
                outFd = open(g_outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
            if (outFd == -1) {
                printf("Failed to open '%s', err=%d", g_outputFile, errno);
            } else {
                dprintf(outFd, "TRACE:\n");
                dumpTrace(outFd);
                if (g_outputFile) {
                    close(outFd);
                }
            }
        } else {
            printf("\ntrace aborted.\n");
            fflush(stdout);
        }
        clearTrace();
    } else if (!ok) {
        fprintf(stderr, "unable to start tracing\n");
    }

    // Reset the trace buffer size to 1.
    if (traceStop) {
        cleanUpUserspaceTracing();
        if (!onlyUserspace) {
            cleanUpVendorTracing();
            cleanUpKernelTracing();
        }
    }

    return g_traceAborted ? 1 : 0;
}
