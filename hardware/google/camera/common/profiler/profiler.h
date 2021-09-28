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

#ifndef HARDWARE_GOOGLE_CAMERA_COMMON_PROFILER_H
#define HARDWARE_GOOGLE_CAMERA_COMMON_PROFILER_H

#include <cutils/properties.h>

#include <limits>
#include <memory>
#include <string>

namespace google {
namespace camera_common {

// The goal of the Profiler is to profile the performance of camemra pipeline.
// However you can use it profile any procedure.
// The profiler prints out the result when the Profiler obejct is deconstructed.
//
// Setprops:
//  - To disable the profiler:
//    $ adb shell setprop persist.vendor.camera.profiler 0
//  - To print the profiling result in standard output:
//    $ adb shell setprop persist.vendor.camera.profiler 1
//  - To dump the profiling result to "/data/vendor/camera/profiler":
//    $ adb shell setprop persist.vendor.camera.profiler 2
//  - To print and dump the profiling result to "/data/vendor/camera/profiler":
//    $ adb shell setprop persist.vendor.camera.profiler 3
//
//  By default the profiler is disabled.
//
// Usage:
//  1. To Create a profiler, please call Profiler::Create(...).
//  2. Use Start() and End() to profile the enclosed code snippet.
//  3. Use SetUseCase to specify the name of the profiling target (purpose).
//  4  If you want to dump the profiling data to the disk, call
//     SetDumpFilePrefix(), which is default to "/vendor/camera/profiler/".
//     The dumped file name is the prefix name + usecase name.
//
// Example Code:
//  In the following example, we use a for loop to profile two fucntions Foo()
//  and Bar; Foo() run once, and Bar() run twice.
//
//   std::unique_ptr<Profiler> profiler = Profiler::Create(Profiler::kPrintBit);
//   profiler->SetUseCase("Profiling Example");
//
//   for (int i = 0; i < 100; i++) {
//     profiler->Start("Foo function", i);
//     Foo()
//     profiler->End("Foo function", i);
//
//     profiler->Start("Bar function", i);
//     Bar()
//     profiler->End("Bar function", i);
//
//     profiler->Start("Bar function", i);
//     Bar()
//     profiler->End("Bar function", i);
//   }
//
// Example Print Out:
//
// UseCase: Profiling Example. Profiled Frames: 100.
//      Foo function           Max:  0.012 ms.   Avg:  0.020 ms x 1 =  0.040 ms
//      Bar function           Max:  0.008 ms.   Avg:  0.019 ms x 2 =  0.039 ms
//                      SUM OF MAX:  0.020 ms,           SUM OF AVG =  0.079 ms
//
class Profiler {
 public:
  // Invalid request id.
  static constexpr int kInvalidRequestId = std::numeric_limits<int>::max();

  // Create profiler.
  static std::shared_ptr<Profiler> Create(int option);

  virtual ~Profiler(){};

  // adb setprop options.
  enum SetPropFlag {
    kDisable = 0,
    kPrintBit = 1 << 0,
    kDumpBit = 1 << 1,
    kStopWatch = 1 << 2
  };

  // Setup the name of use case the profiler is running.
  // Argument:
  //  usecase: the name use case of the profiler is running.
  virtual void SetUseCase(std::string usecase) = 0;

  // Set the file prefix name for dumpping the profiling file.
  // Argument:
  //  dump_file_prefix: file prefix name. In the current setting,
  //    "/data/vendor/camera/" is a valid folder for camera to dump file.
  //    A valid prefix can be "/data/vendor/camera/test_prefix_".
  virtual void SetDumpFilePrefix(std::string dump_file_prefix) = 0;

  // Start to profile.
  // We use start and end to choose which code snippet to be profile.
  // The user specifies the name, and the profiler will print the name and its
  // timing.
  // Arguments:
  //   name: the name of the node to be profiled.
  //   request_id: frame requesd id.
  virtual void Start(const std::string name, int request_id) = 0;

  // End the profileing.
  // Arguments:
  //   name: the name of the node to be profiled. Should be the same in Start().
  //   request_id: frame requesd id.
  virtual void End(const std::string name, int request_id) = 0;

  // Print out the profiling result in the standard output (ANDROID_LOG_ERROR).
  virtual void PrintResult() = 0;

 protected:
  Profiler(){};
};

// A scoped utility class to facilitate profiling.
class ScopedProfiler {
 public:
  // Constructor.
  // Arguments:
  //   profiler: profiler object.
  //   target: the name of the target to be profiled.
  //   request_id: frame requesd id.
  ScopedProfiler(std::shared_ptr<Profiler> profiler, const std::string target,
                 int request_id)
      : profiler_(profiler),
        target_(std::move(target)),
        request_id_(request_id) {
    profiler_->Start(target_, request_id_);
  }

  ScopedProfiler(std::shared_ptr<Profiler> profiler, const std::string target)
      : profiler_(profiler), target_(std::move(target)) {
    request_id_ = Profiler::kInvalidRequestId;
    profiler_->Start(target_, request_id_);
  }

  ScopedProfiler(const std::string target, int option)
      : target_(std::move(target)) {
    profiler_ = Profiler::Create(option);
    request_id_ = Profiler::kInvalidRequestId;
    profiler_->Start(target_, request_id_);
  }

  ~ScopedProfiler() {
    profiler_->End(target_, request_id_);
  }

 private:
  std::shared_ptr<Profiler> profiler_;
  const std::string target_;
  int request_id_;
};

}  // namespace camera_common
}  // namespace google

#endif  // HARDWARE_GOOGLE_CAMERA_COMMON_PROFILER_H
