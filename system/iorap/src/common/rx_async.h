// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef IORAP_SRC_COMMON_RX_ASYNC_H_
#define IORAP_SRC_COMMON_RX_ASYNC_H_

#include "common/async_pool.h"

#include <rxcpp/rx.hpp>

namespace iorap::common {

// Helper functions to operate with rx chains asynchronously.
class RxAsync {
 public:
  // Subscribe to the observable on a new thread asynchronously.
  // If no observe_on/subscribe_on is used, the chain will execute
  // on that new thread.
  //
  // Returns the composite_subscription which can be used to
  // unsubscribe from if we want to abort the chain early.
  template <typename T, typename U>
  static rxcpp::composite_subscription SubscribeAsync(
      AsyncPool& async_pool,
      T&& observable,
      U&& subscriber) {
    rxcpp::composite_subscription subscription;

    async_pool.LaunchAsync([subscription,  // safe copy: ref-counted
                            observable=std::forward<T>(observable),
                            subscriber=std::forward<U>(subscriber)]() mutable {
      observable
        .as_blocking()
        .subscribe(subscription,
                   std::forward<decltype(subscriber)>(subscriber));
    });

    return subscription;
  }

  template <typename T, typename U, typename E>
  static rxcpp::composite_subscription SubscribeAsync(
      AsyncPool& async_pool,
      T&& observable,
      U&& on_next,
      E&& on_error) {
    rxcpp::composite_subscription subscription;

    async_pool.LaunchAsync([subscription,  // safe copy: ref-counted
                            observable=std::forward<T>(observable),
                            on_next=std::forward<U>(on_next),
                            on_error=std::forward<E>(on_error)]() mutable {
      observable
        .as_blocking()
        .subscribe(subscription,
                   std::forward<decltype(on_next)>(on_next),
                   std::forward<decltype(on_error)>(on_error));
    });

    return subscription;
  }
};

}   // namespace iorap::common

#endif  // IORAP_SRC_COMMON_RX_ASYNC_H_
