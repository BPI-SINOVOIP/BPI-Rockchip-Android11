/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "binder/package_version_map.h"
#include "common/debug.h"
#include "common/expected.h"
#include "common/printer.h"
#include "common/rx_async.h"
#include "common/trace.h"
#include "db/app_component_name.h"
#include "db/file_models.h"
#include "db/models.h"
#include "maintenance/controller.h"
#include "maintenance/db_cleaner.h"
#include "manager/event_manager.h"
#include "perfetto/rx_producer.h"
#include "prefetcher/read_ahead.h"
#include "prefetcher/task_id.h"

#include <android-base/chrono_utils.h>
#include <android-base/strings.h>
#include <android-base/properties.h>
#include <rxcpp/rx.hpp>
#include <server_configurable_flags/get_flags.h>
#include <utils/misc.h>
#include <utils/Trace.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <type_traits>
#include <unordered_map>

using rxcpp::observe_on_one_worker;

namespace iorap::manager {

using binder::AppLaunchEvent;
using binder::DexOptEvent;
using binder::JobScheduledEvent;
using binder::RequestId;
using binder::TaskResult;

using common::AsyncPool;
using common::RxAsync;

using perfetto::PerfettoStreamCommand;
using perfetto::PerfettoTraceProto;

using db::AppComponentName;

static std::atomic<bool> s_tracing_allowed{false};
static std::atomic<bool> s_readahead_allowed{false};
static std::atomic<uint64_t> s_min_traces{3};

struct PackageBlacklister {
  // "x.y.z;foo.bar.baz" colon-separated list of substrings
  PackageBlacklister(std::string blacklist_string) {
    LOG(VERBOSE) << "Configuring package blacklister with string: " << blacklist_string;

    std::vector<std::string> split = ::android::base::Split(blacklist_string, ";");

    // Ignore any l/r whitespace or empty strings.
    for (const std::string& s : split) {
      std::string t = ::android::base::Trim(s);
      if (!t.empty()) {
        LOG(INFO) << "Blacklisted package: " << t << "; will not optimize.";
        packages_.push_back(t);
      }
    }
  }

  PackageBlacklister() = default;

  bool IsBlacklisted(const std::string& package_name) const {
    return std::find(packages_.begin(), packages_.end(), package_name) != packages_.end();
  }

  bool IsBlacklisted(const AppComponentName& component_name) const {
    return IsBlacklisted(component_name.package);
  }

  bool IsBlacklisted(const std::optional<AppComponentName>& component_name) const {
    return component_name.has_value() && IsBlacklisted(component_name->package);
  }

 private:
  std::vector<std::string> packages_;
};

using PackageVersionMap = std::unordered_map<std::string, int64_t>;

// Main logic of the #OnAppLaunchEvent scan method.
//
// All functions are called from the same thread as the event manager
// functions.
//
// This is a data type, it's moved (std::move) around from one iteration
// of #scan to another.
struct AppLaunchEventState {
  std::optional<AppComponentName> component_name_;
  // Sequence ID is shared amongst the same app launch sequence,
  // but changes whenever a new app launch sequence begins.
  size_t sequence_id_ = static_cast<size_t>(-1);
  std::optional<AppLaunchEvent::Temperature> temperature_;

  // Push data to perfetto rx chain for associating
  // the raw_trace with the history_id.
  std::optional<rxcpp::subscriber<int>> history_id_subscriber_;
  rxcpp::observable<int> history_id_observable_;

  std::optional<uint64_t> intent_started_ns_;
  std::optional<uint64_t> total_time_ns_;

  // Used by kReportFullyDrawn to find the right history_id.
  // We assume no interleaving between different sequences.
  // This assumption is checked in the Java service code.
  std::optional<uint64_t> recent_history_id_;

  // labeled as 'shared' due to rx not being able to handle move-only objects.
  // lifetime: in practice equivalent to unique_ptr.
  std::shared_ptr<prefetcher::ReadAhead> read_ahead_;
  bool allowed_readahead_{true};
  bool is_read_ahead_{false};
  std::optional<prefetcher::TaskId> read_ahead_task_;

  bool allowed_tracing_{true};
  bool is_tracing_{false};
  std::optional<rxcpp::composite_subscription> rx_lifetime_;
  std::vector<rxcpp::composite_subscription> rx_in_flight_;

  PackageBlacklister package_blacklister_{};

  borrowed<perfetto::RxProducerFactory*> perfetto_factory_;  // not null
  borrowed<observe_on_one_worker*> thread_;  // not null
  borrowed<observe_on_one_worker*> io_thread_;  // not null
  borrowed<AsyncPool*> async_pool_;  // not null

  std::shared_ptr<binder::PackageVersionMap> version_map_;

  explicit AppLaunchEventState(borrowed<perfetto::RxProducerFactory*> perfetto_factory,
                               bool allowed_readahead,
                               bool allowed_tracing,
                               PackageBlacklister package_blacklister,
                               borrowed<observe_on_one_worker*> thread,
                               borrowed<observe_on_one_worker*> io_thread,
                               borrowed<AsyncPool*> async_pool,
                               std::shared_ptr<binder::PackageVersionMap> version_map)
    : read_ahead_{std::make_shared<prefetcher::ReadAhead>()}
  {
    perfetto_factory_ = perfetto_factory;
    DCHECK(perfetto_factory_ != nullptr);

    allowed_readahead_ = allowed_readahead;
    allowed_tracing_ = allowed_tracing;

    package_blacklister_ = package_blacklister;

    thread_ = thread;
    DCHECK(thread_ != nullptr);

    io_thread_ = io_thread;
    DCHECK(io_thread_ != nullptr);

    async_pool_ = async_pool;
    DCHECK(async_pool_ != nullptr);

    version_map_ = version_map;
    DCHECK(version_map_ != nullptr);
  }

  // Updates the values in this struct only as a side effect.
  //
  // May create and fire a new rx chain on the same threads as passed
  // in by the constructors.
  void OnNewEvent(const AppLaunchEvent& event) {
    LOG(VERBOSE) << "AppLaunchEventState#OnNewEvent: " << event;

    android::ScopedTrace trace_db_init{ATRACE_TAG_ACTIVITY_MANAGER,
                                       "IorapNativeService::OnAppLaunchEvent"};

    using Type = AppLaunchEvent::Type;

    DCHECK_GE(event.sequence_id, 0);
    sequence_id_ = static_cast<size_t>(event.sequence_id);
    allowed_readahead_ = s_readahead_allowed;
    allowed_tracing_ = s_tracing_allowed;

    switch (event.type) {
      case Type::kIntentStarted: {
        const std::string& package_name = event.intent_proto->component().package_name();
        const std::string& class_name = event.intent_proto->component().class_name();
        AppComponentName component_name{package_name, class_name};
        component_name = component_name.Canonicalize();
        component_name_ = component_name;

        if (package_blacklister_.IsBlacklisted(component_name)) {
          LOG(DEBUG) << "kIntentStarted: package " << component_name.package
                     << " ignored due to blacklisting.";
          break;
        }

        // Create a new history ID chain for each new app start-up sequence.
        auto history_id_observable = rxcpp::observable<>::create<int>(
          [&](rxcpp::subscriber<int> subscriber) {
            history_id_subscriber_ = std::move(subscriber);
            LOG(VERBOSE) << " set up the history id subscriber ";
          })
          .tap([](int history_id) { LOG(VERBOSE) << " tap rx history id = " << history_id; })
          .replay(1);  // Remember the history id in case we subscribe too late.

        history_id_observable_ = history_id_observable;

        // Immediately turn observable hot, creating the subscriber.
        history_id_observable.connect();

        DCHECK(!IsTracing());

        // The time should be set before perfetto tracing.
        // Record the timestamp even no perfetto tracing is triggered,
        // because the tracing may start in the following ActivityLaunched
        // event. Otherwise, there will be no starting timestamp and
        // trace without starting timestamp is not considered for compilation.
        if (event.timestamp_nanos >= 0) {
          intent_started_ns_ = event.timestamp_nanos;
        } else {
          LOG(WARNING) << "Negative event timestamp: " << event.timestamp_nanos;
        }

        // Optimistically start tracing if we have the activity in the intent.
        if (!event.intent_proto->has_component()) {
          // Can't do anything if there is no component in the proto.
          LOG(VERBOSE) << "AppLaunchEventState#OnNewEvent: no component, can't trace";
          break;
        }

        if (allowed_readahead_) {
          StartReadAhead(sequence_id_, component_name);
        }
        if (allowed_tracing_ && !IsReadAhead()) {
          rx_lifetime_ = StartTracing(std::move(component_name));
        }

        break;
      }
      case Type::kIntentFailed:
        if (package_blacklister_.IsBlacklisted(component_name_)) {
          LOG(VERBOSE) << "kIntentFailed: package " << component_name_->package
                       << " ignored due to blacklisting.";
          break;
        }

        AbortTrace();
        AbortReadAhead();

        if (history_id_subscriber_) {
          history_id_subscriber_->on_error(rxcpp::util::make_error_ptr(
            std::ios_base::failure("Aborting due to intent failed")));
          history_id_subscriber_ = std::nullopt;
        }

        break;
      case Type::kActivityLaunched: {
        const std::string& title = event.activity_record_proto->identifier().title();
        if (!AppComponentName::HasAppComponentName(title)) {
          // Proto comment claim this is sometimes a window title.
          // We need the actual 'package/component' here, so just ignore it if it's a title.
          LOG(WARNING) << "App launched without a component name: " << event;
          break;
        }

        AppComponentName component_name = AppComponentName::FromString(title);
        component_name = component_name.Canonicalize();
        component_name_ = component_name;

        if (package_blacklister_.IsBlacklisted(component_name_)) {
          LOG(VERBOSE) << "kActivityLaunched: package " << component_name_->package
                       << " ignored due to blacklisting.";
          break;
        }

        // Cancel tracing for warm/hot.
        // Restart tracing if the activity was unexpected.

        AppLaunchEvent::Temperature temperature = event.temperature;
        temperature_ = temperature;
        if (temperature != AppLaunchEvent::Temperature::kCold) {
          LOG(DEBUG) << "AppLaunchEventState#OnNewEvent aborting trace due to non-cold temperature";

          AbortTrace();
          AbortReadAhead();
        } else if (!IsTracing() && !IsReadAhead()) {  // and the temperature is Cold.
          // Start late trace when intent didn't have a component name
          LOG(VERBOSE) << "AppLaunchEventState#OnNewEvent need to start new trace";

          if (allowed_readahead_ && !IsReadAhead()) {
            StartReadAhead(sequence_id_, component_name);
          }
          if (allowed_tracing_ && !IsTracing() && !IsReadAhead()) {
            rx_lifetime_ = StartTracing(std::move(component_name));
          }
        } else {
          // FIXME: match actual component name against intent component name.
          // abort traces if they don't match.

          if (allowed_tracing_) {
            LOG(VERBOSE) << "AppLaunchEventState#OnNewEvent already tracing";
          }
          LOG(VERBOSE) << "AppLaunchEventState#OnNewEvent already doing readahead";
        }
        break;
      }
      case Type::kActivityLaunchFinished:
        if (package_blacklister_.IsBlacklisted(component_name_)) {
          LOG(VERBOSE) << "kActivityLaunchFinished: package " << component_name_->package
                       << " ignored due to blacklisting.";
          break;
        }

        if (event.timestamp_nanos >= 0) {
           total_time_ns_ = event.timestamp_nanos;
        }
        RecordDbLaunchHistory();
        // Finish tracing and collect trace buffer.
        //
        // TODO: this happens automatically when perfetto finishes its
        // trace duration.
        if (IsTracing()) {
          MarkPendingTrace();
        }
        FinishReadAhead();
        break;
      case Type::kActivityLaunchCancelled:
        if (package_blacklister_.IsBlacklisted(component_name_)) {
          LOG(VERBOSE) << "kActivityLaunchCancelled: package " << component_name_->package
                       << " ignored due to blacklisting.";
          break;
        }

        // Abort tracing.
        AbortTrace();
        AbortReadAhead();
        break;
      case Type::kReportFullyDrawn: {
        if (package_blacklister_.IsBlacklisted(component_name_)) {
          LOG(VERBOSE) << "kReportFullyDrawn: package " << component_name_->package
                       << " ignored due to blacklisting.";
          break;
        }

        if (!recent_history_id_) {
          LOG(WARNING) << "Dangling kReportFullyDrawn event";
          return;
        }
        UpdateReportFullyDrawn(*recent_history_id_, event.timestamp_nanos);
        recent_history_id_ = std::nullopt;
        break;
      }
      default:
        DCHECK(false) << "invalid type: " << event;  // binder layer should've rejected this.
        LOG(ERROR) << "invalid type: " << event;  // binder layer should've rejected this.
    }
  }

  // Is there an in-flight readahead task currently?
  bool IsReadAhead() const {
    return read_ahead_task_.has_value();
  }

  // Gets the compiled trace.
  // If a compiled trace exists in sqlite, use that one. Otherwise, try
  // to find a prebuilt one.
  std::optional<std::string> GetCompiledTrace(const AppComponentName& component_name) {
    ScopedFormatTrace atrace_get_compiled_trace(ATRACE_TAG_ACTIVITY_MANAGER, "GetCompiledTrace");
    // Firstly, try to find the compiled trace from sqlite.
    android::base::Timer timer{};
    db::DbHandle db{db::SchemaModel::GetSingleton()};
    std::optional<int> version =
        version_map_->GetOrQueryPackageVersion(component_name.package);
    if (!version) {
      LOG(DEBUG) << "The version is NULL, maybe package manager is down.";
      return std::nullopt;
    }
    db::VersionedComponentName vcn{component_name.package,
                                   component_name.activity_name,
                                   *version};

    std::optional<db::PrefetchFileModel> compiled_trace =
          db::PrefetchFileModel::SelectByVersionedComponentName(db, vcn);

    std::chrono::milliseconds duration_ms = timer.duration();
    LOG(DEBUG) << "EventManager: Looking up compiled trace done in "
               << duration_ms.count() // the count of ticks.
               << "ms.";

    if (compiled_trace) {
      if (std::filesystem::exists(compiled_trace->file_path)) {
        return compiled_trace->file_path;
      } else {
        LOG(DEBUG) << "Compiled trace in sqlite doesn't exists. file_path: "
                   << compiled_trace->file_path;
      }
    }

    LOG(DEBUG) << "Cannot find compiled trace in sqlite for package_name: "
               << component_name.package
               << " activity_name: "
               << component_name.activity_name;

    // If sqlite doesn't have the compiled trace, try the prebuilt path.
    std::string file_path = "/product/iorap-trace/";
    file_path += component_name.ToMakeFileSafeEncodedPkgString();
    file_path += ".compiled_trace.pb";

    if (std::filesystem::exists(file_path)) {
      return file_path;
    }

    LOG(DEBUG) << "Prebuilt compiled trace doesn't exists. file_path: "
               << file_path;

    return std::nullopt;
  }

  void StartReadAhead(size_t id, const AppComponentName& component_name) {
    DCHECK(allowed_readahead_);
    DCHECK(!IsReadAhead());

    std::optional<std::string> file_path = GetCompiledTrace(component_name);
    if (!file_path) {
      LOG(VERBOSE) << "Cannot find a compiled trace.";
      return;
    }

    prefetcher::TaskId task{id, *file_path};
    read_ahead_->BeginTask(task);
    // TODO: non-void return signature?

    read_ahead_task_ = std::move(task);
  }

  void FinishReadAhead() {
    // if no readahead task exist, do nothing.
    if (!IsReadAhead()){
      return;
    }

    read_ahead_->FinishTask(*read_ahead_task_);
    read_ahead_task_ = std::nullopt;
  }

  void AbortReadAhead() {
    FinishReadAhead();
  }

  bool IsTracing() const {
    return is_tracing_;
  }

  std::optional<rxcpp::composite_subscription> StartTracing(
      AppComponentName component_name) {
    DCHECK(allowed_tracing_);
    DCHECK(!IsTracing());

    auto /*observable<PerfettoStreamCommand>*/ perfetto_commands =
      rxcpp::observable<>::just(PerfettoStreamCommand::kStartTracing)
          // wait 1x
          .concat(
              // Pick a value longer than the perfetto config delay_ms, so that we send
              // 'kShutdown' after tracing has already finished.
              rxcpp::observable<>::interval(std::chrono::milliseconds(10000))
                  .take(2)  // kStopTracing, kShutdown.
                  .map([](int value) {
                         // value is 1,2,3,...
                         return static_cast<PerfettoStreamCommand>(value);  // 1,2, ...
                       })
          );

    auto /*observable<PerfettoTraceProto>*/ trace_proto_stream =
        perfetto_factory_->CreateTraceStream(perfetto_commands);
    // This immediately connects to perfetto asynchronously.
    //
    // TODO: create a perfetto handle earlier, to minimize perfetto startup latency.

    rxcpp::composite_subscription lifetime;

    auto stream_via_threads = trace_proto_stream
      .tap([](const PerfettoTraceProto& trace_proto) {
             LOG(VERBOSE) << "StartTracing -- PerfettoTraceProto received (1)";
           })
      .combine_latest(history_id_observable_)
      .observe_on(*thread_)   // All work prior to 'observe_on' is handled on thread_.
      .subscribe_on(*thread_)   // All work prior to 'observe_on' is handled on thread_.
      .observe_on(*io_thread_)  // Write data on an idle-class-priority thread.
      .tap([](std::tuple<PerfettoTraceProto, int> trace_tuple) {
             LOG(VERBOSE) << "StartTracing -- PerfettoTraceProto received (2)";
           });

    std::optional<int> version =
        version_map_->GetOrQueryPackageVersion(component_name_->package);
    if (!version) {
      LOG(DEBUG) << "The version is NULL, maybe package manager is down.";
      return std::nullopt;
    }
    db::VersionedComponentName versioned_component_name{component_name.package,
                                                        component_name.activity_name,
                                                        *version};
    lifetime = RxAsync::SubscribeAsync(*async_pool_,
        std::move(stream_via_threads),
        /*on_next*/[versioned_component_name]
        (std::tuple<PerfettoTraceProto, int> trace_tuple) {
          PerfettoTraceProto& trace_proto = std::get<0>(trace_tuple);
          int history_id = std::get<1>(trace_tuple);

          db::PerfettoTraceFileModel file_model =
            db::PerfettoTraceFileModel::CalculateNewestFilePath(versioned_component_name);

          std::string file_path = file_model.FilePath();

          ScopedFormatTrace atrace_write_to_file(ATRACE_TAG_ACTIVITY_MANAGER,
                                                 "Perfetto Write Trace To File %s",
                                                 file_path.c_str());

          if (!file_model.MkdirWithParents()) {
            LOG(ERROR) << "Cannot save TraceBuffer; failed to mkdirs " << file_path;
            return;
          }

          if (!trace_proto.WriteFullyToFile(file_path)) {
            LOG(ERROR) << "Failed to save TraceBuffer to " << file_path;
          } else {
            LOG(INFO) << "Perfetto TraceBuffer saved to file: " << file_path;

            ScopedFormatTrace atrace_update_raw_traces_table(
                ATRACE_TAG_ACTIVITY_MANAGER,
                "update raw_traces table history_id = %d",
                history_id);
            db::DbHandle db{db::SchemaModel::GetSingleton()};
            std::optional<db::RawTraceModel> raw_trace =
                db::RawTraceModel::Insert(db, history_id, file_path);

            if (!raw_trace) {
              LOG(ERROR) << "Failed to insert raw_traces for " << file_path;
            } else {
              LOG(VERBOSE) << "Inserted into db: " << *raw_trace;

              ScopedFormatTrace atrace_delete_older_files(
                  ATRACE_TAG_ACTIVITY_MANAGER,
                  "Delete older trace files for package");

              // Ensure we don't have too many files per-app.
              db::PerfettoTraceFileModel::DeleteOlderFiles(db, versioned_component_name);
            }
          }
        },
        /*on_error*/[](rxcpp::util::error_ptr err) {
          LOG(ERROR) << "Perfetto trace proto collection error: " << rxcpp::util::what(err);
        });

    is_tracing_ = true;

    return lifetime;
  }

  void AbortTrace() {
    LOG(VERBOSE) << "AppLaunchEventState - AbortTrace";

    // if the tracing is not running, do nothing.
    if (!IsTracing()){
      return;
    }

    is_tracing_ = false;
    if (rx_lifetime_) {
      // TODO: it would be good to call perfetto Destroy.

      rx_in_flight_.erase(std::remove(rx_in_flight_.begin(),
                                      rx_in_flight_.end(), *rx_lifetime_),
                          rx_in_flight_.end());

      LOG(VERBOSE) << "AppLaunchEventState - AbortTrace - Unsubscribe";
      rx_lifetime_->unsubscribe();

      rx_lifetime_.reset();
    }
  }

  void MarkPendingTrace() {
    LOG(VERBOSE) << "AppLaunchEventState - MarkPendingTrace";
    DCHECK(is_tracing_);
    DCHECK(rx_lifetime_.has_value());

    if (rx_lifetime_) {
      LOG(VERBOSE) << "AppLaunchEventState - MarkPendingTrace - lifetime moved";
      // Don't unsubscribe because that would cause the perfetto TraceBuffer
      // to get dropped on the floor.
      //
      // Instead, we want to let it finish and write it out to a file.
      rx_in_flight_.push_back(*std::move(rx_lifetime_));
      rx_lifetime_.reset();
    } else {
      LOG(VERBOSE) << "AppLaunchEventState - MarkPendingTrace - lifetime was empty";
    }

    is_tracing_ = false;
    // FIXME: how do we clear this vector?
  }

  void RecordDbLaunchHistory() {
    std::optional<db::AppLaunchHistoryModel> history = InsertDbLaunchHistory();

    // RecordDbLaunchHistory happens-after kIntentStarted
    if (!history_id_subscriber_.has_value()) {
      LOG(WARNING) << "Logic error? Should always have a subscriber here.";
      return;
    }

    // Ensure that the history id rx chain is terminated either with an error or with
    // the newly inserted app_launch_histories.id
    if (!history) {
      history_id_subscriber_->on_error(rxcpp::util::make_error_ptr(
          std::ios_base::failure("Failed to insert history id")));
      recent_history_id_ = std::nullopt;
    } else {
      // Note: we must have already subscribed, or this value will disappear.
      LOG(VERBOSE) << "history_id_subscriber on_next history_id=" << history->id;
      history_id_subscriber_->on_next(history->id);
      history_id_subscriber_->on_completed();

      recent_history_id_ = history->id;
    }
    history_id_subscriber_ = std::nullopt;
  }

  std::optional<db::AppLaunchHistoryModel> InsertDbLaunchHistory() {
    // TODO: deferred queue into a different lower priority thread.
    if (!component_name_ || !temperature_) {
      LOG(VERBOSE) << "Skip RecordDbLaunchHistory, no component name available.";

      return std::nullopt;
    }

    android::ScopedTrace trace{ATRACE_TAG_ACTIVITY_MANAGER,
                               "IorapNativeService::RecordDbLaunchHistory"};
    db::DbHandle db{db::SchemaModel::GetSingleton()};

    using namespace iorap::db;

    std::optional<int> version =
        version_map_->GetOrQueryPackageVersion(component_name_->package);
    if (!version) {
      LOG(DEBUG) << "The version is NULL, maybe package manager is down.";
      return std::nullopt;
    }
    std::optional<ActivityModel> activity =
        ActivityModel::SelectOrInsert(db,
                                      component_name_->package,
                                      *version,
                                      component_name_->activity_name);

    if (!activity) {
      LOG(WARNING) << "Failed to query activity row for : " << *component_name_;
      return std::nullopt;
    }

    auto temp = static_cast<db::AppLaunchHistoryModel::Temperature>(*temperature_);

    std::optional<AppLaunchHistoryModel> alh =
        AppLaunchHistoryModel::Insert(db,
                                      activity->id,
                                      temp,
                                      IsTracing(),
                                      IsReadAhead(),
                                      intent_started_ns_,
                                      total_time_ns_,
                                      // ReportFullyDrawn event normally occurs after this. Need update later.
                                      /* report_fully_drawn_ns= */ std::nullopt);
    //Repo
    if (!alh) {
      LOG(WARNING) << "Failed to insert app_launch_histories row";
      return std::nullopt;
    }

    LOG(VERBOSE) << "RecordDbLaunchHistory: " << *alh;
    return alh;
  }

  void UpdateReportFullyDrawn(int history_id, uint64_t timestamp_ns) {
    LOG(DEBUG) << "Update kReportFullyDrawn for history_id:"
               << history_id
               << " timestamp_ns: "
               << timestamp_ns;

    android::ScopedTrace trace{ATRACE_TAG_ACTIVITY_MANAGER,
                               "IorapNativeService::UpdateReportFullyDrawn"};
    db::DbHandle db{db::SchemaModel::GetSingleton()};

    bool result =
        db::AppLaunchHistoryModel::UpdateReportFullyDrawn(db,
                                                          history_id,
                                                          timestamp_ns);

    if (!result) {
      LOG(WARNING) << "Failed to update app_launch_histories row";
    }
  }
};

struct AppLaunchEventDefender {
  binder::AppLaunchEvent::Type last_event_type_{binder::AppLaunchEvent::Type::kUninitialized};

  enum class Result {
    kAccept,      // Pass-through the new event.
    kOverwrite,   // Overwrite the new event with a different event.
    kReject       // Completely reject the new event, it will not be delivered.
  };

  Result OnAppLaunchEvent(binder::RequestId request_id,
                          const binder::AppLaunchEvent& event,
                          binder::AppLaunchEvent* overwrite) {
    using Type = binder::AppLaunchEvent::Type;
    CHECK(overwrite != nullptr);

    // Ensure only legal transitions are allowed.
    switch (last_event_type_) {
      case Type::kUninitialized:
      case Type::kIntentFailed:
      case Type::kActivityLaunchCancelled:
      case Type::kReportFullyDrawn: {  // From a terminal state, only go to kIntentStarted
        if (event.type != Type::kIntentStarted) {
          LOG(WARNING) << "Rejecting transition from " << last_event_type_ << " to " << event.type;
          last_event_type_ = Type::kUninitialized;
          return Result::kReject;
        } else {
          LOG(VERBOSE) << "Accept transition from " << last_event_type_ << " to " << event.type;
          last_event_type_ = event.type;
          return Result::kAccept;
        }
      }
      case Type::kIntentStarted: {
        if (event.type == Type::kIntentFailed ||
            event.type == Type::kActivityLaunched) {
          LOG(VERBOSE) << "Accept transition from " << last_event_type_ << " to " << event.type;
          last_event_type_ = event.type;
          return Result::kAccept;
        } else {
          LOG(WARNING) << "Overwriting transition from kIntentStarted to "
                       << event.type << " into kIntentFailed";
          last_event_type_ = Type::kIntentFailed;

          *overwrite = event;
          overwrite->type = Type::kIntentFailed;
          return Result::kOverwrite;
        }
      }
      case Type::kActivityLaunched: {
        if (event.type == Type::kActivityLaunchFinished ||
            event.type == Type::kActivityLaunchCancelled) {
          LOG(VERBOSE) << "Accept transition from " << last_event_type_ << " to " << event.type;
          last_event_type_ = event.type;
          return Result::kAccept;
        } else {
          LOG(WARNING) << "Overwriting transition from kActivityLaunched to "
                       << event.type << " into kActivityLaunchCancelled";
          last_event_type_ = Type::kActivityLaunchCancelled;

          *overwrite = event;
          overwrite->type = Type::kActivityLaunchCancelled;
          return Result::kOverwrite;
        }
      }
      case Type::kActivityLaunchFinished: {
        if (event.type == Type::kIntentStarted ||
            event.type == Type::kReportFullyDrawn) {
          LOG(VERBOSE) << "Accept transition from " << last_event_type_ << " to " << event.type;
          last_event_type_ = event.type;
          return Result::kAccept;
        } else {
          LOG(WARNING) << "Rejecting transition from " << last_event_type_ << " to " << event.type;
          last_event_type_ = Type::kUninitialized;
          return Result::kReject;
        }
      }
    }
  }
};

// Convert callback pattern into reactive pattern.
struct AppLaunchEventSubject {
  using RefWrapper =
    std::reference_wrapper<const AppLaunchEvent>;

  AppLaunchEventSubject() {}

  void Subscribe(rxcpp::subscriber<RefWrapper> subscriber) {
    DCHECK(ready_ != true) << "Cannot Subscribe twice";

    subscriber_ = std::move(subscriber);

    // Release edge of synchronizes-with AcquireIsReady.
    ready_.store(true);
  }

  void OnNext(const AppLaunchEvent& e) {
    if (!AcquireIsReady()) {
      return;
    }

    if (!subscriber_->is_subscribed()) {
      return;
    }

    /*
     * TODO: fix upstream.
     *
     * Rx workaround: this fails to compile when
     * the observable is a reference type:
     *
     * external/Reactive-Extensions/RxCpp/Rx/v2/src/rxcpp/rx-observer.hpp:354:18: error: multiple overloads of 'on_next' instantiate to the same signature 'void (const iorap::binder::AppLaunchEvent &) const'
     *   virtual void on_next(T&&) const {};
     *
     * external/Reactive-Extensions/RxCpp/Rx/v2/src/rxcpp/rx-observer.hpp:353:18: note: previous declaration is here
     *   virtual void on_next(T&) const {};
     *
     * (The workaround is to use reference_wrapper instead
     *  of const AppLaunchEvent&)
     */
    subscriber_->on_next(std::cref(e));

  }

  void OnCompleted() {
    if (!AcquireIsReady()) {
      return;
    }

    subscriber_->on_completed();
  }

 private:
  bool AcquireIsReady() {
    // Synchronizes-with the release-edge in Subscribe.
    // This can happen much later, only once the subscription actually happens.

    // However, as far as I know, 'rxcpp::subscriber' is not thread safe,
    // (but the observable chain itself can be made thread-safe via #observe_on, etc).
    // so we must avoid reading it until it has been fully synchronized.
    //
    // TODO: investigate rxcpp subscribers and see if we can get rid of this atomics,
    // to make it simpler.
    return ready_.load();
  }

  // TODO: also track the RequestId ?

  std::atomic<bool> ready_{false};


  std::optional<rxcpp::subscriber<RefWrapper>> subscriber_;
};

// Convert callback pattern into reactive pattern.
struct JobScheduledEventSubject {
  JobScheduledEventSubject() {}

  void Subscribe(rxcpp::subscriber<std::pair<RequestId, JobScheduledEvent>> subscriber) {
    DCHECK(ready_ != true) << "Cannot Subscribe twice";

    subscriber_ = std::move(subscriber);

    // Release edge of synchronizes-with AcquireIsReady.
    ready_.store(true);
  }

  void OnNext(RequestId request_id, JobScheduledEvent e) {
    if (!AcquireIsReady()) {
      return;
    }

    if (!subscriber_->is_subscribed()) {
      return;
    }

    subscriber_->on_next(std::pair<RequestId, JobScheduledEvent>{std::move(request_id), std::move(e)});

  }

  void OnCompleted() {
    if (!AcquireIsReady()) {
      return;
    }

    subscriber_->on_completed();
  }

 private:
  bool AcquireIsReady() {
    // Synchronizes-with the release-edge in Subscribe.
    // This can happen much later, only once the subscription actually happens.

    // However, as far as I know, 'rxcpp::subscriber' is not thread safe,
    // (but the observable chain itself can be made thread-safe via #observe_on, etc).
    // so we must avoid reading it until it has been fully synchronized.
    //
    // TODO: investigate rxcpp subscribers and see if we can get rid of this atomics,
    // to make it simpler.
    return ready_.load();
  }

  // TODO: also track the RequestId ?

  std::atomic<bool> ready_{false};

  std::optional<rxcpp::subscriber<std::pair<RequestId, JobScheduledEvent>>> subscriber_;
};

std::ostream& operator<<(std::ostream& os, const android::content::pm::PackageChangeEvent& event) {
  os << "PackageChangeEvent{";
  os << "packageName=" << event.packageName << ",";
  os << "version=" << event.version << ",";
  os << "lastUpdateTimeMillis=" << event.lastUpdateTimeMillis;
  os << "}";
  return os;
}

class EventManager::Impl {
 public:
  Impl(/*borrow*/perfetto::RxProducerFactory& perfetto_factory)
    : perfetto_factory_(perfetto_factory),
      worker_thread_(rxcpp::observe_on_new_thread()),
      worker_thread2_(rxcpp::observe_on_new_thread()),
      io_thread_(perfetto::ObserveOnNewIoThread()) {
    // Try to create version map
    RetryCreateVersionMap();

    iorap::common::StderrLogPrinter printer{"iorapd"};
    RefreshSystemProperties(printer);

    rx_lifetime_ = InitializeRxGraph();
    rx_lifetime_jobs_ = InitializeRxGraphForJobScheduledEvents();

    android::add_sysprop_change_callback(&Impl::OnSyspropChanged, /*priority*/-10000);
  }

  void RetryCreateVersionMap() {
    android::base::Timer timer{};
    version_map_ = binder::PackageVersionMap::Create();
    std::chrono::milliseconds duration_ms = timer.duration();
    LOG(DEBUG) << "Got versions for "
               << version_map_->Size()
               << " packages in "
               << duration_ms.count()
               << "ms";
  }

  void SetTaskResultCallbacks(std::shared_ptr<TaskResultCallbacks> callbacks) {
    DCHECK(callbacks_.expired());
    callbacks_ = callbacks;
  }

  void Join() {
    async_pool_.Join();
  }

  bool OnAppLaunchEvent(RequestId request_id,
                        const AppLaunchEvent& event) {
    LOG(VERBOSE) << "EventManager::OnAppLaunchEvent("
                 << "request_id=" << request_id.request_id << ","
                 << event;

    // Filter any incoming events through a defender that enforces
    // that all state transitions are as contractually documented in
    // ActivityMetricsLaunchObserver's javadoc.
    AppLaunchEvent overwrite_event{};
    AppLaunchEventDefender::Result result =
        app_launch_event_defender_.OnAppLaunchEvent(request_id, event, /*out*/&overwrite_event);

    switch (result) {
      case AppLaunchEventDefender::Result::kAccept:
        app_launch_event_subject_.OnNext(event);
        return true;
      case AppLaunchEventDefender::Result::kOverwrite:
        app_launch_event_subject_.OnNext(overwrite_event);
        return false;
      case AppLaunchEventDefender::Result::kReject:
        // Intentionally left-empty: we drop the event completely.
        return false;
    }

    // In theory returns BAD_VALUE to the other side of this binder connection.
    // In practice we use 'oneway' flags so this doesn't matter on a regular build.
    return false;
  }

  bool OnDexOptEvent(RequestId request_id,
                     const DexOptEvent& event) {
    LOG(VERBOSE) << "EventManager::OnDexOptEvent("
                 << "request_id=" << request_id.request_id << ","
                 << event.package_name
                 << ")";

    return PurgePackage(event.package_name);
  }

  bool OnJobScheduledEvent(RequestId request_id,
                           const JobScheduledEvent& event) {
    LOG(VERBOSE) << "EventManager::OnJobScheduledEvent("
                 << "request_id=" << request_id.request_id << ",event=TODO).";

    job_scheduled_event_subject_.OnNext(std::move(request_id), event);

    return true;  // No errors.
  }

  bool OnPackageChanged(const android::content::pm::PackageChangeEvent& event) {
    LOG(DEBUG) << "Received " << event;
    if (event.isDeleted) {
      // Do nothing if the package is deleted rignt now.
      // The package will be removed from db during maintenance.
      return true;
    }
    // Update the version map.
    if (version_map_->Update(event.packageName, event.version)) {
      return true;
    }

    // Sometimes a package is updated without any version change.
    // Clean it up in this case.
    db::DbHandle db{db::SchemaModel::GetSingleton()};
    db::CleanUpFilesForPackage(db, event.packageName, event.version);
    return true;
  }

  void Dump(/*borrow*/::android::Printer& printer) {
    ::iorap::prefetcher::ReadAhead::Dump(printer);
    ::iorap::perfetto::PerfettoConsumerImpl::Dump(/*borrow*/printer);
    ::iorap::maintenance::Dump(db::SchemaModel::GetSingleton(), printer);
  }

  rxcpp::composite_subscription InitializeRxGraph() {
    LOG(VERBOSE) << "EventManager::InitializeRxGraph";

    app_launch_events_ = rxcpp::observable<>::create<AppLaunchEventRefWrapper>(
      [&](rxcpp::subscriber<AppLaunchEventRefWrapper> subscriber) {
        app_launch_event_subject_.Subscribe(std::move(subscriber));
      });

    rxcpp::composite_subscription lifetime;

    if (!tracing_allowed_) {
      LOG(WARNING) << "Tracing disabled by system property";
    }
    if (!readahead_allowed_) {
      LOG(WARNING) << "Readahead disabled by system property";
    }

    AppLaunchEventState initial_state{&perfetto_factory_,
                                      readahead_allowed_,
                                      tracing_allowed_,
                                      package_blacklister_,
                                      &worker_thread2_,
                                      &io_thread_,
                                      &async_pool_,
                                      version_map_};
    app_launch_events_
      .subscribe_on(worker_thread_)
      .scan(std::move(initial_state),
            [](AppLaunchEventState state, AppLaunchEventRefWrapper event) {
              state.OnNewEvent(event.get());
              return state;
            })
      .subscribe(/*out*/lifetime, [](const AppLaunchEventState& state) {
                   // Intentionally left blank.
                   (void)state;
                 });

    return lifetime;
  }

  // Runs the maintenance code to compile perfetto traces to compiled
  // trace.
  void StartMaintenance(bool output_text,
                        std::optional<std::string> inode_textcache,
                        bool verbose,
                        bool recompile,
                        uint64_t min_traces) {
    ScopedFormatTrace atrace_bg_scope(ATRACE_TAG_PACKAGE_MANAGER,
                                      "Background Job Scope");

    {
      ScopedFormatTrace atrace_update_versions(ATRACE_TAG_PACKAGE_MANAGER,
                                               "Update package versions map cache");
      // Update the version map.
      version_map_->UpdateAll();
    }

    db::DbHandle db{db::SchemaModel::GetSingleton()};
    {
      ScopedFormatTrace atrace_cleanup_db(ATRACE_TAG_PACKAGE_MANAGER,
                                          "Clean up obsolete data in database");
      // Cleanup the obsolete data in the database.
      maintenance::CleanUpDatabase(db, version_map_);
    }

    {
      ScopedFormatTrace atrace_compile_apps(ATRACE_TAG_PACKAGE_MANAGER,
                                            "Compile apps on device");
      // Compilation
      maintenance::ControllerParameters params{
        output_text,
        inode_textcache,
        verbose,
        recompile,
        min_traces,
        std::make_shared<maintenance::Exec>()};

      LOG(DEBUG) << "StartMaintenance: min_traces=" << min_traces;
      maintenance::CompileAppsOnDevice(db, params);
    }
  }

  rxcpp::composite_subscription InitializeRxGraphForJobScheduledEvents() {
    LOG(VERBOSE) << "EventManager::InitializeRxGraphForJobScheduledEvents";

    using RequestAndJobEvent = std::pair<RequestId, JobScheduledEvent>;

    job_scheduled_events_ = rxcpp::observable<>::create<RequestAndJobEvent>(
      [&](rxcpp::subscriber<RequestAndJobEvent> subscriber) {
        job_scheduled_event_subject_.Subscribe(std::move(subscriber));
      });

    rxcpp::composite_subscription lifetime;

    job_scheduled_events_
      .observe_on(worker_thread_)  // async handling.
      .tap([this](const RequestAndJobEvent& e) {
        LOG(VERBOSE) << "EventManager#JobScheduledEvent#tap(1) - job begins";
        this->NotifyProgress(e.first, TaskResult{TaskResult::State::kBegan});

        StartMaintenance(/*output_text=*/false,
                         /*inode_textcache=*/std::nullopt,
                         /*verbose=*/false,
                         /*recompile=*/false,
                         s_min_traces);

        // TODO: probably this shouldn't be emitted until most of the usual DCHECKs
        // (for example, validate a job isn't already started, the request is not reused, etc).
        // In this way we could block from the client until it sees 'kBegan' and Log.wtf otherwise.
      })
      .tap([](const RequestAndJobEvent& e) {
        // TODO. Actual work.
        LOG(VERBOSE) << "EventManager#JobScheduledEvent#tap(2) - job is being processed";

        // TODO: abort functionality for in-flight jobs.
        //
        // maybe something like scan that returns an observable<Job> + flat map to that job.
        // then we could unsubscribe from the scan to do a partial abort? need to try it and see if it works.
        //
        // other option is to create a new outer subscription for each job id which seems less ideal.
      })
      .subscribe(/*out*/lifetime,
        /*on_next*/
        [this](const RequestAndJobEvent& e) {
          LOG(VERBOSE) << "EventManager#JobScheduledEvent#subscribe - job completed";
          this->NotifyComplete(e.first, TaskResult{TaskResult::State::kCompleted});
        }
#if 0
        ,
        /*on_error*/
        [](rxcpp::util::error_ptr err) {
          LOG(ERROR) << "Scheduled job event failed: " << rxcpp::util::what(err);

          //std::shared_ptr<TaskResultCallbacks> callbacks = callbacks_.lock();
          //if (callbacks != nullptr) {
            // FIXME: How do we get the request ID back out of the error? Seems like a problem.
            // callbacks->OnComplete(, TaskResult{TaskResult::kError});
            // We may have to wrap with an iorap::expected instead of using on_error.
          //}

          // FIXME: need to add a 'OnErrorResumeNext' operator?
          DCHECK(false) << "forgot to implement OnErrorResumeNext";
        }
#endif
      );

    // TODO: error output should happen via an observable.

    return lifetime;
  }

  void NotifyComplete(RequestId request_id, TaskResult result) {
      std::shared_ptr<TaskResultCallbacks> callbacks = callbacks_.lock();
      if (callbacks != nullptr) {
        callbacks->OnComplete(std::move(request_id), std::move(result));
      } else {
        LOG(WARNING) << "EventManager: TaskResultCallbacks may have been released early";
      }
  }

  void NotifyProgress(RequestId request_id, TaskResult result) {
      std::shared_ptr<TaskResultCallbacks> callbacks = callbacks_.lock();
      if (callbacks != nullptr) {
        callbacks->OnProgress(std::move(request_id), std::move(result));
      } else {
        LOG(WARNING) << "EventManager: TaskResultCallbacks may have been released early";
      }
  }

  static void OnSyspropChanged() {
    LOG(DEBUG) << "OnSyspropChanged";
  }

  void RefreshSystemProperties(::android::Printer& printer) {
    // TODO: read all properties from one config class.
    // PH properties do not work if they contain ".". "_" was instead used here.
    const char* ph_namespace = "runtime_native_boot";
    tracing_allowed_ = server_configurable_flags::GetServerConfigurableFlag(
        ph_namespace,
        "iorap_perfetto_enable",
        ::android::base::GetProperty("iorapd.perfetto.enable", /*default*/"true")) == "true";
    s_tracing_allowed = tracing_allowed_;
    printer.printFormatLine("iorapd.perfetto.enable = %s", tracing_allowed_ ? "true" : "false");

    readahead_allowed_ = server_configurable_flags::GetServerConfigurableFlag(
        ph_namespace,
        "iorap_readahead_enable",
        ::android::base::GetProperty("iorapd.readahead.enable", /*default*/"true")) == "true";
    s_readahead_allowed = readahead_allowed_;
    printer.printFormatLine("iorapd.readahead.enable = %s", s_readahead_allowed ? "true" : "false");

    s_min_traces =
        ::android::base::GetUintProperty<uint64_t>("iorapd.maintenance.min_traces", /*default*/1);
    uint64_t min_traces = s_min_traces;
    printer.printFormatLine("iorapd.maintenance.min_traces = %" PRIu64, min_traces);

    package_blacklister_ = PackageBlacklister{
        /* Colon-separated string list of blacklisted packages, e.g.
         * "foo.bar.baz;com.fake.name" would blacklist {"foo.bar.baz", "com.fake.name"} packages.
         *
         * Blacklisted packages are ignored by iorapd.
         */
        server_configurable_flags::GetServerConfigurableFlag(
            ph_namespace,
            "iorap_blacklisted_packages",
            ::android::base::GetProperty("iorapd.blacklist_packages",
                                         /*default*/""))
    };

    LOG(DEBUG) << "RefreshSystemProperties";
  }

  bool PurgePackage(::android::Printer& printer, const std::string& package_name) {
    (void)printer;
    return PurgePackage(package_name);
  }

  bool PurgePackage(const std::string& package_name) {
    db::DbHandle db{db::SchemaModel::GetSingleton()};
    db::CleanUpFilesForPackage(db, package_name);
    LOG(DEBUG) << "PurgePackage: " << package_name;
    return true;
  }

  bool CompilePackage(::android::Printer& printer, const std::string& package_name) {
    (void)printer;

    ScopedFormatTrace atrace_compile_app(ATRACE_TAG_PACKAGE_MANAGER,
                                         "Compile one app on device");

    maintenance::ControllerParameters params{
      /*output_text*/false,
      /*inode_textcache*/std::nullopt,
      WOULD_LOG(VERBOSE),
      /*recompile*/false,
      s_min_traces,
      std::make_shared<maintenance::Exec>()};

    db::DbHandle db{db::SchemaModel::GetSingleton()};
    bool res = maintenance::CompileSingleAppOnDevice(db, std::move(params), package_name);
    LOG(DEBUG) << "CompilePackage: " << package_name;

    return res;
  }

  bool readahead_allowed_{true};

  perfetto::RxProducerFactory& perfetto_factory_;
  bool tracing_allowed_{true};

  PackageBlacklister package_blacklister_{};

  std::weak_ptr<TaskResultCallbacks> callbacks_;  // avoid cycles with weakptr.

  using AppLaunchEventRefWrapper = AppLaunchEventSubject::RefWrapper;
  rxcpp::observable<AppLaunchEventRefWrapper> app_launch_events_;
  AppLaunchEventSubject app_launch_event_subject_;
  AppLaunchEventDefender app_launch_event_defender_;

  rxcpp::observable<std::pair<RequestId, JobScheduledEvent>> job_scheduled_events_;
  JobScheduledEventSubject job_scheduled_event_subject_;

  rxcpp::observable<RequestId> completed_requests_;

  // regular-priority thread to handle binder callbacks.
  observe_on_one_worker worker_thread_;
  observe_on_one_worker worker_thread2_;
  // low priority idle-class thread for IO operations.
  observe_on_one_worker io_thread_;
  // async futures pool for async rx operations.
  AsyncPool async_pool_;

  rxcpp::composite_subscription rx_lifetime_;  // app launch events
  rxcpp::composite_subscription rx_lifetime_jobs_;  // job scheduled events

  // package version map
  std::shared_ptr<binder::PackageVersionMap> version_map_;

//INTENTIONAL_COMPILER_ERROR_HERE:
  // FIXME:
  // ok so we want to expose a 'BlockingSubscribe' or a 'Subscribe' or some kind of function
  // that the main thread can call. This would subscribe on all the observables we internally
  // have here (probably on an event-manager-dedicated thread for simplicity).
  //
  // ideally we'd just reuse the binder thread to handle the events but I'm not super sure,
  // maybe this already works with the identity_current_thread coordination?
};
using Impl = EventManager::Impl;

EventManager::EventManager(perfetto::RxProducerFactory& perfetto_factory)
    : impl_(new Impl(perfetto_factory)) {}

std::shared_ptr<EventManager> EventManager::Create() {
  static perfetto::PerfettoDependencies::Injector injector{
    perfetto::PerfettoDependencies::CreateComponent
  };
  static perfetto::RxProducerFactory producer_factory{
    /*borrow*/injector
  };
  return EventManager::Create(/*borrow*/producer_factory);
}

std::shared_ptr<EventManager> EventManager::Create(perfetto::RxProducerFactory& perfetto_factory) {
  std::shared_ptr<EventManager> p{new EventManager{/*borrow*/perfetto_factory}};
  return p;
}

void EventManager::SetTaskResultCallbacks(std::shared_ptr<TaskResultCallbacks> callbacks) {
  return impl_->SetTaskResultCallbacks(std::move(callbacks));
}

void EventManager::Join() {
  return impl_->Join();
}

bool EventManager::OnAppLaunchEvent(RequestId request_id,
                                    const AppLaunchEvent& event) {
  return impl_->OnAppLaunchEvent(request_id, event);
}

bool EventManager::OnDexOptEvent(RequestId request_id,
                                 const DexOptEvent& event) {
  return impl_->OnDexOptEvent(request_id, event);
}

bool EventManager::OnJobScheduledEvent(RequestId request_id,
                                       const JobScheduledEvent& event) {
  return impl_->OnJobScheduledEvent(request_id, event);
}

bool EventManager::OnPackageChanged(const android::content::pm::PackageChangeEvent& event) {
  return impl_->OnPackageChanged(event);
}

void EventManager::Dump(/*borrow*/::android::Printer& printer) {
  return impl_->Dump(printer);
}

void EventManager::RefreshSystemProperties(::android::Printer& printer) {
  return impl_->RefreshSystemProperties(printer);
}

bool EventManager::PurgePackage(::android::Printer& printer, const std::string& package_name) {
  return impl_->PurgePackage(printer, package_name);
}

bool EventManager::CompilePackage(::android::Printer& printer, const std::string& package_name) {
  return impl_->CompilePackage(printer, package_name);
}

}  // namespace iorap::manager
