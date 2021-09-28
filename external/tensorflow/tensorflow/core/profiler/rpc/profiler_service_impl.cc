/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/core/profiler/rpc/profiler_service_impl.h"

#include "grpcpp/support/status.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/env_time.h"
#include "tensorflow/core/profiler/convert/op_stats_to_input_pipeline_analysis.h"
#include "tensorflow/core/profiler/convert/op_stats_to_overview_page.h"
#include "tensorflow/core/profiler/convert/op_stats_to_tf_stats.h"
#include "tensorflow/core/profiler/convert/xplane_to_op_stats.h"
#include "tensorflow/core/profiler/convert/xplane_to_trace_events.h"
#include "tensorflow/core/profiler/lib/profiler_session.h"
#include "tensorflow/core/profiler/protobuf/hardware_types.pb.h"
#include "tensorflow/core/profiler/protobuf/input_pipeline.pb.h"
#include "tensorflow/core/profiler/protobuf/op_stats.pb.h"
#include "tensorflow/core/profiler/protobuf/overview_page.pb.h"
#include "tensorflow/core/profiler/protobuf/tf_stats.pb.h"
#include "tensorflow/core/profiler/protobuf/xplane.pb.h"
#include "tensorflow/core/profiler/utils/group_events.h"
#include "tensorflow/core/protobuf/trace_events.pb.h"
#include "tensorflow/core/util/ptr_util.h"

namespace tensorflow {
namespace {

const absl::string_view kTensorflowStats = "tensorflow_stats";
const absl::string_view kInputPipeline = "input_pipeline";
const absl::string_view kOverviewPage = "overview_page";

profiler::HardwareType HardwareTypeFromRunEnvironment(
    const profiler::RunEnvironment& run_env) {
  if (run_env.device_type() == "GPU") return profiler::HardwareType::GPU;
  if (run_env.device_type() == "CPU") return profiler::HardwareType::CPU_ONLY;
  return profiler::HardwareType::UNKNOWN_HARDWARE;
}

template <typename Proto>
void AddToolData(absl::string_view tool_name, const Proto& tool_output,
                 ProfileResponse* response) {
  auto* tool_data = response->add_tool_data();
  tool_data->set_name(string(tool_name));
  tool_output.SerializeToString(tool_data->mutable_data());
}

// Returns the tool name with extension.
std::string ToolName(absl::string_view tool) {
  return absl::StrCat(tool, ".pb");
}

Status CollectDataToResponse(const ProfileRequest& req,
                             ProfilerSession* profiler, uint64 start_time_ns,
                             ProfileResponse* response) {
  profiler::XSpace xspace;
  TF_RETURN_IF_ERROR(profiler->CollectData(&xspace));
  GroupTfEvents(&xspace, /*event_group_name_map=*/nullptr);
  {
    uint64 end_time_ns = EnvTime::NowNanos();
    profiler::Trace trace;
    profiler::ConvertXSpaceToTraceEvents(start_time_ns, end_time_ns, xspace,
                                         &trace);
    trace.SerializeToString(response->mutable_encoded_trace());
  }
  absl::flat_hash_set<absl::string_view> tools(req.tools().begin(),
                                               req.tools().end());
  if (!tools.empty()) {
    profiler::OpStats op_stats = profiler::ConvertXSpaceToOpStats(xspace);
    profiler::HardwareType hw_type =
        HardwareTypeFromRunEnvironment(op_stats.run_environment());
    if (tools.contains(kOverviewPage)) {
      profiler::OverviewPage overview_page_db =
          profiler::ConvertOpStatsToOverviewPage(op_stats, hw_type);
      AddToolData(ToolName(kOverviewPage), overview_page_db, response);
    }
    if (tools.contains(kInputPipeline)) {
      profiler::InputPipelineAnalysisResult input_pipeline_analysis =
          profiler::ConvertOpStatsToInputPipelineAnalysis(op_stats, hw_type);
      AddToolData(ToolName(kInputPipeline), input_pipeline_analysis, response);
    }
    if (tools.contains(kTensorflowStats)) {
      profiler::TfStatsDatabase tf_stats_db =
          profiler::ConvertOpStatsToTfStats(op_stats);
      AddToolData(ToolName(kTensorflowStats), tf_stats_db, response);
    }
  }
  return Status::OK();
}

class ProfilerServiceImpl : public grpc::ProfilerService::Service {
 public:
  ::grpc::Status Monitor(::grpc::ServerContext* ctx, const MonitorRequest* req,
                         MonitorResponse* response) override {
    return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "unimplemented.");
  }

  ::grpc::Status Profile(::grpc::ServerContext* ctx, const ProfileRequest* req,
                         ProfileResponse* response) override {
    LOG(INFO) << "Received a profile request: " << req->DebugString();
    uint64 start_time_ns = EnvTime::NowNanos();
    std::unique_ptr<ProfilerSession> profiler = ProfilerSession::Create();
    Status status = profiler->Status();
    if (!status.ok()) {
      return ::grpc::Status(::grpc::StatusCode::INTERNAL,
                            status.error_message());
    }

    Env* env = Env::Default();
    for (size_t i = 0; i < req->duration_ms(); ++i) {
      env->SleepForMicroseconds(EnvTime::kMillisToMicros);
      if (ctx->IsCancelled()) {
        return ::grpc::Status::CANCELLED;
      }
    }

    status =
        CollectDataToResponse(*req, profiler.get(), start_time_ns, response);
    if (!status.ok()) {
      return ::grpc::Status(::grpc::StatusCode::INTERNAL,
                            status.error_message());
    }

    return ::grpc::Status::OK;
  }
};

}  // namespace

std::unique_ptr<grpc::ProfilerService::Service> CreateProfilerService() {
  return MakeUnique<ProfilerServiceImpl>();
}

}  // namespace tensorflow
