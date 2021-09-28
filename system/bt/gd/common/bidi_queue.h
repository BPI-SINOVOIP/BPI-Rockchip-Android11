/******************************************************************************
 *
 *  Copyright 2019 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#pragma once

#include "common/callback.h"
#include "os/queue.h"

namespace bluetooth {
namespace common {

template <typename TENQUEUE, typename TDEQUEUE>
class BidiQueueEnd
    : public ::bluetooth::os::IQueueEnqueue<TENQUEUE>,
      public ::bluetooth::os::IQueueDequeue<TDEQUEUE> {
 public:
  using EnqueueCallback = Callback<std::unique_ptr<TENQUEUE>()>;
  using DequeueCallback = Callback<void()>;

  BidiQueueEnd(::bluetooth::os::IQueueEnqueue<TENQUEUE>* tx, ::bluetooth::os::IQueueDequeue<TDEQUEUE>* rx)
      : tx_(tx), rx_(rx) {
  }

  void RegisterEnqueue(::bluetooth::os::Handler* handler, EnqueueCallback callback) override {
    tx_->RegisterEnqueue(handler, callback);
  }

  void UnregisterEnqueue() override {
    tx_->UnregisterEnqueue();
  }

  void RegisterDequeue(::bluetooth::os::Handler* handler, DequeueCallback callback) override {
    rx_->RegisterDequeue(handler, callback);
  }

  void UnregisterDequeue() override {
    rx_->UnregisterDequeue();
  }

  std::unique_ptr<TDEQUEUE> TryDequeue() override {
    return rx_->TryDequeue();
  }

 private:
  ::bluetooth::os::IQueueEnqueue<TENQUEUE>* tx_;
  ::bluetooth::os::IQueueDequeue<TDEQUEUE>* rx_;
};

template <typename TUP, typename TDOWN>
class BidiQueue {
 public:
  explicit BidiQueue(size_t capacity)
      : up_queue_(capacity),
        down_queue_(capacity),
        up_end_(&down_queue_, &up_queue_),
        down_end_(&up_queue_, &down_queue_) {
  }

  BidiQueueEnd<TDOWN, TUP>* GetUpEnd() {
    return &up_end_;
  }

  BidiQueueEnd<TUP, TDOWN>* GetDownEnd() {
    return &down_end_;
  }

 private:
  ::bluetooth::os::Queue<TUP> up_queue_;
  ::bluetooth::os::Queue<TDOWN> down_queue_;
  BidiQueueEnd<TDOWN, TUP> up_end_;
  BidiQueueEnd<TUP, TDOWN> down_end_;
};

}  // namespace common
}  // namespace bluetooth
