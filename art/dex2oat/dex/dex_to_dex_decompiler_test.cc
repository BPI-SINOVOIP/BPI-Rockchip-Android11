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

#include "dex_to_dex_decompiler.h"

#include "base/casts.h"
#include "class_linker.h"
#include "common_compiler_driver_test.h"
#include "compiled_method-inl.h"
#include "compiler_callbacks.h"
#include "dex/class_accessor-inl.h"
#include "dex/dex_file.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_options.h"
#include "handle_scope-inl.h"
#include "mirror/class_loader.h"
#include "quick_compiler_callbacks.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"
#include "verifier/verifier_deps.h"

namespace art {

class DexToDexDecompilerTest : public CommonCompilerDriverTest {
 public:
  void CompileAll(jobject class_loader) REQUIRES(!Locks::mutator_lock_) {
    TimingLogger timings("DexToDexDecompilerTest::CompileAll", false, false);
    compiler_options_->image_type_ = CompilerOptions::ImageType::kNone;
    compiler_options_->SetCompilerFilter(CompilerFilter::kQuicken);
    // Create the main VerifierDeps, here instead of in the compiler since we want to aggregate
    // the results for all the dex files, not just the results for the current dex file.
    down_cast<QuickCompilerCallbacks*>(Runtime::Current()->GetCompilerCallbacks())->SetVerifierDeps(
        new verifier::VerifierDeps(GetDexFiles(class_loader)));
    std::vector<const DexFile*> dex_files = GetDexFiles(class_loader);
    CommonCompilerDriverTest::CompileAll(class_loader, dex_files, &timings);
  }

  void RunTest(const char* dex_name) {
    Thread* self = Thread::Current();
    // First load the original dex file.
    jobject original_class_loader;
    {
      ScopedObjectAccess soa(self);
      original_class_loader = LoadDex(dex_name);
    }
    const DexFile* original_dex_file = GetDexFiles(original_class_loader)[0];

    // Load the dex file again and make it writable to quicken them.
    jobject class_loader;
    const DexFile* updated_dex_file = nullptr;
    {
      ScopedObjectAccess soa(self);
      class_loader = LoadDex(dex_name);
      updated_dex_file = GetDexFiles(class_loader)[0];
      Runtime::Current()->GetClassLinker()->RegisterDexFile(
          *updated_dex_file, soa.Decode<mirror::ClassLoader>(class_loader));
    }
    // The dex files should be identical.
    int cmp = memcmp(original_dex_file->Begin(),
                     updated_dex_file->Begin(),
                     updated_dex_file->Size());
    ASSERT_EQ(0, cmp);

    updated_dex_file->EnableWrite();
    CompileAll(class_loader);
    // The dex files should be different after quickening.
    cmp = memcmp(original_dex_file->Begin(), updated_dex_file->Begin(), updated_dex_file->Size());
    ASSERT_NE(0, cmp);

    // Unquicken the dex file.
    for (ClassAccessor accessor : updated_dex_file->GetClasses()) {
      // Unquicken each method.
      for (const ClassAccessor::Method& method : accessor.GetMethods()) {
        CompiledMethod* compiled_method = compiler_driver_->GetCompiledMethod(
            method.GetReference());
        ArrayRef<const uint8_t> table;
        if (compiled_method != nullptr) {
          table = compiled_method->GetVmapTable();
        }
        optimizer::ArtDecompileDEX(*updated_dex_file,
                                   *accessor.GetCodeItem(method),
                                   table,
                                   /* decompile_return_instruction= */ true);
      }
    }

    // Make sure after unquickening we go back to the same contents as the original dex file.
    cmp = memcmp(original_dex_file->Begin(), updated_dex_file->Begin(), updated_dex_file->Size());
    ASSERT_EQ(0, cmp);
  }
};

TEST_F(DexToDexDecompilerTest, VerifierDeps) {
  RunTest("VerifierDeps");
}

TEST_F(DexToDexDecompilerTest, DexToDexDecompiler) {
  RunTest("DexToDexDecompiler");
}

}  // namespace art
