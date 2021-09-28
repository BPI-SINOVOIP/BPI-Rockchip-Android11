/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_JAVA_FRAME_ROOT_INFO_H_
#define ART_RUNTIME_JAVA_FRAME_ROOT_INFO_H_

#include <iosfwd>
#include <limits>

#include "base/locks.h"
#include "base/macros.h"
#include "gc_root.h"

namespace art {

class StackVisitor;

class JavaFrameRootInfo final : public RootInfo {
 public:
  static_assert(std::numeric_limits<size_t>::max() > std::numeric_limits<uint16_t>::max(),
                "No extra space in vreg to store meta-data");
  // Unable to determine what register number the root is from.
  static constexpr size_t kUnknownVreg = -1;
  // The register number for the root might be determinable but we did not attempt to find that
  // information.
  static constexpr size_t kImpreciseVreg = -2;
  // The root is from the declaring class of the current method.
  static constexpr size_t kMethodDeclaringClass = -3;
  // The root is from the argument to a Proxy invoke.
  static constexpr size_t kProxyReferenceArgument = -4;
  // The maximum precise vreg number
  static constexpr size_t kMaxVReg = std::numeric_limits<uint16_t>::max();

  JavaFrameRootInfo(uint32_t thread_id, const StackVisitor* stack_visitor, size_t vreg)
     : RootInfo(kRootJavaFrame, thread_id), stack_visitor_(stack_visitor), vreg_(vreg) {
  }
  void Describe(std::ostream& os) const override
      REQUIRES_SHARED(Locks::mutator_lock_);

  size_t GetVReg() const {
    return vreg_;
  }
  const StackVisitor* GetVisitor() const {
    return stack_visitor_;
  }

 private:
  const StackVisitor* const stack_visitor_;
  const size_t vreg_;
};

}  // namespace art

#endif  // ART_RUNTIME_JAVA_FRAME_ROOT_INFO_H_
