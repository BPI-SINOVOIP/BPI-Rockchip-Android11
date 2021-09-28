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

#include <android/hidl/base/1.0/IBase.h>
#include <android/sensor.h>

#ifndef HARDWARE_GOOGLE_PIXEL_ORIENTATION_COLLECTOR_H
#define HARDWARE_GOOGLE_PIXEL_ORIENTATION_COLLECTOR_H

namespace android {
namespace hardware {
namespace google {
namespace pixel {

/**
 * A class to help use Senser
 */
class OrientationCollector : public RefBase {
  public:
    static sp<OrientationCollector> createOrientationCollector();

    int32_t pollOrientation(int *orientation);
    int32_t init();
    void disableOrientationSensor();

  private:
    ASensorEventQueue *mQueue;
    ASensorManager *mSensorManager = nullptr;
    ASensorRef mOrientationSensor;

    int getEvents(ASensorEvent *event_list, size_t event_list_size, int *event_count);
};

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_ORIENTATION_COLLECTOR_H
