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
#include "profiler.h"

#include <cutils/properties.h>
#include <log/log.h>
#include <sys/stat.h>

#include <fstream>
#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace google {
namespace camera_common {
namespace {

#undef LOG_TAG
#define LOG_TAG "profiler"

// Profiler implementatoin.
class ProfilerImpl : public Profiler {
 public:
  ProfilerImpl(SetPropFlag setting) : setting_(setting){
    object_init_time_ = CurrentTime();
  };
  ~ProfilerImpl();

  // Setup the name of use case the profiler is running.
  // Argument:
  //  usecase: the name use case of the profiler is running.
  void SetUseCase(std::string usecase) override final {
    use_case_ = std::move(usecase);
  }

  // Set the file prefix name for dumpping the profiling file.
  // Argument:
  //  dump_file_prefix: file prefix name. In the current setting,
  //    "/data/vendor/camera/" is a valid folder for camera to dump file.
  //    A valid prefix can be "/data/vendor/camera/test_prefix_".
  void SetDumpFilePrefix(std::string dump_file_prefix) override final;

  // Start to profile.
  // We use start and end to choose which code snippet to be profile.
  // The user specifies the name, and the profiler will print the name and its
  // timing.
  // Arguments:
  //   name: the name of the node to be profiled.
  //   request_id: frame requesd id.
  void Start(const std::string name,
             int request_id = kInvalidRequestId) override final;

  // End the profileing.
  // Arguments:
  //   name: the name of the node to be profiled. Should be the same in Start().
  //   request_id: frame requesd id.
  void End(const std::string name,
           int request_id = kInvalidRequestId) override final;

  // Print out the profiling result in the standard output (ANDROID_LOG_ERROR).
  virtual void PrintResult() override;

 protected:
  // A structure to hold start time, end time, and count of profiling code
  // snippet.
  struct TimeSlot {
    int64_t start;
    int64_t end;
    int32_t count;
    TimeSlot() : start(0), end(0), count(0) {
    }
  };

  // A structure to store node's profiling result.
  struct TimeResult {
    std::string node_name;
    float max_dt;
    float avg_dt;
    float avg_count;
    TimeResult(std::string node_name, float max_dt, float avg_dt, float count)
        : node_name(node_name),
          max_dt(max_dt),
          avg_dt(avg_dt),
          avg_count(count) {
    }
  };

  using TimeSeries = std::vector<TimeSlot>;
  using NodeTimingMap = std::unordered_map<std::string, TimeSeries>;

  static constexpr int64_t kNsPerSec = 1000000000;
  static constexpr float kNanoToMilli = 0.000001f;

  // The setting_ is used to memorize the getprop result.
  SetPropFlag setting_;
  // The map to record the timing of all nodes.
  NodeTimingMap timing_map_;
  // Use case name.
  std::string use_case_;
  // The prefix for the dump filename.
  std::string dump_file_prefix_;
  // Mutex lock.
  std::mutex lock_;

  // Get boot time.
  int64_t CurrentTime() const {
    if (timespec now; clock_gettime(CLOCK_BOOTTIME, &now) == 0) {
      return now.tv_sec * kNsPerSec + now.tv_nsec;
    } else {
      ALOGE("clock_gettime failed");
      return -1;
    }
  }

  // Timestamp of the class object initialized.
  int64_t object_init_time_;

  // Create folder if not exist.
  void CreateFolder(std::string folder_path);

  // Dump the result to the disk.
  // Argument:
  //   filepath: file path to dump file.
  void DumpResult(std::string filepath);
};

ProfilerImpl::~ProfilerImpl() {
  if (setting_ == SetPropFlag::kDisable || timing_map_.size() == 0) {
    return;
  }
  if (setting_ & SetPropFlag::kPrintBit) {
    PrintResult();
  }
  if (setting_ & SetPropFlag::kDumpBit) {
    DumpResult(dump_file_prefix_ + use_case_ + "-TS" +
        std::to_string(object_init_time_) + ".txt");
  }
}

void ProfilerImpl::CreateFolder(std::string folder_path) {
  struct stat folder_stat;
  memset(&folder_stat, 0, sizeof(folder_stat));
  if (stat(folder_path.c_str(), &folder_stat) != 0) {
    if (errno != ENOENT ||
        mkdir(folder_path.c_str(), S_IRWXU | S_IRWXG | S_IRWXO) == 0) {
      ALOGE("Failed to create %s. errno: %d", folder_path.c_str(), errno);
    }
  }
}

void ProfilerImpl::SetDumpFilePrefix(std::string dump_file_prefix) {
  dump_file_prefix_ = dump_file_prefix;
  if (setting_ & SetPropFlag::kDumpBit) {
    if (auto index = dump_file_prefix_.rfind('/'); index != std::string::npos) {
      CreateFolder(dump_file_prefix_.substr(0, index));
    }
  }
}

void ProfilerImpl::Start(const std::string name, int request_id) {
  if (setting_ == SetPropFlag::kDisable) {
    return;
  }
  int index = (request_id == kInvalidRequestId ? 0 : request_id);

  std::lock_guard<std::mutex> lk(lock_);
  TimeSeries& time_series = timing_map_[name];
  for (int i = time_series.size(); i <= index; i++) {
    time_series.push_back(TimeSlot());
  }
  TimeSlot& slot = time_series[index];
  slot.start += CurrentTime();
}

void ProfilerImpl::End(const std::string name, int request_id) {
  if (setting_ == SetPropFlag::kDisable) {
    return;
  }
  int index = (request_id == kInvalidRequestId ? 0 : request_id);

  std::lock_guard<std::mutex> lk(lock_);

  if (index < timing_map_[name].size()) {
    TimeSlot& slot = timing_map_[name][index];
    slot.end += CurrentTime();
    slot.count++;
  }
}

void ProfilerImpl::PrintResult() {
  ALOGE("UseCase: %s. Profiled Frames: %d.", use_case_.c_str(),
        static_cast<int>(timing_map_.begin()->second.size()));

  std::vector<TimeResult> time_results;

  float sum_avg = 0.f;
  float max_max = 0.f;
  float sum_max = 0.f;
  for (const auto& [node_name, time_series] : timing_map_) {
    int num_frames = 0;
    int num_samples = 0;
    float sum_dt = 0;
    float max_dt = 0;
    for (const auto& slot : time_series) {
      if (slot.count > 0) {
        float elapsed = (slot.end - slot.start) * kNanoToMilli;
        sum_dt += elapsed;
        num_samples += slot.count;
        max_dt = std::max(max_dt, elapsed);
        num_frames++;
      }
    }
    if (num_samples == 0) {
      continue;
    }
    float avg = sum_dt / std::max(1, num_samples);
    float avg_count = static_cast<float>(num_samples) /
                      static_cast<float>(std::max(1, num_frames));
    sum_avg += avg * avg_count;
    sum_max += max_dt;
    max_max = std::max(max_max, max_dt);

    time_results.push_back({node_name, max_dt, avg * avg_count, avg_count});
  }

  std::sort(time_results.begin(), time_results.end(),
            [](auto a, auto b) { return a.avg_dt > b.avg_dt; });

  for (const auto it : time_results) {
    ALOGE("%51.51s Max: %8.3f ms       Avg: %7.3f ms (Count = %3.1f)",
          it.node_name.c_str(), it.max_dt, it.avg_dt, it.avg_count);
  }

  ALOGE("%43.43s     MAX SUM: %8.3f ms,  AVG SUM: %7.3f ms", "", sum_max,
        sum_avg);
  ALOGE("");
}

void ProfilerImpl::DumpResult(std::string filepath) {
  if (std::ofstream fout(filepath, std::ios::out); fout.is_open()) {
    for (const auto& [node_name, time_series] : timing_map_) {
      fout << node_name << " ";
      for (const auto& time_slot : time_series) {
        float elapsed = static_cast<float>(time_slot.end - time_slot.start) /
                        std::max(1, time_slot.count);
        fout << elapsed * kNanoToMilli << " ";
      }
      fout << "\n";
    }
    fout.close();
  }
}

class ProfilerStopwatchImpl : public ProfilerImpl {
 public:
  ProfilerStopwatchImpl(SetPropFlag setting) : ProfilerImpl(setting){};

  ~ProfilerStopwatchImpl() {
    if (setting_ == SetPropFlag::kDisable || timing_map_.size() == 0) {
      return;
    }
    if (setting_ & SetPropFlag::kPrintBit) {
      // Virtual function won't work in parent class's destructor. need to call
      // it by ourself.
      PrintResult();
      // Erase the print bit to prevent parent class print again.
      setting_ = static_cast<SetPropFlag>(setting_ & (!SetPropFlag::kPrintBit));
    }
  }

  // Print out the profiling result in the standard output (ANDROID_LOG_ERROR)
  // with stopwatch mode.
  void PrintResult() override {
    ALOGE("Profiling Case: %s", use_case_.c_str());

    // Sort by end time.
    std::list<std::pair<std::string, TimeSlot>> time_results;
    for (const auto& [node_name, time_series] : timing_map_) {
      for (const auto& slot : time_series) {
        if (slot.count > 0 && time_results.size() < time_results.max_size()) {
          time_results.push_back({node_name, slot});
        }
      }
    }
    time_results.sort([](const auto& a, const auto& b) {
      return a.second.end < b.second.end;
    });

    for (const auto& [node_name, slot] : time_results) {
      if (slot.count > 0) {
        float elapsed = (slot.end - slot.start) * kNanoToMilli;
        ALOGE("%51.51s: %8.3f ms", node_name.c_str(), elapsed);
      }
    }

    ALOGE("");
  }
};

// Dummpy profiler class.
class ProfilerDummy : public Profiler {
 public:
  ProfilerDummy(){};
  ~ProfilerDummy(){};

  void SetUseCase(std::string) override final {
  }

  void SetDumpFilePrefix(std::string) override final {
  }

  void Start(const std::string, int) override final {
  }

  void End(const std::string, int) override final {
  }

  void PrintResult() override final {
  }
};

}  // anonymous namespace

std::shared_ptr<Profiler> Profiler::Create(int option) {
  SetPropFlag flag = static_cast<SetPropFlag>(option);

  if (flag == SetPropFlag::kDisable) {
    return std::make_unique<ProfilerDummy>();
  } else if (flag & SetPropFlag::kStopWatch) {
    return std::make_unique<ProfilerStopwatchImpl>(flag);
  } else {
    return std::make_unique<ProfilerImpl>(flag);
  }
}

}  // namespace camera_common
}  // namespace google
