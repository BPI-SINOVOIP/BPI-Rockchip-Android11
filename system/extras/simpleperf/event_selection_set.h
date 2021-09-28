/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef SIMPLE_PERF_EVENT_SELECTION_SET_H_
#define SIMPLE_PERF_EVENT_SELECTION_SET_H_

#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#include <android-base/macros.h>

#include "event_attr.h"
#include "event_fd.h"
#include "event_type.h"
#include "IOEventLoop.h"
#include "perf_event.h"
#include "record.h"
#include "RecordReadThread.h"

constexpr double DEFAULT_PERIOD_TO_CHECK_MONITORED_TARGETS_IN_SEC = 1;
constexpr uint64_t DEFAULT_SAMPLE_FREQ_FOR_NONTRACEPOINT_EVENT = 4000;
constexpr uint64_t DEFAULT_SAMPLE_PERIOD_FOR_TRACEPOINT_EVENT = 1;

struct CounterInfo {
  pid_t tid;
  int cpu;
  PerfCounter counter;
};

struct CountersInfo {
  uint32_t group_id;
  std::string event_name;
  std::string event_modifier;
  std::vector<CounterInfo> counters;
};

struct SampleSpeed {
  // There are two ways to set sample speed:
  // 1. sample_freq: take [sample_freq] samples every second.
  // 2. sample_period: take one sample every [sample_period] events happen.
  uint64_t sample_freq;
  uint64_t sample_period;
  SampleSpeed(uint64_t freq = 0, uint64_t period = 0) : sample_freq(freq), sample_period(period) {}
  bool UseFreq() const {
    // Only use one way to set sample speed.
    CHECK_NE(sample_freq != 0u, sample_period != 0u);
    return sample_freq != 0u;
  }
};

// EventSelectionSet helps to monitor events. It is used in following steps:
// 1. Create an EventSelectionSet, and add event types to monitor by calling
//    AddEventType() or AddEventGroup().
// 2. Define how to monitor events by calling SetEnableOnExec(), SampleIdAll(),
//    SetSampleFreq(), etc.
// 3. Start monitoring by calling OpenEventFilesForCpus() or
//    OpenEventFilesForThreadsOnCpus(). If SetEnableOnExec() has been called
//    in step 2, monitor will be delayed until the monitored thread calls
//    exec().
// 4. Read counters by calling ReadCounters(), or read mapped event records
//    by calling MmapEventFiles(), PrepareToReadMmapEventData() and
//    FinishReadMmapEventData().
// 5. Stop monitoring automatically in the destructor of EventSelectionSet by
//    closing perf event files.

class EventSelectionSet {
 public:
  EventSelectionSet(bool for_stat_cmd);
  ~EventSelectionSet();

  bool empty() const { return groups_.empty(); }

  bool AddEventType(const std::string& event_name, size_t* group_id = nullptr);
  bool AddEventGroup(const std::vector<std::string>& event_names, size_t* group_id = nullptr);
  std::vector<const EventType*> GetEvents() const;
  std::vector<const EventType*> GetTracepointEvents() const;
  bool ExcludeKernel() const;
  bool HasAuxTrace() const { return has_aux_trace_; }
  std::vector<EventAttrWithId> GetEventAttrWithId() const;

  void SetEnableOnExec(bool enable);
  bool GetEnableOnExec();
  void SampleIdAll();
  void SetSampleSpeed(size_t group_id, const SampleSpeed& speed);
  bool SetBranchSampling(uint64_t branch_sample_type);
  void EnableFpCallChainSampling();
  bool EnableDwarfCallChainSampling(uint32_t dump_stack_size);
  void SetInherit(bool enable);
  void SetClockId(int clock_id);
  bool NeedKernelSymbol() const;
  void SetRecordNotExecutableMaps(bool record);
  bool RecordNotExecutableMaps() const;
  void SetIncludeFilters(std::vector<std::string>&& filters) {
    include_filters_ = std::move(filters);
  }

  template <typename Collection = std::vector<pid_t>>
  void AddMonitoredProcesses(const Collection& processes) {
    processes_.insert(processes.begin(), processes.end());
  }

  template <typename Collection = std::vector<pid_t>>
  void AddMonitoredThreads(const Collection& threads) {
    threads_.insert(threads.begin(), threads.end());
  }

  const std::set<pid_t>& GetMonitoredProcesses() const { return processes_; }

  const std::set<pid_t>& GetMonitoredThreads() const { return threads_; }

  void ClearMonitoredTargets() {
    processes_.clear();
    threads_.clear();
  }

  bool HasMonitoredTarget() const {
    return !processes_.empty() || !threads_.empty();
  }

  IOEventLoop* GetIOEventLoop() {
    return loop_.get();
  }

  // If cpus = {}, monitor on all cpus, with a perf event file for each cpu.
  // If cpus = {-1}, monitor on all cpus, with a perf event file shared by all cpus.
  // Otherwise, monitor on selected cpus, with a perf event file for each cpu.
  bool OpenEventFiles(const std::vector<int>& cpus);
  bool ReadCounters(std::vector<CountersInfo>* counters);
  bool MmapEventFiles(size_t min_mmap_pages, size_t max_mmap_pages, size_t aux_buffer_size,
                      size_t record_buffer_size, bool allow_cutting_samples, bool exclude_perf);
  bool PrepareToReadMmapEventData(const std::function<bool(Record*)>& callback);
  bool SyncKernelBuffer();
  bool FinishReadMmapEventData();

  const simpleperf::RecordStat& GetRecordStat() {
    return record_read_thread_->GetStat();
  }

  // Stop profiling if all monitored processes/threads don't exist.
  bool StopWhenNoMoreTargets(double check_interval_in_sec =
                                 DEFAULT_PERIOD_TO_CHECK_MONITORED_TARGETS_IN_SEC);

  bool SetEnableEvents(bool enable);

 private:
  struct EventSelection {
    EventTypeAndModifier event_type_modifier;
    perf_event_attr event_attr;
    std::vector<std::unique_ptr<EventFd>> event_fds;
    // counters for event files closed for cpu hotplug events
    std::vector<CounterInfo> hotplugged_counters;
    std::vector<int> allowed_cpus;
  };
  typedef std::vector<EventSelection> EventSelectionGroup;

  bool BuildAndCheckEventSelection(const std::string& event_name, bool first_event,
                                   EventSelection* selection);
  void UnionSampleType();
  bool OpenEventFilesOnGroup(EventSelectionGroup& group, pid_t tid, int cpu,
                             std::string* failed_event_type);
  bool ApplyFilters();
  bool ReadMmapEventData(bool with_time_limit);

  bool CheckMonitoredTargets();
  bool HasSampler();

  const bool for_stat_cmd_;

  std::vector<EventSelectionGroup> groups_;
  std::set<pid_t> processes_;
  std::set<pid_t> threads_;

  std::unique_ptr<IOEventLoop> loop_;
  std::function<bool(Record*)> record_callback_;

  std::unique_ptr<simpleperf::RecordReadThread> record_read_thread_;

  bool has_aux_trace_ = false;
  std::vector<std::string> include_filters_;

  DISALLOW_COPY_AND_ASSIGN(EventSelectionSet);
};

bool IsBranchSamplingSupported();
bool IsDwarfCallChainSamplingSupported();
bool IsDumpingRegsForTracepointEventsSupported();
bool IsSettingClockIdSupported();
bool IsMmap2Supported();

#endif  // SIMPLE_PERF_EVENT_SELECTION_SET_H_
