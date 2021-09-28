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

#ifndef ART_RUNTIME_RUNTIME_H_
#define ART_RUNTIME_RUNTIME_H_

#include <jni.h>
#include <stdio.h>

#include <iosfwd>
#include <set>
#include <string>
#include <utility>
#include <memory>
#include <vector>

#include "base/locks.h"
#include "base/macros.h"
#include "base/mem_map.h"
#include "base/string_view_cpp20.h"
#include "deoptimization_kind.h"
#include "dex/dex_file_types.h"
#include "experimental_flags.h"
#include "gc/space/image_space_loading_order.h"
#include "gc_root.h"
#include "instrumentation.h"
#include "jdwp_provider.h"
#include "jni/jni_id_manager.h"
#include "jni_id_type.h"
#include "obj_ptr.h"
#include "offsets.h"
#include "process_state.h"
#include "quick/quick_method_frame_info.h"
#include "reflective_value_visitor.h"
#include "runtime_stats.h"

namespace art {

namespace gc {
class AbstractSystemWeakHolder;
class Heap;
}  // namespace gc

namespace hiddenapi {
enum class EnforcementPolicy;
}  // namespace hiddenapi

namespace jit {
class Jit;
class JitCodeCache;
class JitOptions;
}  // namespace jit

namespace mirror {
class Array;
class ClassLoader;
class DexCache;
template<class T> class ObjectArray;
template<class T> class PrimitiveArray;
typedef PrimitiveArray<int8_t> ByteArray;
class String;
class Throwable;
}  // namespace mirror
namespace ti {
class Agent;
class AgentSpec;
}  // namespace ti
namespace verifier {
class MethodVerifier;
enum class VerifyMode : int8_t;
}  // namespace verifier
class ArenaPool;
class ArtMethod;
enum class CalleeSaveType: uint32_t;
class ClassLinker;
class CompilerCallbacks;
class Dex2oatImageTest;
class DexFile;
enum class InstructionSet;
class InternTable;
class IsMarkedVisitor;
class JavaVMExt;
class LinearAlloc;
class MonitorList;
class MonitorPool;
class NullPointerHandler;
class OatFileAssistantTest;
class OatFileManager;
class Plugin;
struct RuntimeArgumentMap;
class RuntimeCallbacks;
class SignalCatcher;
class StackOverflowHandler;
class SuspensionHandler;
class ThreadList;
class ThreadPool;
class Trace;
struct TraceConfig;
class Transaction;

typedef std::vector<std::pair<std::string, const void*>> RuntimeOptions;

class Runtime {
 public:
  // Parse raw runtime options.
  static bool ParseOptions(const RuntimeOptions& raw_options,
                           bool ignore_unrecognized,
                           RuntimeArgumentMap* runtime_options);

  // Creates and initializes a new runtime.
  static bool Create(RuntimeArgumentMap&& runtime_options)
      SHARED_TRYLOCK_FUNCTION(true, Locks::mutator_lock_);

  // Creates and initializes a new runtime.
  static bool Create(const RuntimeOptions& raw_options, bool ignore_unrecognized)
      SHARED_TRYLOCK_FUNCTION(true, Locks::mutator_lock_);

  bool EnsurePluginLoaded(const char* plugin_name, std::string* error_msg);
  bool EnsurePerfettoPlugin(std::string* error_msg);

  // IsAotCompiler for compilers that don't have a running runtime. Only dex2oat currently.
  bool IsAotCompiler() const {
    return !UseJitCompilation() && IsCompiler();
  }

  // IsCompiler is any runtime which has a running compiler, either dex2oat or JIT.
  bool IsCompiler() const {
    return compiler_callbacks_ != nullptr;
  }

  // If a compiler, are we compiling a boot image?
  bool IsCompilingBootImage() const;

  bool CanRelocate() const;

  bool ShouldRelocate() const {
    return must_relocate_ && CanRelocate();
  }

  bool MustRelocateIfPossible() const {
    return must_relocate_;
  }

  bool IsImageDex2OatEnabled() const {
    return image_dex2oat_enabled_;
  }

  CompilerCallbacks* GetCompilerCallbacks() {
    return compiler_callbacks_;
  }

  void SetCompilerCallbacks(CompilerCallbacks* callbacks) {
    CHECK(callbacks != nullptr);
    compiler_callbacks_ = callbacks;
  }

  bool IsZygote() const {
    return is_zygote_;
  }

  bool IsPrimaryZygote() const {
    return is_primary_zygote_;
  }

  bool IsSystemServer() const {
    return is_system_server_;
  }

  void SetAsSystemServer() {
    is_system_server_ = true;
    is_zygote_ = false;
    is_primary_zygote_ = false;
  }

  void SetAsZygoteChild(bool is_system_server, bool is_zygote) {
    // System server should have been set earlier in SetAsSystemServer.
    CHECK_EQ(is_system_server_, is_system_server);
    is_zygote_ = is_zygote;
    is_primary_zygote_ = false;
  }

  bool IsExplicitGcDisabled() const {
    return is_explicit_gc_disabled_;
  }

  std::string GetCompilerExecutable() const;

  const std::vector<std::string>& GetCompilerOptions() const {
    return compiler_options_;
  }

  void AddCompilerOption(const std::string& option) {
    compiler_options_.push_back(option);
  }

  const std::vector<std::string>& GetImageCompilerOptions() const {
    return image_compiler_options_;
  }

  const std::string& GetImageLocation() const {
    return image_location_;
  }

  // Starts a runtime, which may cause threads to be started and code to run.
  bool Start() UNLOCK_FUNCTION(Locks::mutator_lock_);

  bool IsShuttingDown(Thread* self);
  bool IsShuttingDownLocked() const REQUIRES(Locks::runtime_shutdown_lock_) {
    return shutting_down_;
  }

  size_t NumberOfThreadsBeingBorn() const REQUIRES(Locks::runtime_shutdown_lock_) {
    return threads_being_born_;
  }

  void StartThreadBirth() REQUIRES(Locks::runtime_shutdown_lock_) {
    threads_being_born_++;
  }

  void EndThreadBirth() REQUIRES(Locks::runtime_shutdown_lock_);

  bool IsStarted() const {
    return started_;
  }

  bool IsFinishedStarting() const {
    return finished_starting_;
  }

  void RunRootClinits(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);

  static Runtime* Current() {
    return instance_;
  }

  // Aborts semi-cleanly. Used in the implementation of LOG(FATAL), which most
  // callers should prefer.
  NO_RETURN static void Abort(const char* msg) REQUIRES(!Locks::abort_lock_);

  // Returns the "main" ThreadGroup, used when attaching user threads.
  jobject GetMainThreadGroup() const;

  // Returns the "system" ThreadGroup, used when attaching our internal threads.
  jobject GetSystemThreadGroup() const;

  // Returns the system ClassLoader which represents the CLASSPATH.
  jobject GetSystemClassLoader() const;

  // Attaches the calling native thread to the runtime.
  bool AttachCurrentThread(const char* thread_name, bool as_daemon, jobject thread_group,
                           bool create_peer);

  void CallExitHook(jint status);

  // Detaches the current native thread from the runtime.
  void DetachCurrentThread() REQUIRES(!Locks::mutator_lock_);

  void DumpDeoptimizations(std::ostream& os);
  void DumpForSigQuit(std::ostream& os);
  void DumpLockHolders(std::ostream& os);

  ~Runtime();

  const std::vector<std::string>& GetBootClassPath() const {
    return boot_class_path_;
  }

  const std::vector<std::string>& GetBootClassPathLocations() const {
    DCHECK(boot_class_path_locations_.empty() ||
           boot_class_path_locations_.size() == boot_class_path_.size());
    return boot_class_path_locations_.empty() ? boot_class_path_ : boot_class_path_locations_;
  }

  const std::string& GetClassPathString() const {
    return class_path_string_;
  }

  ClassLinker* GetClassLinker() const {
    return class_linker_;
  }

  jni::JniIdManager* GetJniIdManager() const {
    return jni_id_manager_.get();
  }

  size_t GetDefaultStackSize() const {
    return default_stack_size_;
  }

  unsigned int GetFinalizerTimeoutMs() const {
    return finalizer_timeout_ms_;
  }

  gc::Heap* GetHeap() const {
    return heap_;
  }

  InternTable* GetInternTable() const {
    DCHECK(intern_table_ != nullptr);
    return intern_table_;
  }

  JavaVMExt* GetJavaVM() const {
    return java_vm_.get();
  }

  size_t GetMaxSpinsBeforeThinLockInflation() const {
    return max_spins_before_thin_lock_inflation_;
  }

  MonitorList* GetMonitorList() const {
    return monitor_list_;
  }

  MonitorPool* GetMonitorPool() const {
    return monitor_pool_;
  }

  // Is the given object the special object used to mark a cleared JNI weak global?
  bool IsClearedJniWeakGlobal(ObjPtr<mirror::Object> obj) REQUIRES_SHARED(Locks::mutator_lock_);

  // Get the special object used to mark a cleared JNI weak global.
  mirror::Object* GetClearedJniWeakGlobal() REQUIRES_SHARED(Locks::mutator_lock_);

  mirror::Throwable* GetPreAllocatedOutOfMemoryErrorWhenThrowingException()
      REQUIRES_SHARED(Locks::mutator_lock_);
  mirror::Throwable* GetPreAllocatedOutOfMemoryErrorWhenThrowingOOME()
      REQUIRES_SHARED(Locks::mutator_lock_);
  mirror::Throwable* GetPreAllocatedOutOfMemoryErrorWhenHandlingStackOverflow()
      REQUIRES_SHARED(Locks::mutator_lock_);

  mirror::Throwable* GetPreAllocatedNoClassDefFoundError()
      REQUIRES_SHARED(Locks::mutator_lock_);

  const std::vector<std::string>& GetProperties() const {
    return properties_;
  }

  ThreadList* GetThreadList() const {
    return thread_list_;
  }

  static const char* GetVersion() {
    return "2.1.0";
  }

  bool IsMethodHandlesEnabled() const {
    return true;
  }

  void DisallowNewSystemWeaks() REQUIRES_SHARED(Locks::mutator_lock_);
  void AllowNewSystemWeaks() REQUIRES_SHARED(Locks::mutator_lock_);
  // broadcast_for_checkpoint is true when we broadcast for making blocking threads to respond to
  // checkpoint requests. It's false when we broadcast to unblock blocking threads after system weak
  // access is reenabled.
  void BroadcastForNewSystemWeaks(bool broadcast_for_checkpoint = false);

  // Visit all the roots. If only_dirty is true then non-dirty roots won't be visited. If
  // clean_dirty is true then dirty roots will be marked as non-dirty after visiting.
  void VisitRoots(RootVisitor* visitor, VisitRootFlags flags = kVisitRootFlagAllRoots)
      REQUIRES(!Locks::classlinker_classes_lock_, !Locks::trace_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Visit image roots, only used for hprof since the GC uses the image space mod union table
  // instead.
  void VisitImageRoots(RootVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_);

  // Visit all of the roots we can safely visit concurrently.
  void VisitConcurrentRoots(RootVisitor* visitor,
                            VisitRootFlags flags = kVisitRootFlagAllRoots)
      REQUIRES(!Locks::classlinker_classes_lock_, !Locks::trace_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Visit all of the non thread roots, we can do this with mutators unpaused.
  void VisitNonThreadRoots(RootVisitor* visitor)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void VisitTransactionRoots(RootVisitor* visitor)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Sweep system weaks, the system weak is deleted if the visitor return null. Otherwise, the
  // system weak is updated to be the visitor's returned value.
  void SweepSystemWeaks(IsMarkedVisitor* visitor)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Walk all reflective objects and visit their targets as well as any method/fields held by the
  // runtime threads that are marked as being reflective.
  void VisitReflectiveTargets(ReflectiveValueVisitor* visitor) REQUIRES(Locks::mutator_lock_);
  // Helper for visiting reflective targets with lambdas for both field and method reflective
  // targets.
  template <typename FieldVis, typename MethodVis>
  void VisitReflectiveTargets(FieldVis&& fv, MethodVis&& mv) REQUIRES(Locks::mutator_lock_) {
    FunctionReflectiveValueVisitor frvv(fv, mv);
    VisitReflectiveTargets(&frvv);
  }

  // Returns a special method that calls into a trampoline for runtime method resolution
  ArtMethod* GetResolutionMethod();

  bool HasResolutionMethod() const {
    return resolution_method_ != nullptr;
  }

  void SetResolutionMethod(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);
  void ClearResolutionMethod() {
    resolution_method_ = nullptr;
  }

  ArtMethod* CreateResolutionMethod() REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns a special method that calls into a trampoline for runtime imt conflicts.
  ArtMethod* GetImtConflictMethod();
  ArtMethod* GetImtUnimplementedMethod();

  bool HasImtConflictMethod() const {
    return imt_conflict_method_ != nullptr;
  }

  void ClearImtConflictMethod() {
    imt_conflict_method_ = nullptr;
  }

  void FixupConflictTables() REQUIRES_SHARED(Locks::mutator_lock_);
  void SetImtConflictMethod(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);
  void SetImtUnimplementedMethod(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);

  ArtMethod* CreateImtConflictMethod(LinearAlloc* linear_alloc)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void ClearImtUnimplementedMethod() {
    imt_unimplemented_method_ = nullptr;
  }

  bool HasCalleeSaveMethod(CalleeSaveType type) const {
    return callee_save_methods_[static_cast<size_t>(type)] != 0u;
  }

  ArtMethod* GetCalleeSaveMethod(CalleeSaveType type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtMethod* GetCalleeSaveMethodUnchecked(CalleeSaveType type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  QuickMethodFrameInfo GetRuntimeMethodFrameInfo(ArtMethod* method)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static constexpr size_t GetCalleeSaveMethodOffset(CalleeSaveType type) {
    return OFFSETOF_MEMBER(Runtime, callee_save_methods_[static_cast<size_t>(type)]);
  }

  InstructionSet GetInstructionSet() const {
    return instruction_set_;
  }

  void SetInstructionSet(InstructionSet instruction_set);
  void ClearInstructionSet();

  void SetCalleeSaveMethod(ArtMethod* method, CalleeSaveType type);
  void ClearCalleeSaveMethods();

  ArtMethod* CreateCalleeSaveMethod() REQUIRES_SHARED(Locks::mutator_lock_);

  uint64_t GetStat(int kind);

  RuntimeStats* GetStats() {
    return &stats_;
  }

  bool HasStatsEnabled() const {
    return stats_enabled_;
  }

  void ResetStats(int kinds);

  void SetStatsEnabled(bool new_state)
      REQUIRES(!Locks::instrument_entrypoints_lock_, !Locks::mutator_lock_);

  enum class NativeBridgeAction {  // private
    kUnload,
    kInitialize
  };

  jit::Jit* GetJit() const {
    return jit_.get();
  }

  jit::JitCodeCache* GetJitCodeCache() const {
    return jit_code_cache_.get();
  }

  // Returns true if JIT compilations are enabled. GetJit() will be not null in this case.
  bool UseJitCompilation() const;

  void PreZygoteFork();
  void PostZygoteFork();
  void InitNonZygoteOrPostFork(
      JNIEnv* env,
      bool is_system_server,
      bool is_child_zygote,
      NativeBridgeAction action,
      const char* isa,
      bool profile_system_server = false);

  const instrumentation::Instrumentation* GetInstrumentation() const {
    return &instrumentation_;
  }

  instrumentation::Instrumentation* GetInstrumentation() {
    return &instrumentation_;
  }

  void RegisterAppInfo(const std::vector<std::string>& code_paths,
                       const std::string& profile_output_filename);

  // Transaction support.
  bool IsActiveTransaction() const;
  void EnterTransactionMode(bool strict, mirror::Class* root);
  void ExitTransactionMode();
  void RollbackAllTransactions() REQUIRES_SHARED(Locks::mutator_lock_);
  // Transaction rollback and exit transaction are always done together, it's convenience to
  // do them in one function.
  void RollbackAndExitTransactionMode() REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsTransactionAborted() const;
  const std::unique_ptr<Transaction>& GetTransaction() const;
  bool IsActiveStrictTransactionMode() const;

  void AbortTransactionAndThrowAbortError(Thread* self, const std::string& abort_message)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void ThrowTransactionAbortError(Thread* self)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void RecordWriteFieldBoolean(mirror::Object* obj, MemberOffset field_offset, uint8_t value,
                               bool is_volatile) const;
  void RecordWriteFieldByte(mirror::Object* obj, MemberOffset field_offset, int8_t value,
                            bool is_volatile) const;
  void RecordWriteFieldChar(mirror::Object* obj, MemberOffset field_offset, uint16_t value,
                            bool is_volatile) const;
  void RecordWriteFieldShort(mirror::Object* obj, MemberOffset field_offset, int16_t value,
                          bool is_volatile) const;
  void RecordWriteField32(mirror::Object* obj, MemberOffset field_offset, uint32_t value,
                          bool is_volatile) const;
  void RecordWriteField64(mirror::Object* obj, MemberOffset field_offset, uint64_t value,
                          bool is_volatile) const;
  void RecordWriteFieldReference(mirror::Object* obj,
                                 MemberOffset field_offset,
                                 ObjPtr<mirror::Object> value,
                                 bool is_volatile) const
      REQUIRES_SHARED(Locks::mutator_lock_);
  void RecordWriteArray(mirror::Array* array, size_t index, uint64_t value) const
      REQUIRES_SHARED(Locks::mutator_lock_);
  void RecordStrongStringInsertion(ObjPtr<mirror::String> s) const
      REQUIRES(Locks::intern_table_lock_);
  void RecordWeakStringInsertion(ObjPtr<mirror::String> s) const
      REQUIRES(Locks::intern_table_lock_);
  void RecordStrongStringRemoval(ObjPtr<mirror::String> s) const
      REQUIRES(Locks::intern_table_lock_);
  void RecordWeakStringRemoval(ObjPtr<mirror::String> s) const
      REQUIRES(Locks::intern_table_lock_);
  void RecordResolveString(ObjPtr<mirror::DexCache> dex_cache, dex::StringIndex string_idx) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  void SetFaultMessage(const std::string& message);

  void AddCurrentRuntimeFeaturesAsDex2OatArguments(std::vector<std::string>* arg_vector) const;

  bool ExplicitStackOverflowChecks() const {
    return !implicit_so_checks_;
  }

  void DisableVerifier();
  bool IsVerificationEnabled() const;
  bool IsVerificationSoftFail() const;

  void SetHiddenApiEnforcementPolicy(hiddenapi::EnforcementPolicy policy) {
    hidden_api_policy_ = policy;
  }

  hiddenapi::EnforcementPolicy GetHiddenApiEnforcementPolicy() const {
    return hidden_api_policy_;
  }

  void SetCorePlatformApiEnforcementPolicy(hiddenapi::EnforcementPolicy policy) {
    core_platform_api_policy_ = policy;
  }

  hiddenapi::EnforcementPolicy GetCorePlatformApiEnforcementPolicy() const {
    return core_platform_api_policy_;
  }

  void SetTestApiEnforcementPolicy(hiddenapi::EnforcementPolicy policy) {
    test_api_policy_ = policy;
  }

  hiddenapi::EnforcementPolicy GetTestApiEnforcementPolicy() const {
    return test_api_policy_;
  }

  void SetHiddenApiExemptions(const std::vector<std::string>& exemptions) {
    hidden_api_exemptions_ = exemptions;
  }

  const std::vector<std::string>& GetHiddenApiExemptions() {
    return hidden_api_exemptions_;
  }

  void SetDedupeHiddenApiWarnings(bool value) {
    dedupe_hidden_api_warnings_ = value;
  }

  bool ShouldDedupeHiddenApiWarnings() {
    return dedupe_hidden_api_warnings_;
  }

  void SetHiddenApiEventLogSampleRate(uint32_t rate) {
    hidden_api_access_event_log_rate_ = rate;
  }

  uint32_t GetHiddenApiEventLogSampleRate() const {
    return hidden_api_access_event_log_rate_;
  }

  const std::string& GetProcessPackageName() const {
    return process_package_name_;
  }

  void SetProcessPackageName(const char* package_name) {
    if (package_name == nullptr) {
      process_package_name_.clear();
    } else {
      process_package_name_ = package_name;
    }
  }

  const std::string& GetProcessDataDirectory() const {
    return process_data_directory_;
  }

  void SetProcessDataDirectory(const char* data_dir) {
    if (data_dir == nullptr) {
      process_data_directory_.clear();
    } else {
      process_data_directory_ = data_dir;
    }
  }

  bool IsDexFileFallbackEnabled() const {
    return allow_dex_file_fallback_;
  }

  const std::vector<std::string>& GetCpuAbilist() const {
    return cpu_abilist_;
  }

  bool IsRunningOnMemoryTool() const {
    return is_running_on_memory_tool_;
  }

  void SetTargetSdkVersion(uint32_t version) {
    target_sdk_version_ = version;
  }

  uint32_t GetTargetSdkVersion() const {
    return target_sdk_version_;
  }

  void SetDisabledCompatChanges(const std::set<uint64_t>& disabled_changes) {
    disabled_compat_changes_ = disabled_changes;
  }

  std::set<uint64_t> GetDisabledCompatChanges() const {
    return disabled_compat_changes_;
  }

  bool isChangeEnabled(uint64_t change_id) const {
    // TODO(145743810): add an up call to java to log to statsd
    return disabled_compat_changes_.count(change_id) == 0;
  }

  uint32_t GetZygoteMaxFailedBoots() const {
    return zygote_max_failed_boots_;
  }

  bool AreExperimentalFlagsEnabled(ExperimentalFlags flags) {
    return (experimental_flags_ & flags) != ExperimentalFlags::kNone;
  }

  void CreateJitCodeCache(bool rwx_memory_allowed);

  // Create the JIT and instrumentation and code cache.
  void CreateJit();

  ArenaPool* GetArenaPool() {
    return arena_pool_.get();
  }
  ArenaPool* GetJitArenaPool() {
    return jit_arena_pool_.get();
  }
  const ArenaPool* GetArenaPool() const {
    return arena_pool_.get();
  }

  void ReclaimArenaPoolMemory();

  LinearAlloc* GetLinearAlloc() {
    return linear_alloc_.get();
  }

  jit::JitOptions* GetJITOptions() {
    return jit_options_.get();
  }

  bool IsJavaDebuggable() const {
    return is_java_debuggable_;
  }

  void SetProfileableFromShell(bool value) {
    is_profileable_from_shell_ = value;
  }

  bool IsProfileableFromShell() const {
    return is_profileable_from_shell_;
  }

  void SetJavaDebuggable(bool value);

  // Deoptimize the boot image, called for Java debuggable apps.
  void DeoptimizeBootImage() REQUIRES(Locks::mutator_lock_);

  bool IsNativeDebuggable() const {
    return is_native_debuggable_;
  }

  void SetNativeDebuggable(bool value) {
    is_native_debuggable_ = value;
  }

  void SetSignalHookDebuggable(bool value);

  bool AreNonStandardExitsEnabled() const {
    return non_standard_exits_enabled_;
  }

  void SetNonStandardExitsEnabled() {
    DoAndMaybeSwitchInterpreter([=](){ non_standard_exits_enabled_ = true; });
  }

  bool AreAsyncExceptionsThrown() const {
    return async_exceptions_thrown_;
  }

  void SetAsyncExceptionsThrown() {
    DoAndMaybeSwitchInterpreter([=](){ async_exceptions_thrown_ = true; });
  }

  // Change state and re-check which interpreter should be used.
  //
  // This must be called whenever there is an event that forces
  // us to use different interpreter (e.g. debugger is attached).
  //
  // Changing the state using the lamda gives us some multihreading safety.
  // It ensures that two calls do not interfere with each other and
  // it makes it possible to DCHECK that thread local flag is correct.
  template<typename Action>
  static void DoAndMaybeSwitchInterpreter(Action lamda);

  // Returns the build fingerprint, if set. Otherwise an empty string is returned.
  std::string GetFingerprint() {
    return fingerprint_;
  }

  // Called from class linker.
  void SetSentinel(ObjPtr<mirror::Object> sentinel) REQUIRES_SHARED(Locks::mutator_lock_);
  // For testing purpose only.
  // TODO: Remove this when this is no longer needed (b/116087961).
  GcRoot<mirror::Object> GetSentinel() REQUIRES_SHARED(Locks::mutator_lock_);


  // Use a sentinel for marking entries in a table that have been cleared.
  // This helps diagnosing in case code tries to wrongly access such
  // entries.
  static mirror::Class* GetWeakClassSentinel() {
    return reinterpret_cast<mirror::Class*>(0xebadbeef);
  }

  // Helper for the GC to process a weak class in a table.
  static void ProcessWeakClass(GcRoot<mirror::Class>* root_ptr,
                               IsMarkedVisitor* visitor,
                               mirror::Class* update)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Create a normal LinearAlloc or low 4gb version if we are 64 bit AOT compiler.
  LinearAlloc* CreateLinearAlloc();

  OatFileManager& GetOatFileManager() const {
    DCHECK(oat_file_manager_ != nullptr);
    return *oat_file_manager_;
  }

  double GetHashTableMinLoadFactor() const;
  double GetHashTableMaxLoadFactor() const;

  bool IsSafeMode() const {
    return safe_mode_;
  }

  void SetSafeMode(bool mode) {
    safe_mode_ = mode;
  }

  bool GetDumpNativeStackOnSigQuit() const {
    return dump_native_stack_on_sig_quit_;
  }

  bool GetPrunedDalvikCache() const {
    return pruned_dalvik_cache_;
  }

  void SetPrunedDalvikCache(bool pruned) {
    pruned_dalvik_cache_ = pruned;
  }

  void UpdateProcessState(ProcessState process_state);

  // Returns true if we currently care about long mutator pause.
  bool InJankPerceptibleProcessState() const {
    return process_state_ == kProcessStateJankPerceptible;
  }

  void RegisterSensitiveThread() const;

  void SetZygoteNoThreadSection(bool val) {
    zygote_no_threads_ = val;
  }

  bool IsZygoteNoThreadSection() const {
    return zygote_no_threads_;
  }

  // Returns if the code can be deoptimized asynchronously. Code may be compiled with some
  // optimization that makes it impossible to deoptimize.
  bool IsAsyncDeoptimizeable(uintptr_t code) const REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns a saved copy of the environment (getenv/setenv values).
  // Used by Fork to protect against overwriting LD_LIBRARY_PATH, etc.
  char** GetEnvSnapshot() const {
    return env_snapshot_.GetSnapshot();
  }

  void AddSystemWeakHolder(gc::AbstractSystemWeakHolder* holder);
  void RemoveSystemWeakHolder(gc::AbstractSystemWeakHolder* holder);

  void AttachAgent(JNIEnv* env, const std::string& agent_arg, jobject class_loader);

  const std::list<std::unique_ptr<ti::Agent>>& GetAgents() const {
    return agents_;
  }

  RuntimeCallbacks* GetRuntimeCallbacks();

  bool HasLoadedPlugins() const {
    return !plugins_.empty();
  }

  void InitThreadGroups(Thread* self);

  void SetDumpGCPerformanceOnShutdown(bool value) {
    dump_gc_performance_on_shutdown_ = value;
  }

  bool GetDumpGCPerformanceOnShutdown() const {
    return dump_gc_performance_on_shutdown_;
  }

  void IncrementDeoptimizationCount(DeoptimizationKind kind) {
    DCHECK_LE(kind, DeoptimizationKind::kLast);
    deoptimization_counts_[static_cast<size_t>(kind)]++;
  }

  uint32_t GetNumberOfDeoptimizations() const {
    uint32_t result = 0;
    for (size_t i = 0; i <= static_cast<size_t>(DeoptimizationKind::kLast); ++i) {
      result += deoptimization_counts_[i];
    }
    return result;
  }

  // Whether or not we use MADV_RANDOM on files that are thought to have random access patterns.
  // This is beneficial for low RAM devices since it reduces page cache thrashing.
  bool MAdviseRandomAccess() const {
    return madvise_random_access_;
  }

  const std::string& GetJdwpOptions() {
    return jdwp_options_;
  }

  JdwpProvider GetJdwpProvider() const {
    return jdwp_provider_;
  }

  JniIdType GetJniIdType() const {
    return jni_ids_indirection_;
  }

  bool CanSetJniIdType() const {
    return GetJniIdType() == JniIdType::kSwapablePointer;
  }

  // Changes the JniIdType to the given type. Only allowed if CanSetJniIdType(). All threads must be
  // suspended to call this function.
  void SetJniIdType(JniIdType t);

  uint32_t GetVerifierLoggingThresholdMs() const {
    return verifier_logging_threshold_ms_;
  }

  // Atomically delete the thread pool if the reference count is 0.
  bool DeleteThreadPool() REQUIRES(!Locks::runtime_thread_pool_lock_);

  // Wait for all the thread workers to be attached.
  void WaitForThreadPoolWorkersToStart() REQUIRES(!Locks::runtime_thread_pool_lock_);

  // Scoped usage of the runtime thread pool. Prevents the pool from being
  // deleted. Note that the thread pool is only for startup and gets deleted after.
  class ScopedThreadPoolUsage {
   public:
    ScopedThreadPoolUsage();
    ~ScopedThreadPoolUsage();

    // Return the thread pool.
    ThreadPool* GetThreadPool() const {
      return thread_pool_;
    }

   private:
    ThreadPool* const thread_pool_;
  };

  bool LoadAppImageStartupCache() const {
    return load_app_image_startup_cache_;
  }

  void SetLoadAppImageStartupCacheEnabled(bool enabled) {
    load_app_image_startup_cache_ = enabled;
  }

  // Reset the startup completed status so that we can call NotifyStartupCompleted again. Should
  // only be used for testing.
  void ResetStartupCompleted();

  // Notify the runtime that application startup is considered completed. Only has effect for the
  // first call.
  void NotifyStartupCompleted();

  // Return true if startup is already completed.
  bool GetStartupCompleted() const;

  gc::space::ImageSpaceLoadingOrder GetImageSpaceLoadingOrder() const {
    return image_space_loading_order_;
  }

  bool IsVerifierMissingKThrowFatal() const {
    return verifier_missing_kthrow_fatal_;
  }

  bool IsPerfettoHprofEnabled() const {
    return perfetto_hprof_enabled_;
  }

  // Return true if we should load oat files as executable or not.
  bool GetOatFilesExecutable() const;

 private:
  static void InitPlatformSignalHandlers();

  Runtime();

  void BlockSignals();

  bool Init(RuntimeArgumentMap&& runtime_options)
      SHARED_TRYLOCK_FUNCTION(true, Locks::mutator_lock_);
  void InitNativeMethods() REQUIRES(!Locks::mutator_lock_);
  void RegisterRuntimeNativeMethods(JNIEnv* env);

  void StartDaemonThreads();
  void StartSignalCatcher();

  void MaybeSaveJitProfilingInfo();

  // Visit all of the thread roots.
  void VisitThreadRoots(RootVisitor* visitor, VisitRootFlags flags)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Visit all other roots which must be done with mutators suspended.
  void VisitNonConcurrentRoots(RootVisitor* visitor, VisitRootFlags flags)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Constant roots are the roots which never change after the runtime is initialized, they only
  // need to be visited once per GC cycle.
  void VisitConstantRoots(RootVisitor* visitor)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Note: To be lock-free, GetFaultMessage temporarily replaces the lock message with null.
  //       As such, there is a window where a call will return an empty string. In general,
  //       only aborting code should retrieve this data (via GetFaultMessageForAbortLogging
  //       friend).
  std::string GetFaultMessage();

  ThreadPool* AcquireThreadPool() REQUIRES(!Locks::runtime_thread_pool_lock_);
  void ReleaseThreadPool() REQUIRES(!Locks::runtime_thread_pool_lock_);

  // A pointer to the active runtime or null.
  static Runtime* instance_;

  // NOTE: these must match the gc::ProcessState values as they come directly from the framework.
  static constexpr int kProfileForground = 0;
  static constexpr int kProfileBackground = 1;

  static constexpr uint32_t kCalleeSaveSize = 6u;

  // 64 bit so that we can share the same asm offsets for both 32 and 64 bits.
  uint64_t callee_save_methods_[kCalleeSaveSize];
  // Pre-allocated exceptions (see Runtime::Init).
  GcRoot<mirror::Throwable> pre_allocated_OutOfMemoryError_when_throwing_exception_;
  GcRoot<mirror::Throwable> pre_allocated_OutOfMemoryError_when_throwing_oome_;
  GcRoot<mirror::Throwable> pre_allocated_OutOfMemoryError_when_handling_stack_overflow_;
  GcRoot<mirror::Throwable> pre_allocated_NoClassDefFoundError_;
  ArtMethod* resolution_method_;
  ArtMethod* imt_conflict_method_;
  // Unresolved method has the same behavior as the conflict method, it is used by the class linker
  // for differentiating between unfilled imt slots vs conflict slots in superclasses.
  ArtMethod* imt_unimplemented_method_;

  // Special sentinel object used to invalid conditions in JNI (cleared weak references) and
  // JDWP (invalid references).
  GcRoot<mirror::Object> sentinel_;

  InstructionSet instruction_set_;

  CompilerCallbacks* compiler_callbacks_;
  bool is_zygote_;
  bool is_primary_zygote_;
  bool is_system_server_;
  bool must_relocate_;
  bool is_concurrent_gc_enabled_;
  bool is_explicit_gc_disabled_;
  bool image_dex2oat_enabled_;

  std::string compiler_executable_;
  std::vector<std::string> compiler_options_;
  std::vector<std::string> image_compiler_options_;
  std::string image_location_;

  std::vector<std::string> boot_class_path_;
  std::vector<std::string> boot_class_path_locations_;
  std::string class_path_string_;
  std::vector<std::string> properties_;

  std::list<ti::AgentSpec> agent_specs_;
  std::list<std::unique_ptr<ti::Agent>> agents_;
  std::vector<Plugin> plugins_;

  // The default stack size for managed threads created by the runtime.
  size_t default_stack_size_;

  // Finalizers running for longer than this many milliseconds abort the runtime.
  unsigned int finalizer_timeout_ms_;

  gc::Heap* heap_;

  std::unique_ptr<ArenaPool> jit_arena_pool_;
  std::unique_ptr<ArenaPool> arena_pool_;
  // Special low 4gb pool for compiler linear alloc. We need ArtFields to be in low 4gb if we are
  // compiling using a 32 bit image on a 64 bit compiler in case we resolve things in the image
  // since the field arrays are int arrays in this case.
  std::unique_ptr<ArenaPool> low_4gb_arena_pool_;

  // Shared linear alloc for now.
  std::unique_ptr<LinearAlloc> linear_alloc_;

  // The number of spins that are done before thread suspension is used to forcibly inflate.
  size_t max_spins_before_thin_lock_inflation_;
  MonitorList* monitor_list_;
  MonitorPool* monitor_pool_;

  ThreadList* thread_list_;

  InternTable* intern_table_;

  ClassLinker* class_linker_;

  SignalCatcher* signal_catcher_;

  std::unique_ptr<jni::JniIdManager> jni_id_manager_;

  std::unique_ptr<JavaVMExt> java_vm_;

  std::unique_ptr<jit::Jit> jit_;
  std::unique_ptr<jit::JitCodeCache> jit_code_cache_;
  std::unique_ptr<jit::JitOptions> jit_options_;

  // Runtime thread pool. The pool is only for startup and gets deleted after.
  std::unique_ptr<ThreadPool> thread_pool_ GUARDED_BY(Locks::runtime_thread_pool_lock_);
  size_t thread_pool_ref_count_ GUARDED_BY(Locks::runtime_thread_pool_lock_);

  // Fault message, printed when we get a SIGSEGV. Stored as a native-heap object and accessed
  // lock-free, so needs to be atomic.
  std::atomic<std::string*> fault_message_;

  // A non-zero value indicates that a thread has been created but not yet initialized. Guarded by
  // the shutdown lock so that threads aren't born while we're shutting down.
  size_t threads_being_born_ GUARDED_BY(Locks::runtime_shutdown_lock_);

  // Waited upon until no threads are being born.
  std::unique_ptr<ConditionVariable> shutdown_cond_ GUARDED_BY(Locks::runtime_shutdown_lock_);

  // Set when runtime shutdown is past the point that new threads may attach.
  bool shutting_down_ GUARDED_BY(Locks::runtime_shutdown_lock_);

  // The runtime is starting to shutdown but is blocked waiting on shutdown_cond_.
  bool shutting_down_started_ GUARDED_BY(Locks::runtime_shutdown_lock_);

  bool started_;

  // New flag added which tells us if the runtime has finished starting. If
  // this flag is set then the Daemon threads are created and the class loader
  // is created. This flag is needed for knowing if its safe to request CMS.
  bool finished_starting_;

  // Hooks supported by JNI_CreateJavaVM
  jint (*vfprintf_)(FILE* stream, const char* format, va_list ap);
  void (*exit_)(jint status);
  void (*abort_)();

  bool stats_enabled_;
  RuntimeStats stats_;

  const bool is_running_on_memory_tool_;

  std::unique_ptr<TraceConfig> trace_config_;

  instrumentation::Instrumentation instrumentation_;

  jobject main_thread_group_;
  jobject system_thread_group_;

  // As returned by ClassLoader.getSystemClassLoader().
  jobject system_class_loader_;

  // If true, then we dump the GC cumulative timings on shutdown.
  bool dump_gc_performance_on_shutdown_;

  // Transactions used for pre-initializing classes at compilation time.
  // Support nested transactions, maintain a list containing all transactions. Transactions are
  // handled under a stack discipline. Because GC needs to go over all transactions, we choose list
  // as substantial data structure instead of stack.
  std::list<std::unique_ptr<Transaction>> preinitialization_transactions_;

  // If kNone, verification is disabled. kEnable by default.
  verifier::VerifyMode verify_;

  // If true, the runtime may use dex files directly with the interpreter if an oat file is not
  // available/usable.
  bool allow_dex_file_fallback_;

  // List of supported cpu abis.
  std::vector<std::string> cpu_abilist_;

  // Specifies target SDK version to allow workarounds for certain API levels.
  uint32_t target_sdk_version_;

  // A set of disabled compat changes for the running app, all other changes are enabled.
  std::set<uint64_t> disabled_compat_changes_;

  // Implicit checks flags.
  bool implicit_null_checks_;       // NullPointer checks are implicit.
  bool implicit_so_checks_;         // StackOverflow checks are implicit.
  bool implicit_suspend_checks_;    // Thread suspension checks are implicit.

  // Whether or not the sig chain (and implicitly the fault handler) should be
  // disabled. Tools like dex2oat don't need them. This enables
  // building a statically link version of dex2oat.
  bool no_sig_chain_;

  // Force the use of native bridge even if the app ISA matches the runtime ISA.
  bool force_native_bridge_;

  // Whether or not a native bridge has been loaded.
  //
  // The native bridge allows running native code compiled for a foreign ISA. The way it works is,
  // if standard dlopen fails to load native library associated with native activity, it calls to
  // the native bridge to load it and then gets the trampoline for the entry to native activity.
  //
  // The option 'native_bridge_library_filename' specifies the name of the native bridge.
  // When non-empty the native bridge will be loaded from the given file. An empty value means
  // that there's no native bridge.
  bool is_native_bridge_loaded_;

  // Whether we are running under native debugger.
  bool is_native_debuggable_;

  // whether or not any async exceptions have ever been thrown. This is used to speed up the
  // MterpShouldSwitchInterpreters function.
  bool async_exceptions_thrown_;

  // Whether anything is going to be using the shadow-frame APIs to force a function to return
  // early. Doing this requires that (1) we be debuggable and (2) that mterp is exited.
  bool non_standard_exits_enabled_;

  // Whether Java code needs to be debuggable.
  bool is_java_debuggable_;

  bool is_profileable_from_shell_ = false;

  // The maximum number of failed boots we allow before pruning the dalvik cache
  // and trying again. This option is only inspected when we're running as a
  // zygote.
  uint32_t zygote_max_failed_boots_;

  // Enable experimental opcodes that aren't fully specified yet. The intent is to
  // eventually publish them as public-usable opcodes, but they aren't ready yet.
  //
  // Experimental opcodes should not be used by other production code.
  ExperimentalFlags experimental_flags_;

  // Contains the build fingerprint, if given as a parameter.
  std::string fingerprint_;

  // Oat file manager, keeps track of what oat files are open.
  OatFileManager* oat_file_manager_;

  // Whether or not we are on a low RAM device.
  bool is_low_memory_mode_;

  // Whether or not we use MADV_RANDOM on files that are thought to have random access patterns.
  // This is beneficial for low RAM devices since it reduces page cache thrashing.
  bool madvise_random_access_;

  // Whether the application should run in safe mode, that is, interpreter only.
  bool safe_mode_;

  // Whether access checks on hidden API should be performed.
  hiddenapi::EnforcementPolicy hidden_api_policy_;

  // Whether access checks on core platform API should be performed.
  hiddenapi::EnforcementPolicy core_platform_api_policy_;

  // Whether access checks on test API should be performed.
  hiddenapi::EnforcementPolicy test_api_policy_;

  // List of signature prefixes of methods that have been removed from the blacklist, and treated
  // as if whitelisted.
  std::vector<std::string> hidden_api_exemptions_;

  // Do not warn about the same hidden API access violation twice.
  // This is only used for testing.
  bool dedupe_hidden_api_warnings_;

  // How often to log hidden API access to the event log. An integer between 0
  // (never) and 0x10000 (always).
  uint32_t hidden_api_access_event_log_rate_;

  // The package of the app running in this process.
  std::string process_package_name_;

  // The data directory of the app running in this process.
  std::string process_data_directory_;

  // Whether threads should dump their native stack on SIGQUIT.
  bool dump_native_stack_on_sig_quit_;

  // Whether the dalvik cache was pruned when initializing the runtime.
  bool pruned_dalvik_cache_;

  // Whether or not we currently care about pause times.
  ProcessState process_state_;

  // Whether zygote code is in a section that should not start threads.
  bool zygote_no_threads_;

  // The string containing requested jdwp options
  std::string jdwp_options_;

  // The jdwp provider we were configured with.
  JdwpProvider jdwp_provider_;

  // True if jmethodID and jfieldID are opaque Indices. When false (the default) these are simply
  // pointers. This is set by -Xopaque-jni-ids:{true,false}.
  JniIdType jni_ids_indirection_;

  // Set to false in cases where we want to directly control when jni-id
  // indirection is changed. This is intended only for testing JNI id swapping.
  bool automatically_set_jni_ids_indirection_;

  // Saved environment.
  class EnvSnapshot {
   public:
    EnvSnapshot() = default;
    void TakeSnapshot();
    char** GetSnapshot() const;

   private:
    std::unique_ptr<char*[]> c_env_vector_;
    std::vector<std::unique_ptr<std::string>> name_value_pairs_;

    DISALLOW_COPY_AND_ASSIGN(EnvSnapshot);
  } env_snapshot_;

  // Generic system-weak holders.
  std::vector<gc::AbstractSystemWeakHolder*> system_weak_holders_;

  std::unique_ptr<RuntimeCallbacks> callbacks_;

  std::atomic<uint32_t> deoptimization_counts_[
      static_cast<uint32_t>(DeoptimizationKind::kLast) + 1];

  MemMap protected_fault_page_;

  uint32_t verifier_logging_threshold_ms_;

  bool load_app_image_startup_cache_ = false;

  // If startup has completed, must happen at most once.
  std::atomic<bool> startup_completed_ = false;

  gc::space::ImageSpaceLoadingOrder image_space_loading_order_ =
      gc::space::ImageSpaceLoadingOrder::kSystemFirst;

  bool verifier_missing_kthrow_fatal_;
  bool perfetto_hprof_enabled_;

  // Note: See comments on GetFaultMessage.
  friend std::string GetFaultMessageForAbortLogging();
  friend class Dex2oatImageTest;
  friend class ScopedThreadPoolUsage;
  friend class OatFileAssistantTest;
  class NotifyStartupCompletedTask;

  DISALLOW_COPY_AND_ASSIGN(Runtime);
};

}  // namespace art

#endif  // ART_RUNTIME_RUNTIME_H_
