/*
 * Copyright (C) 2020 The Android Open Source Project
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

#pragma once

#include <math.h>
#include <sys/types.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include <android-base/stringprintf.h>

#include "event_selection_set.h"

namespace simpleperf {

struct CounterSum {
  uint64_t value = 0;
  uint64_t time_enabled = 0;
  uint64_t time_running = 0;

  void FromCounter(const PerfCounter& counter) {
    value = counter.value;
    time_enabled = counter.time_enabled;
    time_running = counter.time_running;
  }

  void ToCounter(PerfCounter& counter) const {
    counter.value = value;
    counter.time_enabled = time_enabled;
    counter.time_running = time_running;
  }

  CounterSum operator+(const CounterSum& other) const {
    CounterSum res;
    res.value = value + other.value;
    res.time_enabled = time_enabled + other.time_enabled;
    res.time_running = time_running + other.time_running;
    return res;
  }

  CounterSum operator-(const CounterSum& other) const {
    CounterSum res;
    res.value = value - other.value;
    res.time_enabled = time_enabled - other.time_enabled;
    res.time_running = time_running - other.time_running;
    return res;
  }
};

struct ThreadInfo {
  pid_t tid;
  pid_t pid;
  std::string name;
};

struct CounterSummary {
  std::string type_name;
  std::string modifier;
  uint32_t group_id;
  const ThreadInfo* thread;
  int cpu;  // -1 represents all cpus
  uint64_t count;
  uint64_t runtime_in_ns;
  double scale;
  std::string readable_count;
  std::string comment;
  bool auto_generated;

  CounterSummary(const std::string& type_name, const std::string& modifier, uint32_t group_id,
                 const ThreadInfo* thread, int cpu, uint64_t count, uint64_t runtime_in_ns,
                 double scale, bool auto_generated, bool csv)
      : type_name(type_name),
        modifier(modifier),
        group_id(group_id),
        thread(thread),
        cpu(cpu),
        count(count),
        runtime_in_ns(runtime_in_ns),
        scale(scale),
        auto_generated(auto_generated) {
    readable_count = ReadableCountValue(csv);
  }

  bool IsMonitoredAtTheSameTime(const CounterSummary& other) const {
    // Two summaries are monitored at the same time if they are in the same
    // group or are monitored all the time.
    if (group_id == other.group_id) {
      return true;
    }
    return IsMonitoredAllTheTime() && other.IsMonitoredAllTheTime();
  }

  std::string Name() const {
    if (modifier.empty()) {
      return type_name;
    }
    return type_name + ":" + modifier;
  }

  bool IsMonitoredAllTheTime() const {
    // If an event runs all the time it is enabled (by not sharing hardware
    // counters with other events), the scale of its summary is usually within
    // [1, 1 + 1e-5]. By setting SCALE_ERROR_LIMIT to 1e-5, We can identify
    // events monitored all the time in most cases while keeping the report
    // error rate <= 1e-5.
    constexpr double SCALE_ERROR_LIMIT = 1e-5;
    return (fabs(scale - 1.0) < SCALE_ERROR_LIMIT);
  }

 private:
  std::string ReadableCountValue(bool csv) {
    if (type_name == "cpu-clock" || type_name == "task-clock") {
      // Convert nanoseconds to milliseconds.
      double value = count / 1e6;
      return android::base::StringPrintf("%lf(ms)", value);
    } else {
      // Convert big numbers to human friendly mode. For example,
      // 1000000 will be converted to 1,000,000.
      std::string s = android::base::StringPrintf("%" PRIu64, count);
      if (csv) {
        return s;
      } else {
        for (size_t i = s.size() - 1, j = 1; i > 0; --i, ++j) {
          if (j == 3) {
            s.insert(s.begin() + i, ',');
            j = 0;
          }
        }
        return s;
      }
    }
  }
};

// Build a vector of CounterSummary.
class CounterSummaryBuilder {
 public:
  CounterSummaryBuilder(bool report_per_thread, bool report_per_core, bool csv,
                        const std::unordered_map<pid_t, ThreadInfo>& thread_map)
      : report_per_thread_(report_per_thread),
        report_per_core_(report_per_core),
        csv_(csv),
        thread_map_(thread_map) {}

  void AddCountersForOneEventType(const CountersInfo& info) {
    std::unordered_map<uint64_t, CounterSum> sum_map;
    for (const auto& counter : info.counters) {
      uint64_t key = 0;
      if (report_per_thread_) {
        key |= counter.tid;
      }
      if (report_per_core_) {
        key |= static_cast<uint64_t>(counter.cpu) << 32;
      }
      CounterSum& sum = sum_map[key];
      CounterSum add;
      add.FromCounter(counter.counter);
      sum = sum + add;
    }
    size_t pre_sum_count = summaries_.size();
    for (const auto& pair : sum_map) {
      pid_t tid = report_per_thread_ ? static_cast<pid_t>(pair.first & UINT32_MAX) : 0;
      int cpu = report_per_core_ ? static_cast<int>(pair.first >> 32) : -1;
      const CounterSum& sum = pair.second;
      AddSummary(info, tid, cpu, sum);
    }
    if (report_per_thread_ || report_per_core_) {
      SortSummaries(summaries_.begin() + pre_sum_count, summaries_.end());
    }
  }

  std::vector<CounterSummary> Build() {
    std::vector<CounterSummary> res = std::move(summaries_);
    summaries_.clear();
    return res;
  }

 private:
  void AddSummary(const CountersInfo& info, pid_t tid, int cpu, const CounterSum& sum) {
    double scale = 1.0;
    if (sum.time_running < sum.time_enabled && sum.time_running != 0) {
      scale = static_cast<double>(sum.time_enabled) / sum.time_running;
    }
    if ((report_per_thread_ || report_per_core_) && sum.time_running == 0) {
      // No need to report threads or cpus not running.
      return;
    }
    const ThreadInfo* thread = nullptr;
    if (report_per_thread_) {
      auto it = thread_map_.find(tid);
      CHECK(it != thread_map_.end());
      thread = &it->second;
    }
    summaries_.emplace_back(info.event_name, info.event_modifier, info.group_id, thread, cpu,
                            sum.value, sum.time_running, scale, false, csv_);
  }

  void SortSummaries(std::vector<CounterSummary>::iterator begin,
                     std::vector<CounterSummary>::iterator end) {
    if (report_per_thread_ && report_per_core_) {
      // First sort by event count for all cpus in a thread, then sort by event count of each cpu.
      std::unordered_map<pid_t, uint64_t> count_per_thread;
      for (auto it = begin; it != end; ++it) {
        count_per_thread[it->thread->tid] += it->count;
      }
      std::sort(begin, end, [&](const CounterSummary& s1, const CounterSummary& s2) {
        pid_t tid1 = s1.thread->tid;
        pid_t tid2 = s2.thread->tid;
        if (tid1 != tid2) {
          if (count_per_thread[tid1] != count_per_thread[tid2]) {
            return count_per_thread[tid1] > count_per_thread[tid2];
          }
          return tid1 < tid2;
        }
        return s1.count > s2.count;
      });
    } else {
      std::sort(begin, end, [](const CounterSummary& s1, const CounterSummary& s2) {
        return s1.count > s2.count;
      });
    }
  };

  const bool report_per_thread_;
  const bool report_per_core_;
  const bool csv_;
  const std::unordered_map<pid_t, ThreadInfo>& thread_map_;
  std::vector<CounterSummary> summaries_;
};

class CounterSummaries {
 public:
  explicit CounterSummaries(std::vector<CounterSummary>&& summaries, bool csv)
      : summaries_(std::move(summaries)), csv_(csv) {}
  const std::vector<CounterSummary>& Summaries() { return summaries_; }

  const CounterSummary* FindSummary(const std::string& type_name, const std::string& modifier,
                                    const ThreadInfo* thread, int cpu);

  // If we have two summaries monitoring the same event type at the same time,
  // that one is for user space only, and the other is for kernel space only;
  // then we can automatically generate a summary combining the two results.
  // For example, a summary of branch-misses:u and a summary for branch-misses:k
  // can generate a summary of branch-misses.
  void AutoGenerateSummaries();
  void GenerateComments(double duration_in_sec);
  void Show(FILE* fp);
  void ShowCSV(FILE* fp);
  void ShowText(FILE* fp);

 private:
  std::string GetCommentForSummary(const CounterSummary& s, double duration_in_sec);
  std::string GetRateComment(const CounterSummary& s, char sep);
  bool FindRunningTimeForSummary(const CounterSummary& summary, double* running_time_in_sec);

 private:
  std::vector<CounterSummary> summaries_;
  bool csv_;
};

}  // namespace simpleperf