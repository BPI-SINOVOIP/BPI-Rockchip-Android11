/*
 * Copyright (C) 2016 The Android Open Source Project
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
#ifndef ANDROID_DRM_BASEPARAMETER_H_
#define ANDROID_DRM_BASEPARAMETER_H_

#include <baseparameter_api.h>

namespace android {

class DrmBaseparameter{
public:
  DrmBaseparameter();
  ~DrmBaseparameter();
  int Init();
  int UpdateConnectorBaseInfo(unsigned int connector_type,unsigned int connector_id,struct disp_info *info);
  int DumpConnectorBaseInfo(unsigned int connector_type,unsigned int connector_id,struct disp_info *info);
  int SetScreenInfo(unsigned int connector_type, unsigned int connector_id, int index, struct screen_info *info);

private:
  mutable pthread_mutex_t lock_;
  baseparameter_api api_;
  bool init_success_;
};
} // // namespace android

#endif // ANDROID_DRM_BASEPARAMETER_H_