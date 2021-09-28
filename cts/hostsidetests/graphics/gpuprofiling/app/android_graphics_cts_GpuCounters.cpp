/*
 * Copyright 2020 The Android Open Source Project
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
 *
 */

#define LOG_TAG "GpuProfilingData"

#include <chrono>
#include <csignal>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include <android/log.h>
#include <dlfcn.h>

#define ALOGI(msg, ...)                                                        \
  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, (msg), __VA_ARGS__)
#define ALOGE(msg, ...)                                                        \
  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, (msg), __VA_ARGS__)
#define ALOGD(msg, ...)                                                        \
  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, (msg), __VA_ARGS__)
#define REQUIRE_SUCCESS(fn, name)                                              \
  do {                                                                         \
    if (VK_SUCCESS != fn) {                                                    \
      ALOGE("Vulkan Error in %s", name);                                       \
      return -1;                                                               \
    }                                                                          \
  } while (0)

namespace {

typedef void (*FN_PTR)(void);

/**
 * Load the vendor provided counter producer library.
 * startCounterProducer is a thin rewrite of the same producer loading logic in
 * github.com/google/agi
 */

int startCounterProducer() {
  ALOGI("%s", "Loading producer library");
  char *error;
  std::string libDir = sizeof(void *) == 8 ? "lib64" : "lib";
  std::string producerPath = "/vendor/" + libDir + "/libgpudataproducer.so";

  ALOGI("Trying %s", producerPath.c_str());
  void *handle = dlopen(producerPath.c_str(), RTLD_GLOBAL);
  if ((error = dlerror()) != nullptr || handle == nullptr) {
    ALOGE("Error loading lib: %s", error);
    return -1;
  }

  FN_PTR startFunc = (FN_PTR)dlsym(handle, "start");
  if ((error = dlerror()) != nullptr) {
    ALOGE("Error looking for start symbol: %s", error);
    dlclose(handle);
    return -1;
  }

  if (startFunc == nullptr) {
    ALOGE("Did not find the producer library %s", producerPath.c_str());
    ALOGE("LD_LIBRARY_PATH=%s", getenv("LD_LIBRARY_PATH"));
    return -1;
  }

  ALOGI("Calling start at %p", startFunc);
  (*startFunc)();
  ALOGI("Producer %s has exited.", producerPath.c_str());
  dlclose(handle);
  return 0;
}

volatile std::sig_atomic_t done = 0;

} // anonymous namespace

int main() {
  std::signal(SIGTERM, [](int /*signal*/) {
    ALOGI("%s", "SIGTERM received");
    done = 1;
  });
  std::thread dummy([&]() {
    int result = startCounterProducer();
    ALOGI("%s %d", "startCounterProducer returned", result);
  });
  ALOGI("%s", "Waiting for host");
  while (!done) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return 0;
}
