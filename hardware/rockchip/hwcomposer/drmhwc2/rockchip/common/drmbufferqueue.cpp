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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_TAG "drm-bufferqueue"

#include <drmbufferqueue.h>
#include <rockchip/utils/drmdebug.h>

namespace android{

#define DrmBufferQeueueMaxSize 4

DrmBufferQueue::DrmBufferQueue():
  sName_(""),
  iMaxBufferSize_(DrmBufferQeueueMaxSize),
  currentBuffer_(NULL){
}

DrmBufferQueue::~DrmBufferQueue(){
  while(bufferQueue_.size() > 0){
    bufferQueue_.pop();
  }
}

bool DrmBufferQueue::NeedsReallocation(int w, int h, int format){
  if(currentBuffer_->GetWidth()  != w ||
     currentBuffer_->GetHeight() != h ||
     currentBuffer_->GetFormat() != format){
    HWC2_ALOGD_IF_DEBUG("old[%d,%d,%d]=>[%d,%d,%d] queue.size()=%zu name=%s",
                 currentBuffer_->GetWidth(),
                 currentBuffer_->GetHeight(),
                 currentBuffer_->GetFormat(),
                 w,h,format,
                 bufferQueue_.size(), sName_.c_str());
      return true;
  }
  return false;
}

std::shared_ptr<DrmBuffer> DrmBufferQueue::FrontDrmBuffer(){
  if(bufferQueue_.size() > 0){
    currentBuffer_ = bufferQueue_.front();
    HWC2_ALOGD_IF_DEBUG("Id=%" PRIu64 " Buffer=%p , queue.size()=%zu, Front success!",
        currentBuffer_->GetId(),currentBuffer_.get(),bufferQueue_.size());
    return currentBuffer_;
  }
  return NULL;
}

std::shared_ptr<DrmBuffer> DrmBufferQueue::BackDrmBuffer(){
  if(bufferQueue_.size() > 0){
    currentBuffer_ = bufferQueue_.back();
    HWC2_ALOGD_IF_DEBUG("Id=%" PRIu64 " Buffer=%p , queue.size()=%zu, Back success!",
        currentBuffer_->GetId(),currentBuffer_.get(),bufferQueue_.size());
    return currentBuffer_;
  }
  return NULL;
}

std::shared_ptr<DrmBuffer> DrmBufferQueue::DequeueDrmBuffer(int w, int h, int format, std::string name){
  HWC2_ALOGD_IF_DEBUG("w=%d, h=%d, format=%d name=%s",w, h, format, name.c_str());
  if(bufferQueue_.size() == iMaxBufferSize_){
    currentBuffer_ = bufferQueue_.front();
    bufferQueue_.pop();
    if(NeedsReallocation(w, h, format)){
      currentBuffer_ = std::make_shared<DrmBuffer>(w, h, format, name);
      if(currentBuffer_->Init()){
        HWC2_ALOGE("DrmBuffer Init fail, w=%d h=%d format=%d name=%s",
                   w, h, format, name.c_str());
        return NULL;
      }
      HWC2_ALOGD_IF_DEBUG(" NeedsReallocation sucess! w=%d h=%d format=%d name=%s",w, h, format, name.c_str());
      return currentBuffer_;
    }
    currentBuffer_->WaitReleaseFence();
    currentBuffer_->WaitFinishFence();
    HWC2_ALOGD_IF_DEBUG("Id=%" PRIu64 " fd=%d Buffer=%p, queue.size()=%zu, dequeue success!",
      currentBuffer_->GetId(),currentBuffer_->GetFd(),currentBuffer_.get(),bufferQueue_.size());
    return currentBuffer_;
  }else{
    currentBuffer_ = std::make_shared<DrmBuffer>(w, h, format, name);
    if(currentBuffer_->Init()){
      HWC2_ALOGE("DrmBuffer Init fail, w=%d h=%d format=%d name=%s",
                 w, h, format, name.c_str());
      return NULL;
    }
  }
  HWC2_ALOGD_IF_DEBUG("Id=%" PRIu64 " fd=%d Buffer=%p, queue.size()=%zu, dequeue success!",
      currentBuffer_->GetId(),currentBuffer_->GetFd(),currentBuffer_.get(),bufferQueue_.size());
  return currentBuffer_;
}

int DrmBufferQueue::QueueBuffer(const std::shared_ptr<DrmBuffer> buffer){
  HWC2_ALOGD_IF_DEBUG("Id=%" PRIu64 " Buffer=%p , queue.size()=%zu",buffer->GetId(),buffer.get(),bufferQueue_.size());
  if(currentBuffer_ != NULL){
    if(buffer == currentBuffer_){
      HWC2_ALOGD_IF_DEBUG("Id=%" PRIu64 " fd=%d Buffer=%p , queue.size()=%zu, queue success!",buffer->GetId(),
        currentBuffer_->GetFd(), buffer.get(),bufferQueue_.size());
      bufferQueue_.push(currentBuffer_);
      currentBuffer_ = NULL;
      return 0;
    }
  }
  HWC2_ALOGE("Queue fail, Id=%" PRIu64 " fd=%d current=%p buffer=%p queue.size()=%zu name=%s",
                 buffer->GetId(), currentBuffer_->GetFd(), currentBuffer_.get(), buffer.get(),bufferQueue_.size(), sName_.c_str());
  return -1;
}

}//namespace android

