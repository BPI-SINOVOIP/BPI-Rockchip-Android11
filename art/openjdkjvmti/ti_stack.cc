/* Copyright (C) 2016 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "ti_stack.h"

#include <algorithm>
#include <initializer_list>
#include <list>
#include <unordered_map>
#include <vector>

#include "android-base/macros.h"
#include "android-base/thread_annotations.h"
#include "arch/context.h"
#include "art_field-inl.h"
#include "art_method-inl.h"
#include "art_jvmti.h"
#include "art_method-inl.h"
#include "barrier.h"
#include "base/bit_utils.h"
#include "base/enums.h"
#include "base/locks.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "deopt_manager.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file.h"
#include "dex/dex_file_annotations.h"
#include "dex/dex_file_types.h"
#include "dex/dex_instruction-inl.h"
#include "dex/primitive.h"
#include "events.h"
#include "gc_root.h"
#include "handle_scope-inl.h"
#include "instrumentation.h"
#include "interpreter/shadow_frame-inl.h"
#include "interpreter/shadow_frame.h"
#include "jni/jni_env_ext.h"
#include "jni/jni_internal.h"
#include "jvalue-inl.h"
#include "jvalue.h"
#include "jvmti.h"
#include "mirror/class.h"
#include "mirror/dex_cache.h"
#include "nativehelper/scoped_local_ref.h"
#include "scoped_thread_state_change-inl.h"
#include "scoped_thread_state_change.h"
#include "stack.h"
#include "thread.h"
#include "thread_state.h"
#include "ti_logging.h"
#include "ti_thread.h"
#include "thread-current-inl.h"
#include "thread_list.h"
#include "thread_pool.h"
#include "ti_thread.h"
#include "well_known_classes.h"

namespace openjdkjvmti {

template <typename FrameFn>
struct GetStackTraceVisitor : public art::StackVisitor {
  GetStackTraceVisitor(art::Thread* thread_in,
                       size_t start_,
                       size_t stop_,
                       FrameFn fn_)
      : StackVisitor(thread_in, nullptr, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        fn(fn_),
        start(start_),
        stop(stop_) {}
  GetStackTraceVisitor(const GetStackTraceVisitor&) = default;
  GetStackTraceVisitor(GetStackTraceVisitor&&) noexcept = default;

  bool VisitFrame() override REQUIRES_SHARED(art::Locks::mutator_lock_) {
    art::ArtMethod* m = GetMethod();
    if (m->IsRuntimeMethod()) {
      return true;
    }

    if (start == 0) {
      m = m->GetInterfaceMethodIfProxy(art::kRuntimePointerSize);
      jmethodID id = art::jni::EncodeArtMethod(m);

      uint32_t dex_pc = GetDexPc(false);
      jlong dex_location = (dex_pc == art::dex::kDexNoIndex) ? -1 : static_cast<jlong>(dex_pc);

      jvmtiFrameInfo info = { id, dex_location };
      fn(info);

      if (stop == 1) {
        return false;  // We're done.
      } else if (stop > 0) {
        stop--;
      }
    } else {
      start--;
    }

    return true;
  }

  FrameFn fn;
  size_t start;
  size_t stop;
};

art::ShadowFrame* FindFrameAtDepthVisitor::GetOrCreateShadowFrame(bool* created_frame) {
  art::ShadowFrame* cur = GetCurrentShadowFrame();
  if (cur == nullptr) {
    *created_frame = true;
    art::ArtMethod* method = GetMethod();
    const uint16_t num_regs = method->DexInstructionData().RegistersSize();
    cur = GetThread()->FindOrCreateDebuggerShadowFrame(GetFrameId(),
                                                       num_regs,
                                                       method,
                                                       GetDexPc());
    DCHECK(cur != nullptr);
  } else {
    *created_frame = false;
  }
  return cur;
}

template <typename FrameFn>
GetStackTraceVisitor<FrameFn> MakeStackTraceVisitor(art::Thread* thread_in,
                                                    size_t start,
                                                    size_t stop,
                                                    FrameFn fn) {
  return GetStackTraceVisitor<FrameFn>(thread_in, start, stop, fn);
}

struct GetStackTraceVectorClosure : public art::Closure {
 public:
  GetStackTraceVectorClosure(size_t start, size_t stop)
      : start_input(start),
        stop_input(stop),
        start_result(0),
        stop_result(0) {}

  void Run(art::Thread* self) override REQUIRES_SHARED(art::Locks::mutator_lock_) {
    auto frames_fn = [&](jvmtiFrameInfo info) {
      frames.push_back(info);
    };
    auto visitor = MakeStackTraceVisitor(self, start_input, stop_input, frames_fn);
    visitor.WalkStack(/* include_transitions= */ false);

    start_result = visitor.start;
    stop_result = visitor.stop;
  }

  const size_t start_input;
  const size_t stop_input;

  std::vector<jvmtiFrameInfo> frames;
  size_t start_result;
  size_t stop_result;
};

static jvmtiError TranslateFrameVector(const std::vector<jvmtiFrameInfo>& frames,
                                       jint start_depth,
                                       size_t start_result,
                                       jint max_frame_count,
                                       jvmtiFrameInfo* frame_buffer,
                                       jint* count_ptr) {
  size_t collected_frames = frames.size();

  // Assume we're here having collected something.
  DCHECK_GT(max_frame_count, 0);

  // Frames from the top.
  if (start_depth >= 0) {
    if (start_result != 0) {
      // Not enough frames.
      return ERR(ILLEGAL_ARGUMENT);
    }
    DCHECK_LE(collected_frames, static_cast<size_t>(max_frame_count));
    if (frames.size() > 0) {
      memcpy(frame_buffer, frames.data(), collected_frames * sizeof(jvmtiFrameInfo));
    }
    *count_ptr = static_cast<jint>(frames.size());
    return ERR(NONE);
  }

  // Frames from the bottom.
  if (collected_frames < static_cast<size_t>(-start_depth)) {
    return ERR(ILLEGAL_ARGUMENT);
  }

  size_t count = std::min(static_cast<size_t>(-start_depth), static_cast<size_t>(max_frame_count));
  memcpy(frame_buffer,
         &frames.data()[collected_frames + start_depth],
         count * sizeof(jvmtiFrameInfo));
  *count_ptr = static_cast<jint>(count);
  return ERR(NONE);
}

struct GetStackTraceDirectClosure : public art::Closure {
 public:
  GetStackTraceDirectClosure(jvmtiFrameInfo* frame_buffer_, size_t start, size_t stop)
      : frame_buffer(frame_buffer_),
        start_input(start),
        stop_input(stop),
        index(0) {
    DCHECK_GE(start_input, 0u);
  }

  void Run(art::Thread* self) override REQUIRES_SHARED(art::Locks::mutator_lock_) {
    auto frames_fn = [&](jvmtiFrameInfo info) {
      frame_buffer[index] = info;
      ++index;
    };
    auto visitor = MakeStackTraceVisitor(self, start_input, stop_input, frames_fn);
    visitor.WalkStack(/* include_transitions= */ false);
  }

  jvmtiFrameInfo* frame_buffer;

  const size_t start_input;
  const size_t stop_input;

  size_t index = 0;
};

jvmtiError StackUtil::GetStackTrace(jvmtiEnv* jvmti_env,
                                    jthread java_thread,
                                    jint start_depth,
                                    jint max_frame_count,
                                    jvmtiFrameInfo* frame_buffer,
                                    jint* count_ptr) {
  // It is not great that we have to hold these locks for so long, but it is necessary to ensure
  // that the thread isn't dying on us.
  art::ScopedObjectAccess soa(art::Thread::Current());
  art::Locks::thread_list_lock_->ExclusiveLock(soa.Self());

  art::Thread* thread;
  jvmtiError thread_error = ERR(INTERNAL);
  if (!ThreadUtil::GetAliveNativeThread(java_thread, soa, &thread, &thread_error)) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(soa.Self());
    return thread_error;
  }
  DCHECK(thread != nullptr);

  art::ThreadState state = thread->GetState();
  if (state == art::ThreadState::kStarting || thread->IsStillStarting()) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(soa.Self());
    return ERR(THREAD_NOT_ALIVE);
  }

  if (max_frame_count < 0) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(soa.Self());
    return ERR(ILLEGAL_ARGUMENT);
  }
  if (frame_buffer == nullptr || count_ptr == nullptr) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(soa.Self());
    return ERR(NULL_POINTER);
  }

  if (max_frame_count == 0) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(soa.Self());
    *count_ptr = 0;
    return ERR(NONE);
  }

  if (start_depth >= 0) {
    // Fast path: Regular order of stack trace. Fill into the frame_buffer directly.
    GetStackTraceDirectClosure closure(frame_buffer,
                                       static_cast<size_t>(start_depth),
                                       static_cast<size_t>(max_frame_count));
    // RequestSynchronousCheckpoint releases the thread_list_lock_ as a part of its execution.
    if (!thread->RequestSynchronousCheckpoint(&closure)) {
      return ERR(THREAD_NOT_ALIVE);
    }
    *count_ptr = static_cast<jint>(closure.index);
    if (closure.index == 0) {
      JVMTI_LOG(INFO, jvmti_env) << "The stack is not large enough for a start_depth of "
                                 << start_depth << ".";
      return ERR(ILLEGAL_ARGUMENT);
    }
    return ERR(NONE);
  } else {
    GetStackTraceVectorClosure closure(0, 0);
    // RequestSynchronousCheckpoint releases the thread_list_lock_ as a part of its execution.
    if (!thread->RequestSynchronousCheckpoint(&closure)) {
      return ERR(THREAD_NOT_ALIVE);
    }

    return TranslateFrameVector(closure.frames,
                                start_depth,
                                closure.start_result,
                                max_frame_count,
                                frame_buffer,
                                count_ptr);
  }
}

template <typename Data>
struct GetAllStackTracesVectorClosure : public art::Closure {
  GetAllStackTracesVectorClosure(size_t stop, Data* data_)
      : barrier(0), stop_input(stop), data(data_) {}

  void Run(art::Thread* thread) override
      REQUIRES_SHARED(art::Locks::mutator_lock_)
      REQUIRES(!data->mutex) {
    art::Thread* self = art::Thread::Current();
    Work(thread, self);
    barrier.Pass(self);
  }

  void Work(art::Thread* thread, art::Thread* self)
      REQUIRES_SHARED(art::Locks::mutator_lock_)
      REQUIRES(!data->mutex) {
    // Skip threads that are still starting.
    if (thread->IsStillStarting()) {
      return;
    }

    std::vector<jvmtiFrameInfo>* thread_frames = data->GetFrameStorageFor(self, thread);
    if (thread_frames == nullptr) {
      return;
    }

    // Now collect the data.
    auto frames_fn = [&](jvmtiFrameInfo info) {
      thread_frames->push_back(info);
    };
    auto visitor = MakeStackTraceVisitor(thread, 0u, stop_input, frames_fn);
    visitor.WalkStack(/* include_transitions= */ false);
  }

  art::Barrier barrier;
  const size_t stop_input;
  Data* data;
};

template <typename Data>
static void RunCheckpointAndWait(Data* data, size_t max_frame_count)
    REQUIRES_SHARED(art::Locks::mutator_lock_) {
  // Note: requires the mutator lock as the checkpoint requires the mutator lock.
  GetAllStackTracesVectorClosure<Data> closure(max_frame_count, data);
  size_t barrier_count = art::Runtime::Current()->GetThreadList()->RunCheckpoint(&closure, nullptr);
  if (barrier_count == 0) {
    return;
  }
  art::Thread* self = art::Thread::Current();
  art::ScopedThreadStateChange tsc(self, art::ThreadState::kWaitingForCheckPointsToRun);
  closure.barrier.Increment(self, barrier_count);
}

jvmtiError StackUtil::GetAllStackTraces(jvmtiEnv* env,
                                        jint max_frame_count,
                                        jvmtiStackInfo** stack_info_ptr,
                                        jint* thread_count_ptr) {
  if (max_frame_count < 0) {
    return ERR(ILLEGAL_ARGUMENT);
  }
  if (stack_info_ptr == nullptr || thread_count_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  struct AllStackTracesData {
    AllStackTracesData() : mutex("GetAllStackTraces", art::LockLevel::kAbortLock) {}
    ~AllStackTracesData() {
      JNIEnv* jni_env = art::Thread::Current()->GetJniEnv();
      for (jthread global_thread_ref : thread_peers) {
        jni_env->DeleteGlobalRef(global_thread_ref);
      }
    }

    std::vector<jvmtiFrameInfo>* GetFrameStorageFor(art::Thread* self, art::Thread* thread)
        REQUIRES_SHARED(art::Locks::mutator_lock_)
        REQUIRES(!mutex) {
      art::MutexLock mu(self, mutex);

      threads.push_back(thread);

      jthread peer = art::Runtime::Current()->GetJavaVM()->AddGlobalRef(
          self, thread->GetPeerFromOtherThread());
      thread_peers.push_back(peer);

      frames.emplace_back(new std::vector<jvmtiFrameInfo>());
      return frames.back().get();
    }

    art::Mutex mutex;

    // Storage. Only access directly after completion.

    std::vector<art::Thread*> threads;
    // "thread_peers" contains global references to their peers.
    std::vector<jthread> thread_peers;

    std::vector<std::unique_ptr<std::vector<jvmtiFrameInfo>>> frames;
  };

  AllStackTracesData data;
  art::Thread* current = art::Thread::Current();
  {
    art::ScopedObjectAccess soa(current);
    RunCheckpointAndWait(&data, static_cast<size_t>(max_frame_count));
  }

  // Convert the data into our output format.

  // Note: we use an array of jvmtiStackInfo for convenience. The spec says we need to
  //       allocate one big chunk for this and the actual frames, which means we need
  //       to either be conservative or rearrange things later (the latter is implemented).
  std::unique_ptr<jvmtiStackInfo[]> stack_info_array(new jvmtiStackInfo[data.frames.size()]);
  std::vector<std::unique_ptr<jvmtiFrameInfo[]>> frame_infos;
  frame_infos.reserve(data.frames.size());

  // Now run through and add data for each thread.
  size_t sum_frames = 0;
  for (size_t index = 0; index < data.frames.size(); ++index) {
    jvmtiStackInfo& stack_info = stack_info_array.get()[index];
    memset(&stack_info, 0, sizeof(jvmtiStackInfo));

    const std::vector<jvmtiFrameInfo>& thread_frames = *data.frames[index].get();

    // For the time being, set the thread to null. We'll fix it up in the second stage.
    stack_info.thread = nullptr;
    stack_info.state = JVMTI_THREAD_STATE_SUSPENDED;

    size_t collected_frames = thread_frames.size();
    if (max_frame_count == 0 || collected_frames == 0) {
      stack_info.frame_count = 0;
      stack_info.frame_buffer = nullptr;
      continue;
    }
    DCHECK_LE(collected_frames, static_cast<size_t>(max_frame_count));

    jvmtiFrameInfo* frame_info = new jvmtiFrameInfo[collected_frames];
    frame_infos.emplace_back(frame_info);

    jint count;
    jvmtiError translate_result = TranslateFrameVector(thread_frames,
                                                       0,
                                                       0,
                                                       static_cast<jint>(collected_frames),
                                                       frame_info,
                                                       &count);
    DCHECK(translate_result == JVMTI_ERROR_NONE);
    stack_info.frame_count = static_cast<jint>(collected_frames);
    stack_info.frame_buffer = frame_info;
    sum_frames += static_cast<size_t>(count);
  }

  // No errors, yet. Now put it all into an output buffer.
  size_t rounded_stack_info_size = art::RoundUp(sizeof(jvmtiStackInfo) * data.frames.size(),
                                                alignof(jvmtiFrameInfo));
  size_t chunk_size = rounded_stack_info_size + sum_frames * sizeof(jvmtiFrameInfo);
  unsigned char* chunk_data;
  jvmtiError alloc_result = env->Allocate(chunk_size, &chunk_data);
  if (alloc_result != ERR(NONE)) {
    return alloc_result;
  }

  jvmtiStackInfo* stack_info = reinterpret_cast<jvmtiStackInfo*>(chunk_data);
  // First copy in all the basic data.
  memcpy(stack_info, stack_info_array.get(), sizeof(jvmtiStackInfo) * data.frames.size());

  // Now copy the frames and fix up the pointers.
  jvmtiFrameInfo* frame_info = reinterpret_cast<jvmtiFrameInfo*>(
      chunk_data + rounded_stack_info_size);
  for (size_t i = 0; i < data.frames.size(); ++i) {
    jvmtiStackInfo& old_stack_info = stack_info_array.get()[i];
    jvmtiStackInfo& new_stack_info = stack_info[i];

    // Translate the global ref into a local ref.
    new_stack_info.thread =
        static_cast<JNIEnv*>(current->GetJniEnv())->NewLocalRef(data.thread_peers[i]);

    if (old_stack_info.frame_count > 0) {
      // Only copy when there's data - leave the nullptr alone.
      size_t frames_size = static_cast<size_t>(old_stack_info.frame_count) * sizeof(jvmtiFrameInfo);
      memcpy(frame_info, old_stack_info.frame_buffer, frames_size);
      new_stack_info.frame_buffer = frame_info;
      frame_info += old_stack_info.frame_count;
    }
  }

  *stack_info_ptr = stack_info;
  *thread_count_ptr = static_cast<jint>(data.frames.size());

  return ERR(NONE);
}

jvmtiError StackUtil::GetThreadListStackTraces(jvmtiEnv* env,
                                               jint thread_count,
                                               const jthread* thread_list,
                                               jint max_frame_count,
                                               jvmtiStackInfo** stack_info_ptr) {
  if (max_frame_count < 0) {
    return ERR(ILLEGAL_ARGUMENT);
  }
  if (thread_count < 0) {
    return ERR(ILLEGAL_ARGUMENT);
  }
  if (thread_count == 0) {
    *stack_info_ptr = nullptr;
    return ERR(NONE);
  }
  if (thread_list == nullptr || stack_info_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::Thread* current = art::Thread::Current();
  art::ScopedObjectAccess soa(current);      // Now we know we have the shared lock.

  struct SelectStackTracesData {
    SelectStackTracesData() : mutex("GetSelectStackTraces", art::LockLevel::kAbortLock) {}

    std::vector<jvmtiFrameInfo>* GetFrameStorageFor(art::Thread* self, art::Thread* thread)
              REQUIRES_SHARED(art::Locks::mutator_lock_)
              REQUIRES(!mutex) {
      art::ObjPtr<art::mirror::Object> peer = thread->GetPeerFromOtherThread();
      for (size_t index = 0; index != handles.size(); ++index) {
        if (peer == handles[index].Get()) {
          // Found the thread.
          art::MutexLock mu(self, mutex);

          threads.push_back(thread);
          thread_list_indices.push_back(index);

          frames.emplace_back(new std::vector<jvmtiFrameInfo>());
          return frames.back().get();
        }
      }
      return nullptr;
    }

    art::Mutex mutex;

    // Selection data.

    std::vector<art::Handle<art::mirror::Object>> handles;

    // Storage. Only access directly after completion.

    std::vector<art::Thread*> threads;
    std::vector<size_t> thread_list_indices;

    std::vector<std::unique_ptr<std::vector<jvmtiFrameInfo>>> frames;
  };

  SelectStackTracesData data;

  // Decode all threads to raw pointers. Put them into a handle scope to avoid any moving GC bugs.
  art::VariableSizedHandleScope hs(current);
  for (jint i = 0; i != thread_count; ++i) {
    if (thread_list[i] == nullptr) {
      return ERR(INVALID_THREAD);
    }
    if (!soa.Env()->IsInstanceOf(thread_list[i], art::WellKnownClasses::java_lang_Thread)) {
      return ERR(INVALID_THREAD);
    }
    data.handles.push_back(hs.NewHandle(soa.Decode<art::mirror::Object>(thread_list[i])));
  }

  RunCheckpointAndWait(&data, static_cast<size_t>(max_frame_count));

  // Convert the data into our output format.

  // Note: we use an array of jvmtiStackInfo for convenience. The spec says we need to
  //       allocate one big chunk for this and the actual frames, which means we need
  //       to either be conservative or rearrange things later (the latter is implemented).
  std::unique_ptr<jvmtiStackInfo[]> stack_info_array(new jvmtiStackInfo[data.frames.size()]);
  std::vector<std::unique_ptr<jvmtiFrameInfo[]>> frame_infos;
  frame_infos.reserve(data.frames.size());

  // Now run through and add data for each thread.
  size_t sum_frames = 0;
  for (size_t index = 0; index < data.frames.size(); ++index) {
    jvmtiStackInfo& stack_info = stack_info_array.get()[index];
    memset(&stack_info, 0, sizeof(jvmtiStackInfo));

    art::Thread* self = data.threads[index];
    const std::vector<jvmtiFrameInfo>& thread_frames = *data.frames[index].get();

    // For the time being, set the thread to null. We don't have good ScopedLocalRef
    // infrastructure.
    DCHECK(self->GetPeerFromOtherThread() != nullptr);
    stack_info.thread = nullptr;
    stack_info.state = JVMTI_THREAD_STATE_SUSPENDED;

    size_t collected_frames = thread_frames.size();
    if (max_frame_count == 0 || collected_frames == 0) {
      stack_info.frame_count = 0;
      stack_info.frame_buffer = nullptr;
      continue;
    }
    DCHECK_LE(collected_frames, static_cast<size_t>(max_frame_count));

    jvmtiFrameInfo* frame_info = new jvmtiFrameInfo[collected_frames];
    frame_infos.emplace_back(frame_info);

    jint count;
    jvmtiError translate_result = TranslateFrameVector(thread_frames,
                                                       0,
                                                       0,
                                                       static_cast<jint>(collected_frames),
                                                       frame_info,
                                                       &count);
    DCHECK(translate_result == JVMTI_ERROR_NONE);
    stack_info.frame_count = static_cast<jint>(collected_frames);
    stack_info.frame_buffer = frame_info;
    sum_frames += static_cast<size_t>(count);
  }

  // No errors, yet. Now put it all into an output buffer. Note that this is not frames.size(),
  // potentially.
  size_t rounded_stack_info_size = art::RoundUp(sizeof(jvmtiStackInfo) * thread_count,
                                                alignof(jvmtiFrameInfo));
  size_t chunk_size = rounded_stack_info_size + sum_frames * sizeof(jvmtiFrameInfo);
  unsigned char* chunk_data;
  jvmtiError alloc_result = env->Allocate(chunk_size, &chunk_data);
  if (alloc_result != ERR(NONE)) {
    return alloc_result;
  }

  jvmtiStackInfo* stack_info = reinterpret_cast<jvmtiStackInfo*>(chunk_data);
  jvmtiFrameInfo* frame_info = reinterpret_cast<jvmtiFrameInfo*>(
      chunk_data + rounded_stack_info_size);

  for (size_t i = 0; i < static_cast<size_t>(thread_count); ++i) {
    // Check whether we found a running thread for this.
    // Note: For simplicity, and with the expectation that the list is usually small, use a simple
    //       search. (The list is *not* sorted!)
    auto it = std::find(data.thread_list_indices.begin(), data.thread_list_indices.end(), i);
    if (it == data.thread_list_indices.end()) {
      // No native thread. Must be new or dead. We need to fill out the stack info now.
      // (Need to read the Java "started" field to know whether this is starting or terminated.)
      art::ObjPtr<art::mirror::Object> peer = soa.Decode<art::mirror::Object>(thread_list[i]);
      art::ObjPtr<art::mirror::Class> klass = peer->GetClass();
      art::ArtField* started_field = klass->FindDeclaredInstanceField("started", "Z");
      CHECK(started_field != nullptr);
      bool started = started_field->GetBoolean(peer) != 0;
      constexpr jint kStartedState = JVMTI_JAVA_LANG_THREAD_STATE_NEW;
      constexpr jint kTerminatedState = JVMTI_THREAD_STATE_TERMINATED |
          JVMTI_JAVA_LANG_THREAD_STATE_TERMINATED;
      stack_info[i].thread = reinterpret_cast<JNIEnv*>(soa.Env())->NewLocalRef(thread_list[i]);
      stack_info[i].state = started ? kTerminatedState : kStartedState;
      stack_info[i].frame_count = 0;
      stack_info[i].frame_buffer = nullptr;
    } else {
      // Had a native thread and frames.
      size_t f_index = it - data.thread_list_indices.begin();

      jvmtiStackInfo& old_stack_info = stack_info_array.get()[f_index];
      jvmtiStackInfo& new_stack_info = stack_info[i];

      memcpy(&new_stack_info, &old_stack_info, sizeof(jvmtiStackInfo));
      new_stack_info.thread = reinterpret_cast<JNIEnv*>(soa.Env())->NewLocalRef(thread_list[i]);
      if (old_stack_info.frame_count > 0) {
        // Only copy when there's data - leave the nullptr alone.
        size_t frames_size =
            static_cast<size_t>(old_stack_info.frame_count) * sizeof(jvmtiFrameInfo);
        memcpy(frame_info, old_stack_info.frame_buffer, frames_size);
        new_stack_info.frame_buffer = frame_info;
        frame_info += old_stack_info.frame_count;
      }
    }
  }

  *stack_info_ptr = stack_info;

  return ERR(NONE);
}

struct GetFrameCountClosure : public art::Closure {
 public:
  GetFrameCountClosure() : count(0) {}

  void Run(art::Thread* self) override REQUIRES_SHARED(art::Locks::mutator_lock_) {
    // This is not StackVisitor::ComputeNumFrames, as runtime methods and transitions must not be
    // counted.
    art::StackVisitor::WalkStack(
        [&](const art::StackVisitor* stack_visitor) REQUIRES_SHARED(art::Locks::mutator_lock_) {
          art::ArtMethod* m = stack_visitor->GetMethod();
          if (m != nullptr && !m->IsRuntimeMethod()) {
            count++;
          }
          return true;
        },
        self,
        /* context= */ nullptr,
        art::StackVisitor::StackWalkKind::kIncludeInlinedFrames);
  }

  size_t count;
};

jvmtiError StackUtil::GetFrameCount(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                    jthread java_thread,
                                    jint* count_ptr) {
  // It is not great that we have to hold these locks for so long, but it is necessary to ensure
  // that the thread isn't dying on us.
  art::ScopedObjectAccess soa(art::Thread::Current());
  art::Locks::thread_list_lock_->ExclusiveLock(soa.Self());

  art::Thread* thread;
  jvmtiError thread_error = ERR(INTERNAL);
  if (!ThreadUtil::GetAliveNativeThread(java_thread, soa, &thread, &thread_error)) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(soa.Self());
    return thread_error;
  }

  DCHECK(thread != nullptr);
  art::ThreadState state = thread->GetState();
  if (state == art::ThreadState::kStarting || thread->IsStillStarting()) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(soa.Self());
    return ERR(THREAD_NOT_ALIVE);
  }

  if (count_ptr == nullptr) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(soa.Self());
    return ERR(NULL_POINTER);
  }

  GetFrameCountClosure closure;
  // RequestSynchronousCheckpoint releases the thread_list_lock_ as a part of its execution.
  if (!thread->RequestSynchronousCheckpoint(&closure)) {
    return ERR(THREAD_NOT_ALIVE);
  }

  *count_ptr = closure.count;
  return ERR(NONE);
}

struct GetLocationClosure : public art::Closure {
 public:
  explicit GetLocationClosure(size_t n_in) : n(n_in), method(nullptr), dex_pc(0) {}

  void Run(art::Thread* self) override REQUIRES_SHARED(art::Locks::mutator_lock_) {
    // Walks up the stack 'n' callers.
    size_t count = 0u;
    art::StackVisitor::WalkStack(
        [&](const art::StackVisitor* stack_visitor) REQUIRES_SHARED(art::Locks::mutator_lock_) {
          art::ArtMethod* m = stack_visitor->GetMethod();
          if (m != nullptr && !m->IsRuntimeMethod()) {
            DCHECK(method == nullptr);
            if (count == n) {
              method = m;
              dex_pc = stack_visitor->GetDexPc(/*abort_on_failure=*/false);
              return false;
            }
            count++;
          }
          return true;
        },
        self,
        /* context= */ nullptr,
        art::StackVisitor::StackWalkKind::kIncludeInlinedFrames);
  }

  const size_t n;
  art::ArtMethod* method;
  uint32_t dex_pc;
};

jvmtiError StackUtil::GetFrameLocation(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                       jthread java_thread,
                                       jint depth,
                                       jmethodID* method_ptr,
                                       jlocation* location_ptr) {
  // It is not great that we have to hold these locks for so long, but it is necessary to ensure
  // that the thread isn't dying on us.
  art::ScopedObjectAccess soa(art::Thread::Current());
  art::Locks::thread_list_lock_->ExclusiveLock(soa.Self());

  art::Thread* thread;
  jvmtiError thread_error = ERR(INTERNAL);
  if (!ThreadUtil::GetAliveNativeThread(java_thread, soa, &thread, &thread_error)) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(soa.Self());
    return thread_error;
  }
  DCHECK(thread != nullptr);

  art::ThreadState state = thread->GetState();
  if (state == art::ThreadState::kStarting || thread->IsStillStarting()) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(soa.Self());
    return ERR(THREAD_NOT_ALIVE);
  }

  if (depth < 0) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(soa.Self());
    return ERR(ILLEGAL_ARGUMENT);
  }
  if (method_ptr == nullptr || location_ptr == nullptr) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(soa.Self());
    return ERR(NULL_POINTER);
  }

  GetLocationClosure closure(static_cast<size_t>(depth));
  // RequestSynchronousCheckpoint releases the thread_list_lock_ as a part of its execution.
  if (!thread->RequestSynchronousCheckpoint(&closure)) {
    return ERR(THREAD_NOT_ALIVE);
  }

  if (closure.method == nullptr) {
    return ERR(NO_MORE_FRAMES);
  }

  *method_ptr = art::jni::EncodeArtMethod(closure.method);
  if (closure.method->IsNative() || closure.method->IsProxyMethod()) {
    *location_ptr = -1;
  } else {
    if (closure.dex_pc == art::dex::kDexNoIndex) {
      return ERR(INTERNAL);
    }
    *location_ptr = static_cast<jlocation>(closure.dex_pc);
  }

  return ERR(NONE);
}

struct MonitorVisitor : public art::StackVisitor, public art::SingleRootVisitor {
  // We need a context because VisitLocks needs it retrieve the monitor objects.
  explicit MonitorVisitor(art::Thread* thread)
      REQUIRES_SHARED(art::Locks::mutator_lock_)
      : art::StackVisitor(thread,
                          art::Context::Create(),
                          art::StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        hs(art::Thread::Current()),
        current_stack_depth(0) {}

  ~MonitorVisitor() {
    delete context_;
  }

  bool VisitFrame() override REQUIRES_SHARED(art::Locks::mutator_lock_) {
    art::Locks::mutator_lock_->AssertSharedHeld(art::Thread::Current());
    if (!GetMethod()->IsRuntimeMethod()) {
      art::Monitor::VisitLocks(this, AppendOwnedMonitors, this);
      ++current_stack_depth;
    }
    return true;
  }

  static void AppendOwnedMonitors(art::ObjPtr<art::mirror::Object> owned_monitor, void* arg)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    art::Locks::mutator_lock_->AssertSharedHeld(art::Thread::Current());
    MonitorVisitor* visitor = reinterpret_cast<MonitorVisitor*>(arg);
    // Filter out duplicates.
    for (const art::Handle<art::mirror::Object>& monitor : visitor->monitors) {
      if (monitor.Get() == owned_monitor) {
        return;
      }
    }
    visitor->monitors.push_back(visitor->hs.NewHandle(owned_monitor));
    visitor->stack_depths.push_back(visitor->current_stack_depth);
  }

  void VisitRoot(art::mirror::Object* obj, const art::RootInfo& info ATTRIBUTE_UNUSED)
      override REQUIRES_SHARED(art::Locks::mutator_lock_) {
    for (const art::Handle<art::mirror::Object>& m : monitors) {
      if (m.Get() == obj) {
        return;
      }
    }
    monitors.push_back(hs.NewHandle(obj));
    stack_depths.push_back(-1);
  }

  art::VariableSizedHandleScope hs;
  jint current_stack_depth;
  std::vector<art::Handle<art::mirror::Object>> monitors;
  std::vector<jint> stack_depths;
};

template<typename Fn>
struct MonitorInfoClosure : public art::Closure {
 public:
  explicit MonitorInfoClosure(Fn handle_results)
      : err_(OK), handle_results_(handle_results) {}

  void Run(art::Thread* target) override REQUIRES_SHARED(art::Locks::mutator_lock_) {
    art::Locks::mutator_lock_->AssertSharedHeld(art::Thread::Current());
    // Find the monitors on the stack.
    MonitorVisitor visitor(target);
    visitor.WalkStack(/* include_transitions= */ false);
    // Find any other monitors, including ones acquired in native code.
    art::RootInfo root_info(art::kRootVMInternal);
    target->GetJniEnv()->VisitMonitorRoots(&visitor, root_info);
    err_ = handle_results_(visitor);
  }

  jvmtiError GetError() {
    return err_;
  }

 private:
  jvmtiError err_;
  Fn handle_results_;
};


template <typename Fn>
static jvmtiError GetOwnedMonitorInfoCommon(const art::ScopedObjectAccessAlreadyRunnable& soa,
                                            jthread thread,
                                            Fn handle_results)
    REQUIRES_SHARED(art::Locks::mutator_lock_) {
  art::Thread* self = art::Thread::Current();
  MonitorInfoClosure<Fn> closure(handle_results);
  bool called_method = false;
  {
    art::Locks::thread_list_lock_->ExclusiveLock(self);
    art::Thread* target = nullptr;
    jvmtiError err = ERR(INTERNAL);
    if (!ThreadUtil::GetAliveNativeThread(thread, soa, &target, &err)) {
      art::Locks::thread_list_lock_->ExclusiveUnlock(self);
      return err;
    }
    if (target != self) {
      called_method = true;
      // RequestSynchronousCheckpoint releases the thread_list_lock_ as a part of its execution.
      // Since this deals with object references we need to avoid going to sleep.
      art::ScopedAssertNoThreadSuspension sants("Getting owned monitor usage");
      if (!target->RequestSynchronousCheckpoint(&closure, art::ThreadState::kRunnable)) {
        return ERR(THREAD_NOT_ALIVE);
      }
    } else {
      art::Locks::thread_list_lock_->ExclusiveUnlock(self);
    }
  }
  // Cannot call the closure on the current thread if we have thread_list_lock since we need to call
  // into the verifier which can cause the current thread to suspend for gc. Suspending would be a
  // bad thing to do if we hold the ThreadListLock. For other threads since we are running it on a
  // checkpoint we are fine but if the thread is the current one we need to drop the mutex first.
  if (!called_method) {
    closure.Run(self);
  }
  return closure.GetError();
}

jvmtiError StackUtil::GetOwnedMonitorStackDepthInfo(jvmtiEnv* env,
                                                    jthread thread,
                                                    jint* info_cnt,
                                                    jvmtiMonitorStackDepthInfo** info_ptr) {
  if (info_cnt == nullptr || info_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }
  art::ScopedObjectAccess soa(art::Thread::Current());
  std::vector<art::GcRoot<art::mirror::Object>> mons;
  std::vector<uint32_t> depths;
  auto handle_fun = [&] (MonitorVisitor& visitor) REQUIRES_SHARED(art::Locks::mutator_lock_) {
    for (size_t i = 0; i < visitor.monitors.size(); i++) {
      mons.push_back(art::GcRoot<art::mirror::Object>(visitor.monitors[i].Get()));
      depths.push_back(visitor.stack_depths[i]);
    }
    return OK;
  };
  jvmtiError err = GetOwnedMonitorInfoCommon(soa, thread, handle_fun);
  if (err != OK) {
    return err;
  }
  auto nbytes = sizeof(jvmtiMonitorStackDepthInfo) * mons.size();
  err = env->Allocate(nbytes, reinterpret_cast<unsigned char**>(info_ptr));
  if (err != OK) {
    return err;
  }
  *info_cnt = mons.size();
  for (uint32_t i = 0; i < mons.size(); i++) {
    (*info_ptr)[i] = {
      soa.AddLocalReference<jobject>(mons[i].Read()),
      static_cast<jint>(depths[i])
    };
  }
  return err;
}

jvmtiError StackUtil::GetOwnedMonitorInfo(jvmtiEnv* env,
                                          jthread thread,
                                          jint* owned_monitor_count_ptr,
                                          jobject** owned_monitors_ptr) {
  if (owned_monitor_count_ptr == nullptr || owned_monitors_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }
  art::ScopedObjectAccess soa(art::Thread::Current());
  std::vector<art::GcRoot<art::mirror::Object>> mons;
  auto handle_fun = [&] (MonitorVisitor& visitor) REQUIRES_SHARED(art::Locks::mutator_lock_) {
    for (size_t i = 0; i < visitor.monitors.size(); i++) {
      mons.push_back(art::GcRoot<art::mirror::Object>(visitor.monitors[i].Get()));
    }
    return OK;
  };
  jvmtiError err = GetOwnedMonitorInfoCommon(soa, thread, handle_fun);
  if (err != OK) {
    return err;
  }
  auto nbytes = sizeof(jobject) * mons.size();
  err = env->Allocate(nbytes, reinterpret_cast<unsigned char**>(owned_monitors_ptr));
  if (err != OK) {
    return err;
  }
  *owned_monitor_count_ptr = mons.size();
  for (uint32_t i = 0; i < mons.size(); i++) {
    (*owned_monitors_ptr)[i] = soa.AddLocalReference<jobject>(mons[i].Read());
  }
  return err;
}

jvmtiError StackUtil::NotifyFramePop(jvmtiEnv* env, jthread thread, jint depth) {
  if (depth < 0) {
    return ERR(ILLEGAL_ARGUMENT);
  }
  ArtJvmTiEnv* tienv = ArtJvmTiEnv::AsArtJvmTiEnv(env);
  art::Thread* self = art::Thread::Current();
  art::Thread* target;

  ScopedNoUserCodeSuspension snucs(self);
  // From now on we know we cannot get suspended by user-code.
  // NB This does a SuspendCheck (during thread state change) so we need to make
  // sure we don't have the 'suspend_lock' locked here.
  art::ScopedObjectAccess soa(self);
  art::Locks::thread_list_lock_->ExclusiveLock(self);
  jvmtiError err = ERR(INTERNAL);
  if (!ThreadUtil::GetAliveNativeThread(thread, soa, &target, &err)) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(self);
    return err;
  }
  if (target != self) {
    // TODO This is part of the spec but we could easily avoid needing to do it.
    // We would just put all the logic into a sync-checkpoint.
    art::Locks::thread_suspend_count_lock_->ExclusiveLock(self);
    if (target->GetUserCodeSuspendCount() == 0) {
      art::Locks::thread_suspend_count_lock_->ExclusiveUnlock(self);
      art::Locks::thread_list_lock_->ExclusiveUnlock(self);
      return ERR(THREAD_NOT_SUSPENDED);
    }
    art::Locks::thread_suspend_count_lock_->ExclusiveUnlock(self);
  }
  // We hold the user_code_suspension_lock_ so the target thread is staying
  // suspended until we are done (unless it's 'self' in which case we don't care
  // since we aren't going to be returning).
  // TODO We could implement this using a synchronous checkpoint and not bother
  // with any of the suspension stuff. The spec does specifically say to return
  // THREAD_NOT_SUSPENDED though. Find the requested stack frame.
  std::unique_ptr<art::Context> context(art::Context::Create());
  FindFrameAtDepthVisitor visitor(target, context.get(), depth);
  visitor.WalkStack();
  if (!visitor.FoundFrame()) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(self);
    return ERR(NO_MORE_FRAMES);
  }
  art::ArtMethod* method = visitor.GetMethod();
  if (method->IsNative()) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(self);
    return ERR(OPAQUE_FRAME);
  }
  // From here we are sure to succeed.
  bool needs_instrument = false;
  // Get/create a shadow frame
  art::ShadowFrame* shadow_frame =
      visitor.GetOrCreateShadowFrame(&needs_instrument);
  {
    art::WriterMutexLock lk(self, tienv->event_info_mutex_);
    if (LIKELY(!shadow_frame->NeedsNotifyPop())) {
      // Ensure we won't miss exceptions being thrown if we get jit-compiled. We
      // only do this for the first NotifyPopFrame.
      target->IncrementForceInterpreterCount();

      // Mark shadow frame as needs_notify_pop_
      shadow_frame->SetNotifyPop(true);
    }
    tienv->notify_frames.insert(shadow_frame);
  }
  // Make sure can we will go to the interpreter and use the shadow frames.
  if (needs_instrument) {
    art::FunctionClosure fc([](art::Thread* self) REQUIRES_SHARED(art::Locks::mutator_lock_) {
      DeoptManager::Get()->DeoptimizeThread(self);
    });
    target->RequestSynchronousCheckpoint(&fc);
  } else {
    art::Locks::thread_list_lock_->ExclusiveUnlock(self);
  }
  return OK;
}

namespace {

enum class NonStandardExitType {
  kPopFrame,
  kForceReturn,
};

template<NonStandardExitType kExitType>
class NonStandardExitFrames {
 public:
  NonStandardExitFrames(art::Thread* self, jvmtiEnv* env, jthread thread)
      REQUIRES(!art::Locks::thread_suspend_count_lock_)
      ACQUIRE_SHARED(art::Locks::mutator_lock_)
      ACQUIRE(art::Locks::thread_list_lock_, art::Locks::user_code_suspension_lock_)
      : snucs_(self) {
    // We keep the user-code-suspend-count lock.
    art::Locks::user_code_suspension_lock_->AssertExclusiveHeld(self);

    // From now on we know we cannot get suspended by user-code.
    // NB This does a SuspendCheck (during thread state change) so we need to make sure we don't
    // have the 'suspend_lock' locked here.
    old_state_ = self->TransitionFromSuspendedToRunnable();
    art::ScopedObjectAccessUnchecked soau(self);

    art::Locks::thread_list_lock_->ExclusiveLock(self);

    if (!ThreadUtil::GetAliveNativeThread(thread, soau, &target_, &result_)) {
      return;
    }
    {
      art::MutexLock tscl_mu(self, *art::Locks::thread_suspend_count_lock_);
      if (target_ != self && target_->GetUserCodeSuspendCount() == 0) {
        // We cannot be the current thread for this function.
        result_ = ERR(THREAD_NOT_SUSPENDED);
        return;
      }
    }
    JvmtiGlobalTLSData* tls_data = ThreadUtil::GetGlobalTLSData(target_);
    constexpr art::StackVisitor::StackWalkKind kWalkKind =
        art::StackVisitor::StackWalkKind::kIncludeInlinedFrames;
    if (tls_data != nullptr &&
        tls_data->disable_pop_frame_depth != JvmtiGlobalTLSData::kNoDisallowedPopFrame &&
        tls_data->disable_pop_frame_depth ==
            art::StackVisitor::ComputeNumFrames(target_, kWalkKind)) {
      JVMTI_LOG(WARNING, env) << "Disallowing frame pop due to in-progress class-load/prepare. "
                              << "Frame at depth " << tls_data->disable_pop_frame_depth << " was "
                              << "marked as un-poppable by the jvmti plugin. See b/117615146 for "
                              << "more information.";
      result_ = ERR(OPAQUE_FRAME);
      return;
    }
    // We hold the user_code_suspension_lock_ so the target thread is staying suspended until we are
    // done.
    std::unique_ptr<art::Context> context(art::Context::Create());
    FindFrameAtDepthVisitor final_frame(target_, context.get(), 0);
    FindFrameAtDepthVisitor penultimate_frame(target_, context.get(), 1);
    final_frame.WalkStack();
    penultimate_frame.WalkStack();

    if (!final_frame.FoundFrame() || !penultimate_frame.FoundFrame()) {
      // Cannot do it if there is only one frame!
      JVMTI_LOG(INFO, env) << "Can not pop final frame off of a stack";
      result_ = ERR(NO_MORE_FRAMES);
      return;
    }

    art::ArtMethod* called_method = final_frame.GetMethod();
    art::ArtMethod* calling_method = penultimate_frame.GetMethod();
    if (!CheckFunctions(env, calling_method, called_method)) {
      return;
    }
    DCHECK(!called_method->IsNative()) << called_method->PrettyMethod();

    // From here we are sure to succeed.
    result_ = OK;

    // Get/create a shadow frame
    final_frame_ = final_frame.GetOrCreateShadowFrame(&created_final_frame_);
    penultimate_frame_ =
        (calling_method->IsNative()
             ? nullptr
             : penultimate_frame.GetOrCreateShadowFrame(&created_penultimate_frame_));

    final_frame_id_ = final_frame.GetFrameId();
    penultimate_frame_id_ = penultimate_frame.GetFrameId();

    CHECK_NE(final_frame_, penultimate_frame_) << "Frames at different depths not different!";
  }

  bool CheckFunctions(jvmtiEnv* env, art::ArtMethod* calling, art::ArtMethod* called)
      REQUIRES(art::Locks::thread_list_lock_, art::Locks::user_code_suspension_lock_)
      REQUIRES_SHARED(art::Locks::mutator_lock_);

  ~NonStandardExitFrames() RELEASE_SHARED(art::Locks::mutator_lock_)
      REQUIRES(!art::Locks::thread_list_lock_)
      RELEASE(art::Locks::user_code_suspension_lock_) {
    art::Thread* self = art::Thread::Current();
    DCHECK_EQ(old_state_, art::ThreadState::kNative)
        << "Unexpected thread state on entering PopFrame!";
    self->TransitionFromRunnableToSuspended(old_state_);
  }

  ScopedNoUserCodeSuspension snucs_;
  art::ShadowFrame* final_frame_ GUARDED_BY(art::Locks::user_code_suspension_lock_) = nullptr;
  art::ShadowFrame* penultimate_frame_ GUARDED_BY(art::Locks::user_code_suspension_lock_) = nullptr;
  bool created_final_frame_ GUARDED_BY(art::Locks::user_code_suspension_lock_) = false;
  bool created_penultimate_frame_ GUARDED_BY(art::Locks::user_code_suspension_lock_) = false;
  uint32_t final_frame_id_ GUARDED_BY(art::Locks::user_code_suspension_lock_) = -1;
  uint32_t penultimate_frame_id_ GUARDED_BY(art::Locks::user_code_suspension_lock_) = -1;
  art::Thread* target_ GUARDED_BY(art::Locks::thread_list_lock_) = nullptr;
  art::ThreadState old_state_ = art::ThreadState::kTerminated;
  jvmtiError result_ = ERR(INTERNAL);
};

template <>
bool NonStandardExitFrames<NonStandardExitType::kForceReturn>::CheckFunctions(
    jvmtiEnv* env, art::ArtMethod* calling ATTRIBUTE_UNUSED, art::ArtMethod* called) {
  if (UNLIKELY(called->IsNative())) {
    result_ = ERR(OPAQUE_FRAME);
    JVMTI_LOG(INFO, env) << "Cannot force early return from " << called->PrettyMethod()
                         << " because it is native.";
    return false;
  } else {
    return true;
  }
}

template <>
bool NonStandardExitFrames<NonStandardExitType::kPopFrame>::CheckFunctions(
    jvmtiEnv* env, art::ArtMethod* calling, art::ArtMethod* called) {
  if (UNLIKELY(calling->IsNative() || called->IsNative())) {
    result_ = ERR(OPAQUE_FRAME);
    JVMTI_LOG(INFO, env) << "Cannot force early return from " << called->PrettyMethod() << " to "
                         << calling->PrettyMethod() << " because at least one of them is native.";
    return false;
  } else {
    return true;
  }
}

class SetupMethodExitEvents {
 public:
  SetupMethodExitEvents(art::Thread* self,
                        EventHandler* event_handler,
                        jthread target) REQUIRES(!art::Locks::mutator_lock_,
                                                 !art::Locks::user_code_suspension_lock_,
                                                 !art::Locks::thread_list_lock_)
      : self_(self), event_handler_(event_handler), target_(target) {
    DCHECK(target != nullptr);
    art::Locks::mutator_lock_->AssertNotHeld(self_);
    art::Locks::user_code_suspension_lock_->AssertNotHeld(self_);
    art::Locks::thread_list_lock_->AssertNotHeld(self_);
    event_handler_->SetInternalEvent(
        target_, ArtJvmtiEvent::kForceEarlyReturnUpdateReturnValue, JVMTI_ENABLE);
  }

  ~SetupMethodExitEvents() REQUIRES(!art::Locks::mutator_lock_,
                                    !art::Locks::user_code_suspension_lock_,
                                    !art::Locks::thread_list_lock_) {
    art::Locks::mutator_lock_->AssertNotHeld(self_);
    art::Locks::user_code_suspension_lock_->AssertNotHeld(self_);
    art::Locks::thread_list_lock_->AssertNotHeld(self_);
    if (failed_) {
      event_handler_->SetInternalEvent(
          target_, ArtJvmtiEvent::kForceEarlyReturnUpdateReturnValue, JVMTI_DISABLE);
    }
  }

  void NotifyFailure() {
    failed_ = true;
  }

 private:
  art::Thread* self_;
  EventHandler* event_handler_;
  jthread target_;
  bool failed_ = false;
};

template <typename T>
void AddDelayedMethodExitEvent(EventHandler* handler, art::ShadowFrame* frame, T value)
    REQUIRES_SHARED(art::Locks::mutator_lock_)
    REQUIRES(art::Locks::user_code_suspension_lock_, art::Locks::thread_list_lock_);

template <typename T>
void AddDelayedMethodExitEvent(EventHandler* handler, art::ShadowFrame* frame, T value) {
  art::JValue val = art::JValue::FromPrimitive(value);
  jvalue jval{ .j = val.GetJ() };
  handler->AddDelayedNonStandardExitEvent(frame, false, jval);
}

template <>
void AddDelayedMethodExitEvent<std::nullptr_t>(EventHandler* handler,
                                               art::ShadowFrame* frame,
                                               std::nullptr_t null_val ATTRIBUTE_UNUSED) {
  jvalue jval;
  memset(&jval, 0, sizeof(jval));
  handler->AddDelayedNonStandardExitEvent(frame, false, jval);
}

template <>
void AddDelayedMethodExitEvent<jobject>(EventHandler* handler,
                                        art::ShadowFrame* frame,
                                        jobject obj) {
  jvalue jval{ .l = art::Thread::Current()->GetJniEnv()->NewGlobalRef(obj) };
  handler->AddDelayedNonStandardExitEvent(frame, true, jval);
}

template <typename T>
bool ValidReturnType(art::Thread* self, art::ObjPtr<art::mirror::Class> return_type, T value)
    REQUIRES_SHARED(art::Locks::mutator_lock_)
        REQUIRES(art::Locks::user_code_suspension_lock_, art::Locks::thread_list_lock_);

#define SIMPLE_VALID_RETURN_TYPE(type, ...)                                                        \
  template <>                                                                                      \
  bool ValidReturnType<type>(art::Thread * self ATTRIBUTE_UNUSED,                                  \
                             art::ObjPtr<art::mirror::Class> return_type,                          \
                             type value ATTRIBUTE_UNUSED) {                                        \
    static constexpr std::initializer_list<art::Primitive::Type> types{ __VA_ARGS__ };             \
    return std::find(types.begin(), types.end(), return_type->GetPrimitiveType()) != types.end();  \
  }

SIMPLE_VALID_RETURN_TYPE(jlong, art::Primitive::kPrimLong);
SIMPLE_VALID_RETURN_TYPE(jfloat, art::Primitive::kPrimFloat);
SIMPLE_VALID_RETURN_TYPE(jdouble, art::Primitive::kPrimDouble);
SIMPLE_VALID_RETURN_TYPE(nullptr_t, art::Primitive::kPrimVoid);
SIMPLE_VALID_RETURN_TYPE(jint,
                         art::Primitive::kPrimInt,
                         art::Primitive::kPrimChar,
                         art::Primitive::kPrimBoolean,
                         art::Primitive::kPrimShort,
                         art::Primitive::kPrimByte);
#undef SIMPLE_VALID_RETURN_TYPE

template <>
bool ValidReturnType<jobject>(art::Thread* self,
                              art::ObjPtr<art::mirror::Class> return_type,
                              jobject return_value) {
  if (return_type->IsPrimitive()) {
    return false;
  }
  if (return_value == nullptr) {
    // Null can be used for anything.
    return true;
  }
  return return_type->IsAssignableFrom(self->DecodeJObject(return_value)->GetClass());
}

}  // namespace

jvmtiError StackUtil::PopFrame(jvmtiEnv* env, jthread thread) {
  art::Thread* self = art::Thread::Current();
  NonStandardExitFrames<NonStandardExitType::kPopFrame> frames(self, env, thread);
  if (frames.result_ != OK) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(self);
    return frames.result_;
  }
  // Tell the shadow-frame to return immediately and skip all exit events.
  frames.penultimate_frame_->SetForceRetryInstruction(true);
  frames.final_frame_->SetForcePopFrame(true);
  frames.final_frame_->SetSkipMethodExitEvents(true);
  if (frames.created_final_frame_ || frames.created_penultimate_frame_) {
    art::FunctionClosure fc([](art::Thread* self) REQUIRES_SHARED(art::Locks::mutator_lock_){
      DeoptManager::Get()->DeoptimizeThread(self);
    });
    frames.target_->RequestSynchronousCheckpoint(&fc);
  } else {
    art::Locks::thread_list_lock_->ExclusiveUnlock(self);
  }
  return OK;
}

template <typename T>
jvmtiError
StackUtil::ForceEarlyReturn(jvmtiEnv* env, EventHandler* event_handler, jthread thread, T value) {
  art::Thread* self = art::Thread::Current();
  // We don't want to use the null == current-thread idiom since for events (that we use internally
  // to implement force-early-return) we instead have null == all threads. Instead just get the
  // current jthread if needed.
  ScopedLocalRef<jthread> cur_thread(self->GetJniEnv(), nullptr);
  if (UNLIKELY(thread == nullptr)) {
    art::ScopedObjectAccess soa(self);
    cur_thread.reset(soa.AddLocalReference<jthread>(self->GetPeer()));
    thread = cur_thread.get();
  }
  // This sets up the exit events we implement early return using before we have the locks and
  // thanks to destructor ordering will tear them down if something goes wrong.
  SetupMethodExitEvents smee(self, event_handler, thread);
  NonStandardExitFrames<NonStandardExitType::kForceReturn> frames(self, env, thread);
  if (frames.result_ != OK) {
    smee.NotifyFailure();
    art::Locks::thread_list_lock_->ExclusiveUnlock(self);
    return frames.result_;
  } else if (!ValidReturnType<T>(
                 self, frames.final_frame_->GetMethod()->ResolveReturnType(), value)) {
    smee.NotifyFailure();
    art::Locks::thread_list_lock_->ExclusiveUnlock(self);
    return ERR(TYPE_MISMATCH);
  } else if (frames.final_frame_->GetForcePopFrame()) {
    // TODO We should really support this.
    smee.NotifyFailure();
    std::string thread_name;
    frames.target_->GetThreadName(thread_name);
    JVMTI_LOG(WARNING, env) << "PopFrame or force-return already pending on thread " << thread_name;
    art::Locks::thread_list_lock_->ExclusiveUnlock(self);
    return ERR(OPAQUE_FRAME);
  }
  // Tell the shadow-frame to return immediately and skip all exit events.
  frames.final_frame_->SetForcePopFrame(true);
  AddDelayedMethodExitEvent<T>(event_handler, frames.final_frame_, value);
  if (frames.created_final_frame_ || frames.created_penultimate_frame_) {
    art::FunctionClosure fc([](art::Thread* self) REQUIRES_SHARED(art::Locks::mutator_lock_){
      DeoptManager::Get()->DeoptimizeThread(self);
    });
    frames.target_->RequestSynchronousCheckpoint(&fc);
  } else {
    art::Locks::thread_list_lock_->ExclusiveUnlock(self);
  }
  return OK;
}

// Instantiate the ForceEarlyReturn templates.
template jvmtiError StackUtil::ForceEarlyReturn(jvmtiEnv*, EventHandler*, jthread, jint);
template jvmtiError StackUtil::ForceEarlyReturn(jvmtiEnv*, EventHandler*, jthread, jlong);
template jvmtiError StackUtil::ForceEarlyReturn(jvmtiEnv*, EventHandler*, jthread, jfloat);
template jvmtiError StackUtil::ForceEarlyReturn(jvmtiEnv*, EventHandler*, jthread, jdouble);
template jvmtiError StackUtil::ForceEarlyReturn(jvmtiEnv*, EventHandler*, jthread, jobject);
template jvmtiError StackUtil::ForceEarlyReturn(jvmtiEnv*, EventHandler*, jthread, nullptr_t);

}  // namespace openjdkjvmti
