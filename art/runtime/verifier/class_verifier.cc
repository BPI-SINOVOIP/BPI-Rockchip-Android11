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

#include "class_verifier.h"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "art_method-inl.h"
#include "base/enums.h"
#include "base/locks.h"
#include "base/logging.h"
#include "base/systrace.h"
#include "base/utils.h"
#include "class_linker.h"
#include "compiler_callbacks.h"
#include "dex/class_accessor-inl.h"
#include "dex/class_reference.h"
#include "dex/descriptors_names.h"
#include "dex/dex_file-inl.h"
#include "handle.h"
#include "handle_scope-inl.h"
#include "method_verifier-inl.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache.h"
#include "runtime.h"
#include "thread.h"
#include "verifier/method_verifier.h"
#include "verifier/reg_type_cache.h"

namespace art {
namespace verifier {

using android::base::StringPrintf;

// We print a warning blurb about "dx --no-optimize" when we find monitor-locking issues. Make
// sure we only print this once.
static bool gPrintedDxMonitorText = false;

class StandardVerifyCallback : public VerifierCallback {
 public:
  void SetDontCompile(ArtMethod* m, bool value) override REQUIRES_SHARED(Locks::mutator_lock_) {
    if (value) {
      m->SetDontCompile();
    }
  }
  void SetMustCountLocks(ArtMethod* m, bool value) override REQUIRES_SHARED(Locks::mutator_lock_) {
    if (value) {
      m->SetMustCountLocks();
    }
  }
};

FailureKind ClassVerifier::ReverifyClass(Thread* self,
                                         ObjPtr<mirror::Class> klass,
                                         HardFailLogMode log_level,
                                         uint32_t api_level,
                                         std::string* error) {
  DCHECK(!Runtime::Current()->IsAotCompiler());
  StackHandleScope<1> hs(self);
  Handle<mirror::Class> h_klass(hs.NewHandle(klass));
  // We don't want to mess with these while other mutators are possibly looking at them. Instead we
  // will wait until we can update them while everything is suspended.
  class DelayedVerifyCallback : public VerifierCallback {
   public:
    void SetDontCompile(ArtMethod* m, bool value) override REQUIRES_SHARED(Locks::mutator_lock_) {
      dont_compiles_.push_back({ m, value });
    }
    void SetMustCountLocks(ArtMethod* m, bool value) override
        REQUIRES_SHARED(Locks::mutator_lock_) {
      count_locks_.push_back({ m, value });
    }
    void UpdateFlags(bool skip_access_checks) REQUIRES(Locks::mutator_lock_) {
      for (auto it : count_locks_) {
        VLOG(verifier_debug) << "Setting " << it.first->PrettyMethod() << " count locks to "
                             << it.second;
        if (it.second) {
          it.first->SetMustCountLocks();
        } else {
          it.first->ClearMustCountLocks();
        }
        if (skip_access_checks && it.first->IsInvokable() && !it.first->IsNative()) {
          it.first->SetSkipAccessChecks();
        }
      }
      for (auto it : dont_compiles_) {
        VLOG(verifier_debug) << "Setting " << it.first->PrettyMethod() << " dont-compile to "
                             << it.second;
        if (it.second) {
          it.first->SetDontCompile();
        } else {
          it.first->ClearDontCompile();
        }
      }
    }

   private:
    std::vector<std::pair<ArtMethod*, bool>> dont_compiles_;
    std::vector<std::pair<ArtMethod*, bool>> count_locks_;
  };
  DelayedVerifyCallback dvc;
  FailureKind res = CommonVerifyClass(self,
                                      h_klass.Get(),
                                      /*callbacks=*/nullptr,
                                      &dvc,
                                      /*allow_soft_failures=*/false,
                                      log_level,
                                      api_level,
                                      error);
  DCHECK_NE(res, FailureKind::kHardFailure);
  ScopedThreadSuspension sts(Thread::Current(), ThreadState::kSuspended);
  ScopedSuspendAll ssa("Update method flags for reverify");
  dvc.UpdateFlags(res == FailureKind::kNoFailure);
  return res;
}

FailureKind ClassVerifier::VerifyClass(Thread* self,
                                       ObjPtr<mirror::Class> klass,
                                       CompilerCallbacks* callbacks,
                                       bool allow_soft_failures,
                                       HardFailLogMode log_level,
                                       uint32_t api_level,
                                       std::string* error) {
  if (klass->IsVerified()) {
    return FailureKind::kNoFailure;
  }
  StandardVerifyCallback svc;
  return CommonVerifyClass(self,
                           klass,
                           callbacks,
                           &svc,
                           allow_soft_failures,
                           log_level,
                           api_level,
                           error);
}

FailureKind ClassVerifier::CommonVerifyClass(Thread* self,
                                             ObjPtr<mirror::Class> klass,
                                             CompilerCallbacks* callbacks,
                                             VerifierCallback* verifier_callback,
                                             bool allow_soft_failures,
                                             HardFailLogMode log_level,
                                             uint32_t api_level,
                                             std::string* error) {
  bool early_failure = false;
  std::string failure_message;
  const DexFile& dex_file = klass->GetDexFile();
  const dex::ClassDef* class_def = klass->GetClassDef();
  ObjPtr<mirror::Class> super = klass->GetSuperClass();
  std::string temp;
  if (super == nullptr && strcmp("Ljava/lang/Object;", klass->GetDescriptor(&temp)) != 0) {
    early_failure = true;
    failure_message = " that has no super class";
  } else if (super != nullptr && super->IsFinal()) {
    early_failure = true;
    failure_message = " that attempts to sub-class final class " + super->PrettyDescriptor();
  } else if (class_def == nullptr) {
    early_failure = true;
    failure_message = " that isn't present in dex file " + dex_file.GetLocation();
  }
  if (early_failure) {
    *error = "Verifier rejected class " + klass->PrettyDescriptor() + failure_message;
    if (callbacks != nullptr) {
      ClassReference ref(&dex_file, klass->GetDexClassDefIndex());
      callbacks->ClassRejected(ref);
    }
    return FailureKind::kHardFailure;
  }
  StackHandleScope<2> hs(self);
  Handle<mirror::DexCache> dex_cache(hs.NewHandle(klass->GetDexCache()));
  Handle<mirror::ClassLoader> class_loader(hs.NewHandle(klass->GetClassLoader()));
  return VerifyClass(self,
                     &dex_file,
                     dex_cache,
                     class_loader,
                     *class_def,
                     callbacks,
                     verifier_callback,
                     allow_soft_failures,
                     log_level,
                     api_level,
                     error);
}


FailureKind ClassVerifier::VerifyClass(Thread* self,
                                       const DexFile* dex_file,
                                       Handle<mirror::DexCache> dex_cache,
                                       Handle<mirror::ClassLoader> class_loader,
                                       const dex::ClassDef& class_def,
                                       CompilerCallbacks* callbacks,
                                       bool allow_soft_failures,
                                       HardFailLogMode log_level,
                                       uint32_t api_level,
                                       std::string* error) {
  StandardVerifyCallback svc;
  return VerifyClass(self,
                     dex_file,
                     dex_cache,
                     class_loader,
                     class_def,
                     callbacks,
                     &svc,
                     allow_soft_failures,
                     log_level,
                     api_level,
                     error);
}

FailureKind ClassVerifier::VerifyClass(Thread* self,
                                       const DexFile* dex_file,
                                       Handle<mirror::DexCache> dex_cache,
                                       Handle<mirror::ClassLoader> class_loader,
                                       const dex::ClassDef& class_def,
                                       CompilerCallbacks* callbacks,
                                       VerifierCallback* verifier_callback,
                                       bool allow_soft_failures,
                                       HardFailLogMode log_level,
                                       uint32_t api_level,
                                       std::string* error) {
  // A class must not be abstract and final.
  if ((class_def.access_flags_ & (kAccAbstract | kAccFinal)) == (kAccAbstract | kAccFinal)) {
    *error = "Verifier rejected class ";
    *error += PrettyDescriptor(dex_file->GetClassDescriptor(class_def));
    *error += ": class is abstract and final.";
    return FailureKind::kHardFailure;
  }

  ClassAccessor accessor(*dex_file, class_def);
  SCOPED_TRACE << "VerifyClass " << PrettyDescriptor(accessor.GetDescriptor());

  int64_t previous_method_idx[2] = { -1, -1 };
  MethodVerifier::FailureData failure_data;
  ClassLinker* const linker = Runtime::Current()->GetClassLinker();

  for (const ClassAccessor::Method& method : accessor.GetMethods()) {
    int64_t* previous_idx = &previous_method_idx[method.IsStaticOrDirect() ? 0u : 1u];
    self->AllowThreadSuspension();
    const uint32_t method_idx = method.GetIndex();
    if (method_idx == *previous_idx) {
      // smali can create dex files with two encoded_methods sharing the same method_idx
      // http://code.google.com/p/smali/issues/detail?id=119
      continue;
    }
    *previous_idx = method_idx;
    const InvokeType type = method.GetInvokeType(class_def.access_flags_);
    ArtMethod* resolved_method = linker->ResolveMethod<ClassLinker::ResolveMode::kNoChecks>(
        method_idx, dex_cache, class_loader, /* referrer= */ nullptr, type);
    if (resolved_method == nullptr) {
      DCHECK(self->IsExceptionPending());
      // We couldn't resolve the method, but continue regardless.
      self->ClearException();
    } else {
      DCHECK(resolved_method->GetDeclaringClassUnchecked() != nullptr) << type;
    }
    std::string hard_failure_msg;
    MethodVerifier::FailureData result =
        MethodVerifier::VerifyMethod(self,
                                     linker,
                                     Runtime::Current()->GetArenaPool(),
                                     method_idx,
                                     dex_file,
                                     dex_cache,
                                     class_loader,
                                     class_def,
                                     method.GetCodeItem(),
                                     resolved_method,
                                     method.GetAccessFlags(),
                                     callbacks,
                                     verifier_callback,
                                     allow_soft_failures,
                                     log_level,
                                     /*need_precise_constants=*/ false,
                                     api_level,
                                     Runtime::Current()->IsAotCompiler(),
                                     &hard_failure_msg);
    if (result.kind == FailureKind::kHardFailure) {
      if (failure_data.kind == FailureKind::kHardFailure) {
        // If we logged an error before, we need a newline.
        *error += "\n";
      } else {
        // If we didn't log a hard failure before, print the header of the message.
        *error += "Verifier rejected class ";
        *error += PrettyDescriptor(dex_file->GetClassDescriptor(class_def));
        *error += ":";
      }
      *error += " ";
      *error += hard_failure_msg;
    }
    failure_data.Merge(result);
  }

  if (failure_data.kind == FailureKind::kNoFailure) {
    return FailureKind::kNoFailure;
  } else {
    if ((failure_data.types & VERIFY_ERROR_LOCKING) != 0) {
      // Print a warning about expected slow-down. Use a string temporary to print one contiguous
      // warning.
      std::string tmp =
          StringPrintf("Class %s failed lock verification and will run slower.",
                       PrettyDescriptor(accessor.GetDescriptor()).c_str());
      if (!gPrintedDxMonitorText) {
        tmp = tmp + "\nCommon causes for lock verification issues are non-optimized dex code\n"
                    "and incorrect proguard optimizations.";
        gPrintedDxMonitorText = true;
      }
      LOG(WARNING) << tmp;
    }
    return failure_data.kind;
  }
}

void ClassVerifier::Init(ClassLinker* class_linker) {
  MethodVerifier::Init(class_linker);
}

void ClassVerifier::Shutdown() {
  MethodVerifier::Shutdown();
}

void ClassVerifier::VisitStaticRoots(RootVisitor* visitor) {
  MethodVerifier::VisitStaticRoots(visitor);
}

}  // namespace verifier
}  // namespace art
