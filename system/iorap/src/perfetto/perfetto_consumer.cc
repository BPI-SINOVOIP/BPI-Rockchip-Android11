// Copyright (C) 2020 The Android Open Source Project
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

#include "perfetto/perfetto_consumer.h"

#include "common/trace.h"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <utils/Looper.h>
#include <utils/Printer.h>

#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

#include <inttypes.h>
#include <time.h>

namespace iorap::perfetto {

using State = PerfettoConsumer::State;
using Handle = PerfettoConsumer::Handle;
static constexpr Handle kInvalidHandle = PerfettoConsumer::kInvalidHandle;
using OnStateChangedCb = PerfettoConsumer::OnStateChangedCb;
using TraceBuffer = PerfettoConsumer::TraceBuffer;

enum class StateKind {
  kUncreated,
  kCreated,
  kStartedTracing,
  kReadTracing,
  kTimedOutDestroyed,  // same as kDestroyed but timed out.
  kDestroyed,          // calling kDestroyed before timing out.
};

std::ostream& operator<<(std::ostream& os, StateKind kind) {
  switch (kind) {
    case StateKind::kUncreated:
      os << "kUncreated";
      break;
    case StateKind::kCreated:
      os << "kCreated";
      break;
    case StateKind::kStartedTracing:
      os << "kStartedTracing";
      break;
    case StateKind::kReadTracing:
      os << "kReadTracing";
      break;
    case StateKind::kTimedOutDestroyed:
      os << "kTimedOutDestroyed";
      break;
    case StateKind::kDestroyed:
      os << "kDestroyed";
      break;
    default:
      os << "(invalid)";
      break;
  }
  return os;
}

std::string ToString(StateKind kind) {
  std::stringstream ss;
  ss << kind;
  return ss.str();
}

static constexpr uint64_t kSecToNano = 1000000000LL;

static uint64_t GetTimeNanoseconds() {
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);

  uint64_t now_ns = (now.tv_sec * kSecToNano + now.tv_nsec);
  return now_ns;
}

// Describe the state of our handle in detail for debugging/logging.
struct HandleDescription {
  Handle handle_;
  StateKind kind_{StateKind::kUncreated};  // Our state. required for correctness.
  OnStateChangedCb callback_{nullptr};  // Required for Destroy callbacks.
  void* callback_arg_{nullptr};

  // For dumping to logs:
  State state_{State::kSessionNotFound};  // perfetto state
  std::optional<uint64_t> started_tracing_ns_; // when StartedTracing last called.
  std::optional<uint64_t> read_trace_ns_;  // When ReadTrace last called.
  std::uint64_t last_transition_ns_{0};
  std::optional<uint64_t> trace_cookie_;  // atrace beginning at StartTracing.
  bool trace_ended_{false};               // atrace ending at ReadTrace or Destroy.

  HandleDescription(Handle handle): handle_(handle) {}
};

// pimpl idiom to hide the implementation details from header
//
// Track and verify that our perfetto usage is sane.
struct PerfettoConsumerImpl::Impl {
  Impl() : raw_{new PerfettoConsumerRawImpl{}},
      message_handler_{new TraceMessageHandler{this}} {
    std::thread watchdog_thread{ [this]() {
      ::android::sp<::android::Looper> looper;
      {
        std::lock_guard<std::mutex> guard{looper_mutex_};
        looper = ::android::Looper::prepare(/*opts*/0);
        looper_ = looper;
      }

      static constexpr int kTimeoutMillis = std::numeric_limits<int>::max();

      while (true) {
        // Execute any pending callbacks, otherwise just block forever.
        int result = looper->pollAll(kTimeoutMillis);

        if (result == ::android::Looper::POLL_ERROR) {
          LOG(ERROR) << "PerfettoConsumerImpl::Looper got a POLL_ERROR";
        } else {
          LOG(DEBUG) << "PerfettoConsumerImpl::Looper result was " << result;
        }
      }
    }};

    // Let thread run freely on its own.
    watchdog_thread.detach();

    // Block until looper_ is prepared.
    while (true) {
      std::lock_guard<std::mutex> guard{looper_mutex_};
      if (looper_ != nullptr) {
        break;
      }
    }
  }

 private:
  std::unique_ptr<PerfettoConsumerRawImpl> raw_;
  std::map<Handle, HandleDescription> states_;

  // We need this to be a counter to avoid memory leaks.
  Handle last_created_{0};
  Handle last_destroyed_{0};
  uint64_t trace_cookie_{0};

  std::mutex mutex_;  // Guard above values.

  ::android::sp<::android::Looper> looper_;
  std::mutex looper_mutex_;  // Guard looper_.

  struct TraceMessageHandler : public ::android::MessageHandler {
    TraceMessageHandler(Impl* impl) : impl_{impl} {
      CHECK(impl != nullptr);
    }

    Impl* impl_;

    virtual void handleMessage(const ::android::Message& message) override {
      impl_->OnTraceMessage(static_cast<Handle>(message.what));
    }
  };

  ::android::sp<TraceMessageHandler> message_handler_;

 public:
  Handle Create(const void* config_proto,
                size_t config_len,
                OnStateChangedCb callback,
                void* callback_arg) {
    LOG(VERBOSE) << "PerfettoConsumer::Create("
                 << "config_len=" << config_len << ")";
    Handle handle = raw_->Create(config_proto, config_len, callback, callback_arg);

    std::lock_guard<std::mutex> guard{mutex_};

    // Assume every Handle starts at 0 and then increments by 1 every Create.
    ++last_created_;
    CHECK_EQ(last_created_, handle) << "perfetto handle had unexpected behavior.";
    // Without this^ increment-by-1 behavior our detection of untracked state values is broken.
    // If we have to, we can go with Untracked=Uncreated|Destroyed but it's better to distinguish
    // the two if possible.

    HandleDescription handle_desc{handle};
    handle_desc.handle_ = handle;
    handle_desc.callback_ = callback;
    handle_desc.callback_arg_ = callback_arg;
    UpdateHandleDescription(/*inout*/&handle_desc, StateKind::kCreated);

    // assume we never wrap around due to using int64
    bool inserted = states_.insert({handle, handle_desc}).second;
    CHECK(inserted) << "perfetto handle was re-used: " << handle;

    return handle;
  }

  void StartTracing(Handle handle) {
    LOG(DEBUG) << "PerfettoConsumer::StartTracing(handle=" << handle << ")";

    uint64_t trace_cookie;
    {
      std::lock_guard<std::mutex> guard{mutex_};

      auto it = states_.find(handle);
      if (it == states_.end()) {
        LOG(ERROR) << "Cannot StartTracing(" << handle << "), untracked handle";
        return;
      }
      HandleDescription& handle_desc = it->second;

      raw_->StartTracing(handle);
      UpdateHandleDescription(/*inout*/&handle_desc, StateKind::kStartedTracing);
    }

    // Use a looper here to add a timeout and immediately destroy the trace buffer.
    CHECK_LE(static_cast<int64_t>(handle), static_cast<int64_t>(std::numeric_limits<int>::max()));
    int message_code = static_cast<int>(handle);
    ::android::Message message{message_code};

    std::lock_guard<std::mutex> looper_guard{looper_mutex_};
    looper_->sendMessageDelayed(static_cast<nsecs_t>(GetPropertyTraceTimeoutNs()),
                                message_handler_,
                                message);
  }

  TraceBuffer ReadTrace(Handle handle) {
    LOG(DEBUG) << "PerfettoConsumer::ReadTrace(handle=" << handle << ")";

    std::lock_guard<std::mutex> guard{mutex_};

    auto it = states_.find(handle);
    if (it == states_.end()) {
      LOG(ERROR) << "Cannot ReadTrace(" << handle << "), untracked handle";
      return TraceBuffer{};
    }

    HandleDescription& handle_desc = it->second;

    TraceBuffer trace_buffer = raw_->ReadTrace(handle);
    UpdateHandleDescription(/*inout*/&handle_desc, StateKind::kReadTracing);

    return trace_buffer;
  }

  void Destroy(Handle handle) {
    HandleDescription handle_desc{handle};
    TryDestroy(handle, /*do_destroy*/true, /*out*/&handle_desc);;
  }

  bool TryDestroy(Handle handle, bool do_destroy, /*out*/HandleDescription* handle_desc_out) {
    CHECK(handle_desc_out != nullptr);

    LOG(VERBOSE) << "PerfettoConsumer::Destroy(handle=" << handle << ")";

    std::lock_guard<std::mutex> guard{mutex_};

    auto it = states_.find(handle);
    if (it == states_.end()) {
      // Leniency for calling Destroy multiple times. It's not a mistake.
      LOG(ERROR) << "Cannot Destroy(" << handle << "), untracked handle";
      return false;
    }

    HandleDescription& handle_desc = it->second;

    if (do_destroy) {
      raw_->Destroy(handle);
    }
    UpdateHandleDescription(/*inout*/&handle_desc, StateKind::kDestroyed);

    *handle_desc_out = handle_desc;

    // No longer track this handle to avoid memory leaks.
    last_destroyed_ = handle;
    states_.erase(it);

    return true;
  }

  State PollState(Handle handle) {
    // Just pass-through the call, we never use it directly anyway.
    return raw_->PollState(handle);
  }

  // Either fetch or infer the current handle state from a handle.
  // Meant for debugging/logging only.
  HandleDescription GetOrInferHandleDescription(Handle handle) {
    std::lock_guard<std::mutex> guard{mutex_};

    auto it = states_.find(handle);
    if (it == states_.end()) {
      HandleDescription state{handle};
      // If it's untracked it hasn't been created yet, or it was already destroyed.
      if (IsDestroyed(handle)) {
        UpdateHandleDescription(/*inout*/&state, StateKind::kDestroyed);
      } else {
        if (!IsUncreated(handle)) {
          LOG(WARNING) << "bad state detection";
        }
        UpdateHandleDescription(/*inout*/&state, StateKind::kUncreated);
      }
      return state;
    }
    return it->second;
  }

  void OnTraceMessage(Handle handle) {
    LOG(VERBOSE) << "OnTraceMessage(" << static_cast<int64_t>(handle) << ")";
    HandleDescription handle_desc{handle};
    {
      std::lock_guard<std::mutex> guard{mutex_};

      auto it = states_.find(handle);
      if (it == states_.end()) {
        // Handle values are never re-used, so we can simply ignore the message here
        // instead of having to remove it from the message queue.
        LOG(VERBOSE) << "OnTraceMessage(" << static_cast<int64_t>(handle)
                     << ") no longer tracked handle";
        return;
      }
      handle_desc = it->second;
    }

    // First check. Has this trace been active for too long?
    uint64_t now_ns = GetTimeNanoseconds();
    if (handle_desc.kind_ == StateKind::kStartedTracing) {
      // Ignore other kinds of traces because they don't exhaust perfetto resources.
      CHECK(handle_desc.started_tracing_ns_.has_value()) << static_cast<int64_t>(handle);

      uint64_t started_tracing_ns = *handle_desc.started_tracing_ns_;

      if ((now_ns - started_tracing_ns) > GetPropertyTraceTimeoutNs()) {
        LOG(WARNING) << "Perfetto Handle timed out after " << (now_ns - started_tracing_ns) << "ns"
                     << ", forcibly destroying";

        // Let the callback handler call Destroy.
        handle_desc.callback_(handle, State::kTraceFailed, handle_desc.callback_arg_);
      }
    }

    // Second check. Are there too many traces now? Cull the old traces.
    std::vector<HandleDescription> handle_list;
    do {
      std::lock_guard<std::mutex> guard{mutex_};

      size_t max_trace_count = GetPropertyMaxTraceCount();
      if (states_.size() > max_trace_count) {
        size_t overflow_count = states_.size() - max_trace_count;
        LOG(WARNING) << "Too many perfetto handles, overflowed by " << overflow_count
                     << ", pruning down to " << max_trace_count;
      } else {
        break;
      }

      size_t prune_count = states_.size() - max_trace_count;
      auto it = states_.begin();
      for (size_t i = 0; i < prune_count; ++i) {
        // Simply prune by handle 1,2,3,4...
        // We could do better with a timestamp if we wanted to.
        ++it;
        handle_list.push_back(it->second);
      }
    } while (false);

    for (HandleDescription& handle_desc : handle_list) {
      LOG(DEBUG) << "Perfetto handle pruned: " << static_cast<int64_t>(handle);

      // Let the callback handler call Destroy.
      handle_desc.callback_(handle, State::kTraceFailed, handle_desc.callback_arg_);
    }
  }

 private:
  static uint64_t GetPropertyTraceTimeoutNs() {
    static uint64_t value =                       // property is timeout in seconds
        ::android::base::GetUintProperty<uint64_t>("iorapd.perfetto.timeout", /*default*/10);
    return value * kSecToNano;
  }

  static size_t GetPropertyMaxTraceCount() {
    static size_t value =
        ::android::base::GetUintProperty<size_t>("iorapd.perfetto.max_traces", /*default*/5);
    return value;
  }

  void UpdateHandleDescription(/*inout*/HandleDescription* handle_desc, StateKind kind) {
    CHECK(handle_desc != nullptr);
    handle_desc->kind_ = kind;
    handle_desc->state_ = raw_->PollState(handle_desc->handle_);

    handle_desc->last_transition_ns_ = GetTimeNanoseconds();
    if (kind == StateKind::kStartedTracing) {
      if (!handle_desc->started_tracing_ns_) {
        handle_desc->started_tracing_ns_ = handle_desc->last_transition_ns_;

        handle_desc->trace_cookie_ = ++trace_cookie_;

        atrace_async_begin(ATRACE_TAG_ACTIVITY_MANAGER,
                           "Perfetto Scoped Trace",
                           *handle_desc->trace_cookie_);
        atrace_int(ATRACE_TAG_ACTIVITY_MANAGER,
                   "Perfetto::Trace Handle",
                   static_cast<int32_t>(handle_desc->handle_));
      }
    }

    if (kind == StateKind::kReadTracing) {
      if (!handle_desc->read_trace_ns_) {
        handle_desc->read_trace_ns_ = handle_desc->last_transition_ns_;

        if (handle_desc->trace_cookie_.has_value() && !handle_desc->trace_ended_) {
          atrace_async_end(ATRACE_TAG_ACTIVITY_MANAGER,
                           "Perfetto Scoped Trace",
                           handle_desc->trace_cookie_.value());

          handle_desc->trace_ended_ = true;
        }
      }
    }

    // If Destroy is called prior to ReadTrace, mark the atrace as finished.
    if (kind == StateKind::kDestroyed && handle_desc->trace_cookie_ && !handle_desc->trace_ended_) {
      atrace_async_end(ATRACE_TAG_ACTIVITY_MANAGER,
                       "Perfetto Scoped Trace",
                       *handle_desc->trace_cookie_);
      handle_desc->trace_ended_ = true;
    }
  }

  // The following state detection is for debugging only.
  // We figure out if something is destroyed, uncreated, or live.

  // Does not distinguish between kTimedOutDestroyed and kDestroyed.
  bool IsDestroyed(Handle handle) const {
    auto it = states_.find(handle);
    if (it != states_.end()) {
      // Tracked values are not destroyed yet.
      return false;
    }

    if (handle == kInvalidHandle) {
      return false;
    }

    // The following assumes handles are incrementally generated:
    if (it == states_.end()) {
      // value is in range of [0, last_destroyed]  => destroyed.
      return handle <= last_destroyed_;
    }

    auto min_it = states_.begin();
    if (handle < min_it->first) {
      // value smaller than anything tracked: it was destroyed and we stopped tracking it.
      return true;
    }

    auto max_it = states_.rbegin();
    if (handle > max_it->first) {
      // value too big: it's uncreated;
      return false;
    }

    // else it was a value that was previously tracked within [min,max] but no longer
    return true;
  }

  bool IsUncreated(Handle handle) const {
    auto it = states_.find(handle);
    if (it != states_.end()) {
      // Tracked values are not uncreated.
      return false;
    }

    if (handle == kInvalidHandle) {
      // Strangely enough, an invalid handle can never be created.
      return true;
    }

    // The following assumes handles are incrementally generated:
    if (it == states_.end()) {
      // value is in range of (last_destroyed, inf)  => uncreated.
      return handle > last_destroyed_;
    }

    auto min_it = states_.begin();
    if (handle < min_it->first) {
      // value smaller than anything tracked: it was destroyed and we stopped tracking it.
      return false;
    }

    auto max_it = states_.rbegin();
    if (handle > max_it->first) {
      // value too big: it's uncreated;
      return true;
    }

    // else it was a value that was previously tracked within [min,max] but no longer
    return false;
  }

 public:
  void Dump(::android::Printer& printer) {
    // Locking can fail if we dump during a deadlock, so just do a best-effort lock here.
    bool is_it_locked = mutex_.try_lock();

    printer.printFormatLine("Perfetto consumer state:");
    if (!is_it_locked) {
      printer.printLine("""""  (possible deadlock)");
    }
    printer.printFormatLine("  Last destroyed handle: %" PRId64, last_destroyed_);
    printer.printFormatLine("  Last created handle: %" PRId64, last_created_);
    printer.printFormatLine("");
    printer.printFormatLine("  In-flight handles:");

    for (auto it = states_.begin(); it != states_.end(); ++it) {
      HandleDescription& handle_desc = it->second;
      uint64_t started_tracing =
          handle_desc.started_tracing_ns_ ? *handle_desc.started_tracing_ns_ : 0;
      printer.printFormatLine("    Handle %" PRId64, handle_desc.handle_);
      printer.printFormatLine("      Kind: %s", ToString(handle_desc.kind_).c_str());
      printer.printFormatLine("      Perfetto State: %d", static_cast<int>(handle_desc.state_));
      printer.printFormatLine("      Started tracing at: %" PRIu64, started_tracing);
      printer.printFormatLine("      Last transition at: %" PRIu64,
                              handle_desc.last_transition_ns_);
    }
    if (states_.empty()) {
      printer.printFormatLine("    (None)");
    }

    printer.printFormatLine("");

    if (is_it_locked) {  // u.b. if calling unlock on an unlocked mutex.
      mutex_.unlock();
    }
  }

  static PerfettoConsumerImpl::Impl* GetImplSingleton() {
    static PerfettoConsumerImpl::Impl impl;
    return &impl;
  }
};

// Use a singleton because fruit instantiates a new PerfettoConsumer object for every
// new rx chain in RxProducerFactory. However, we want to track all perfetto transitions globally
// through 1 impl object.
//
// TODO: Avoiding a singleton would mean a more significant refactoring to remove the fruit/perfetto
// usage.


//
// Forward all calls to PerfettoConsumerImpl::Impl
//

PerfettoConsumerImpl::~PerfettoConsumerImpl() {
  // delete impl_;  // TODO: no singleton
}

void PerfettoConsumerImpl::Initialize() {
  // impl_ = new PerfettoConsumerImpl::Impl();  // TODO: no singleton
  impl_ = PerfettoConsumerImpl::Impl::GetImplSingleton();
}

void PerfettoConsumerImpl::Dump(::android::Printer& printer) {
  PerfettoConsumerImpl::Impl::GetImplSingleton()->Dump(/*borrow*/printer);
}

PerfettoConsumer::Handle PerfettoConsumerImpl::Create(const void* config_proto,
                                    size_t config_len,
                                    PerfettoConsumer::OnStateChangedCb callback,
                                    void* callback_arg) {
  return impl_->Create(config_proto,
                       config_len,
                       callback,
                       callback_arg);
}

void PerfettoConsumerImpl::StartTracing(PerfettoConsumer::Handle handle) {
  impl_->StartTracing(handle);
}

PerfettoConsumer::TraceBuffer PerfettoConsumerImpl::ReadTrace(PerfettoConsumer::Handle handle) {
  return impl_->ReadTrace(handle);
}

void PerfettoConsumerImpl::Destroy(PerfettoConsumer::Handle handle) {
  impl_->Destroy(handle);
}

PerfettoConsumer::State PerfettoConsumerImpl::PollState(PerfettoConsumer::Handle handle) {
  return impl_->PollState(handle);
}

}  // namespace iorap::perfetto
