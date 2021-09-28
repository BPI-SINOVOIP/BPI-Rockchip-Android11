// Copyright (C) 2017 The Android Open Source Project
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

#ifndef SERIALIZE_ARENA_PTR_H_
#define SERIALIZE_ARENA_PTR_H_

#include <google/protobuf/arena.h>
#include <memory>

namespace iorap {
namespace serialize {

/**
 * @file
 *
 * Helpers for protobuf arena allocators. We use smart pointers
 * with an arena embedded inside of them to avoid caring about the
 * arena in other parts of libiorap.
 */

// Arena-managed objects must not be deleted manually.
// When the Arena goes out of scope, it cleans everything up itself.
template <typename T>
void DoNotDelete(T*) {}

template <typename T, typename Base = std::unique_ptr<T, decltype(&DoNotDelete<T>)>>
struct ArenaPtr : public Base {
  template <typename... Args>
  static ArenaPtr<T> Make(Args&& ... args) {
    ArenaPtr<T> arena_ptr(nullptr);
    arena_ptr.reset(google::protobuf::Arena::Create<T>(arena_ptr.arena_.get(),
                                                       std::forward<Args>(args)...));
    return arena_ptr;
  }

  ArenaPtr(std::nullptr_t) : Base(nullptr, &DoNotDelete<T>) {}  // NOLINT explicit.

 private:
  // Use a unique_ptr because Arena doesn't support move semantics.
  std::unique_ptr<google::protobuf::Arena> arena_{new google::protobuf::Arena{}};
};

template <typename T, typename Base = std::shared_ptr<T>>
struct ArenaSharedPtr : public Base {
  template <typename... Args>
  static ArenaSharedPtr<T> Make(Args&& ... args) {
    ArenaSharedPtr<T> arena_ptr(nullptr, &DoNotDelete<T>);
    arena_ptr.reset(google::protobuf::Arena::Create<T>(arena_ptr.arena_.get(),
                                                       std::forward<Args>(args)...));
    return arena_ptr;
  }

  ArenaSharedPtr() = default;
  template <typename Deleter>
  ArenaSharedPtr(std::nullptr_t, Deleter d) : Base(nullptr, d) {}  // NOLINT explicit.

 private:
  std::shared_ptr<google::protobuf::Arena> arena_{new google::protobuf::Arena{}};
};

}  // namespace serialize
}  // namespace iorap

#endif

