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

#include "compiler_driver.h"

#include <unistd.h>

#ifndef __APPLE__
#include <malloc.h>  // For mallinfo
#endif

#include <string_view>
#include <unordered_set>
#include <vector>

#include "android-base/logging.h"
#include "android-base/strings.h"

#include "aot_class_linker.h"
#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/arena_allocator.h"
#include "base/array_ref.h"
#include "base/bit_vector.h"
#include "base/enums.h"
#include "base/logging.h"  // For VLOG
#include "base/stl_util.h"
#include "base/string_view_cpp20.h"
#include "base/systrace.h"
#include "base/time_utils.h"
#include "base/timing_logger.h"
#include "class_linker-inl.h"
#include "compiled_method-inl.h"
#include "compiler.h"
#include "compiler_callbacks.h"
#include "compiler_driver-inl.h"
#include "dex/class_accessor-inl.h"
#include "dex/descriptors_names.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_annotations.h"
#include "dex/dex_instruction-inl.h"
#include "dex/dex_to_dex_compiler.h"
#include "dex/verification_results.h"
#include "dex/verified_method.h"
#include "driver/compiler_options.h"
#include "driver/dex_compilation_unit.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/accounting/heap_bitmap.h"
#include "gc/space/image_space.h"
#include "gc/space/space.h"
#include "handle_scope-inl.h"
#include "intrinsics_enum.h"
#include "jni/jni_internal.h"
#include "linker/linker_patch.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object-refvisitor-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/throwable.h"
#include "object_lock.h"
#include "profile/profile_compilation_info.h"
#include "runtime.h"
#include "runtime_intrinsics.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"
#include "thread_list.h"
#include "thread_pool.h"
#include "trampolines/trampoline_compiler.h"
#include "transaction.h"
#include "utils/atomic_dex_ref_map-inl.h"
#include "utils/dex_cache_arrays_layout-inl.h"
#include "utils/swap_space.h"
#include "vdex_file.h"
#include "verifier/class_verifier.h"
#include "verifier/verifier_deps.h"
#include "verifier/verifier_enums.h"

namespace art {

static constexpr bool kTimeCompileMethod = !kIsDebugBuild;

// Print additional info during profile guided compilation.
static constexpr bool kDebugProfileGuidedCompilation = false;

// Max encoded fields allowed for initializing app image. Hardcode the number for now
// because 5000 should be large enough.
static constexpr uint32_t kMaxEncodedFields = 5000;

static double Percentage(size_t x, size_t y) {
  return 100.0 * (static_cast<double>(x)) / (static_cast<double>(x + y));
}

static void DumpStat(size_t x, size_t y, const char* str) {
  if (x == 0 && y == 0) {
    return;
  }
  LOG(INFO) << Percentage(x, y) << "% of " << str << " for " << (x + y) << " cases";
}

class CompilerDriver::AOTCompilationStats {
 public:
  AOTCompilationStats()
      : stats_lock_("AOT compilation statistics lock") {}

  void Dump() {
    DumpStat(resolved_instance_fields_, unresolved_instance_fields_, "instance fields resolved");
    DumpStat(resolved_local_static_fields_ + resolved_static_fields_, unresolved_static_fields_,
             "static fields resolved");
    DumpStat(resolved_local_static_fields_, resolved_static_fields_ + unresolved_static_fields_,
             "static fields local to a class");
    DumpStat(safe_casts_, not_safe_casts_, "check-casts removed based on type information");
    // Note, the code below subtracts the stat value so that when added to the stat value we have
    // 100% of samples. TODO: clean this up.
    DumpStat(type_based_devirtualization_,
             resolved_methods_[kVirtual] + unresolved_methods_[kVirtual] +
             resolved_methods_[kInterface] + unresolved_methods_[kInterface] -
             type_based_devirtualization_,
             "virtual/interface calls made direct based on type information");

    const size_t total = std::accumulate(
        class_status_count_,
        class_status_count_ + static_cast<size_t>(ClassStatus::kLast) + 1,
        0u);
    for (size_t i = 0; i <= static_cast<size_t>(ClassStatus::kLast); ++i) {
      std::ostringstream oss;
      oss << "classes with status " << static_cast<ClassStatus>(i);
      DumpStat(class_status_count_[i], total - class_status_count_[i], oss.str().c_str());
    }

    for (size_t i = 0; i <= kMaxInvokeType; i++) {
      std::ostringstream oss;
      oss << static_cast<InvokeType>(i) << " methods were AOT resolved";
      DumpStat(resolved_methods_[i], unresolved_methods_[i], oss.str().c_str());
      if (virtual_made_direct_[i] > 0) {
        std::ostringstream oss2;
        oss2 << static_cast<InvokeType>(i) << " methods made direct";
        DumpStat(virtual_made_direct_[i],
                 resolved_methods_[i] + unresolved_methods_[i] - virtual_made_direct_[i],
                 oss2.str().c_str());
      }
      if (direct_calls_to_boot_[i] > 0) {
        std::ostringstream oss2;
        oss2 << static_cast<InvokeType>(i) << " method calls are direct into boot";
        DumpStat(direct_calls_to_boot_[i],
                 resolved_methods_[i] + unresolved_methods_[i] - direct_calls_to_boot_[i],
                 oss2.str().c_str());
      }
      if (direct_methods_to_boot_[i] > 0) {
        std::ostringstream oss2;
        oss2 << static_cast<InvokeType>(i) << " method calls have methods in boot";
        DumpStat(direct_methods_to_boot_[i],
                 resolved_methods_[i] + unresolved_methods_[i] - direct_methods_to_boot_[i],
                 oss2.str().c_str());
      }
    }
  }

// Allow lossy statistics in non-debug builds.
#ifndef NDEBUG
#define STATS_LOCK() MutexLock mu(Thread::Current(), stats_lock_)
#else
#define STATS_LOCK()
#endif

  void ResolvedInstanceField() REQUIRES(!stats_lock_) {
    STATS_LOCK();
    resolved_instance_fields_++;
  }

  void UnresolvedInstanceField() REQUIRES(!stats_lock_) {
    STATS_LOCK();
    unresolved_instance_fields_++;
  }

  void ResolvedLocalStaticField() REQUIRES(!stats_lock_) {
    STATS_LOCK();
    resolved_local_static_fields_++;
  }

  void ResolvedStaticField() REQUIRES(!stats_lock_) {
    STATS_LOCK();
    resolved_static_fields_++;
  }

  void UnresolvedStaticField() REQUIRES(!stats_lock_) {
    STATS_LOCK();
    unresolved_static_fields_++;
  }

  // Indicate that type information from the verifier led to devirtualization.
  void PreciseTypeDevirtualization() REQUIRES(!stats_lock_) {
    STATS_LOCK();
    type_based_devirtualization_++;
  }

  // A check-cast could be eliminated due to verifier type analysis.
  void SafeCast() REQUIRES(!stats_lock_) {
    STATS_LOCK();
    safe_casts_++;
  }

  // A check-cast couldn't be eliminated due to verifier type analysis.
  void NotASafeCast() REQUIRES(!stats_lock_) {
    STATS_LOCK();
    not_safe_casts_++;
  }

  // Register a class status.
  void AddClassStatus(ClassStatus status) REQUIRES(!stats_lock_) {
    STATS_LOCK();
    ++class_status_count_[static_cast<size_t>(status)];
  }

 private:
  Mutex stats_lock_;

  size_t resolved_instance_fields_ = 0u;
  size_t unresolved_instance_fields_ = 0u;

  size_t resolved_local_static_fields_ = 0u;
  size_t resolved_static_fields_ = 0u;
  size_t unresolved_static_fields_ = 0u;
  // Type based devirtualization for invoke interface and virtual.
  size_t type_based_devirtualization_ = 0u;

  size_t resolved_methods_[kMaxInvokeType + 1] = {};
  size_t unresolved_methods_[kMaxInvokeType + 1] = {};
  size_t virtual_made_direct_[kMaxInvokeType + 1] = {};
  size_t direct_calls_to_boot_[kMaxInvokeType + 1] = {};
  size_t direct_methods_to_boot_[kMaxInvokeType + 1] = {};

  size_t safe_casts_ = 0u;
  size_t not_safe_casts_ = 0u;

  size_t class_status_count_[static_cast<size_t>(ClassStatus::kLast) + 1] = {};

  DISALLOW_COPY_AND_ASSIGN(AOTCompilationStats);
};

CompilerDriver::CompilerDriver(
    const CompilerOptions* compiler_options,
    Compiler::Kind compiler_kind,
    size_t thread_count,
    int swap_fd)
    : compiler_options_(compiler_options),
      compiler_(),
      compiler_kind_(compiler_kind),
      number_of_soft_verifier_failures_(0),
      had_hard_verifier_failure_(false),
      parallel_thread_count_(thread_count),
      stats_(new AOTCompilationStats),
      compiled_method_storage_(swap_fd),
      max_arena_alloc_(0),
      dex_to_dex_compiler_(this) {
  DCHECK(compiler_options_ != nullptr);

  compiled_method_storage_.SetDedupeEnabled(compiler_options_->DeduplicateCode());
  compiler_.reset(Compiler::Create(*compiler_options, &compiled_method_storage_, compiler_kind));
}

CompilerDriver::~CompilerDriver() {
  compiled_methods_.Visit([this](const DexFileReference& ref ATTRIBUTE_UNUSED,
                                 CompiledMethod* method) {
    if (method != nullptr) {
      CompiledMethod::ReleaseSwapAllocatedCompiledMethod(GetCompiledMethodStorage(), method);
    }
  });
}


#define CREATE_TRAMPOLINE(type, abi, offset)                                            \
    if (Is64BitInstructionSet(GetCompilerOptions().GetInstructionSet())) {              \
      return CreateTrampoline64(GetCompilerOptions().GetInstructionSet(),               \
                                abi,                                                    \
                                type ## _ENTRYPOINT_OFFSET(PointerSize::k64, offset));  \
    } else {                                                                            \
      return CreateTrampoline32(GetCompilerOptions().GetInstructionSet(),               \
                                abi,                                                    \
                                type ## _ENTRYPOINT_OFFSET(PointerSize::k32, offset));  \
    }

std::unique_ptr<const std::vector<uint8_t>> CompilerDriver::CreateJniDlsymLookupTrampoline() const {
  CREATE_TRAMPOLINE(JNI, kJniAbi, pDlsymLookup)
}

std::unique_ptr<const std::vector<uint8_t>>
CompilerDriver::CreateJniDlsymLookupCriticalTrampoline() const {
  CREATE_TRAMPOLINE(JNI, kJniAbi, pDlsymLookupCritical)
}

std::unique_ptr<const std::vector<uint8_t>> CompilerDriver::CreateQuickGenericJniTrampoline()
    const {
  CREATE_TRAMPOLINE(QUICK, kQuickAbi, pQuickGenericJniTrampoline)
}

std::unique_ptr<const std::vector<uint8_t>> CompilerDriver::CreateQuickImtConflictTrampoline()
    const {
  CREATE_TRAMPOLINE(QUICK, kQuickAbi, pQuickImtConflictTrampoline)
}

std::unique_ptr<const std::vector<uint8_t>> CompilerDriver::CreateQuickResolutionTrampoline()
    const {
  CREATE_TRAMPOLINE(QUICK, kQuickAbi, pQuickResolutionTrampoline)
}

std::unique_ptr<const std::vector<uint8_t>> CompilerDriver::CreateQuickToInterpreterBridge()
    const {
  CREATE_TRAMPOLINE(QUICK, kQuickAbi, pQuickToInterpreterBridge)
}
#undef CREATE_TRAMPOLINE

void CompilerDriver::CompileAll(jobject class_loader,
                                const std::vector<const DexFile*>& dex_files,
                                TimingLogger* timings) {
  DCHECK(!Runtime::Current()->IsStarted());

  CheckThreadPools();

  if (GetCompilerOptions().IsBootImage()) {
    // All intrinsics must be in the primary boot image, so we don't need to setup
    // the intrinsics for any other compilation, as those compilations will pick up
    // a boot image that have the ArtMethod already set with the intrinsics flag.
    InitializeIntrinsics();
  }
  // Compile:
  // 1) Compile all classes and methods enabled for compilation. May fall back to dex-to-dex
  //    compilation.
  if (GetCompilerOptions().IsAnyCompilationEnabled()) {
    Compile(class_loader, dex_files, timings);
  }
  if (GetCompilerOptions().GetDumpStats()) {
    stats_->Dump();
  }
}

static optimizer::DexToDexCompiler::CompilationLevel GetDexToDexCompilationLevel(
    Thread* self, const CompilerDriver& driver, Handle<mirror::ClassLoader> class_loader,
    const DexFile& dex_file, const dex::ClassDef& class_def)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // When the dex file is uncompressed in the APK, we do not generate a copy in the .vdex
  // file. As a result, dex2oat will map the dex file read-only, and we only need to check
  // that to know if we can do quickening.
  if (dex_file.GetContainer() != nullptr && dex_file.GetContainer()->IsReadOnly()) {
    return optimizer::DexToDexCompiler::CompilationLevel::kDontDexToDexCompile;
  }
  auto* const runtime = Runtime::Current();
  DCHECK(driver.GetCompilerOptions().IsQuickeningCompilationEnabled());
  const char* descriptor = dex_file.GetClassDescriptor(class_def);
  ClassLinker* class_linker = runtime->GetClassLinker();
  ObjPtr<mirror::Class> klass = class_linker->FindClass(self, descriptor, class_loader);
  if (klass == nullptr) {
    CHECK(self->IsExceptionPending());
    self->ClearException();
    return optimizer::DexToDexCompiler::CompilationLevel::kDontDexToDexCompile;
  }
  // DexToDex at the kOptimize level may introduce quickened opcodes, which replace symbolic
  // references with actual offsets. We cannot re-verify such instructions.
  //
  // We store the verification information in the class status in the oat file, which the linker
  // can validate (checksums) and use to skip load-time verification. It is thus safe to
  // optimize when a class has been fully verified before.
  optimizer::DexToDexCompiler::CompilationLevel max_level =
      optimizer::DexToDexCompiler::CompilationLevel::kOptimize;
  if (driver.GetCompilerOptions().GetDebuggable()) {
    // We are debuggable so definitions of classes might be changed. We don't want to do any
    // optimizations that could break that.
    max_level = optimizer::DexToDexCompiler::CompilationLevel::kDontDexToDexCompile;
  }
  if (klass->IsVerified()) {
    // Class is verified so we can enable DEX-to-DEX compilation for performance.
    return max_level;
  } else {
    // Class verification has failed: do not run DEX-to-DEX optimizations.
    return optimizer::DexToDexCompiler::CompilationLevel::kDontDexToDexCompile;
  }
}

static optimizer::DexToDexCompiler::CompilationLevel GetDexToDexCompilationLevel(
    Thread* self,
    const CompilerDriver& driver,
    jobject jclass_loader,
    const DexFile& dex_file,
    const dex::ClassDef& class_def) {
  ScopedObjectAccess soa(self);
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader(
      hs.NewHandle(soa.Decode<mirror::ClassLoader>(jclass_loader)));
  return GetDexToDexCompilationLevel(self, driver, class_loader, dex_file, class_def);
}

// Does the runtime for the InstructionSet provide an implementation returned by
// GetQuickGenericJniStub allowing down calls that aren't compiled using a JNI compiler?
static bool InstructionSetHasGenericJniStub(InstructionSet isa) {
  switch (isa) {
    case InstructionSet::kArm:
    case InstructionSet::kArm64:
    case InstructionSet::kThumb2:
    case InstructionSet::kX86:
    case InstructionSet::kX86_64: return true;
    default: return false;
  }
}

template <typename CompileFn>
static void CompileMethodHarness(
    Thread* self,
    CompilerDriver* driver,
    const dex::CodeItem* code_item,
    uint32_t access_flags,
    InvokeType invoke_type,
    uint16_t class_def_idx,
    uint32_t method_idx,
    Handle<mirror::ClassLoader> class_loader,
    const DexFile& dex_file,
    optimizer::DexToDexCompiler::CompilationLevel dex_to_dex_compilation_level,
    Handle<mirror::DexCache> dex_cache,
    CompileFn compile_fn) {
  DCHECK(driver != nullptr);
  CompiledMethod* compiled_method;
  uint64_t start_ns = kTimeCompileMethod ? NanoTime() : 0;
  MethodReference method_ref(&dex_file, method_idx);

  compiled_method = compile_fn(self,
                               driver,
                               code_item,
                               access_flags,
                               invoke_type,
                               class_def_idx,
                               method_idx,
                               class_loader,
                               dex_file,
                               dex_to_dex_compilation_level,
                               dex_cache);

  if (kTimeCompileMethod) {
    uint64_t duration_ns = NanoTime() - start_ns;
    if (duration_ns > MsToNs(driver->GetCompiler()->GetMaximumCompilationTimeBeforeWarning())) {
      LOG(WARNING) << "Compilation of " << dex_file.PrettyMethod(method_idx)
                   << " took " << PrettyDuration(duration_ns);
    }
  }

  if (compiled_method != nullptr) {
    driver->AddCompiledMethod(method_ref, compiled_method);
  }

  if (self->IsExceptionPending()) {
    ScopedObjectAccess soa(self);
    LOG(FATAL) << "Unexpected exception compiling: " << dex_file.PrettyMethod(method_idx) << "\n"
        << self->GetException()->Dump();
  }
}

static void CompileMethodDex2Dex(
    Thread* self,
    CompilerDriver* driver,
    const dex::CodeItem* code_item,
    uint32_t access_flags,
    InvokeType invoke_type,
    uint16_t class_def_idx,
    uint32_t method_idx,
    Handle<mirror::ClassLoader> class_loader,
    const DexFile& dex_file,
    optimizer::DexToDexCompiler::CompilationLevel dex_to_dex_compilation_level,
    Handle<mirror::DexCache> dex_cache) {
  auto dex_2_dex_fn = [](Thread* self ATTRIBUTE_UNUSED,
      CompilerDriver* driver,
      const dex::CodeItem* code_item,
      uint32_t access_flags,
      InvokeType invoke_type,
      uint16_t class_def_idx,
      uint32_t method_idx,
      Handle<mirror::ClassLoader> class_loader,
      const DexFile& dex_file,
      optimizer::DexToDexCompiler::CompilationLevel dex_to_dex_compilation_level,
      Handle<mirror::DexCache> dex_cache ATTRIBUTE_UNUSED) -> CompiledMethod* {
    DCHECK(driver != nullptr);
    MethodReference method_ref(&dex_file, method_idx);

    optimizer::DexToDexCompiler* const compiler = &driver->GetDexToDexCompiler();

    if (compiler->ShouldCompileMethod(method_ref)) {
      const VerificationResults* results = driver->GetCompilerOptions().GetVerificationResults();
      DCHECK(results != nullptr);
      const VerifiedMethod* verified_method = results->GetVerifiedMethod(method_ref);
      // Do not optimize if a VerifiedMethod is missing. SafeCast elision,
      // for example, relies on it.
      return compiler->CompileMethod(
          code_item,
          access_flags,
          invoke_type,
          class_def_idx,
          method_idx,
          class_loader,
          dex_file,
          (verified_method != nullptr)
          ? dex_to_dex_compilation_level
              : optimizer::DexToDexCompiler::CompilationLevel::kDontDexToDexCompile);
    }
    return nullptr;
  };
  CompileMethodHarness(self,
                       driver,
                       code_item,
                       access_flags,
                       invoke_type,
                       class_def_idx,
                       method_idx,
                       class_loader,
                       dex_file,
                       dex_to_dex_compilation_level,
                       dex_cache,
                       dex_2_dex_fn);
}

static void CompileMethodQuick(
    Thread* self,
    CompilerDriver* driver,
    const dex::CodeItem* code_item,
    uint32_t access_flags,
    InvokeType invoke_type,
    uint16_t class_def_idx,
    uint32_t method_idx,
    Handle<mirror::ClassLoader> class_loader,
    const DexFile& dex_file,
    optimizer::DexToDexCompiler::CompilationLevel dex_to_dex_compilation_level,
    Handle<mirror::DexCache> dex_cache) {
  auto quick_fn = [](
      Thread* self,
      CompilerDriver* driver,
      const dex::CodeItem* code_item,
      uint32_t access_flags,
      InvokeType invoke_type,
      uint16_t class_def_idx,
      uint32_t method_idx,
      Handle<mirror::ClassLoader> class_loader,
      const DexFile& dex_file,
      optimizer::DexToDexCompiler::CompilationLevel dex_to_dex_compilation_level,
      Handle<mirror::DexCache> dex_cache) {
    DCHECK(driver != nullptr);
    CompiledMethod* compiled_method = nullptr;
    MethodReference method_ref(&dex_file, method_idx);

    if ((access_flags & kAccNative) != 0) {
      // Are we extracting only and have support for generic JNI down calls?
      if (!driver->GetCompilerOptions().IsJniCompilationEnabled() &&
          InstructionSetHasGenericJniStub(driver->GetCompilerOptions().GetInstructionSet())) {
        // Leaving this empty will trigger the generic JNI version
      } else {
        // Query any JNI optimization annotations such as @FastNative or @CriticalNative.
        access_flags |= annotations::GetNativeMethodAnnotationAccessFlags(
            dex_file, dex_file.GetClassDef(class_def_idx), method_idx);

        compiled_method = driver->GetCompiler()->JniCompile(
            access_flags, method_idx, dex_file, dex_cache);
        CHECK(compiled_method != nullptr);
      }
    } else if ((access_flags & kAccAbstract) != 0) {
      // Abstract methods don't have code.
    } else {
      const VerificationResults* results = driver->GetCompilerOptions().GetVerificationResults();
      DCHECK(results != nullptr);
      const VerifiedMethod* verified_method = results->GetVerifiedMethod(method_ref);
      bool compile =
          // Basic checks, e.g., not <clinit>.
          results->IsCandidateForCompilation(method_ref, access_flags) &&
          // Did not fail to create VerifiedMethod metadata.
          verified_method != nullptr &&
          // Do not have failures that should punt to the interpreter.
          !verified_method->HasRuntimeThrow() &&
          (verified_method->GetEncounteredVerificationFailures() &
              (verifier::VERIFY_ERROR_FORCE_INTERPRETER | verifier::VERIFY_ERROR_LOCKING)) == 0 &&
              // Is eligable for compilation by methods-to-compile filter.
              driver->ShouldCompileBasedOnProfile(method_ref);

      if (compile) {
        // NOTE: if compiler declines to compile this method, it will return null.
        compiled_method = driver->GetCompiler()->Compile(code_item,
                                                         access_flags,
                                                         invoke_type,
                                                         class_def_idx,
                                                         method_idx,
                                                         class_loader,
                                                         dex_file,
                                                         dex_cache);
        ProfileMethodsCheck check_type =
            driver->GetCompilerOptions().CheckProfiledMethodsCompiled();
        if (UNLIKELY(check_type != ProfileMethodsCheck::kNone)) {
          bool violation = driver->ShouldCompileBasedOnProfile(method_ref) &&
                               (compiled_method == nullptr);
          if (violation) {
            std::ostringstream oss;
            oss << "Failed to compile "
                << method_ref.dex_file->PrettyMethod(method_ref.index)
                << "[" << method_ref.dex_file->GetLocation() << "]"
                << " as expected by profile";
            switch (check_type) {
              case ProfileMethodsCheck::kNone:
                break;
              case ProfileMethodsCheck::kLog:
                LOG(ERROR) << oss.str();
                break;
              case ProfileMethodsCheck::kAbort:
                LOG(FATAL_WITHOUT_ABORT) << oss.str();
                _exit(1);
            }
          }
        }
      }
      if (compiled_method == nullptr &&
          dex_to_dex_compilation_level !=
              optimizer::DexToDexCompiler::CompilationLevel::kDontDexToDexCompile) {
        DCHECK(!Runtime::Current()->UseJitCompilation());
        // TODO: add a command-line option to disable DEX-to-DEX compilation ?
        driver->GetDexToDexCompiler().MarkForCompilation(self, method_ref);
      }
    }
    return compiled_method;
  };
  CompileMethodHarness(self,
                       driver,
                       code_item,
                       access_flags,
                       invoke_type,
                       class_def_idx,
                       method_idx,
                       class_loader,
                       dex_file,
                       dex_to_dex_compilation_level,
                       dex_cache,
                       quick_fn);
}

void CompilerDriver::Resolve(jobject class_loader,
                             const std::vector<const DexFile*>& dex_files,
                             TimingLogger* timings) {
  // Resolution allocates classes and needs to run single-threaded to be deterministic.
  bool force_determinism = GetCompilerOptions().IsForceDeterminism();
  ThreadPool* resolve_thread_pool = force_determinism
                                     ? single_thread_pool_.get()
                                     : parallel_thread_pool_.get();
  size_t resolve_thread_count = force_determinism ? 1U : parallel_thread_count_;

  for (size_t i = 0; i != dex_files.size(); ++i) {
    const DexFile* dex_file = dex_files[i];
    CHECK(dex_file != nullptr);
    ResolveDexFile(class_loader,
                   *dex_file,
                   dex_files,
                   resolve_thread_pool,
                   resolve_thread_count,
                   timings);
  }
}

void CompilerDriver::ResolveConstStrings(const std::vector<const DexFile*>& dex_files,
                                         bool only_startup_strings,
                                         TimingLogger* timings) {
  if (only_startup_strings && GetCompilerOptions().GetProfileCompilationInfo() == nullptr) {
    // If there is no profile, don't resolve any strings. Resolving all of the strings in the image
    // will cause a bloated app image and slow down startup.
    return;
  }
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<1> hs(soa.Self());
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  MutableHandle<mirror::DexCache> dex_cache(hs.NewHandle<mirror::DexCache>(nullptr));
  size_t num_instructions = 0u;

  for (const DexFile* dex_file : dex_files) {
    dex_cache.Assign(class_linker->FindDexCache(soa.Self(), *dex_file));
    bool added_preresolved_string_array = false;
    if (only_startup_strings) {
      // When resolving startup strings, create the preresolved strings array.
      added_preresolved_string_array = dex_cache->AddPreResolvedStringsArray();
    }
    TimingLogger::ScopedTiming t("Resolve const-string Strings", timings);

    // TODO: Implement a profile-based filter for the boot image. See b/76145463.
    for (ClassAccessor accessor : dex_file->GetClasses()) {
      const ProfileCompilationInfo* profile_compilation_info =
          GetCompilerOptions().GetProfileCompilationInfo();

      const bool is_startup_class =
          profile_compilation_info != nullptr &&
          profile_compilation_info->ContainsClass(*dex_file, accessor.GetClassIdx());

      // Skip methods that failed to verify since they may contain invalid Dex code.
      if (GetClassStatus(ClassReference(dex_file, accessor.GetClassDefIndex())) <
          ClassStatus::kRetryVerificationAtRuntime) {
        continue;
      }

      for (const ClassAccessor::Method& method : accessor.GetMethods()) {
        const bool is_clinit = (method.GetAccessFlags() & kAccConstructor) != 0 &&
            (method.GetAccessFlags() & kAccStatic) != 0;
        const bool is_startup_clinit = is_startup_class && is_clinit;

        if (profile_compilation_info != nullptr && !is_startup_clinit) {
          ProfileCompilationInfo::MethodHotness hotness =
              profile_compilation_info->GetMethodHotness(method.GetReference());
          if (added_preresolved_string_array ? !hotness.IsStartup() : !hotness.IsInProfile()) {
            continue;
          }
        }

        // Resolve const-strings in the code. Done to have deterministic allocation behavior. Right
        // now this is single-threaded for simplicity.
        // TODO: Collect the relevant string indices in parallel, then allocate them sequentially
        // in a stable order.
        for (const DexInstructionPcPair& inst : method.GetInstructions()) {
          switch (inst->Opcode()) {
            case Instruction::CONST_STRING:
            case Instruction::CONST_STRING_JUMBO: {
              dex::StringIndex string_index((inst->Opcode() == Instruction::CONST_STRING)
                  ? inst->VRegB_21c()
                  : inst->VRegB_31c());
              ObjPtr<mirror::String> string = class_linker->ResolveString(string_index, dex_cache);
              CHECK(string != nullptr) << "Could not allocate a string when forcing determinism";
              if (added_preresolved_string_array) {
                dex_cache->GetPreResolvedStrings()[string_index.index_] =
                    GcRoot<mirror::String>(string);
              }
              ++num_instructions;
              break;
            }

            default:
              break;
          }
        }
      }
    }
  }
  VLOG(compiler) << "Resolved " << num_instructions << " const string instructions";
}

// Initialize type check bit strings for check-cast and instance-of in the code. Done to have
// deterministic allocation behavior. Right now this is single-threaded for simplicity.
// TODO: Collect the relevant type indices in parallel, then process them sequentially in a
//       stable order.

static void InitializeTypeCheckBitstrings(CompilerDriver* driver,
                                          ClassLinker* class_linker,
                                          Handle<mirror::DexCache> dex_cache,
                                          const DexFile& dex_file,
                                          const ClassAccessor::Method& method)
      REQUIRES_SHARED(Locks::mutator_lock_) {
  for (const DexInstructionPcPair& inst : method.GetInstructions()) {
    switch (inst->Opcode()) {
      case Instruction::CHECK_CAST:
      case Instruction::INSTANCE_OF: {
        dex::TypeIndex type_index(
            (inst->Opcode() == Instruction::CHECK_CAST) ? inst->VRegB_21c() : inst->VRegC_22c());
        const char* descriptor = dex_file.StringByTypeIdx(type_index);
        // We currently do not use the bitstring type check for array or final (including
        // primitive) classes. We may reconsider this in future if it's deemed to be beneficial.
        // And we cannot use it for classes outside the boot image as we do not know the runtime
        // value of their bitstring when compiling (it may not even get assigned at runtime).
        if (descriptor[0] == 'L' && driver->GetCompilerOptions().IsImageClass(descriptor)) {
          ObjPtr<mirror::Class> klass =
              class_linker->LookupResolvedType(type_index,
                                               dex_cache.Get(),
                                               /* class_loader= */ nullptr);
          CHECK(klass != nullptr) << descriptor << " should have been previously resolved.";
          // Now assign the bitstring if the class is not final. Keep this in sync with sharpening.
          if (!klass->IsFinal()) {
            MutexLock subtype_check_lock(Thread::Current(), *Locks::subtype_check_lock_);
            SubtypeCheck<ObjPtr<mirror::Class>>::EnsureAssigned(klass);
          }
        }
        break;
      }

      default:
        break;
    }
  }
}

static void InitializeTypeCheckBitstrings(CompilerDriver* driver,
                                          const std::vector<const DexFile*>& dex_files,
                                          TimingLogger* timings) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<1> hs(soa.Self());
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  MutableHandle<mirror::DexCache> dex_cache(hs.NewHandle<mirror::DexCache>(nullptr));

  for (const DexFile* dex_file : dex_files) {
    dex_cache.Assign(class_linker->FindDexCache(soa.Self(), *dex_file));
    TimingLogger::ScopedTiming t("Initialize type check bitstrings", timings);

    for (ClassAccessor accessor : dex_file->GetClasses()) {
      // Direct and virtual methods.
      for (const ClassAccessor::Method& method : accessor.GetMethods()) {
        InitializeTypeCheckBitstrings(driver, class_linker, dex_cache, *dex_file, method);
      }
    }
  }
}

inline void CompilerDriver::CheckThreadPools() {
  DCHECK(parallel_thread_pool_ != nullptr);
  DCHECK(single_thread_pool_ != nullptr);
}

static void EnsureVerifiedOrVerifyAtRuntime(jobject jclass_loader,
                                            const std::vector<const DexFile*>& dex_files) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader(
      hs.NewHandle(soa.Decode<mirror::ClassLoader>(jclass_loader)));
  MutableHandle<mirror::Class> cls(hs.NewHandle<mirror::Class>(nullptr));
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();

  for (const DexFile* dex_file : dex_files) {
    for (ClassAccessor accessor : dex_file->GetClasses()) {
      cls.Assign(class_linker->FindClass(soa.Self(), accessor.GetDescriptor(), class_loader));
      if (cls == nullptr) {
        soa.Self()->ClearException();
      } else if (&cls->GetDexFile() == dex_file) {
        DCHECK(cls->IsErroneous() ||
               cls->IsVerified() ||
               cls->ShouldVerifyAtRuntime() ||
               cls->IsVerifiedNeedsAccessChecks())
            << cls->PrettyClass()
            << " " << cls->GetStatus();
      }
    }
  }
}

void CompilerDriver::PrepareDexFilesForOatFile(TimingLogger* timings) {
  compiled_classes_.AddDexFiles(GetCompilerOptions().GetDexFilesForOatFile());

  if (GetCompilerOptions().IsAnyCompilationEnabled()) {
    TimingLogger::ScopedTiming t2("Dex2Dex SetDexFiles", timings);
    dex_to_dex_compiler_.SetDexFiles(GetCompilerOptions().GetDexFilesForOatFile());
  }
}

void CompilerDriver::PreCompile(jobject class_loader,
                                const std::vector<const DexFile*>& dex_files,
                                TimingLogger* timings,
                                /*inout*/ HashSet<std::string>* image_classes,
                                /*out*/ VerificationResults* verification_results) {
  CheckThreadPools();

  VLOG(compiler) << "Before precompile " << GetMemoryUsageString(false);

  // Precompile:
  // 1) Load image classes.
  // 2) Resolve all classes.
  // 3) For deterministic boot image, resolve strings for const-string instructions.
  // 4) Attempt to verify all classes.
  // 5) Attempt to initialize image classes, and trivially initialized classes.
  // 6) Update the set of image classes.
  // 7) For deterministic boot image, initialize bitstrings for type checking.

  LoadImageClasses(timings, image_classes);
  VLOG(compiler) << "LoadImageClasses: " << GetMemoryUsageString(false);

  if (compiler_options_->IsAnyCompilationEnabled()) {
    // Avoid adding the dex files in the case where we aren't going to add compiled methods.
    // This reduces RAM usage for this case.
    for (const DexFile* dex_file : dex_files) {
      // Can be already inserted. This happens for gtests.
      if (!compiled_methods_.HaveDexFile(dex_file)) {
        compiled_methods_.AddDexFile(dex_file);
      }
    }
    // Resolve eagerly to prepare for compilation.
    Resolve(class_loader, dex_files, timings);
    VLOG(compiler) << "Resolve: " << GetMemoryUsageString(false);
  }

  if (compiler_options_->AssumeClassesAreVerified()) {
    VLOG(compiler) << "Verify none mode specified, skipping verification.";
    SetVerified(class_loader, dex_files, timings);
  } else if (compiler_options_->IsVerificationEnabled()) {
    Verify(class_loader, dex_files, timings, verification_results);
    VLOG(compiler) << "Verify: " << GetMemoryUsageString(false);

    if (GetCompilerOptions().IsForceDeterminism() &&
        (GetCompilerOptions().IsBootImage() || GetCompilerOptions().IsBootImageExtension())) {
      // Resolve strings from const-string. Do this now to have a deterministic image.
      ResolveConstStrings(dex_files, /*only_startup_strings=*/ false, timings);
      VLOG(compiler) << "Resolve const-strings: " << GetMemoryUsageString(false);
    } else if (GetCompilerOptions().ResolveStartupConstStrings()) {
      ResolveConstStrings(dex_files, /*only_startup_strings=*/ true, timings);
    }

    if (had_hard_verifier_failure_ && GetCompilerOptions().AbortOnHardVerifierFailure()) {
      // Avoid dumping threads. Even if we shut down the thread pools, there will still be three
      // instances of this thread's stack.
      LOG(FATAL_WITHOUT_ABORT) << "Had a hard failure verifying all classes, and was asked to abort "
                               << "in such situations. Please check the log.";
      _exit(1);
    } else if (number_of_soft_verifier_failures_ > 0 &&
               GetCompilerOptions().AbortOnSoftVerifierFailure()) {
      LOG(FATAL_WITHOUT_ABORT) << "Had " << number_of_soft_verifier_failures_ << " soft failure(s) "
                               << "verifying all classes, and was asked to abort in such situations. "
                               << "Please check the log.";
      _exit(1);
    }
  }

  if (GetCompilerOptions().IsGeneratingImage()) {
    // We can only initialize classes when their verification bit is set.
    if (compiler_options_->AssumeClassesAreVerified() ||
        compiler_options_->IsVerificationEnabled()) {
      if (kIsDebugBuild) {
        EnsureVerifiedOrVerifyAtRuntime(class_loader, dex_files);
      }
      InitializeClasses(class_loader, dex_files, timings);
      VLOG(compiler) << "InitializeClasses: " << GetMemoryUsageString(false);
    }

    UpdateImageClasses(timings, image_classes);
    VLOG(compiler) << "UpdateImageClasses: " << GetMemoryUsageString(false);

    if (kBitstringSubtypeCheckEnabled &&
        GetCompilerOptions().IsForceDeterminism() && GetCompilerOptions().IsBootImage()) {
      // Initialize type check bit string used by check-cast and instanceof.
      // Do this now to have a deterministic image.
      // Note: This is done after UpdateImageClasses() at it relies on the image
      // classes to be final.
      InitializeTypeCheckBitstrings(this, dex_files, timings);
    }
  }
}

bool CompilerDriver::ShouldCompileBasedOnProfile(const MethodReference& method_ref) const {
  // Profile compilation info may be null if no profile is passed.
  if (!CompilerFilter::DependsOnProfile(compiler_options_->GetCompilerFilter())) {
    // Use the compiler filter instead of the presence of profile_compilation_info_ since
    // we may want to have full speed compilation along with profile based layout optimizations.
    return true;
  }
  // If we are using a profile filter but do not have a profile compilation info, compile nothing.
  const ProfileCompilationInfo* profile_compilation_info =
      GetCompilerOptions().GetProfileCompilationInfo();
  if (profile_compilation_info == nullptr) {
    return false;
  }
  // Compile only hot methods, it is the profile saver's job to decide what startup methods to mark
  // as hot.
  bool result = profile_compilation_info->GetMethodHotness(method_ref).IsHot();

  if (kDebugProfileGuidedCompilation) {
    LOG(INFO) << "[ProfileGuidedCompilation] "
        << (result ? "Compiled" : "Skipped") << " method:" << method_ref.PrettyMethod(true);
  }

  return result;
}

class ResolveCatchBlockExceptionsClassVisitor : public ClassVisitor {
 public:
  ResolveCatchBlockExceptionsClassVisitor() : classes_() {}

  bool operator()(ObjPtr<mirror::Class> c) override REQUIRES_SHARED(Locks::mutator_lock_) {
    classes_.push_back(c);
    return true;
  }

  void FindExceptionTypesToResolve(
      std::set<std::pair<dex::TypeIndex, const DexFile*>>* exceptions_to_resolve)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    const auto pointer_size = Runtime::Current()->GetClassLinker()->GetImagePointerSize();
    for (ObjPtr<mirror::Class> klass : classes_) {
      for (ArtMethod& method : klass->GetMethods(pointer_size)) {
        FindExceptionTypesToResolveForMethod(&method, exceptions_to_resolve);
      }
    }
  }

 private:
  void FindExceptionTypesToResolveForMethod(
      ArtMethod* method,
      std::set<std::pair<dex::TypeIndex, const DexFile*>>* exceptions_to_resolve)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (method->GetCodeItem() == nullptr) {
      return;  // native or abstract method
    }
    CodeItemDataAccessor accessor(method->DexInstructionData());
    if (accessor.TriesSize() == 0) {
      return;  // nothing to process
    }
    const uint8_t* encoded_catch_handler_list = accessor.GetCatchHandlerData();
    size_t num_encoded_catch_handlers = DecodeUnsignedLeb128(&encoded_catch_handler_list);
    for (size_t i = 0; i < num_encoded_catch_handlers; i++) {
      int32_t encoded_catch_handler_size = DecodeSignedLeb128(&encoded_catch_handler_list);
      bool has_catch_all = false;
      if (encoded_catch_handler_size <= 0) {
        encoded_catch_handler_size = -encoded_catch_handler_size;
        has_catch_all = true;
      }
      for (int32_t j = 0; j < encoded_catch_handler_size; j++) {
        dex::TypeIndex encoded_catch_handler_handlers_type_idx =
            dex::TypeIndex(DecodeUnsignedLeb128(&encoded_catch_handler_list));
        // Add to set of types to resolve if not already in the dex cache resolved types
        if (!method->IsResolvedTypeIdx(encoded_catch_handler_handlers_type_idx)) {
          exceptions_to_resolve->emplace(encoded_catch_handler_handlers_type_idx,
                                         method->GetDexFile());
        }
        // ignore address associated with catch handler
        DecodeUnsignedLeb128(&encoded_catch_handler_list);
      }
      if (has_catch_all) {
        // ignore catch all address
        DecodeUnsignedLeb128(&encoded_catch_handler_list);
      }
    }
  }

  std::vector<ObjPtr<mirror::Class>> classes_;
};

static inline bool CanIncludeInCurrentImage(ObjPtr<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(klass != nullptr);
  gc::Heap* heap = Runtime::Current()->GetHeap();
  if (heap->GetBootImageSpaces().empty()) {
    return true;  // We can include any class when compiling the primary boot image.
  }
  if (heap->ObjectIsInBootImageSpace(klass)) {
    return false;  // Already included in the boot image we're compiling against.
  }
  return AotClassLinker::CanReferenceInBootImageExtension(klass, heap);
}

class RecordImageClassesVisitor : public ClassVisitor {
 public:
  explicit RecordImageClassesVisitor(HashSet<std::string>* image_classes)
      : image_classes_(image_classes) {}

  bool operator()(ObjPtr<mirror::Class> klass) override REQUIRES_SHARED(Locks::mutator_lock_) {
    bool resolved = klass->IsResolved();
    DCHECK(resolved || klass->IsErroneousUnresolved());
    bool can_include_in_image = LIKELY(resolved) && CanIncludeInCurrentImage(klass);
    std::string temp;
    std::string_view descriptor(klass->GetDescriptor(&temp));
    if (can_include_in_image) {
      image_classes_->insert(std::string(descriptor));  // Does nothing if already present.
    } else {
      auto it = image_classes_->find(descriptor);
      if (it != image_classes_->end()) {
        VLOG(compiler) << "Removing " << (resolved ? "unsuitable" : "unresolved")
            << " class from image classes: " << descriptor;
        image_classes_->erase(it);
      }
    }
    return true;
  }

 private:
  HashSet<std::string>* const image_classes_;
};

// Make a list of descriptors for classes to include in the image
void CompilerDriver::LoadImageClasses(TimingLogger* timings,
                                      /*inout*/ HashSet<std::string>* image_classes) {
  CHECK(timings != nullptr);
  if (!GetCompilerOptions().IsBootImage() && !GetCompilerOptions().IsBootImageExtension()) {
    return;
  }

  // Make sure the File[] class is in the primary boot image. b/150319075
  // TODO: Implement support for array classes in profiles and remove this workaround. b/148067697
  if (GetCompilerOptions().IsBootImage()) {
    image_classes->insert("[Ljava/io/File;");
  }

  TimingLogger::ScopedTiming t("LoadImageClasses", timings);
  // Make a first pass to load all classes explicitly listed in the file
  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  CHECK(image_classes != nullptr);
  for (auto it = image_classes->begin(), end = image_classes->end(); it != end;) {
    const std::string& descriptor(*it);
    StackHandleScope<1> hs(self);
    Handle<mirror::Class> klass(
        hs.NewHandle(class_linker->FindSystemClass(self, descriptor.c_str())));
    if (klass == nullptr) {
      VLOG(compiler) << "Failed to find class " << descriptor;
      it = image_classes->erase(it);  // May cause some descriptors to be revisited.
      self->ClearException();
    } else {
      ++it;
    }
  }

  // Resolve exception classes referenced by the loaded classes. The catch logic assumes
  // exceptions are resolved by the verifier when there is a catch block in an interested method.
  // Do this here so that exception classes appear to have been specified image classes.
  std::set<std::pair<dex::TypeIndex, const DexFile*>> unresolved_exception_types;
  StackHandleScope<1> hs(self);
  Handle<mirror::Class> java_lang_Throwable(
      hs.NewHandle(class_linker->FindSystemClass(self, "Ljava/lang/Throwable;")));
  do {
    unresolved_exception_types.clear();
    {
      // Thread suspension is not allowed while ResolveCatchBlockExceptionsClassVisitor
      // is using a std::vector<ObjPtr<mirror::Class>>.
      ScopedAssertNoThreadSuspension ants(__FUNCTION__);
      ResolveCatchBlockExceptionsClassVisitor visitor;
      class_linker->VisitClasses(&visitor);
      visitor.FindExceptionTypesToResolve(&unresolved_exception_types);
    }
    for (const auto& exception_type : unresolved_exception_types) {
      dex::TypeIndex exception_type_idx = exception_type.first;
      const DexFile* dex_file = exception_type.second;
      StackHandleScope<1> hs2(self);
      Handle<mirror::DexCache> dex_cache(hs2.NewHandle(class_linker->RegisterDexFile(*dex_file,
                                                                                     nullptr)));
      ObjPtr<mirror::Class> klass =
          (dex_cache != nullptr)
              ? class_linker->ResolveType(exception_type_idx,
                                          dex_cache,
                                          ScopedNullHandle<mirror::ClassLoader>())
              : nullptr;
      if (klass == nullptr) {
        const dex::TypeId& type_id = dex_file->GetTypeId(exception_type_idx);
        const char* descriptor = dex_file->GetTypeDescriptor(type_id);
        LOG(FATAL) << "Failed to resolve class " << descriptor;
      }
      DCHECK(java_lang_Throwable->IsAssignableFrom(klass));
    }
    // Resolving exceptions may load classes that reference more exceptions, iterate until no
    // more are found
  } while (!unresolved_exception_types.empty());

  // We walk the roots looking for classes so that we'll pick up the
  // above classes plus any classes them depend on such super
  // classes, interfaces, and the required ClassLinker roots.
  RecordImageClassesVisitor visitor(image_classes);
  class_linker->VisitClasses(&visitor);

  if (GetCompilerOptions().IsBootImage()) {
    CHECK(!image_classes->empty());
  }
}

static void MaybeAddToImageClasses(Thread* self,
                                   ObjPtr<mirror::Class> klass,
                                   HashSet<std::string>* image_classes)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK_EQ(self, Thread::Current());
  Runtime* runtime = Runtime::Current();
  gc::Heap* heap = runtime->GetHeap();
  if (heap->ObjectIsInBootImageSpace(klass)) {
    // We're compiling a boot image extension and the class is already
    // in the boot image we're compiling against.
    return;
  }
  const PointerSize pointer_size = runtime->GetClassLinker()->GetImagePointerSize();
  std::string temp;
  while (!klass->IsObjectClass()) {
    const char* descriptor = klass->GetDescriptor(&temp);
    if (image_classes->find(std::string_view(descriptor)) != image_classes->end()) {
      break;  // Previously inserted.
    }
    image_classes->insert(descriptor);
    VLOG(compiler) << "Adding " << descriptor << " to image classes";
    for (size_t i = 0, num_interfaces = klass->NumDirectInterfaces(); i != num_interfaces; ++i) {
      ObjPtr<mirror::Class> interface = mirror::Class::GetDirectInterface(self, klass, i);
      DCHECK(interface != nullptr);
      MaybeAddToImageClasses(self, interface, image_classes);
    }
    for (auto& m : klass->GetVirtualMethods(pointer_size)) {
      MaybeAddToImageClasses(self, m.GetDeclaringClass(), image_classes);
    }
    if (klass->IsArrayClass()) {
      MaybeAddToImageClasses(self, klass->GetComponentType(), image_classes);
    }
    klass = klass->GetSuperClass();
  }
}

// Keeps all the data for the update together. Also doubles as the reference visitor.
// Note: we can use object pointers because we suspend all threads.
class ClinitImageUpdate {
 public:
  ClinitImageUpdate(HashSet<std::string>* image_class_descriptors,
                    Thread* self) REQUIRES_SHARED(Locks::mutator_lock_)
      : hs_(self),
        image_class_descriptors_(image_class_descriptors),
        self_(self) {
    CHECK(image_class_descriptors != nullptr);

    // Make sure nobody interferes with us.
    old_cause_ = self->StartAssertNoThreadSuspension("Boot image closure");
  }

  ~ClinitImageUpdate() {
    // Allow others to suspend again.
    self_->EndAssertNoThreadSuspension(old_cause_);
  }

  // Visitor for VisitReferences.
  void operator()(ObjPtr<mirror::Object> object,
                  MemberOffset field_offset,
                  bool is_static ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    mirror::Object* ref = object->GetFieldObject<mirror::Object>(field_offset);
    if (ref != nullptr) {
      VisitClinitClassesObject(ref);
    }
  }

  // java.lang.ref.Reference visitor for VisitReferences.
  void operator()(ObjPtr<mirror::Class> klass ATTRIBUTE_UNUSED,
                  ObjPtr<mirror::Reference> ref ATTRIBUTE_UNUSED) const {}

  // Ignore class native roots.
  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED)
      const {}
  void VisitRoot(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}

  void Walk() REQUIRES_SHARED(Locks::mutator_lock_) {
    // Find all the already-marked classes.
    WriterMutexLock mu(self_, *Locks::heap_bitmap_lock_);
    FindImageClassesVisitor visitor(this);
    Runtime::Current()->GetClassLinker()->VisitClasses(&visitor);

    // Use the initial classes as roots for a search.
    for (Handle<mirror::Class> klass_root : image_classes_) {
      VisitClinitClassesObject(klass_root.Get());
    }
    ScopedAssertNoThreadSuspension ants(__FUNCTION__);
    for (Handle<mirror::Class> h_klass : to_insert_) {
      MaybeAddToImageClasses(self_, h_klass.Get(), image_class_descriptors_);
    }
  }

 private:
  class FindImageClassesVisitor : public ClassVisitor {
   public:
    explicit FindImageClassesVisitor(ClinitImageUpdate* data)
        : data_(data) {}

    bool operator()(ObjPtr<mirror::Class> klass) override REQUIRES_SHARED(Locks::mutator_lock_) {
      bool resolved = klass->IsResolved();
      DCHECK(resolved || klass->IsErroneousUnresolved());
      bool can_include_in_image = LIKELY(resolved) && CanIncludeInCurrentImage(klass);
      std::string temp;
      std::string_view descriptor(klass->GetDescriptor(&temp));
      auto it = data_->image_class_descriptors_->find(descriptor);
      if (it != data_->image_class_descriptors_->end()) {
        if (can_include_in_image) {
          data_->image_classes_.push_back(data_->hs_.NewHandle(klass));
        } else {
          VLOG(compiler) << "Removing " << (resolved ? "unsuitable" : "unresolved")
              << " class from image classes: " << descriptor;
          data_->image_class_descriptors_->erase(it);
        }
      } else if (can_include_in_image) {
        // Check whether it is initialized and has a clinit. They must be kept, too.
        if (klass->IsInitialized() && klass->FindClassInitializer(
            Runtime::Current()->GetClassLinker()->GetImagePointerSize()) != nullptr) {
          DCHECK(!Runtime::Current()->GetHeap()->ObjectIsInBootImageSpace(klass->GetDexCache()))
              << klass->PrettyDescriptor();
          data_->image_classes_.push_back(data_->hs_.NewHandle(klass));
        }
      }
      return true;
    }

   private:
    ClinitImageUpdate* const data_;
  };

  void VisitClinitClassesObject(mirror::Object* object) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(object != nullptr);
    if (marked_objects_.find(object) != marked_objects_.end()) {
      // Already processed.
      return;
    }

    // Mark it.
    marked_objects_.insert(object);

    if (object->IsClass()) {
      // Add to the TODO list since MaybeAddToImageClasses may cause thread suspension. Thread
      // suspensionb is not safe to do in VisitObjects or VisitReferences.
      to_insert_.push_back(hs_.NewHandle(object->AsClass()));
    } else {
      // Else visit the object's class.
      VisitClinitClassesObject(object->GetClass());
    }

    // If it is not a DexCache, visit all references.
    if (!object->IsDexCache()) {
      object->VisitReferences(*this, *this);
    }
  }

  mutable VariableSizedHandleScope hs_;
  mutable std::vector<Handle<mirror::Class>> to_insert_;
  mutable std::unordered_set<mirror::Object*> marked_objects_;
  HashSet<std::string>* const image_class_descriptors_;
  std::vector<Handle<mirror::Class>> image_classes_;
  Thread* const self_;
  const char* old_cause_;

  DISALLOW_COPY_AND_ASSIGN(ClinitImageUpdate);
};

void CompilerDriver::UpdateImageClasses(TimingLogger* timings,
                                        /*inout*/ HashSet<std::string>* image_classes) {
  if (GetCompilerOptions().IsBootImage() || GetCompilerOptions().IsBootImageExtension()) {
    TimingLogger::ScopedTiming t("UpdateImageClasses", timings);

    // Suspend all threads.
    ScopedSuspendAll ssa(__FUNCTION__);

    ClinitImageUpdate update(image_classes, Thread::Current());

    // Do the marking.
    update.Walk();
  }
}

void CompilerDriver::ProcessedInstanceField(bool resolved) {
  if (!resolved) {
    stats_->UnresolvedInstanceField();
  } else {
    stats_->ResolvedInstanceField();
  }
}

void CompilerDriver::ProcessedStaticField(bool resolved, bool local) {
  if (!resolved) {
    stats_->UnresolvedStaticField();
  } else if (local) {
    stats_->ResolvedLocalStaticField();
  } else {
    stats_->ResolvedStaticField();
  }
}

ArtField* CompilerDriver::ComputeInstanceFieldInfo(uint32_t field_idx,
                                                   const DexCompilationUnit* mUnit,
                                                   bool is_put,
                                                   const ScopedObjectAccess& soa) {
  // Try to resolve the field and compiling method's class.
  ArtField* resolved_field;
  ObjPtr<mirror::Class> referrer_class;
  Handle<mirror::DexCache> dex_cache(mUnit->GetDexCache());
  {
    Handle<mirror::ClassLoader> class_loader = mUnit->GetClassLoader();
    resolved_field = ResolveField(soa, dex_cache, class_loader, field_idx, /* is_static= */ false);
    referrer_class = resolved_field != nullptr
        ? ResolveCompilingMethodsClass(soa, dex_cache, class_loader, mUnit) : nullptr;
  }
  bool can_link = false;
  if (resolved_field != nullptr && referrer_class != nullptr) {
    std::pair<bool, bool> fast_path = IsFastInstanceField(
        dex_cache.Get(), referrer_class, resolved_field, field_idx);
    can_link = is_put ? fast_path.second : fast_path.first;
  }
  ProcessedInstanceField(can_link);
  return can_link ? resolved_field : nullptr;
}

bool CompilerDriver::ComputeInstanceFieldInfo(uint32_t field_idx, const DexCompilationUnit* mUnit,
                                              bool is_put, MemberOffset* field_offset,
                                              bool* is_volatile) {
  ScopedObjectAccess soa(Thread::Current());
  ArtField* resolved_field = ComputeInstanceFieldInfo(field_idx, mUnit, is_put, soa);

  if (resolved_field == nullptr) {
    // Conservative defaults.
    *is_volatile = true;
    *field_offset = MemberOffset(static_cast<size_t>(-1));
    return false;
  } else {
    *is_volatile = resolved_field->IsVolatile();
    *field_offset = resolved_field->GetOffset();
    return true;
  }
}

bool CompilerDriver::IsSafeCast(const DexCompilationUnit* mUnit, uint32_t dex_pc) {
  if (!compiler_options_->IsVerificationEnabled()) {
    // If we didn't verify, every cast has to be treated as non-safe.
    return false;
  }
  DCHECK(mUnit->GetVerifiedMethod() != nullptr);
  bool result = mUnit->GetVerifiedMethod()->IsSafeCast(dex_pc);
  if (result) {
    stats_->SafeCast();
  } else {
    stats_->NotASafeCast();
  }
  return result;
}

class CompilationVisitor {
 public:
  virtual ~CompilationVisitor() {}
  virtual void Visit(size_t index) = 0;
};

class ParallelCompilationManager {
 public:
  ParallelCompilationManager(ClassLinker* class_linker,
                             jobject class_loader,
                             CompilerDriver* compiler,
                             const DexFile* dex_file,
                             const std::vector<const DexFile*>& dex_files,
                             ThreadPool* thread_pool)
    : index_(0),
      class_linker_(class_linker),
      class_loader_(class_loader),
      compiler_(compiler),
      dex_file_(dex_file),
      dex_files_(dex_files),
      thread_pool_(thread_pool) {}

  ClassLinker* GetClassLinker() const {
    CHECK(class_linker_ != nullptr);
    return class_linker_;
  }

  jobject GetClassLoader() const {
    return class_loader_;
  }

  CompilerDriver* GetCompiler() const {
    CHECK(compiler_ != nullptr);
    return compiler_;
  }

  const DexFile* GetDexFile() const {
    CHECK(dex_file_ != nullptr);
    return dex_file_;
  }

  const std::vector<const DexFile*>& GetDexFiles() const {
    return dex_files_;
  }

  void ForAll(size_t begin, size_t end, CompilationVisitor* visitor, size_t work_units)
      REQUIRES(!*Locks::mutator_lock_) {
    ForAllLambda(begin, end, [visitor](size_t index) { visitor->Visit(index); }, work_units);
  }

  template <typename Fn>
  void ForAllLambda(size_t begin, size_t end, Fn fn, size_t work_units)
      REQUIRES(!*Locks::mutator_lock_) {
    Thread* self = Thread::Current();
    self->AssertNoPendingException();
    CHECK_GT(work_units, 0U);

    index_.store(begin, std::memory_order_relaxed);
    for (size_t i = 0; i < work_units; ++i) {
      thread_pool_->AddTask(self, new ForAllClosureLambda<Fn>(this, end, fn));
    }
    thread_pool_->StartWorkers(self);

    // Ensure we're suspended while we're blocked waiting for the other threads to finish (worker
    // thread destructor's called below perform join).
    CHECK_NE(self->GetState(), kRunnable);

    // Wait for all the worker threads to finish.
    thread_pool_->Wait(self, true, false);

    // And stop the workers accepting jobs.
    thread_pool_->StopWorkers(self);
  }

  size_t NextIndex() {
    return index_.fetch_add(1, std::memory_order_seq_cst);
  }

 private:
  template <typename Fn>
  class ForAllClosureLambda : public Task {
   public:
    ForAllClosureLambda(ParallelCompilationManager* manager, size_t end, Fn fn)
        : manager_(manager),
          end_(end),
          fn_(fn) {}

    void Run(Thread* self) override {
      while (true) {
        const size_t index = manager_->NextIndex();
        if (UNLIKELY(index >= end_)) {
          break;
        }
        fn_(index);
        self->AssertNoPendingException();
      }
    }

    void Finalize() override {
      delete this;
    }

   private:
    ParallelCompilationManager* const manager_;
    const size_t end_;
    Fn fn_;
  };

  AtomicInteger index_;
  ClassLinker* const class_linker_;
  const jobject class_loader_;
  CompilerDriver* const compiler_;
  const DexFile* const dex_file_;
  const std::vector<const DexFile*>& dex_files_;
  ThreadPool* const thread_pool_;

  DISALLOW_COPY_AND_ASSIGN(ParallelCompilationManager);
};

// A fast version of SkipClass above if the class pointer is available
// that avoids the expensive FindInClassPath search.
static bool SkipClass(jobject class_loader, const DexFile& dex_file, ObjPtr<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(klass != nullptr);
  const DexFile& original_dex_file = *klass->GetDexCache()->GetDexFile();
  if (&dex_file != &original_dex_file) {
    if (class_loader == nullptr) {
      LOG(WARNING) << "Skipping class " << klass->PrettyDescriptor() << " from "
                   << dex_file.GetLocation() << " previously found in "
                   << original_dex_file.GetLocation();
    }
    return true;
  }
  return false;
}

static void CheckAndClearResolveException(Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  CHECK(self->IsExceptionPending());
  mirror::Throwable* exception = self->GetException();
  std::string temp;
  const char* descriptor = exception->GetClass()->GetDescriptor(&temp);
  const char* expected_exceptions[] = {
      "Ljava/lang/ClassFormatError;",
      "Ljava/lang/ClassCircularityError;",
      "Ljava/lang/IllegalAccessError;",
      "Ljava/lang/IncompatibleClassChangeError;",
      "Ljava/lang/InstantiationError;",
      "Ljava/lang/LinkageError;",
      "Ljava/lang/NoClassDefFoundError;",
      "Ljava/lang/NoSuchFieldError;",
      "Ljava/lang/NoSuchMethodError;",
      "Ljava/lang/VerifyError;",
  };
  bool found = false;
  for (size_t i = 0; (found == false) && (i < arraysize(expected_exceptions)); ++i) {
    if (strcmp(descriptor, expected_exceptions[i]) == 0) {
      found = true;
    }
  }
  if (!found) {
    LOG(FATAL) << "Unexpected exception " << exception->Dump();
  }
  self->ClearException();
}

class ResolveClassFieldsAndMethodsVisitor : public CompilationVisitor {
 public:
  explicit ResolveClassFieldsAndMethodsVisitor(const ParallelCompilationManager* manager)
      : manager_(manager) {}

  void Visit(size_t class_def_index) override REQUIRES(!Locks::mutator_lock_) {
    ScopedTrace trace(__FUNCTION__);
    Thread* const self = Thread::Current();
    jobject jclass_loader = manager_->GetClassLoader();
    const DexFile& dex_file = *manager_->GetDexFile();
    ClassLinker* class_linker = manager_->GetClassLinker();

    // Method and Field are the worst. We can't resolve without either
    // context from the code use (to disambiguate virtual vs direct
    // method and instance vs static field) or from class
    // definitions. While the compiler will resolve what it can as it
    // needs it, here we try to resolve fields and methods used in class
    // definitions, since many of them many never be referenced by
    // generated code.
    const dex::ClassDef& class_def = dex_file.GetClassDef(class_def_index);
    ScopedObjectAccess soa(self);
    StackHandleScope<2> hs(soa.Self());
    Handle<mirror::ClassLoader> class_loader(
        hs.NewHandle(soa.Decode<mirror::ClassLoader>(jclass_loader)));
    Handle<mirror::DexCache> dex_cache(hs.NewHandle(class_linker->FindDexCache(
        soa.Self(), dex_file)));
    // Resolve the class.
    ObjPtr<mirror::Class> klass =
        class_linker->ResolveType(class_def.class_idx_, dex_cache, class_loader);
    bool resolve_fields_and_methods;
    if (klass == nullptr) {
      // Class couldn't be resolved, for example, super-class is in a different dex file. Don't
      // attempt to resolve methods and fields when there is no declaring class.
      CheckAndClearResolveException(soa.Self());
      resolve_fields_and_methods = false;
    } else {
      // We successfully resolved a class, should we skip it?
      if (SkipClass(jclass_loader, dex_file, klass)) {
        return;
      }
      // We want to resolve the methods and fields eagerly.
      resolve_fields_and_methods = true;
    }

    if (resolve_fields_and_methods) {
      ClassAccessor accessor(dex_file, class_def_index);
      // Optionally resolve fields and methods and figure out if we need a constructor barrier.
      auto method_visitor = [&](const ClassAccessor::Method& method)
          REQUIRES_SHARED(Locks::mutator_lock_) {
        ArtMethod* resolved = class_linker->ResolveMethod<ClassLinker::ResolveMode::kNoChecks>(
            method.GetIndex(),
            dex_cache,
            class_loader,
            /*referrer=*/ nullptr,
            method.GetInvokeType(class_def.access_flags_));
        if (resolved == nullptr) {
          CheckAndClearResolveException(soa.Self());
        }
      };
      accessor.VisitFieldsAndMethods(
          // static fields
          [&](ClassAccessor::Field& field) REQUIRES_SHARED(Locks::mutator_lock_) {
            ArtField* resolved = class_linker->ResolveField(
                field.GetIndex(), dex_cache, class_loader, /*is_static=*/ true);
            if (resolved == nullptr) {
              CheckAndClearResolveException(soa.Self());
            }
          },
          // instance fields
          [&](ClassAccessor::Field& field) REQUIRES_SHARED(Locks::mutator_lock_) {
            ArtField* resolved = class_linker->ResolveField(
                field.GetIndex(), dex_cache, class_loader, /*is_static=*/ false);
            if (resolved == nullptr) {
              CheckAndClearResolveException(soa.Self());
            }
          },
          /*direct_method_visitor=*/ method_visitor,
          /*virtual_method_visitor=*/ method_visitor);
    }
  }

 private:
  const ParallelCompilationManager* const manager_;
};

class ResolveTypeVisitor : public CompilationVisitor {
 public:
  explicit ResolveTypeVisitor(const ParallelCompilationManager* manager) : manager_(manager) {
  }
  void Visit(size_t type_idx) override REQUIRES(!Locks::mutator_lock_) {
  // Class derived values are more complicated, they require the linker and loader.
    ScopedObjectAccess soa(Thread::Current());
    ClassLinker* class_linker = manager_->GetClassLinker();
    const DexFile& dex_file = *manager_->GetDexFile();
    StackHandleScope<2> hs(soa.Self());
    Handle<mirror::ClassLoader> class_loader(
        hs.NewHandle(soa.Decode<mirror::ClassLoader>(manager_->GetClassLoader())));
    Handle<mirror::DexCache> dex_cache(hs.NewHandle(class_linker->RegisterDexFile(
        dex_file,
        class_loader.Get())));
    ObjPtr<mirror::Class> klass = (dex_cache != nullptr)
        ? class_linker->ResolveType(dex::TypeIndex(type_idx), dex_cache, class_loader)
        : nullptr;

    if (klass == nullptr) {
      soa.Self()->AssertPendingException();
      mirror::Throwable* exception = soa.Self()->GetException();
      VLOG(compiler) << "Exception during type resolution: " << exception->Dump();
      if (exception->GetClass()->DescriptorEquals("Ljava/lang/OutOfMemoryError;")) {
        // There's little point continuing compilation if the heap is exhausted.
        LOG(FATAL) << "Out of memory during type resolution for compilation";
      }
      soa.Self()->ClearException();
    }
  }

 private:
  const ParallelCompilationManager* const manager_;
};

void CompilerDriver::ResolveDexFile(jobject class_loader,
                                    const DexFile& dex_file,
                                    const std::vector<const DexFile*>& dex_files,
                                    ThreadPool* thread_pool,
                                    size_t thread_count,
                                    TimingLogger* timings) {
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();

  // TODO: we could resolve strings here, although the string table is largely filled with class
  //       and method names.

  ParallelCompilationManager context(class_linker, class_loader, this, &dex_file, dex_files,
                                     thread_pool);
  if (GetCompilerOptions().IsBootImage() || GetCompilerOptions().IsBootImageExtension()) {
    // For images we resolve all types, such as array, whereas for applications just those with
    // classdefs are resolved by ResolveClassFieldsAndMethods.
    TimingLogger::ScopedTiming t("Resolve Types", timings);
    ResolveTypeVisitor visitor(&context);
    context.ForAll(0, dex_file.NumTypeIds(), &visitor, thread_count);
  }

  TimingLogger::ScopedTiming t("Resolve MethodsAndFields", timings);
  ResolveClassFieldsAndMethodsVisitor visitor(&context);
  context.ForAll(0, dex_file.NumClassDefs(), &visitor, thread_count);
}

void CompilerDriver::SetVerified(jobject class_loader,
                                 const std::vector<const DexFile*>& dex_files,
                                 TimingLogger* timings) {
  // This can be run in parallel.
  for (const DexFile* dex_file : dex_files) {
    CHECK(dex_file != nullptr);
    SetVerifiedDexFile(class_loader,
                       *dex_file,
                       dex_files,
                       parallel_thread_pool_.get(),
                       parallel_thread_count_,
                       timings);
  }
}

static void LoadAndUpdateStatus(const ClassAccessor& accessor,
                                ClassStatus status,
                                Handle<mirror::ClassLoader> class_loader,
                                Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  StackHandleScope<1> hs(self);
  const char* descriptor = accessor.GetDescriptor();
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  Handle<mirror::Class> cls(hs.NewHandle<mirror::Class>(
      class_linker->FindClass(self, descriptor, class_loader)));
  if (cls != nullptr) {
    // Check that the class is resolved with the current dex file. We might get
    // a boot image class, or a class in a different dex file for multidex, and
    // we should not update the status in that case.
    if (&cls->GetDexFile() == &accessor.GetDexFile()) {
      ObjectLock<mirror::Class> lock(self, cls);
      mirror::Class::SetStatus(cls, status, self);
      if (status >= ClassStatus::kVerified) {
        cls->SetVerificationAttempted();
      }
    }
  } else {
    DCHECK(self->IsExceptionPending());
    self->ClearException();
  }
}

bool CompilerDriver::FastVerify(jobject jclass_loader,
                                const std::vector<const DexFile*>& dex_files,
                                TimingLogger* timings,
                                /*out*/ VerificationResults* verification_results) {
  verifier::VerifierDeps* verifier_deps =
      Runtime::Current()->GetCompilerCallbacks()->GetVerifierDeps();
  // If there exist VerifierDeps that aren't the ones we just created to output, use them to verify.
  if (verifier_deps == nullptr || verifier_deps->OutputOnly()) {
    return false;
  }
  TimingLogger::ScopedTiming t("Fast Verify", timings);

  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader(
      hs.NewHandle(soa.Decode<mirror::ClassLoader>(jclass_loader)));
  std::string error_msg;

  if (!verifier_deps->ValidateDependencies(
      soa.Self(),
      class_loader,
      // This returns classpath dex files in no particular order but VerifierDeps
      // does not care about the order.
      classpath_classes_.GetDexFiles(),
      &error_msg)) {
    LOG(WARNING) << "Fast verification failed: " << error_msg;
    return false;
  }

  bool compiler_only_verifies =
      !GetCompilerOptions().IsAnyCompilationEnabled() &&
      !GetCompilerOptions().IsGeneratingImage();

  // We successfully validated the dependencies, now update class status
  // of verified classes. Note that the dependencies also record which classes
  // could not be fully verified; we could try again, but that would hurt verification
  // time. So instead we assume these classes still need to be verified at
  // runtime.
  for (const DexFile* dex_file : dex_files) {
    // Fetch the list of verified classes.
    const std::vector<bool>& verified_classes = verifier_deps->GetVerifiedClasses(*dex_file);
    DCHECK_EQ(verified_classes.size(), dex_file->NumClassDefs());
    for (ClassAccessor accessor : dex_file->GetClasses()) {
      if (verified_classes[accessor.GetClassDefIndex()]) {
        if (compiler_only_verifies) {
          // Just update the compiled_classes_ map. The compiler doesn't need to resolve
          // the type.
          ClassReference ref(dex_file, accessor.GetClassDefIndex());
          const ClassStatus existing = ClassStatus::kNotReady;
          ClassStateTable::InsertResult result =
             compiled_classes_.Insert(ref, existing, ClassStatus::kVerified);
          CHECK_EQ(result, ClassStateTable::kInsertResultSuccess) << ref.dex_file->GetLocation();
        } else {
          // Update the class status, so later compilation stages know they don't need to verify
          // the class.
          LoadAndUpdateStatus(accessor, ClassStatus::kVerified, class_loader, soa.Self());
          // Create `VerifiedMethod`s for each methods, the compiler expects one for
          // quickening or compiling.
          // Note that this means:
          // - We're only going to compile methods that did verify.
          // - Quickening will not do checkcast ellision.
          // TODO(ngeoffray): Reconsider this once we refactor compiler filters.
          for (const ClassAccessor::Method& method : accessor.GetMethods()) {
            verification_results->CreateVerifiedMethodFor(method.GetReference());
          }
        }
      } else if (!compiler_only_verifies) {
        // Make sure later compilation stages know they should not try to verify
        // this class again.
        LoadAndUpdateStatus(accessor,
                            ClassStatus::kRetryVerificationAtRuntime,
                            class_loader,
                            soa.Self());
      }
    }
  }
  return true;
}

void CompilerDriver::Verify(jobject jclass_loader,
                            const std::vector<const DexFile*>& dex_files,
                            TimingLogger* timings,
                            /*out*/ VerificationResults* verification_results) {
  if (FastVerify(jclass_loader, dex_files, timings, verification_results)) {
    return;
  }

  // If there is no existing `verifier_deps` (because of non-existing vdex), or
  // the existing `verifier_deps` is not valid anymore, create a new one for
  // non boot image compilation. The verifier will need it to record the new dependencies.
  // Then dex2oat can update the vdex file with these new dependencies.
  if (!GetCompilerOptions().IsBootImage() && !GetCompilerOptions().IsBootImageExtension()) {
    // Dex2oat creates the verifier deps.
    // Create the main VerifierDeps, and set it to this thread.
    verifier::VerifierDeps* verifier_deps =
        Runtime::Current()->GetCompilerCallbacks()->GetVerifierDeps();
    CHECK(verifier_deps != nullptr);
    Thread::Current()->SetVerifierDeps(verifier_deps);
    // Create per-thread VerifierDeps to avoid contention on the main one.
    // We will merge them after verification.
    for (ThreadPoolWorker* worker : parallel_thread_pool_->GetWorkers()) {
      worker->GetThread()->SetVerifierDeps(
          new verifier::VerifierDeps(GetCompilerOptions().GetDexFilesForOatFile()));
    }
  }

  // Verification updates VerifierDeps and needs to run single-threaded to be deterministic.
  bool force_determinism = GetCompilerOptions().IsForceDeterminism();
  ThreadPool* verify_thread_pool =
      force_determinism ? single_thread_pool_.get() : parallel_thread_pool_.get();
  size_t verify_thread_count = force_determinism ? 1U : parallel_thread_count_;
  for (const DexFile* dex_file : dex_files) {
    CHECK(dex_file != nullptr);
    VerifyDexFile(jclass_loader,
                  *dex_file,
                  dex_files,
                  verify_thread_pool,
                  verify_thread_count,
                  timings);
  }

  if (!GetCompilerOptions().IsBootImage() && !GetCompilerOptions().IsBootImageExtension()) {
    // Merge all VerifierDeps into the main one.
    verifier::VerifierDeps* verifier_deps = Thread::Current()->GetVerifierDeps();
    for (ThreadPoolWorker* worker : parallel_thread_pool_->GetWorkers()) {
      std::unique_ptr<verifier::VerifierDeps> thread_deps(worker->GetThread()->GetVerifierDeps());
      worker->GetThread()->SetVerifierDeps(nullptr);  // We just took ownership.
      verifier_deps->MergeWith(std::move(thread_deps),
                               GetCompilerOptions().GetDexFilesForOatFile());
    }
    Thread::Current()->SetVerifierDeps(nullptr);
  }
}

class VerifyClassVisitor : public CompilationVisitor {
 public:
  VerifyClassVisitor(const ParallelCompilationManager* manager, verifier::HardFailLogMode log_level)
     : manager_(manager),
       log_level_(log_level),
       sdk_version_(Runtime::Current()->GetTargetSdkVersion()) {}

  void Visit(size_t class_def_index) REQUIRES(!Locks::mutator_lock_) override {
    ScopedTrace trace(__FUNCTION__);
    ScopedObjectAccess soa(Thread::Current());
    const DexFile& dex_file = *manager_->GetDexFile();
    const dex::ClassDef& class_def = dex_file.GetClassDef(class_def_index);
    const char* descriptor = dex_file.GetClassDescriptor(class_def);
    ClassLinker* class_linker = manager_->GetClassLinker();
    jobject jclass_loader = manager_->GetClassLoader();
    StackHandleScope<3> hs(soa.Self());
    Handle<mirror::ClassLoader> class_loader(
        hs.NewHandle(soa.Decode<mirror::ClassLoader>(jclass_loader)));
    Handle<mirror::Class> klass(
        hs.NewHandle(class_linker->FindClass(soa.Self(), descriptor, class_loader)));
    verifier::FailureKind failure_kind;
    if (klass == nullptr) {
      CHECK(soa.Self()->IsExceptionPending());
      soa.Self()->ClearException();

      /*
       * At compile time, we can still structurally verify the class even if FindClass fails.
       * This is to ensure the class is structurally sound for compilation. An unsound class
       * will be rejected by the verifier and later skipped during compilation in the compiler.
       */
      Handle<mirror::DexCache> dex_cache(hs.NewHandle(class_linker->FindDexCache(
          soa.Self(), dex_file)));
      std::string error_msg;
      failure_kind =
          verifier::ClassVerifier::VerifyClass(soa.Self(),
                                               &dex_file,
                                               dex_cache,
                                               class_loader,
                                               class_def,
                                               Runtime::Current()->GetCompilerCallbacks(),
                                               true /* allow soft failures */,
                                               log_level_,
                                               sdk_version_,
                                               &error_msg);
      if (failure_kind == verifier::FailureKind::kHardFailure) {
        LOG(ERROR) << "Verification failed on class " << PrettyDescriptor(descriptor)
                   << " because: " << error_msg;
        manager_->GetCompiler()->SetHadHardVerifierFailure();
      } else if (failure_kind == verifier::FailureKind::kSoftFailure) {
        manager_->GetCompiler()->AddSoftVerifierFailure();
      } else {
        // Force a soft failure for the VerifierDeps. This is a sanity measure, as
        // the vdex file already records that the class hasn't been resolved. It avoids
        // trying to do future verification optimizations when processing the vdex file.
        DCHECK(failure_kind == verifier::FailureKind::kNoFailure ||
               failure_kind == verifier::FailureKind::kAccessChecksFailure) << failure_kind;
        failure_kind = verifier::FailureKind::kSoftFailure;
      }
    } else if (&klass->GetDexFile() != &dex_file) {
      // Skip a duplicate class (as the resolved class is from another, earlier dex file).
      // Record the information that we skipped this class in the vdex.
      // If the class resolved to a dex file not covered by the vdex, e.g. boot class path,
      // it is considered external, dependencies on it will be recorded and the vdex will
      // remain usable regardless of whether the class remains redefined or not (in the
      // latter case, this class will be verify-at-runtime).
      // On the other hand, if the class resolved to a dex file covered by the vdex, i.e.
      // a different dex file within the same APK, this class will always be eclipsed by it.
      // Recording that it was redefined is not necessary but will save class resolution
      // time during fast-verify.
      verifier::VerifierDeps::MaybeRecordClassRedefinition(dex_file, class_def);
      return;  // Do not update state.
    } else if (!SkipClass(jclass_loader, dex_file, klass.Get())) {
      CHECK(klass->IsResolved()) << klass->PrettyClass();
      failure_kind = class_linker->VerifyClass(soa.Self(), klass, log_level_);

      if (klass->IsErroneous()) {
        // ClassLinker::VerifyClass throws, which isn't useful in the compiler.
        CHECK(soa.Self()->IsExceptionPending());
        soa.Self()->ClearException();
        manager_->GetCompiler()->SetHadHardVerifierFailure();
      } else if (failure_kind == verifier::FailureKind::kSoftFailure) {
        manager_->GetCompiler()->AddSoftVerifierFailure();
      }

      CHECK(klass->ShouldVerifyAtRuntime() ||
            klass->IsVerifiedNeedsAccessChecks() ||
            klass->IsVerified() ||
            klass->IsErroneous())
          << klass->PrettyDescriptor() << ": state=" << klass->GetStatus();

      // Class has a meaningful status for the compiler now, record it.
      ClassReference ref(manager_->GetDexFile(), class_def_index);
      ClassStatus status = klass->GetStatus();
      if (status == ClassStatus::kInitialized) {
        // Initialized classes shall be visibly initialized when loaded from the image.
        status = ClassStatus::kVisiblyInitialized;
      }
      manager_->GetCompiler()->RecordClassStatus(ref, status);

      // It is *very* problematic if there are resolution errors in the boot classpath.
      //
      // It is also bad if classes fail verification. For example, we rely on things working
      // OK without verification when the decryption dialog is brought up. It is thus highly
      // recommended to compile the boot classpath with
      //   --abort-on-hard-verifier-error --abort-on-soft-verifier-error
      // which is the default build system configuration.
      if (kIsDebugBuild) {
        if (manager_->GetCompiler()->GetCompilerOptions().IsBootImage() ||
            manager_->GetCompiler()->GetCompilerOptions().IsBootImageExtension()) {
          if (!klass->IsResolved() || klass->IsErroneous()) {
            LOG(FATAL) << "Boot classpath class " << klass->PrettyClass()
                       << " failed to resolve/is erroneous: state= " << klass->GetStatus();
            UNREACHABLE();
          }
        }
        if (klass->IsVerified()) {
          DCHECK_EQ(failure_kind, verifier::FailureKind::kNoFailure);
        } else if (klass->IsVerifiedNeedsAccessChecks()) {
          DCHECK_EQ(failure_kind, verifier::FailureKind::kAccessChecksFailure);
        } else if (klass->ShouldVerifyAtRuntime()) {
          DCHECK_EQ(failure_kind, verifier::FailureKind::kSoftFailure);
        } else {
          DCHECK_EQ(failure_kind, verifier::FailureKind::kHardFailure);
        }
      }
    } else {
      // Make the skip a soft failure, essentially being considered as verify at runtime.
      failure_kind = verifier::FailureKind::kSoftFailure;
    }
    verifier::VerifierDeps::MaybeRecordVerificationStatus(dex_file, class_def, failure_kind);
    soa.Self()->AssertNoPendingException();
  }

 private:
  const ParallelCompilationManager* const manager_;
  const verifier::HardFailLogMode log_level_;
  const uint32_t sdk_version_;
};

void CompilerDriver::VerifyDexFile(jobject class_loader,
                                   const DexFile& dex_file,
                                   const std::vector<const DexFile*>& dex_files,
                                   ThreadPool* thread_pool,
                                   size_t thread_count,
                                   TimingLogger* timings) {
  TimingLogger::ScopedTiming t("Verify Dex File", timings);
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  ParallelCompilationManager context(class_linker, class_loader, this, &dex_file, dex_files,
                                     thread_pool);
  bool abort_on_verifier_failures = GetCompilerOptions().AbortOnHardVerifierFailure()
                                    || GetCompilerOptions().AbortOnSoftVerifierFailure();
  verifier::HardFailLogMode log_level = abort_on_verifier_failures
                              ? verifier::HardFailLogMode::kLogInternalFatal
                              : verifier::HardFailLogMode::kLogWarning;
  VerifyClassVisitor visitor(&context, log_level);
  context.ForAll(0, dex_file.NumClassDefs(), &visitor, thread_count);

  // Make initialized classes visibly initialized.
  class_linker->MakeInitializedClassesVisiblyInitialized(Thread::Current(), /*wait=*/ true);
}

class SetVerifiedClassVisitor : public CompilationVisitor {
 public:
  explicit SetVerifiedClassVisitor(const ParallelCompilationManager* manager) : manager_(manager) {}

  void Visit(size_t class_def_index) REQUIRES(!Locks::mutator_lock_) override {
    ScopedTrace trace(__FUNCTION__);
    ScopedObjectAccess soa(Thread::Current());
    const DexFile& dex_file = *manager_->GetDexFile();
    const dex::ClassDef& class_def = dex_file.GetClassDef(class_def_index);
    const char* descriptor = dex_file.GetClassDescriptor(class_def);
    ClassLinker* class_linker = manager_->GetClassLinker();
    jobject jclass_loader = manager_->GetClassLoader();
    StackHandleScope<3> hs(soa.Self());
    Handle<mirror::ClassLoader> class_loader(
        hs.NewHandle(soa.Decode<mirror::ClassLoader>(jclass_loader)));
    Handle<mirror::Class> klass(
        hs.NewHandle(class_linker->FindClass(soa.Self(), descriptor, class_loader)));
    // Class might have failed resolution. Then don't set it to verified.
    if (klass != nullptr) {
      // Only do this if the class is resolved. If even resolution fails, quickening will go very,
      // very wrong.
      if (klass->IsResolved() && !klass->IsErroneousResolved()) {
        if (klass->GetStatus() < ClassStatus::kVerified) {
          ObjectLock<mirror::Class> lock(soa.Self(), klass);
          // Set class status to verified.
          mirror::Class::SetStatus(klass, ClassStatus::kVerified, soa.Self());
          // Mark methods as pre-verified. If we don't do this, the interpreter will run with
          // access checks.
          InstructionSet instruction_set =
              manager_->GetCompiler()->GetCompilerOptions().GetInstructionSet();
          klass->SetSkipAccessChecksFlagOnAllMethods(GetInstructionSetPointerSize(instruction_set));
          klass->SetVerificationAttempted();
        }
        // Record the final class status if necessary.
        ClassReference ref(manager_->GetDexFile(), class_def_index);
        manager_->GetCompiler()->RecordClassStatus(ref, klass->GetStatus());
      }
    } else {
      Thread* self = soa.Self();
      DCHECK(self->IsExceptionPending());
      self->ClearException();
    }
  }

 private:
  const ParallelCompilationManager* const manager_;
};

void CompilerDriver::SetVerifiedDexFile(jobject class_loader,
                                        const DexFile& dex_file,
                                        const std::vector<const DexFile*>& dex_files,
                                        ThreadPool* thread_pool,
                                        size_t thread_count,
                                        TimingLogger* timings) {
  TimingLogger::ScopedTiming t("Set Verified Dex File", timings);
  if (!compiled_classes_.HaveDexFile(&dex_file)) {
    compiled_classes_.AddDexFile(&dex_file);
  }
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  ParallelCompilationManager context(class_linker, class_loader, this, &dex_file, dex_files,
                                     thread_pool);
  SetVerifiedClassVisitor visitor(&context);
  context.ForAll(0, dex_file.NumClassDefs(), &visitor, thread_count);
}

class InitializeClassVisitor : public CompilationVisitor {
 public:
  explicit InitializeClassVisitor(const ParallelCompilationManager* manager) : manager_(manager) {}

  void Visit(size_t class_def_index) override {
    ScopedTrace trace(__FUNCTION__);
    jobject jclass_loader = manager_->GetClassLoader();
    const DexFile& dex_file = *manager_->GetDexFile();
    const dex::ClassDef& class_def = dex_file.GetClassDef(class_def_index);
    const dex::TypeId& class_type_id = dex_file.GetTypeId(class_def.class_idx_);
    const char* descriptor = dex_file.StringDataByIdx(class_type_id.descriptor_idx_);

    ScopedObjectAccess soa(Thread::Current());
    StackHandleScope<3> hs(soa.Self());
    Handle<mirror::ClassLoader> class_loader(
        hs.NewHandle(soa.Decode<mirror::ClassLoader>(jclass_loader)));
    Handle<mirror::Class> klass(
        hs.NewHandle(manager_->GetClassLinker()->FindClass(soa.Self(), descriptor, class_loader)));

    if (klass != nullptr) {
      if (!SkipClass(manager_->GetClassLoader(), dex_file, klass.Get())) {
        TryInitializeClass(klass, class_loader);
      }
      manager_->GetCompiler()->stats_->AddClassStatus(klass->GetStatus());
    }
    // Clear any class not found or verification exceptions.
    soa.Self()->ClearException();
  }

  // A helper function for initializing klass.
  void TryInitializeClass(Handle<mirror::Class> klass, Handle<mirror::ClassLoader>& class_loader)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    const DexFile& dex_file = klass->GetDexFile();
    const dex::ClassDef* class_def = klass->GetClassDef();
    const dex::TypeId& class_type_id = dex_file.GetTypeId(class_def->class_idx_);
    const char* descriptor = dex_file.StringDataByIdx(class_type_id.descriptor_idx_);
    ScopedObjectAccessUnchecked soa(Thread::Current());
    StackHandleScope<3> hs(soa.Self());
    ClassLinker* const class_linker = manager_->GetClassLinker();
    Runtime* const runtime = Runtime::Current();
    const CompilerOptions& compiler_options = manager_->GetCompiler()->GetCompilerOptions();
    const bool is_boot_image = compiler_options.IsBootImage();
    const bool is_boot_image_extension = compiler_options.IsBootImageExtension();
    const bool is_app_image = compiler_options.IsAppImage();

    // For boot image extension, do not initialize classes defined
    // in dex files belonging to the boot image we're compiling against.
    if (is_boot_image_extension &&
        runtime->GetHeap()->ObjectIsInBootImageSpace(klass->GetDexCache())) {
      // Also return early and don't store the class status in the recorded class status.
      return;
    }
    // Do not initialize classes in boot space when compiling app (with or without image).
    if ((!is_boot_image && !is_boot_image_extension) && klass->IsBootStrapClassLoaded()) {
      // Also return early and don't store the class status in the recorded class status.
      return;
    }
    ClassStatus old_status = klass->GetStatus();
    // Only try to initialize classes that were successfully verified.
    if (klass->IsVerified()) {
      // Attempt to initialize the class but bail if we either need to initialize the super-class
      // or static fields.
      class_linker->EnsureInitialized(soa.Self(), klass, false, false);
      old_status = klass->GetStatus();
      if (!klass->IsInitialized()) {
        // We don't want non-trivial class initialization occurring on multiple threads due to
        // deadlock problems. For example, a parent class is initialized (holding its lock) that
        // refers to a sub-class in its static/class initializer causing it to try to acquire the
        // sub-class' lock. While on a second thread the sub-class is initialized (holding its lock)
        // after first initializing its parents, whose locks are acquired. This leads to a
        // parent-to-child and a child-to-parent lock ordering and consequent potential deadlock.
        // We need to use an ObjectLock due to potential suspension in the interpreting code. Rather
        // than use a special Object for the purpose we use the Class of java.lang.Class.
        Handle<mirror::Class> h_klass(hs.NewHandle(klass->GetClass()));
        ObjectLock<mirror::Class> lock(soa.Self(), h_klass);
        // Attempt to initialize allowing initialization of parent classes but still not static
        // fields.
        // Initialize dependencies first only for app or boot image extension,
        // to make TryInitializeClass() recursive.
        bool try_initialize_with_superclasses =
            is_boot_image ? true : InitializeDependencies(klass, class_loader, soa.Self());
        if (try_initialize_with_superclasses) {
          class_linker->EnsureInitialized(soa.Self(), klass, false, true);
          // It's OK to clear the exception here since the compiler is supposed to be fault
          // tolerant and will silently not initialize classes that have exceptions.
          soa.Self()->ClearException();
        }
        // Otherwise it's in app image or boot image extension but superclasses
        // cannot be initialized, no need to proceed.
        old_status = klass->GetStatus();

        bool too_many_encoded_fields = (!is_boot_image && !is_boot_image_extension) &&
            klass->NumStaticFields() > kMaxEncodedFields;

        // If the class was not initialized, we can proceed to see if we can initialize static
        // fields. Limit the max number of encoded fields.
        if (!klass->IsInitialized() &&
            (is_app_image || is_boot_image || is_boot_image_extension) &&
            try_initialize_with_superclasses &&
            !too_many_encoded_fields &&
            compiler_options.IsImageClass(descriptor)) {
          bool can_init_static_fields = false;
          if (is_boot_image || is_boot_image_extension) {
            // We need to initialize static fields, we only do this for image classes that aren't
            // marked with the $NoPreloadHolder (which implies this should not be initialized
            // early).
            can_init_static_fields = !EndsWith(std::string_view(descriptor), "$NoPreloadHolder;");
          } else {
            CHECK(is_app_image);
            // The boot image case doesn't need to recursively initialize the dependencies with
            // special logic since the class linker already does this.
            // Optimization will be disabled in debuggable build, because in debuggable mode we
            // want the <clinit> behavior to be observable for the debugger, so we don't do the
            // <clinit> at compile time.
            can_init_static_fields =
                ClassLinker::kAppImageMayContainStrings &&
                !soa.Self()->IsExceptionPending() &&
                !compiler_options.GetDebuggable() &&
                (compiler_options.InitializeAppImageClasses() ||
                 NoClinitInDependency(klass, soa.Self(), &class_loader));
            // TODO The checking for clinit can be removed since it's already
            // checked when init superclass. Currently keep it because it contains
            // processing of intern strings. Will be removed later when intern strings
            // and clinit are both initialized.
          }

          if (can_init_static_fields) {
            VLOG(compiler) << "Initializing: " << descriptor;
            // TODO multithreading support. We should ensure the current compilation thread has
            // exclusive access to the runtime and the transaction. To achieve this, we could use
            // a ReaderWriterMutex but we're holding the mutator lock so we fail mutex sanity
            // checks in Thread::AssertThreadSuspensionIsAllowable.

            // Resolve and initialize the exception type before enabling the transaction in case
            // the transaction aborts and cannot resolve the type.
            // TransactionAbortError is not initialized ant not in boot image, needed only by
            // compiler and will be pruned by ImageWriter.
            Handle<mirror::Class> exception_class =
                hs.NewHandle(class_linker->FindClass(soa.Self(),
                                                     Transaction::kAbortExceptionSignature,
                                                     class_loader));
            bool exception_initialized =
                class_linker->EnsureInitialized(soa.Self(), exception_class, true, true);
            DCHECK(exception_initialized);

            // Run the class initializer in transaction mode.
            runtime->EnterTransactionMode(is_app_image, klass.Get());

            bool success = class_linker->EnsureInitialized(soa.Self(), klass, true, true);
            // TODO we detach transaction from runtime to indicate we quit the transactional
            // mode which prevents the GC from visiting objects modified during the transaction.
            // Ensure GC is not run so don't access freed objects when aborting transaction.

            {
              ScopedAssertNoThreadSuspension ants("Transaction end");

              if (success) {
                runtime->ExitTransactionMode();
                DCHECK(!runtime->IsActiveTransaction());

                if (is_boot_image || is_boot_image_extension) {
                  // For boot image and boot image extension, we want to put the updated
                  // status in the oat class. This is not the case for app image as we
                  // want to keep the ability to load the oat file without the app image.
                  old_status = klass->GetStatus();
                }
              } else {
                CHECK(soa.Self()->IsExceptionPending());
                mirror::Throwable* exception = soa.Self()->GetException();
                VLOG(compiler) << "Initialization of " << descriptor << " aborted because of "
                               << exception->Dump();
                std::ostream* file_log = manager_->GetCompiler()->
                    GetCompilerOptions().GetInitFailureOutput();
                if (file_log != nullptr) {
                  *file_log << descriptor << "\n";
                  *file_log << exception->Dump() << "\n";
                }
                soa.Self()->ClearException();
                runtime->RollbackAllTransactions();
                CHECK_EQ(old_status, klass->GetStatus()) << "Previous class status not restored";
              }
            }

            if (!success && (is_boot_image || is_boot_image_extension)) {
              // On failure, still intern strings of static fields and seen in <clinit>, as these
              // will be created in the zygote. This is separated from the transaction code just
              // above as we will allocate strings, so must be allowed to suspend.
              // We only need to intern strings for boot image and boot image extension
              // because classes that failed to be initialized will not appear in app image.
              if (&klass->GetDexFile() == manager_->GetDexFile()) {
                InternStrings(klass, class_loader);
              } else {
                DCHECK(!is_boot_image) << "Boot image must have equal dex files";
              }
            }
          }
        }
        // Clear exception in case EnsureInitialized has caused one in the code above.
        // It's OK to clear the exception here since the compiler is supposed to be fault
        // tolerant and will silently not initialize classes that have exceptions.
        soa.Self()->ClearException();

        // If the class still isn't initialized, at least try some checks that initialization
        // would do so they can be skipped at runtime.
        if (!klass->IsInitialized() && class_linker->ValidateSuperClassDescriptors(klass)) {
          old_status = ClassStatus::kSuperclassValidated;
        } else {
          soa.Self()->ClearException();
        }
        soa.Self()->AssertNoPendingException();
      }
    }
    if (old_status == ClassStatus::kInitialized) {
      // Initialized classes shall be visibly initialized when loaded from the image.
      old_status = ClassStatus::kVisiblyInitialized;
    }
    // Record the final class status if necessary.
    ClassReference ref(&dex_file, klass->GetDexClassDefIndex());
    // Back up the status before doing initialization for static encoded fields,
    // because the static encoded branch wants to keep the status to uninitialized.
    manager_->GetCompiler()->RecordClassStatus(ref, old_status);
  }

 private:
  void InternStrings(Handle<mirror::Class> klass, Handle<mirror::ClassLoader> class_loader)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(manager_->GetCompiler()->GetCompilerOptions().IsBootImage() ||
           manager_->GetCompiler()->GetCompilerOptions().IsBootImageExtension());
    DCHECK(klass->IsVerified());
    DCHECK(!klass->IsInitialized());

    StackHandleScope<1> hs(Thread::Current());
    Handle<mirror::DexCache> dex_cache = hs.NewHandle(klass->GetDexCache());
    const dex::ClassDef* class_def = klass->GetClassDef();
    ClassLinker* class_linker = manager_->GetClassLinker();

    // Check encoded final field values for strings and intern.
    annotations::RuntimeEncodedStaticFieldValueIterator value_it(dex_cache,
                                                                 class_loader,
                                                                 manager_->GetClassLinker(),
                                                                 *class_def);
    for ( ; value_it.HasNext(); value_it.Next()) {
      if (value_it.GetValueType() == annotations::RuntimeEncodedStaticFieldValueIterator::kString) {
        // Resolve the string. This will intern the string.
        art::ObjPtr<mirror::String> resolved = class_linker->ResolveString(
            dex::StringIndex(value_it.GetJavaValue().i), dex_cache);
        CHECK(resolved != nullptr);
      }
    }

    // Intern strings seen in <clinit>.
    ArtMethod* clinit = klass->FindClassInitializer(class_linker->GetImagePointerSize());
    if (clinit != nullptr) {
      for (const DexInstructionPcPair& inst : clinit->DexInstructions()) {
        if (inst->Opcode() == Instruction::CONST_STRING) {
          ObjPtr<mirror::String> s = class_linker->ResolveString(
              dex::StringIndex(inst->VRegB_21c()), dex_cache);
          CHECK(s != nullptr);
        } else if (inst->Opcode() == Instruction::CONST_STRING_JUMBO) {
          ObjPtr<mirror::String> s = class_linker->ResolveString(
              dex::StringIndex(inst->VRegB_31c()), dex_cache);
          CHECK(s != nullptr);
        }
      }
    }
  }

  bool ResolveTypesOfMethods(Thread* self, ArtMethod* m)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Return value of ResolveReturnType() is discarded because resolve will be done internally.
    ObjPtr<mirror::Class> rtn_type = m->ResolveReturnType();
    if (rtn_type == nullptr) {
      self->ClearException();
      return false;
    }
    const dex::TypeList* types = m->GetParameterTypeList();
    if (types != nullptr) {
      for (uint32_t i = 0; i < types->Size(); ++i) {
        dex::TypeIndex param_type_idx = types->GetTypeItem(i).type_idx_;
        ObjPtr<mirror::Class> param_type = m->ResolveClassFromTypeIndex(param_type_idx);
        if (param_type == nullptr) {
          self->ClearException();
          return false;
        }
      }
    }
    return true;
  }

  // Pre resolve types mentioned in all method signatures before start a transaction
  // since ResolveType doesn't work in transaction mode.
  bool PreResolveTypes(Thread* self, const Handle<mirror::Class>& klass)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    PointerSize pointer_size = manager_->GetClassLinker()->GetImagePointerSize();
    for (ArtMethod& m : klass->GetMethods(pointer_size)) {
      if (!ResolveTypesOfMethods(self, &m)) {
        return false;
      }
    }
    if (klass->IsInterface()) {
      return true;
    } else if (klass->HasSuperClass()) {
      StackHandleScope<1> hs(self);
      MutableHandle<mirror::Class> super_klass(hs.NewHandle<mirror::Class>(klass->GetSuperClass()));
      for (int i = super_klass->GetVTableLength() - 1; i >= 0; --i) {
        ArtMethod* m = klass->GetVTableEntry(i, pointer_size);
        ArtMethod* super_m = super_klass->GetVTableEntry(i, pointer_size);
        if (!ResolveTypesOfMethods(self, m) || !ResolveTypesOfMethods(self, super_m)) {
          return false;
        }
      }
      for (int32_t i = 0; i < klass->GetIfTableCount(); ++i) {
        super_klass.Assign(klass->GetIfTable()->GetInterface(i));
        if (klass->GetClassLoader() != super_klass->GetClassLoader()) {
          uint32_t num_methods = super_klass->NumVirtualMethods();
          for (uint32_t j = 0; j < num_methods; ++j) {
            ArtMethod* m = klass->GetIfTable()->GetMethodArray(i)->GetElementPtrSize<ArtMethod*>(
                j, pointer_size);
            ArtMethod* super_m = super_klass->GetVirtualMethod(j, pointer_size);
            if (!ResolveTypesOfMethods(self, m) || !ResolveTypesOfMethods(self, super_m)) {
              return false;
            }
          }
        }
      }
    }
    return true;
  }

  // Initialize the klass's dependencies recursively before initializing itself.
  // Checking for interfaces is also necessary since interfaces that contain
  // default methods must be initialized before the class.
  bool InitializeDependencies(const Handle<mirror::Class>& klass,
                              Handle<mirror::ClassLoader> class_loader,
                              Thread* self)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (klass->HasSuperClass()) {
      StackHandleScope<1> hs(self);
      Handle<mirror::Class> super_class = hs.NewHandle(klass->GetSuperClass());
      if (!super_class->IsInitialized()) {
        this->TryInitializeClass(super_class, class_loader);
        if (!super_class->IsInitialized()) {
          return false;
        }
      }
    }

    if (!klass->IsInterface()) {
      size_t num_interfaces = klass->GetIfTableCount();
      for (size_t i = 0; i < num_interfaces; ++i) {
        StackHandleScope<1> hs(self);
        Handle<mirror::Class> iface = hs.NewHandle(klass->GetIfTable()->GetInterface(i));
        if (iface->HasDefaultMethods() && !iface->IsInitialized()) {
          TryInitializeClass(iface, class_loader);
          if (!iface->IsInitialized()) {
            return false;
          }
        }
      }
    }

    return PreResolveTypes(self, klass);
  }

  // In this phase the classes containing class initializers are ignored. Make sure no
  // clinit appears in kalss's super class chain and interfaces.
  bool NoClinitInDependency(const Handle<mirror::Class>& klass,
                            Thread* self,
                            Handle<mirror::ClassLoader>* class_loader)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* clinit =
        klass->FindClassInitializer(manager_->GetClassLinker()->GetImagePointerSize());
    if (clinit != nullptr) {
      VLOG(compiler) << klass->PrettyClass() << ' ' << clinit->PrettyMethod(true);
      return false;
    }
    if (klass->HasSuperClass()) {
      ObjPtr<mirror::Class> super_class = klass->GetSuperClass();
      StackHandleScope<1> hs(self);
      Handle<mirror::Class> handle_scope_super(hs.NewHandle(super_class));
      if (!NoClinitInDependency(handle_scope_super, self, class_loader)) {
        return false;
      }
    }

    uint32_t num_if = klass->NumDirectInterfaces();
    for (size_t i = 0; i < num_if; i++) {
      ObjPtr<mirror::Class>
          interface = mirror::Class::GetDirectInterface(self, klass.Get(), i);
      StackHandleScope<1> hs(self);
      Handle<mirror::Class> handle_interface(hs.NewHandle(interface));
      if (!NoClinitInDependency(handle_interface, self, class_loader)) {
        return false;
      }
    }

    return true;
  }

  const ParallelCompilationManager* const manager_;
};

void CompilerDriver::InitializeClasses(jobject jni_class_loader,
                                       const DexFile& dex_file,
                                       const std::vector<const DexFile*>& dex_files,
                                       TimingLogger* timings) {
  TimingLogger::ScopedTiming t("InitializeNoClinit", timings);

  // Initialization allocates objects and needs to run single-threaded to be deterministic.
  bool force_determinism = GetCompilerOptions().IsForceDeterminism();
  ThreadPool* init_thread_pool = force_determinism
                                     ? single_thread_pool_.get()
                                     : parallel_thread_pool_.get();
  size_t init_thread_count = force_determinism ? 1U : parallel_thread_count_;

  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  ParallelCompilationManager context(class_linker, jni_class_loader, this, &dex_file, dex_files,
                                     init_thread_pool);

  if (GetCompilerOptions().IsBootImage() ||
      GetCompilerOptions().IsBootImageExtension() ||
      GetCompilerOptions().IsAppImage()) {
    // Set the concurrency thread to 1 to support initialization for images since transaction
    // doesn't support multithreading now.
    // TODO: remove this when transactional mode supports multithreading.
    init_thread_count = 1U;
  }
  InitializeClassVisitor visitor(&context);
  context.ForAll(0, dex_file.NumClassDefs(), &visitor, init_thread_count);

  // Make initialized classes visibly initialized.
  class_linker->MakeInitializedClassesVisiblyInitialized(Thread::Current(), /*wait=*/ true);
}

class InitializeArrayClassesAndCreateConflictTablesVisitor : public ClassVisitor {
 public:
  explicit InitializeArrayClassesAndCreateConflictTablesVisitor(VariableSizedHandleScope& hs)
      : hs_(hs) {}

  bool operator()(ObjPtr<mirror::Class> klass) override
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (Runtime::Current()->GetHeap()->ObjectIsInBootImageSpace(klass)) {
      return true;
    }
    if (klass->IsArrayClass()) {
      StackHandleScope<1> hs(Thread::Current());
      auto h_klass = hs.NewHandleWrapper(&klass);
      Runtime::Current()->GetClassLinker()->EnsureInitialized(hs.Self(), h_klass, true, true);
    }
    // Collect handles since there may be thread suspension in future EnsureInitialized.
    to_visit_.push_back(hs_.NewHandle(klass));
    return true;
  }

  void FillAllIMTAndConflictTables() REQUIRES_SHARED(Locks::mutator_lock_) {
    for (Handle<mirror::Class> c : to_visit_) {
      // Create the conflict tables.
      FillIMTAndConflictTables(c.Get());
    }
  }

 private:
  void FillIMTAndConflictTables(ObjPtr<mirror::Class> klass)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!klass->ShouldHaveImt()) {
      return;
    }
    if (visited_classes_.find(klass) != visited_classes_.end()) {
      return;
    }
    if (klass->HasSuperClass()) {
      FillIMTAndConflictTables(klass->GetSuperClass());
    }
    if (!klass->IsTemp()) {
      Runtime::Current()->GetClassLinker()->FillIMTAndConflictTables(klass);
    }
    visited_classes_.insert(klass);
  }

  VariableSizedHandleScope& hs_;
  std::vector<Handle<mirror::Class>> to_visit_;
  std::unordered_set<ObjPtr<mirror::Class>, HashObjPtr> visited_classes_;
};

void CompilerDriver::InitializeClasses(jobject class_loader,
                                       const std::vector<const DexFile*>& dex_files,
                                       TimingLogger* timings) {
  for (size_t i = 0; i != dex_files.size(); ++i) {
    const DexFile* dex_file = dex_files[i];
    CHECK(dex_file != nullptr);
    InitializeClasses(class_loader, *dex_file, dex_files, timings);
  }
  if (GetCompilerOptions().IsBootImage() ||
      GetCompilerOptions().IsBootImageExtension() ||
      GetCompilerOptions().IsAppImage()) {
    // Make sure that we call EnsureIntiailized on all the array classes to call
    // SetVerificationAttempted so that the access flags are set. If we do not do this they get
    // changed at runtime resulting in more dirty image pages.
    // Also create conflict tables.
    // Only useful if we are compiling an image.
    ScopedObjectAccess soa(Thread::Current());
    VariableSizedHandleScope hs(soa.Self());
    InitializeArrayClassesAndCreateConflictTablesVisitor visitor(hs);
    Runtime::Current()->GetClassLinker()->VisitClassesWithoutClassesLock(&visitor);
    visitor.FillAllIMTAndConflictTables();
  }
  if (GetCompilerOptions().IsBootImage() || GetCompilerOptions().IsBootImageExtension()) {
    // Prune garbage objects created during aborted transactions.
    Runtime::Current()->GetHeap()->CollectGarbage(/* clear_soft_references= */ true);
  }
}

template <typename CompileFn>
static void CompileDexFile(CompilerDriver* driver,
                           jobject class_loader,
                           const DexFile& dex_file,
                           const std::vector<const DexFile*>& dex_files,
                           ThreadPool* thread_pool,
                           size_t thread_count,
                           TimingLogger* timings,
                           const char* timing_name,
                           CompileFn compile_fn) {
  TimingLogger::ScopedTiming t(timing_name, timings);
  ParallelCompilationManager context(Runtime::Current()->GetClassLinker(),
                                     class_loader,
                                     driver,
                                     &dex_file,
                                     dex_files,
                                     thread_pool);

  auto compile = [&context, &compile_fn](size_t class_def_index) {
    const DexFile& dex_file = *context.GetDexFile();
    SCOPED_TRACE << "compile " << dex_file.GetLocation() << "@" << class_def_index;
    ClassLinker* class_linker = context.GetClassLinker();
    jobject jclass_loader = context.GetClassLoader();
    ClassReference ref(&dex_file, class_def_index);
    const dex::ClassDef& class_def = dex_file.GetClassDef(class_def_index);
    ClassAccessor accessor(dex_file, class_def_index);
    CompilerDriver* const driver = context.GetCompiler();
    // Skip compiling classes with generic verifier failures since they will still fail at runtime
    if (driver->GetCompilerOptions().GetVerificationResults()->IsClassRejected(ref)) {
      return;
    }
    // Use a scoped object access to perform to the quick SkipClass check.
    ScopedObjectAccess soa(Thread::Current());
    StackHandleScope<3> hs(soa.Self());
    Handle<mirror::ClassLoader> class_loader(
        hs.NewHandle(soa.Decode<mirror::ClassLoader>(jclass_loader)));
    Handle<mirror::Class> klass(
        hs.NewHandle(class_linker->FindClass(soa.Self(), accessor.GetDescriptor(), class_loader)));
    Handle<mirror::DexCache> dex_cache;
    if (klass == nullptr) {
      soa.Self()->AssertPendingException();
      soa.Self()->ClearException();
      dex_cache = hs.NewHandle(class_linker->FindDexCache(soa.Self(), dex_file));
    } else if (SkipClass(jclass_loader, dex_file, klass.Get())) {
      return;
    } else if (&klass->GetDexFile() != &dex_file) {
      // Skip a duplicate class (as the resolved class is from another, earlier dex file).
      return;  // Do not update state.
    } else {
      dex_cache = hs.NewHandle(klass->GetDexCache());
    }

    // Avoid suspension if there are no methods to compile.
    if (accessor.NumDirectMethods() + accessor.NumVirtualMethods() == 0) {
      return;
    }

    // Go to native so that we don't block GC during compilation.
    ScopedThreadSuspension sts(soa.Self(), kNative);

    // Can we run DEX-to-DEX compiler on this class ?
    optimizer::DexToDexCompiler::CompilationLevel dex_to_dex_compilation_level =
        GetDexToDexCompilationLevel(soa.Self(), *driver, jclass_loader, dex_file, class_def);

    // Compile direct and virtual methods.
    int64_t previous_method_idx = -1;
    for (const ClassAccessor::Method& method : accessor.GetMethods()) {
      const uint32_t method_idx = method.GetIndex();
      if (method_idx == previous_method_idx) {
        // smali can create dex files with two encoded_methods sharing the same method_idx
        // http://code.google.com/p/smali/issues/detail?id=119
        continue;
      }
      previous_method_idx = method_idx;
      compile_fn(soa.Self(),
                 driver,
                 method.GetCodeItem(),
                 method.GetAccessFlags(),
                 method.GetInvokeType(class_def.access_flags_),
                 class_def_index,
                 method_idx,
                 class_loader,
                 dex_file,
                 dex_to_dex_compilation_level,
                 dex_cache);
    }
  };
  context.ForAllLambda(0, dex_file.NumClassDefs(), compile, thread_count);
}

void CompilerDriver::Compile(jobject class_loader,
                             const std::vector<const DexFile*>& dex_files,
                             TimingLogger* timings) {
  if (kDebugProfileGuidedCompilation) {
    const ProfileCompilationInfo* profile_compilation_info =
        GetCompilerOptions().GetProfileCompilationInfo();
    LOG(INFO) << "[ProfileGuidedCompilation] " <<
        ((profile_compilation_info == nullptr)
            ? "null"
            : profile_compilation_info->DumpInfo(dex_files));
  }

  dex_to_dex_compiler_.ClearState();
  for (const DexFile* dex_file : dex_files) {
    CHECK(dex_file != nullptr);
    CompileDexFile(this,
                   class_loader,
                   *dex_file,
                   dex_files,
                   parallel_thread_pool_.get(),
                   parallel_thread_count_,
                   timings,
                   "Compile Dex File Quick",
                   CompileMethodQuick);
    const ArenaPool* const arena_pool = Runtime::Current()->GetArenaPool();
    const size_t arena_alloc = arena_pool->GetBytesAllocated();
    max_arena_alloc_ = std::max(arena_alloc, max_arena_alloc_);
    Runtime::Current()->ReclaimArenaPoolMemory();
  }

  if (dex_to_dex_compiler_.NumCodeItemsToQuicken(Thread::Current()) > 0u) {
    // TODO: Not visit all of the dex files, its probably rare that only one would have quickened
    // methods though.
    for (const DexFile* dex_file : dex_files) {
      CompileDexFile(this,
                     class_loader,
                     *dex_file,
                     dex_files,
                     parallel_thread_pool_.get(),
                     parallel_thread_count_,
                     timings,
                     "Compile Dex File Dex2Dex",
                     CompileMethodDex2Dex);
    }
    dex_to_dex_compiler_.ClearState();
  }

  VLOG(compiler) << "Compile: " << GetMemoryUsageString(false);
}

void CompilerDriver::AddCompiledMethod(const MethodReference& method_ref,
                                       CompiledMethod* const compiled_method) {
  DCHECK(GetCompiledMethod(method_ref) == nullptr) << method_ref.PrettyMethod();
  MethodTable::InsertResult result = compiled_methods_.Insert(method_ref,
                                                              /*expected*/ nullptr,
                                                              compiled_method);
  CHECK(result == MethodTable::kInsertResultSuccess);
  DCHECK(GetCompiledMethod(method_ref) != nullptr) << method_ref.PrettyMethod();
}

CompiledMethod* CompilerDriver::RemoveCompiledMethod(const MethodReference& method_ref) {
  CompiledMethod* ret = nullptr;
  CHECK(compiled_methods_.Remove(method_ref, &ret));
  return ret;
}

bool CompilerDriver::GetCompiledClass(const ClassReference& ref, ClassStatus* status) const {
  DCHECK(status != nullptr);
  // The table doesn't know if something wasn't inserted. For this case it will return
  // ClassStatus::kNotReady. To handle this, just assume anything we didn't try to verify
  // is not compiled.
  if (!compiled_classes_.Get(ref, status) ||
      *status < ClassStatus::kRetryVerificationAtRuntime) {
    return false;
  }
  return true;
}

ClassStatus CompilerDriver::GetClassStatus(const ClassReference& ref) const {
  ClassStatus status = ClassStatus::kNotReady;
  if (!GetCompiledClass(ref, &status)) {
    classpath_classes_.Get(ref, &status);
  }
  return status;
}

void CompilerDriver::RecordClassStatus(const ClassReference& ref, ClassStatus status) {
  switch (status) {
    case ClassStatus::kErrorResolved:
    case ClassStatus::kErrorUnresolved:
    case ClassStatus::kNotReady:
    case ClassStatus::kResolved:
    case ClassStatus::kRetryVerificationAtRuntime:
    case ClassStatus::kVerifiedNeedsAccessChecks:
    case ClassStatus::kVerified:
    case ClassStatus::kSuperclassValidated:
    case ClassStatus::kVisiblyInitialized:
      break;  // Expected states.
    default:
      LOG(FATAL) << "Unexpected class status for class "
          << PrettyDescriptor(
              ref.dex_file->GetClassDescriptor(ref.dex_file->GetClassDef(ref.index)))
          << " of " << status;
  }

  ClassStateTable::InsertResult result;
  ClassStateTable* table = &compiled_classes_;
  do {
    ClassStatus existing = ClassStatus::kNotReady;
    if (!table->Get(ref, &existing)) {
      // A classpath class.
      if (kIsDebugBuild) {
        // Check to make sure it's not a dex file for an oat file we are compiling since these
        // should always succeed. These do not include classes in for used libraries.
        for (const DexFile* dex_file : GetCompilerOptions().GetDexFilesForOatFile()) {
          CHECK_NE(ref.dex_file, dex_file) << ref.dex_file->GetLocation();
        }
      }
      if (!classpath_classes_.HaveDexFile(ref.dex_file)) {
        // Boot classpath dex file.
        return;
      }
      table = &classpath_classes_;
      table->Get(ref, &existing);
    }
    if (existing >= status) {
      // Existing status is already better than we expect, break.
      break;
    }
    // Update the status if we now have a greater one. This happens with vdex,
    // which records a class is verified, but does not resolve it.
    result = table->Insert(ref, existing, status);
    CHECK(result != ClassStateTable::kInsertResultInvalidDexFile) << ref.dex_file->GetLocation();
  } while (result != ClassStateTable::kInsertResultSuccess);
}

CompiledMethod* CompilerDriver::GetCompiledMethod(MethodReference ref) const {
  CompiledMethod* compiled_method = nullptr;
  compiled_methods_.Get(ref, &compiled_method);
  return compiled_method;
}

std::string CompilerDriver::GetMemoryUsageString(bool extended) const {
  std::ostringstream oss;
  const gc::Heap* const heap = Runtime::Current()->GetHeap();
  const size_t java_alloc = heap->GetBytesAllocated();
  oss << "arena alloc=" << PrettySize(max_arena_alloc_) << " (" << max_arena_alloc_ << "B)";
  oss << " java alloc=" << PrettySize(java_alloc) << " (" << java_alloc << "B)";
#if defined(__BIONIC__) || defined(__GLIBC__)
  const struct mallinfo info = mallinfo();
  const size_t allocated_space = static_cast<size_t>(info.uordblks);
  const size_t free_space = static_cast<size_t>(info.fordblks);
  oss << " native alloc=" << PrettySize(allocated_space) << " (" << allocated_space << "B)"
      << " free=" << PrettySize(free_space) << " (" << free_space << "B)";
#endif
  compiled_method_storage_.DumpMemoryUsage(oss, extended);
  return oss.str();
}

void CompilerDriver::InitializeThreadPools() {
  size_t parallel_count = parallel_thread_count_ > 0 ? parallel_thread_count_ - 1 : 0;
  parallel_thread_pool_.reset(
      new ThreadPool("Compiler driver thread pool", parallel_count));
  single_thread_pool_.reset(new ThreadPool("Single-threaded Compiler driver thread pool", 0));
}

void CompilerDriver::FreeThreadPools() {
  parallel_thread_pool_.reset();
  single_thread_pool_.reset();
}

void CompilerDriver::SetClasspathDexFiles(const std::vector<const DexFile*>& dex_files) {
  classpath_classes_.AddDexFiles(dex_files);
}

}  // namespace art
