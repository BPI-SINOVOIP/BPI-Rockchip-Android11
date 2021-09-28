/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "dalvik_system_ZygoteHooks.h"

#include <stdlib.h>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "arch/instruction_set.h"
#include "art_method-inl.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "base/runtime_debug.h"
#include "debugger.h"
#include "hidden_api.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "jni/java_vm_ext.h"
#include "jni/jni_internal.h"
#include "native_util.h"
#include "nativehelper/jni_macros.h"
#include "nativehelper/scoped_utf_chars.h"
#include "non_debuggable_classes.h"
#include "oat_file.h"
#include "oat_file_manager.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "thread-current-inl.h"
#include "thread_list.h"
#include "trace.h"

#include <sys/resource.h>

namespace art {

// Set to true to always determine the non-debuggable classes even if we would not allow a debugger
// to actually attach.
static bool kAlwaysCollectNonDebuggableClasses =
    RegisterRuntimeDebugFlag(&kAlwaysCollectNonDebuggableClasses);

using android::base::StringPrintf;

class ClassSet {
 public:
  // The number of classes we reasonably expect to have to look at. Realistically the number is more
  // ~10 but there is little harm in having some extra.
  static constexpr int kClassSetCapacity = 100;

  explicit ClassSet(Thread* const self) : self_(self) {
    self_->GetJniEnv()->PushFrame(kClassSetCapacity);
  }

  ~ClassSet() {
    self_->GetJniEnv()->PopFrame();
  }

  void AddClass(ObjPtr<mirror::Class> klass) REQUIRES(Locks::mutator_lock_) {
    class_set_.insert(self_->GetJniEnv()->AddLocalReference<jclass>(klass));
  }

  const std::unordered_set<jclass>& GetClasses() const {
    return class_set_;
  }

 private:
  Thread* const self_;
  std::unordered_set<jclass> class_set_;
};

static void DoCollectNonDebuggableCallback(Thread* thread, void* data)
    REQUIRES(Locks::mutator_lock_) {
  class NonDebuggableStacksVisitor : public StackVisitor {
   public:
    NonDebuggableStacksVisitor(Thread* t, ClassSet* class_set)
        : StackVisitor(t, nullptr, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
          class_set_(class_set) {}

    ~NonDebuggableStacksVisitor() override {}

    bool VisitFrame() override REQUIRES(Locks::mutator_lock_) {
      if (GetMethod()->IsRuntimeMethod()) {
        return true;
      }
      class_set_->AddClass(GetMethod()->GetDeclaringClass());
      if (kIsDebugBuild) {
        LOG(INFO) << GetMethod()->GetDeclaringClass()->PrettyClass()
                  << " might not be fully debuggable/deoptimizable due to "
                  << GetMethod()->PrettyMethod() << " appearing on the stack during zygote fork.";
      }
      return true;
    }

   private:
    ClassSet* class_set_;
  };
  NonDebuggableStacksVisitor visitor(thread, reinterpret_cast<ClassSet*>(data));
  visitor.WalkStack();
}

static void CollectNonDebuggableClasses() REQUIRES(!Locks::mutator_lock_) {
  Runtime* const runtime = Runtime::Current();
  Thread* const self = Thread::Current();
  // Get the mutator lock.
  ScopedObjectAccess soa(self);
  ClassSet classes(self);
  {
    // Drop the shared mutator lock.
    ScopedThreadSuspension sts(self, art::ThreadState::kNative);
    // Get exclusive mutator lock with suspend all.
    ScopedSuspendAll suspend("Checking stacks for non-obsoletable methods!",
                             /*long_suspend=*/false);
    MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
    runtime->GetThreadList()->ForEach(DoCollectNonDebuggableCallback, &classes);
  }
  for (jclass klass : classes.GetClasses()) {
    NonDebuggableClasses::AddNonDebuggableClass(klass);
  }
}

// Must match values in com.android.internal.os.Zygote.
enum {
  DEBUG_ENABLE_JDWP                   = 1,
  DEBUG_ENABLE_CHECKJNI               = 1 << 1,
  DEBUG_ENABLE_ASSERT                 = 1 << 2,
  DEBUG_ENABLE_SAFEMODE               = 1 << 3,
  DEBUG_ENABLE_JNI_LOGGING            = 1 << 4,
  DEBUG_GENERATE_DEBUG_INFO           = 1 << 5,
  DEBUG_ALWAYS_JIT                    = 1 << 6,
  DEBUG_NATIVE_DEBUGGABLE             = 1 << 7,
  DEBUG_JAVA_DEBUGGABLE               = 1 << 8,
  DISABLE_VERIFIER                    = 1 << 9,
  ONLY_USE_SYSTEM_OAT_FILES           = 1 << 10,
  DEBUG_GENERATE_MINI_DEBUG_INFO      = 1 << 11,
  HIDDEN_API_ENFORCEMENT_POLICY_MASK  = (1 << 12)
                                      | (1 << 13),
  PROFILE_SYSTEM_SERVER               = 1 << 14,
  PROFILE_FROM_SHELL                  = 1 << 15,
  USE_APP_IMAGE_STARTUP_CACHE         = 1 << 16,
  DEBUG_IGNORE_APP_SIGNAL_HANDLER     = 1 << 17,
  DISABLE_TEST_API_ENFORCEMENT_POLICY = 1 << 18,

  // bits to shift (flags & HIDDEN_API_ENFORCEMENT_POLICY_MASK) by to get a value
  // corresponding to hiddenapi::EnforcementPolicy
  API_ENFORCEMENT_POLICY_SHIFT = CTZ(HIDDEN_API_ENFORCEMENT_POLICY_MASK),
};

static uint32_t EnableDebugFeatures(uint32_t runtime_flags) {
  Runtime* const runtime = Runtime::Current();
  if ((runtime_flags & DEBUG_ENABLE_CHECKJNI) != 0) {
    JavaVMExt* vm = runtime->GetJavaVM();
    if (!vm->IsCheckJniEnabled()) {
      LOG(INFO) << "Late-enabling -Xcheck:jni";
      vm->SetCheckJniEnabled(true);
      // There's only one thread running at this point, so only one JNIEnv to fix up.
      Thread::Current()->GetJniEnv()->SetCheckJniEnabled(true);
    } else {
      LOG(INFO) << "Not late-enabling -Xcheck:jni (already on)";
    }
    runtime_flags &= ~DEBUG_ENABLE_CHECKJNI;
  }

  if ((runtime_flags & DEBUG_ENABLE_JNI_LOGGING) != 0) {
    gLogVerbosity.third_party_jni = true;
    runtime_flags &= ~DEBUG_ENABLE_JNI_LOGGING;
  }

  Dbg::SetJdwpAllowed((runtime_flags & DEBUG_ENABLE_JDWP) != 0);
  runtime_flags &= ~DEBUG_ENABLE_JDWP;

  const bool safe_mode = (runtime_flags & DEBUG_ENABLE_SAFEMODE) != 0;
  if (safe_mode) {
    // Only quicken oat files.
    runtime->AddCompilerOption("--compiler-filter=quicken");
    runtime->SetSafeMode(true);
    runtime_flags &= ~DEBUG_ENABLE_SAFEMODE;
  }

  // This is for backwards compatibility with Dalvik.
  runtime_flags &= ~DEBUG_ENABLE_ASSERT;

  if ((runtime_flags & DEBUG_ALWAYS_JIT) != 0) {
    jit::JitOptions* jit_options = runtime->GetJITOptions();
    CHECK(jit_options != nullptr);
    Runtime::Current()->DoAndMaybeSwitchInterpreter([=]() {
        jit_options->SetJitAtFirstUse();
    });
    runtime_flags &= ~DEBUG_ALWAYS_JIT;
  }

  bool needs_non_debuggable_classes = false;
  if ((runtime_flags & DEBUG_JAVA_DEBUGGABLE) != 0) {
    runtime->AddCompilerOption("--debuggable");
    runtime_flags |= DEBUG_GENERATE_MINI_DEBUG_INFO;
    runtime->SetJavaDebuggable(true);
    {
      // Deoptimize the boot image as it may be non-debuggable.
      ScopedSuspendAll ssa(__FUNCTION__);
      runtime->DeoptimizeBootImage();
    }
    runtime_flags &= ~DEBUG_JAVA_DEBUGGABLE;
    needs_non_debuggable_classes = true;
  }
  if (needs_non_debuggable_classes || kAlwaysCollectNonDebuggableClasses) {
    CollectNonDebuggableClasses();
  }

  if ((runtime_flags & DEBUG_NATIVE_DEBUGGABLE) != 0) {
    runtime->AddCompilerOption("--debuggable");
    runtime_flags |= DEBUG_GENERATE_DEBUG_INFO;
    runtime->SetNativeDebuggable(true);
    runtime_flags &= ~DEBUG_NATIVE_DEBUGGABLE;
  }

  if ((runtime_flags & DEBUG_GENERATE_MINI_DEBUG_INFO) != 0) {
    // Generate native minimal debug information to allow backtracing.
    runtime->AddCompilerOption("--generate-mini-debug-info");
    runtime_flags &= ~DEBUG_GENERATE_MINI_DEBUG_INFO;
  }

  if ((runtime_flags & DEBUG_GENERATE_DEBUG_INFO) != 0) {
    // Generate all native debug information we can (e.g. line-numbers).
    runtime->AddCompilerOption("--generate-debug-info");
    runtime_flags &= ~DEBUG_GENERATE_DEBUG_INFO;
  }

  if ((runtime_flags & DEBUG_IGNORE_APP_SIGNAL_HANDLER) != 0) {
    runtime->SetSignalHookDebuggable(true);
    runtime_flags &= ~DEBUG_IGNORE_APP_SIGNAL_HANDLER;
  }

  runtime->SetProfileableFromShell((runtime_flags & PROFILE_FROM_SHELL) != 0);
  runtime_flags &= ~PROFILE_FROM_SHELL;

  return runtime_flags;
}

static jlong ZygoteHooks_nativePreFork(JNIEnv* env, jclass) {
  Runtime* runtime = Runtime::Current();
  CHECK(runtime->IsZygote()) << "runtime instance not started with -Xzygote";

  runtime->PreZygoteFork();

  // Grab thread before fork potentially makes Thread::pthread_key_self_ unusable.
  return reinterpret_cast<jlong>(ThreadForEnv(env));
}

static void ZygoteHooks_nativePostZygoteFork(JNIEnv*, jclass) {
  Runtime::Current()->PostZygoteFork();
}

static void ZygoteHooks_nativePostForkSystemServer(JNIEnv* env ATTRIBUTE_UNUSED,
                                                   jclass klass ATTRIBUTE_UNUSED,
                                                   jint runtime_flags) {
  // Set the runtime state as the first thing, in case JIT and other services
  // start querying it.
  Runtime::Current()->SetAsSystemServer();

  // This JIT code cache for system server is created whilst the runtime is still single threaded.
  // System server has a window where it can create executable pages for this purpose, but this is
  // turned off after this hook. Consequently, the only JIT mode supported is the dual-view JIT
  // where one mapping is R->RW and the other is RX. Single view requires RX->RWX->RX.
  if (Runtime::Current()->GetJit() != nullptr) {
    Runtime::Current()->GetJit()->GetCodeCache()->PostForkChildAction(
        /* is_system_server= */ true, /* is_zygote= */ false);
  }
  // Enable profiling if required based on the flags. This is done here instead of in
  // nativePostForkChild since nativePostForkChild is called after loading the system server oat
  // files.
  bool profile_system_server = (runtime_flags & PROFILE_SYSTEM_SERVER) == PROFILE_SYSTEM_SERVER;
  Runtime::Current()->GetJITOptions()->SetSaveProfilingInfo(profile_system_server);
}

static void ZygoteHooks_nativePostForkChild(JNIEnv* env,
                                            jclass,
                                            jlong token,
                                            jint runtime_flags,
                                            jboolean is_system_server,
                                            jboolean is_zygote,
                                            jstring instruction_set) {
  DCHECK(!(is_system_server && is_zygote));
  // Set the runtime state as the first thing, in case JIT and other services
  // start querying it.
  Runtime::Current()->SetAsZygoteChild(is_system_server, is_zygote);

  Thread* thread = reinterpret_cast<Thread*>(token);
  // Our system thread ID, etc, has changed so reset Thread state.
  thread->InitAfterFork();
  runtime_flags = EnableDebugFeatures(runtime_flags);
  hiddenapi::EnforcementPolicy api_enforcement_policy = hiddenapi::EnforcementPolicy::kDisabled;

  Runtime* runtime = Runtime::Current();

  if ((runtime_flags & DISABLE_VERIFIER) != 0) {
    runtime->DisableVerifier();
    runtime_flags &= ~DISABLE_VERIFIER;
  }

  if ((runtime_flags & ONLY_USE_SYSTEM_OAT_FILES) != 0 || is_system_server) {
    runtime->GetOatFileManager().SetOnlyUseSystemOatFiles();
    runtime_flags &= ~ONLY_USE_SYSTEM_OAT_FILES;
  }

  api_enforcement_policy = hiddenapi::EnforcementPolicyFromInt(
      (runtime_flags & HIDDEN_API_ENFORCEMENT_POLICY_MASK) >> API_ENFORCEMENT_POLICY_SHIFT);
  runtime_flags &= ~HIDDEN_API_ENFORCEMENT_POLICY_MASK;

  if ((runtime_flags & DISABLE_TEST_API_ENFORCEMENT_POLICY) != 0u) {
    runtime->SetTestApiEnforcementPolicy(hiddenapi::EnforcementPolicy::kDisabled);
  } else {
    runtime->SetTestApiEnforcementPolicy(hiddenapi::EnforcementPolicy::kEnabled);
  }
  runtime_flags &= ~DISABLE_TEST_API_ENFORCEMENT_POLICY;

  bool profile_system_server = (runtime_flags & PROFILE_SYSTEM_SERVER) == PROFILE_SYSTEM_SERVER;
  runtime_flags &= ~PROFILE_SYSTEM_SERVER;

  runtime->SetLoadAppImageStartupCacheEnabled(
      (runtime_flags & USE_APP_IMAGE_STARTUP_CACHE) != 0u);
  runtime_flags &= ~USE_APP_IMAGE_STARTUP_CACHE;

  if (runtime_flags != 0) {
    LOG(ERROR) << StringPrintf("Unknown bits set in runtime_flags: %#x", runtime_flags);
  }

  runtime->GetHeap()->PostForkChildAction(thread);
  if (runtime->GetJit() != nullptr) {
    if (!is_system_server) {
      // System server already called the JIT cache post fork action in `nativePostForkSystemServer`.
      runtime->GetJit()->GetCodeCache()->PostForkChildAction(
          /* is_system_server= */ false, is_zygote);
    }
    // This must be called after EnableDebugFeatures.
    runtime->GetJit()->PostForkChildAction(is_system_server, is_zygote);
  }

  // Update tracing.
  if (Trace::GetMethodTracingMode() != TracingMode::kTracingInactive) {
    Trace::TraceOutputMode output_mode = Trace::GetOutputMode();
    Trace::TraceMode trace_mode = Trace::GetMode();
    size_t buffer_size = Trace::GetBufferSize();

    // Just drop it.
    Trace::Abort();

    // Only restart if it was streaming mode.
    // TODO: Expose buffer size, so we can also do file mode.
    if (output_mode == Trace::TraceOutputMode::kStreaming) {
      static constexpr size_t kMaxProcessNameLength = 100;
      char name_buf[kMaxProcessNameLength] = {};
      int rc = pthread_getname_np(pthread_self(), name_buf, kMaxProcessNameLength);
      std::string proc_name;

      if (rc == 0) {
          // On success use the pthread name.
          proc_name = name_buf;
      }

      if (proc_name.empty() || proc_name == "zygote" || proc_name == "zygote64") {
        // Either no process name, or the name hasn't been changed, yet. Just use pid.
        pid_t pid = getpid();
        proc_name = StringPrintf("%u", static_cast<uint32_t>(pid));
      }

      std::string trace_file = StringPrintf("/data/misc/trace/%s.trace.bin", proc_name.c_str());
      Trace::Start(trace_file.c_str(),
                   buffer_size,
                   0,   // TODO: Expose flags.
                   output_mode,
                   trace_mode,
                   0);  // TODO: Expose interval.
      if (thread->IsExceptionPending()) {
        ScopedObjectAccess soa(env);
        thread->ClearException();
      }
    }
  }

  bool do_hidden_api_checks = api_enforcement_policy != hiddenapi::EnforcementPolicy::kDisabled;
  DCHECK(!(is_system_server && do_hidden_api_checks))
      << "SystemServer should be forked with EnforcementPolicy::kDisable";
  DCHECK(!(is_zygote && do_hidden_api_checks))
      << "Child zygote processes should be forked with EnforcementPolicy::kDisable";
  runtime->SetHiddenApiEnforcementPolicy(api_enforcement_policy);
  runtime->SetDedupeHiddenApiWarnings(true);
  if (api_enforcement_policy != hiddenapi::EnforcementPolicy::kDisabled &&
      runtime->GetHiddenApiEventLogSampleRate() != 0) {
    // Hidden API checks are enabled, and we are sampling access for the event log. Initialize the
    // random seed, to ensure the sampling is actually random. We do this post-fork, as doing it
    // pre-fork would result in the same sequence for every forked process.
    std::srand(static_cast<uint32_t>(NanoTime()));
  }

  if (instruction_set != nullptr && !is_system_server) {
    ScopedUtfChars isa_string(env, instruction_set);
    InstructionSet isa = GetInstructionSetFromString(isa_string.c_str());
    Runtime::NativeBridgeAction action = Runtime::NativeBridgeAction::kUnload;
    if (isa != InstructionSet::kNone && isa != kRuntimeISA) {
      action = Runtime::NativeBridgeAction::kInitialize;
    }
    runtime->InitNonZygoteOrPostFork(env, is_system_server, is_zygote, action, isa_string.c_str());
  } else {
    runtime->InitNonZygoteOrPostFork(
        env,
        is_system_server,
        is_zygote,
        Runtime::NativeBridgeAction::kUnload,
        /*isa=*/ nullptr,
        profile_system_server);
  }
}

static void ZygoteHooks_startZygoteNoThreadCreation(JNIEnv* env ATTRIBUTE_UNUSED,
                                                    jclass klass ATTRIBUTE_UNUSED) {
  Runtime::Current()->SetZygoteNoThreadSection(true);
}

static void ZygoteHooks_stopZygoteNoThreadCreation(JNIEnv* env ATTRIBUTE_UNUSED,
                                                   jclass klass ATTRIBUTE_UNUSED) {
  Runtime::Current()->SetZygoteNoThreadSection(false);
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(ZygoteHooks, nativePreFork, "()J"),
  NATIVE_METHOD(ZygoteHooks, nativePostZygoteFork, "()V"),
  NATIVE_METHOD(ZygoteHooks, nativePostForkSystemServer, "(I)V"),
  NATIVE_METHOD(ZygoteHooks, nativePostForkChild, "(JIZZLjava/lang/String;)V"),
  NATIVE_METHOD(ZygoteHooks, startZygoteNoThreadCreation, "()V"),
  NATIVE_METHOD(ZygoteHooks, stopZygoteNoThreadCreation, "()V"),
};

void register_dalvik_system_ZygoteHooks(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("dalvik/system/ZygoteHooks");
}

}  // namespace art
