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

#include "DebuggerImpl.h"
#include "ProfilingType.pb.h"
#include "ConfigurationCommand.pb.h"
#include "StatusUtil.h"

#include <android-base/logging.h>
#include <android-base/file.h>
#include <binder/ParcelFileDescriptor.h>

#include <errno.h>

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace client_interface {
namespace aidl_client {
namespace {

using ::std::literals::chrono_literals::operator""ms;
using ::aidl::android::automotive::computepipe::runner::PipeProfilingType;
using ::aidl::android::automotive::computepipe::runner::ProfilingData;

using ::ndk::ScopedAStatus;

constexpr std::chrono::milliseconds kProfilingDataReadTimeout = 50ms;

proto::ProfilingType ToProtoProfilingType(PipeProfilingType type) {
    switch (type) {
        case PipeProfilingType::LATENCY:
            return proto::ProfilingType::LATENCY;
        case PipeProfilingType::TRACE_EVENTS:
            return proto::ProfilingType::TRACE_EVENTS;
    }
}

PipeProfilingType ToAidlProfilingType(proto::ProfilingType type) {
    switch (type) {
        case proto::ProfilingType::LATENCY:
            return PipeProfilingType::LATENCY;
        case proto::ProfilingType::TRACE_EVENTS:
            return PipeProfilingType::TRACE_EVENTS;
        case proto::ProfilingType::DISABLED:
            LOG(ERROR) << "Attempt to convert invalid profiling type to aidl type.";
            return PipeProfilingType::LATENCY;
    }
}

Status RecursiveCreateDir(const std::string& dirName) {
    if (dirName == "/") {
        return Status::SUCCESS;
    }

    DIR *directory = opendir(dirName.c_str());
    // Check if directory exists.
    if (directory) {
        closedir(directory);
        return Status::SUCCESS;
    }

    std::string parentDirName = android::base::Dirname(dirName);
    if (!parentDirName.empty()) {
        Status status = RecursiveCreateDir(parentDirName);
        if (status != Status::SUCCESS) {
            return status;
        }
    }

    // mkdir expects the flag as bits it is essential to send 0777 which is
    // interpreted in octal rather than 777 which is interpreted in decimal.
    if (!mkdir(dirName.c_str(), 0777)) {
        return Status::SUCCESS;
    } else {
        LOG(ERROR) << "Failed to create directory - " << errno;
        return Status::INTERNAL_ERROR;
    }
}

}  // namespace

ndk::ScopedAStatus DebuggerImpl::setPipeProfileOptions(PipeProfilingType in_type) {
    mProfilingType = in_type;
    proto::ConfigurationCommand command;
    command.mutable_set_profile_options()->set_profile_type(ToProtoProfilingType(mProfilingType));
    std::shared_ptr<ClientEngineInterface> engineSp = mEngine.lock();
    if (!engineSp) {
        return ToNdkStatus(Status::INTERNAL_ERROR);
    }
    Status status = engineSp->processClientConfigUpdate(command);
    return ToNdkStatus(status);
}

ScopedAStatus DebuggerImpl::startPipeProfiling() {
    if (mGraphState != GraphState::RUNNING) {
        LOG(ERROR) << "Attempting to start profiling when the graph is not in the running state.";
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    proto::ControlCommand controlCommand;
    *controlCommand.mutable_start_pipe_profile() = proto::StartPipeProfile();
    std::shared_ptr<ClientEngineInterface> engineSp = mEngine.lock();
    if (!engineSp) {
        return ToNdkStatus(Status::INTERNAL_ERROR);
    }
    Status status = engineSp->processClientCommand(controlCommand);
    return ToNdkStatus(status);
}

ScopedAStatus DebuggerImpl::stopPipeProfiling() {
    proto::ControlCommand controlCommand;
    *controlCommand.mutable_stop_pipe_profile() = proto::StopPipeProfile();
    std::shared_ptr<ClientEngineInterface> engineSp = mEngine.lock();
    if (!engineSp) {
        return ToNdkStatus(Status::INTERNAL_ERROR);
    }
    Status status = engineSp->processClientCommand(controlCommand);
    if (status != Status::SUCCESS) {
        return ToNdkStatus(status);
    }

    proto::ControlCommand controlCommand2;
    *controlCommand2.mutable_read_debug_data() = proto::ReadDebugData();
    status = engineSp->processClientCommand(controlCommand2);
    if (status != Status::SUCCESS) {
        return ToNdkStatus(status);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus DebuggerImpl::getPipeProfilingInfo(ProfilingData* _aidl_return) {
    if (!_aidl_return) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    std::unique_lock<std::mutex> lk(mLock);
    if (!mWait.wait_for(lk, kProfilingDataReadTimeout, [this]() {
                    return mProfilingData.size != 0;
                })) {
        LOG(ERROR) << "No profiling data was found.";
        proto::ProfilingType profilingType = ToProtoProfilingType(mProfilingType);
        if (profilingType == proto::ProfilingType::DISABLED) {
            LOG(ERROR) << "Profiling was disabled.";
            return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
        }
        _aidl_return->type = ToAidlProfilingType(profilingType);
        _aidl_return->size = 0;
        return ScopedAStatus::ok();
    }

    _aidl_return->type = mProfilingData.type;
    _aidl_return->size = mProfilingData.size;
    _aidl_return->dataFds = std::move(mProfilingData.dataFds);
    return ScopedAStatus::ok();
}

ScopedAStatus DebuggerImpl::releaseDebugger() {
    if (mGraphState == GraphState::RUNNING || mGraphState == GraphState::RESET) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    proto::ControlCommand controlCommand;
    *controlCommand.mutable_release_debugger() = proto::ReleaseDebugger();
    std::shared_ptr<ClientEngineInterface> engineSp = mEngine.lock();
    if (!engineSp) {
        return ToNdkStatus(Status::INTERNAL_ERROR);
    }
    Status status = engineSp->processClientCommand(controlCommand);

    std::lock_guard<std::mutex> lk(mLock);
    mProfilingData.size = 0;
    mProfilingData.dataFds.clear();
    return ToNdkStatus(status);
}

Status DebuggerImpl::handleConfigPhase(const ClientConfig& e) {
    if (e.isTransitionComplete()) {
        mGraphState = GraphState::CONFIG_DONE;
    }
    return Status::SUCCESS;
}

Status DebuggerImpl::handleExecutionPhase(const RunnerEvent& e) {
    if (e.isTransitionComplete()) {
        mGraphState = GraphState::RUNNING;
    }
    if (e.isAborted()) {
        mGraphState = GraphState::ERR_HALT;
    }
    return Status::SUCCESS;
}

Status DebuggerImpl::handleStopWithFlushPhase(const RunnerEvent& e) {
    if (e.isTransitionComplete()) {
        mGraphState = GraphState::DONE;
    }
    if (e.isAborted()) {
        mGraphState = GraphState::ERR_HALT;
    }
    return Status::SUCCESS;
}

Status DebuggerImpl::handleStopImmediatePhase(const RunnerEvent& e) {
    if (e.isTransitionComplete() || e.isAborted()) {
        mGraphState = GraphState::ERR_HALT;
    }
    return Status::SUCCESS;
}

Status DebuggerImpl::handleResetPhase(const RunnerEvent& e) {
    if (e.isPhaseEntry()) {
        mGraphState = GraphState::RESET;
    }
    return Status::SUCCESS;
}

Status DebuggerImpl::deliverGraphDebugInfo(const std::string& debugData) {
    Status status = RecursiveCreateDir(mProfilingDataDirName);
    if (status != Status::SUCCESS) {
        return status;
    }

    std::string profilingDataFilePath = mProfilingDataDirName + "/" + mGraphOptions.graph_name();
    std::string fileRemoveError;
    if (!android::base::RemoveFileIfExists(profilingDataFilePath, &fileRemoveError)) {
        LOG(ERROR) << "Failed to remove file " << profilingDataFilePath << ", error: "
            << fileRemoveError;
        return Status::INTERNAL_ERROR;
    }
    if (!android::base::WriteStringToFile(debugData, profilingDataFilePath)) {
        LOG(ERROR) << "Failed to write profiling data to file at path " << profilingDataFilePath;
        return Status::INTERNAL_ERROR;
    }

    std::lock_guard<std::mutex> lk(mLock);
    mProfilingData.type = mProfilingType;
    mProfilingData.size = debugData.size();
    mProfilingData.dataFds.emplace_back(
        ndk::ScopedFileDescriptor(open(profilingDataFilePath.c_str(), O_CREAT, O_RDWR)));
    mWait.notify_one();
    return Status::SUCCESS;
}

}  // namespace aidl_client
}  // namespace client_interface
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
