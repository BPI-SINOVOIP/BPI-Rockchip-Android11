/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "binder/iiorap_impl.h"
#include "binder/iiorap_def.h"
#include "common/macros.h"
#include "manager/event_manager.h"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <binder/BinderService.h>
#include <binder/IPCThreadState.h>
#include <include/binder/request_id.h>
#include <utils/Printer.h>

#include <codecvt>
#include <locale>
#include <utility>

/*
 * Definitions for the IIorap binder native service implementation.
 * See also IIorap.aidl.
 */

using Status = ::android::binder::Status;
using ITaskListener = ::com::google::android::startop::iorap::ITaskListener;

namespace iorap {
namespace binder {

namespace {
// Forward declarations.
template<typename ... Args>
Status Send(const char* function_name, Args&& ... args);
}

// Join all parameter declarations by splitting each parameter with a comma.
// Types are used fully.
#define IIORAP_IMPL_ARG_DECLARATIONS(...) \
  IORAP_PP_MAP_SEP(IORAP_BINDER_PARAM_JOIN_ALL, IORAP_PP_COMMA, __VA_ARGS__)
#define IIORAP_IMPL_ARG_NAMES(...) \
  IORAP_PP_MAP_SEP(IORAP_BINDER_PARAM_JOIN_NAMES, IORAP_PP_COMMA, __VA_ARGS__)
#define IIORAP_IMPL_BODY(name, ...)                                                                \
  ::android::binder::Status IIorapImpl::name(IIORAP_IMPL_ARG_DECLARATIONS(__VA_ARGS__)) {          \
    return Send(#name, impl_.get(), IIORAP_IMPL_ARG_NAMES(__VA_ARGS__));                           \
  }

IIORAP_IFACE_DEF(/*begin*/IORAP_PP_NOP, IIORAP_IMPL_BODY, /*end*/IORAP_PP_NOP);

#undef IIORAP_IMPL_BODY
#undef IIORAP_IMPL_ARG_NAMES
#undef IIORAP_IMPL_ARGS

namespace {

struct ServiceParams {
  bool fake_{false};
  std::shared_ptr<manager::EventManager> event_manager_;
};

static std::atomic<bool> s_service_started_{false};
static std::atomic<bool> s_service_params_ready_{false};

// TODO: BinderService constructs IIorapImpl,
// but how do I get a pointer to it afterwards?
//
// This is a workaround for that, by using a global.
static ServiceParams s_service_params_;
static std::atomic<ServiceParams*> s_service_params_atomic_;

// Convert an android::String16 (UTF-16) to a UTF-8 std::string.
static std::string String16ToStdString(const ::android::String16& s16) {
  std::u16string u16{s16.string()};
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> convert;

  std::string res = convert.to_bytes(u16);
  return res;
}

}  // namespace anonymous

class IIorapImpl::Impl {
  // ITaskListener implementation for iorap::manager::EventManager.
  struct EventManagerTaskCallbacks : public iorap::manager::TaskResultCallbacks {
    explicit EventManagerTaskCallbacks(iorap::borrowed<IIorapImpl::Impl*> impl) {
      CHECK(impl != nullptr);
      impl_ = impl;
    }

    virtual void OnProgress(iorap::binder::RequestId request_id, iorap::binder::TaskResult task_result) override {
      impl_->ReplyWithResult(request_id, /*completed*/false, std::move(task_result));
    }
    virtual void OnComplete(iorap::binder::RequestId request_id, iorap::binder::TaskResult task_result) override {
      impl_->ReplyWithResult(request_id, /*completed*/true, std::move(task_result));
    }

    virtual ~EventManagerTaskCallbacks() {}

    iorap::borrowed<IIorapImpl::Impl*> impl_;
  };

 public:
  ~Impl() {
    package_manager_->UnregisterPackageChangeObserver(package_change_observer_);
  }

  void SetTaskListener(const ::android::sp<ITaskListener>& listener) {
    ::android::sp<ITaskListener> old_listener = listener_;
    if (old_listener != nullptr && listener != nullptr) {
      LOG(WARNING) << "IIorap::setTaskListener: already had a task listener set";
    }
    listener_ = listener;
  }

  void ReplyWithResult(const RequestId& request_id, TaskResult::State result_state) {
    ::android::sp<ITaskListener> listener = listener_;
    if (listener == nullptr) {
      // No listener. Cannot send anything back to the client.
      // This could be normal, e.g. client had set listener to null before disconnecting.
      LOG(DEBUG) << "Drop result, no listener registered.";
      // TODO: print the result with ostream operator<<
      return;
    }

    TaskResult result;
    result.state = result_state;

    // TODO: verbose, not info.
    if (result_state == TaskResult::State::kCompleted) {
      LOG(VERBOSE) << "ITaskListener::onComplete (request_id=" << request_id.request_id << ")";
      listener->onComplete(request_id, result);
    } else {
      LOG(VERBOSE) << "ITaskListener::onProgress (request_id=" << request_id.request_id << ")";
      listener->onProgress(request_id, result);
    }
  }

  void ReplyWithResult(const RequestId& request_id, bool completed, TaskResult result) {
    ::android::sp<ITaskListener> listener = listener_;
    if (listener == nullptr) {
      // No listener. Cannot send anything back to the client.
      // This could be normal, e.g. client had set listener to null before disconnecting.
      LOG(DEBUG) << "Drop result, no listener registered.";
      // TODO: print the result with ostream operator<<
      return;
    }

    // TODO: verbose, not info.
    if (completed) {
      LOG(VERBOSE) << "ITaskListener::onComplete (request_id=" << request_id.request_id << ")";
      listener->onComplete(request_id, result);
    } else {
      LOG(VERBOSE) << "ITaskListener::onProgress (request_id=" << request_id.request_id << ")";
      listener->onProgress(request_id, result);
    }
  }

  bool OnAppLaunchEvent(const RequestId& request_id,
                        const AppLaunchEvent& event) {
    if (MaybeHandleFakeBehavior(request_id)) {
      return true;
    }

    return service_params_.event_manager_->OnAppLaunchEvent(request_id, event);
  }

  bool OnDexOptEvent(const RequestId& request_id, const DexOptEvent& event) {
    if (MaybeHandleFakeBehavior(request_id)) {
      return true;
    }

    return service_params_.event_manager_->OnDexOptEvent(request_id, event);
  }

  bool  OnJobScheduledEvent(const RequestId& request_id,
                            const JobScheduledEvent& event) {
    if (MaybeHandleFakeBehavior(request_id)) {
      return true;
    }

    return service_params_.event_manager_->OnJobScheduledEvent(request_id, event);
  }

  void Dump(/*borrow*/::android::Printer& printer,
            const ::android::Vector<::android::String16>& args) {

    if (args.size() == 0) {
      service_params_.event_manager_->Dump(/*borrow*/printer);
      return;
    }

    ::android::String16 arg_prev;
    for (const ::android::String16& arg : args) {
      bool unknown = false;
      if (arg == ::android::String16("--all") || arg == ::android::String16("-a")) {
        // using 'dumpsys' or 'bugreport' passes a single '-a' flag to this function.
        service_params_.event_manager_->Dump(/*borrow*/printer);
      } else if (arg == ::android::String16("--refresh-properties")) {
        service_params_.event_manager_->RefreshSystemProperties(/*borrow*/printer);
        printer.printLine("System properties refreshed.");
      } else if (arg == ::android::String16("--compile-package")) {
        // Intentionally left blank.
      } else if (arg_prev == ::android::String16("--compile-package")) {
        std::string package_name = String16ToStdString(arg);

        if (!service_params_.event_manager_->CompilePackage(/*borrow*/printer, package_name)) {
          printer.printFormatLine("Failed to compile package %s.", package_name.c_str());
        } else {
          printer.printFormatLine("Package %s compiled.", package_name.c_str());
        }
      } else if (arg == ::android::String16("--purge-package")) {
        // Intentionally left blank.
      } else if (arg_prev == ::android::String16("--purge-package")) {
        std::string package_name = String16ToStdString(arg);

        if (!service_params_.event_manager_->PurgePackage(/*borrow*/printer, package_name)) {
          printer.printFormatLine("Failed to purge package %s.", package_name.c_str());
        } else {
          printer.printFormatLine("Package %s purged.", package_name.c_str());
        }
      } else {
        unknown = true;
      }

      if (unknown && arg != ::android::String16("--help")) {
        printer.printLine("Invalid arguments.");
        printer.printLine("");

        printer.printLine("Arguments were:");
        for (const ::android::String16& arg16 : args) {
          printer.printFormatLine("  '%s'", String16ToStdString(arg16).c_str());
        }
        printer.printLine("");
      }

      if (unknown || arg == ::android::String16("--help")) {
        printer.printLine("Iorapd dumpsys commands:");
        printer.printLine("  (none),--all,-a: Print state information for debugging iorapd.");
        printer.printLine("  --help: Display this help menu");
        printer.printLine("  --compile-package <name>: Compile single package on device");
        printer.printLine("  --purge-package <name>: Delete database entries/files for package");
        printer.printLine("  --refresh-properties: Refresh system properties");
        return;
      }

      arg_prev = arg;
    }
  }

  void HandleFakeBehavior(const RequestId& request_id) {
    DCHECK(service_params_.fake_);

    // Send these dummy callbacks for testing only.
    ReplyWithResult(request_id, TaskResult::State::kBegan);
    ReplyWithResult(request_id, TaskResult::State::kOngoing);
    ReplyWithResult(request_id, TaskResult::State::kCompleted);
  }

  // TODO: Subclass IIorap with a separate fake implementation.
  bool MaybeHandleFakeBehavior(const RequestId& request_id) {
    if (service_params_.fake_) {
      HandleFakeBehavior(request_id);
      return true;
    }

    return false;
  }

  ::android::sp<ITaskListener> listener_;

  Impl(ServiceParams p) : service_params_{std::move(p)}, event_manager_callbacks_{new EventManagerTaskCallbacks{this}} {
    CHECK(service_params_.event_manager_ != nullptr);

    service_params_.event_manager_->SetTaskResultCallbacks(
      std::static_pointer_cast<manager::TaskResultCallbacks>(event_manager_callbacks_));

    // Init the package change observer.
    package_manager_ = PackageManagerRemote::Create();

    if (package_manager_ == nullptr) {
      LOG(FATAL) << "Failed to get package manager service in IIorapImpl::Impl";
      return;
    }

    package_change_observer_ =
        new PackageChangeObserver(service_params_.event_manager_);
    package_manager_death_recipient_ =
        new PackageManagerDeathRecipient(package_manager_, package_change_observer_);

    package_manager_->RegisterPackageChangeObserver(package_change_observer_);
    package_manager_->
        RegisterPackageManagerDeathRecipient(package_manager_death_recipient_);

  }

  ServiceParams service_params_;
  std::shared_ptr<EventManagerTaskCallbacks> event_manager_callbacks_;
  android::sp<PackageChangeObserver> package_change_observer_;
  android::sp<PackageManagerDeathRecipient> package_manager_death_recipient_;
  std::shared_ptr<PackageManagerRemote> package_manager_;
};

using Impl = IIorapImpl::Impl;

IIorapImpl::IIorapImpl() {
  // Acquire edge of synchronizes-with IIorapImpl::Start().
  CHECK(s_service_params_ready_.load());
  // Do not turn this into a DCHECK, the above atomic load
  // must happen-before the read of s_service_params_ready_.
  impl_.reset(new Impl(std::move(s_service_params_)));
}

namespace {
  static bool started_ = false;
}
bool IIorapImpl::Start(std::shared_ptr<manager::EventManager> event_manager) {
  if (s_service_started_.load()) {  // Acquire-edge (see bottom of function).
    // Note: Not meant to be idempotent. Two threads could race, and the second
    // one would likely fail the publish.

    LOG(ERROR) << "service was already started";
    return false;  // Already started
  }

  CHECK(event_manager != nullptr);

  {
    // This block of code needs to happen-before IIorapImpl::IIorapImpl.

    // TODO: There should be a simpler way of passing down
    // this data which doesn't involve globals and memory synchronization.
    ServiceParams* p = &s_service_params_;
    // TODO: move all property reads to a dedicated Config class.
    p->fake_ = ::android::base::GetBoolProperty("iorapd.binder.fake", /*default*/false);
    p->event_manager_ = std::move(event_manager);

    // Release edge of synchronizes-with IIorapImpl::IIorapImpl.
    s_service_params_ready_.store(true);
  }

  ::android::IPCThreadState::self()->disableBackgroundScheduling(/*disable*/true);
  ::android::status_t ret = android::BinderService<IIorapImpl>::publish();
  if (ret != android::OK) {
    LOG(ERROR) << "BinderService::publish failed with error code: " << ret;
    return false;
  }

  android::sp<android::ProcessState> ps = android::ProcessState::self();
  // Reduce thread consumption by only using 1 thread.
  // We should also be able to leverage this by avoiding locks, etc.
  ps->setThreadPoolMaxThreadCount(/*maxThreads*/1);
  ps->startThreadPool();
  ps->giveThreadPoolName();

  // Release edge synchronizes-with the top of this function.
  s_service_started_.store(true);

  // TODO: IIRC thread-start(t1) synchronizes-with t1.main, so we should be able
  // to delete the majority of atomics for any pre-thread-start initialization...

  return true;
}

::android::status_t IIorapImpl::dump(int fd, const ::android::Vector<::android::String16>& args) {

  ::android::FdPrinter printer{fd};

  impl_->Dump(printer, args);

  return ::android::NO_ERROR;
}

namespace {

#define MAYBE_HAVE_FAKE_BEHAVIOR(self, request_id) \
  if (self->MaybeHandleFakeBehavior(request_id)) { return ::android::binder::Status::ok(); }

template <typename ... Args>
Status SendArgs(const char* function_name,
                Impl* self,
                const RequestId& request_id,
                Args&&... /*rest*/) {
  LOG(VERBOSE) << "IIorap::" << function_name << " (request_id = " << request_id.request_id << ")";

  MAYBE_HAVE_FAKE_BEHAVIOR(self, request_id);

  // TODO: implementation.
  LOG(ERROR) << "IIorap::" << function_name << " -- not implemented for real code";
  return Status::fromStatusT(::android::INVALID_OPERATION);
}

template <typename ... Args>
Status SendArgs(const char* function_name, Impl* self, Args&&... rest) {
  DCHECK_EQ(std::string(function_name), "setTaskListener");
  LOG(VERBOSE) << "IIorap::setTaskListener";
  self->SetTaskListener(std::forward<Args&&>(rest)...);

  return Status::ok();
}

template <typename ... Args>
Status SendArgs(const char* function_name,
                Impl* self,
                const RequestId& request_id,
                const AppLaunchEvent& app_launch_event) {
  DCHECK_EQ(std::string(function_name), "onAppLaunchEvent");
  LOG(VERBOSE) << "IIorap::onAppLaunchEvent";

  MAYBE_HAVE_FAKE_BEHAVIOR(self, request_id);

  if (self->OnAppLaunchEvent(request_id, app_launch_event)) {
    return Status::ok();
  } else {
    // TODO: I suppose this should write out an exception back,
    // like a service-specific error or something.
    //
    // It depends on whether or not we even have any synchronous
    // errors.
    //
    // Most of the work here is done async, so it should handle
    // async callbacks.
    return Status::fromStatusT(::android::BAD_VALUE);
  }
}

template <typename ... Args>
Status SendArgs(const char* function_name,
                Impl* self,
                const RequestId& request_id,
                const DexOptEvent& event) {
  DCHECK_EQ(std::string(function_name), "onDexOptEvent");
  LOG(VERBOSE) << "IIorap::onDexOptEvent";

  MAYBE_HAVE_FAKE_BEHAVIOR(self, request_id);

  if (self->OnDexOptEvent(request_id, event)) {
    return Status::ok();
  } else {
    return Status::fromStatusT(::android::BAD_VALUE);
  }
}

template <typename ... Args>
Status SendArgs(const char* function_name,
                Impl* self,
                const RequestId& request_id,
                const JobScheduledEvent& event) {
  DCHECK_EQ(std::string(function_name), "onJobScheduledEvent");
  LOG(VERBOSE) << "IIorap::onJobScheduledEvent";

  MAYBE_HAVE_FAKE_BEHAVIOR(self, request_id);

  if (self->OnJobScheduledEvent(request_id, event)) {
    return Status::ok();
  } else {
    // TODO: I suppose this should write out an exception back,
    // like a service-specific error or something.
    //
    // It depends on whether or not we even have any synchronous
    // errors.
    //
    // Most of the work here is done async, so it should handle
    // async callbacks.
    return Status::fromStatusT(::android::BAD_VALUE);
  }
}

template <typename ... Args>
Status Send(const char* function_name, Args&&... args) {
  LOG(VERBOSE) << "IIorap::Send(" << function_name << ")";

  return SendArgs(function_name, std::forward<Args>(args)...);
}
}  // namespace <anonymous>

}  // namespace binder
}  // namespace iorap
