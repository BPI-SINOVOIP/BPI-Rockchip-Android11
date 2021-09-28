/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "arch/context.h"
#include "art_method-inl.h"
#include "dex/code_item_accessors-inl.h"
#include "jni.h"
#include "oat_quick_method_header.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "thread.h"

namespace art {

namespace {

class TestVisitor : public StackVisitor {
 public:
  TestVisitor(Thread* thread, Context* context) REQUIRES_SHARED(Locks::mutator_lock_)
      : StackVisitor(thread, context, StackVisitor::StackWalkKind::kIncludeInlinedFrames) {}

  bool VisitFrame() override REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* m = GetMethod();
    std::string m_name(m->GetName());

    if (m_name.compare("$noinline$testLiveArgument") == 0) {
      found_method_ = true;
      CHECK_EQ(CodeItemDataAccessor(m->DexInstructionData()).RegistersSize(), 3u);
      CheckOptimizedOutRegLiveness(m, 1, kIntVReg, true, 42);
      CheckOptimizedOutRegLiveness(m, 2, kReferenceVReg);
    } else if (m_name.compare("$noinline$testIntervalHole") == 0) {
      found_method_ = true;
      uint32_t number_of_dex_registers =
          CodeItemDataAccessor(m->DexInstructionData()).RegistersSize();
      uint32_t dex_register_of_first_parameter = number_of_dex_registers - 2;
      CheckOptimizedOutRegLiveness(m, dex_register_of_first_parameter, kIntVReg, true, 1);
    } else if (m_name.compare("$noinline$testCodeSinking") == 0) {
      found_method_ = true;
      CheckOptimizedOutRegLiveness(m, 0, kReferenceVReg);
    }

    return true;
  }

  void CheckOptimizedOutRegLiveness(ArtMethod* m,
                                    uint32_t dex_reg,
                                    VRegKind vreg_kind,
                                    bool check_val = false,
                                    uint32_t expected = 0) REQUIRES_SHARED(Locks::mutator_lock_) {
    uint32_t value = 0;
    if (GetCurrentQuickFrame() != nullptr &&
        GetCurrentOatQuickMethodHeader()->IsOptimized() &&
        !Runtime::Current()->IsJavaDebuggable()) {
      CHECK_EQ(GetVReg(m, dex_reg, vreg_kind, &value), false);
    } else {
      CHECK(GetVReg(m, dex_reg, vreg_kind, &value));
      if (check_val) {
        CHECK_EQ(value, expected);
      }
    }
  }

  // Value returned to Java to ensure the required methods have been found and tested.
  bool found_method_ = false;
};

extern "C" JNIEXPORT void JNICALL Java_Main_doStaticNativeCallLiveVreg(JNIEnv*, jclass) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<Context> context(Context::Create());
  TestVisitor visitor(soa.Self(), context.get());
  visitor.WalkStack();
  CHECK(visitor.found_method_);
}

}  // namespace

}  // namespace art
