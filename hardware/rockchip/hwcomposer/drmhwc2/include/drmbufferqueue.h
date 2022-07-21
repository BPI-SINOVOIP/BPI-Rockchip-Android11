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
#ifndef _DRM_BUFFER_QUEUE_H_
#define _DRM_BUFFER_QUEUE_H_

#include "drmbuffer.h"

#include <ui/GraphicBuffer.h>
#include <queue>
namespace android {

#define DRM_RGA_BUFFERQUEUE_MAX_SIZE 3

class DrmBufferQueue{
public:
  DrmBufferQueue();
  ~DrmBufferQueue();
  bool NeedsReallocation(int w, int h, int format);
  std::shared_ptr<DrmBuffer> FrontDrmBuffer();
  std::shared_ptr<DrmBuffer> BackDrmBuffer();

  std::shared_ptr<DrmBuffer> DequeueDrmBuffer(int w, int h, int format, std::string name);
  int QueueBuffer(const std::shared_ptr<DrmBuffer> buffer);

private:
  std::string sName_;
  int iMaxBufferSize_;
  std::shared_ptr<DrmBuffer> currentBuffer_;
  std::queue<std::shared_ptr<DrmBuffer>> bufferQueue_;
};

}// namespace android
#endif // #ifndef _DRM_BUFFER_QUEUE_H_