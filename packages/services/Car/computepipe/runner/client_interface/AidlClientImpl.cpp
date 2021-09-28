// Copyright (C) 2019 The Android Open Source Project
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
// limitations under the License.c

#include "AidlClientImpl.h"

#include <vector>

#include "OutputConfig.pb.h"
#include "PacketDescriptor.pb.h"
#include "PipeOptionsConverter.h"
#include "StatusUtil.h"

#include <aidl/android/automotive/computepipe/runner/PacketDescriptor.h>
#include <aidl/android/automotive/computepipe/runner/PacketDescriptorPacketType.h>
#include <android-base/logging.h>
#include <android/binder_auto_utils.h>

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace client_interface {
namespace aidl_client {
namespace {

using ::aidl::android::automotive::computepipe::runner::IPipeStateCallback;
using ::aidl::android::automotive::computepipe::runner::IPipeStream;
using ::aidl::android::automotive::computepipe::runner::PacketDescriptor;
using ::aidl::android::automotive::computepipe::runner::PacketDescriptorPacketType;
using ::aidl::android::automotive::computepipe::runner::PipeDescriptor;
using ::aidl::android::automotive::computepipe::runner::PipeState;
using ::ndk::ScopedAStatus;

PipeState ToAidlState(GraphState state) {
    switch (state) {
        case RESET:
            return PipeState::RESET;
        case CONFIG_DONE:
            return PipeState::CONFIG_DONE;
        case RUNNING:
            return PipeState::RUNNING;
        case DONE:
            return PipeState::DONE;
        case ERR_HALT:
        default:
            return PipeState::ERR_HALT;
    }
}

void deathNotifier(void* cookie) {
    CHECK(cookie);
    AidlClientImpl* iface = static_cast<AidlClientImpl*>(cookie);
    iface->clientDied();
}

Status ToAidlPacketType(proto::PacketType type, PacketDescriptorPacketType* outType) {
    if (outType == nullptr) {
        return Status::INTERNAL_ERROR;
    }
    switch (type) {
        case proto::SEMANTIC_DATA:
            *outType = PacketDescriptorPacketType::SEMANTIC_DATA;
            return Status::SUCCESS;
        case proto::PIXEL_DATA:
            *outType = PacketDescriptorPacketType::PIXEL_DATA;
            return Status::SUCCESS;
        default:
            LOG(ERROR) << "unknown packet type " << type;
            return Status::INVALID_ARGUMENT;
    }
}

}  // namespace

Status AidlClientImpl::DispatchSemanticData(int32_t streamId,
                                           const std::shared_ptr<MemHandle>& packetHandle) {
    PacketDescriptor desc;

    if (mPacketHandlers.find(streamId) == mPacketHandlers.end()) {
        LOG(ERROR) << "Bad streamId";
        return Status::INVALID_ARGUMENT;
    }
    Status status = ToAidlPacketType(packetHandle->getType(), &desc.type);
    if (status != SUCCESS) {
        return status;
    }
    desc.data = std::vector(reinterpret_cast<const signed char*>(packetHandle->getData()),
        reinterpret_cast<const signed char*>(packetHandle->getData() + packetHandle->getSize()));
    desc.size = packetHandle->getSize();
    if (static_cast<int32_t>(desc.data.size()) != desc.size) {
        LOG(ERROR) << "mismatch in char data size and reported size";
        return Status::INVALID_ARGUMENT;
    }
    desc.sourceTimeStampMillis = packetHandle->getTimeStamp();
    desc.bufId = 0;
    ScopedAStatus ret = mPacketHandlers[streamId]->deliverPacket(desc);
    if (!ret.isOk()) {
        LOG(ERROR) << "Dropping Semantic packet due to error ";
    }
    return Status::SUCCESS;
}

Status AidlClientImpl::DispatchPixelData(int32_t streamId,
                                         const std::shared_ptr<MemHandle>& packetHandle) {
    PacketDescriptor desc;

    if (mPacketHandlers.find(streamId) == mPacketHandlers.end()) {
        LOG(ERROR) << "Bad stream id";
        return Status::INVALID_ARGUMENT;
    }
    Status status = ToAidlPacketType(packetHandle->getType(), &desc.type);
    if (status != Status::SUCCESS) {
        LOG(ERROR) << "Invalid packet type";
        return status;
    }

    // Copies the native handle to the aidl interface.
    const native_handle_t* nativeHandle =
        AHardwareBuffer_getNativeHandle(packetHandle->getHardwareBuffer());
    for (int i = 0; i < nativeHandle->numFds; i++) {
        desc.handle.handle.fds.push_back(ndk::ScopedFileDescriptor(nativeHandle->data[i]));
    }
    for (int i = 0; i < nativeHandle->numInts; i++) {
        desc.handle.handle.ints.push_back(nativeHandle->data[i + nativeHandle->numFds]);
    }

    // Copies buffer descriptor to the aidl interface.
    AHardwareBuffer_Desc bufferDesc;
    AHardwareBuffer_describe(packetHandle->getHardwareBuffer(), &bufferDesc);
    desc.handle.description.width = bufferDesc.width;
    desc.handle.description.height = bufferDesc.height;
    desc.handle.description.stride = bufferDesc.stride;
    desc.handle.description.layers = bufferDesc.layers;
    desc.handle.description.format =
        static_cast<::aidl::android::hardware::graphics::common::PixelFormat>(bufferDesc.format);
    desc.handle.description.usage =
        static_cast<::aidl::android::hardware::graphics::common::BufferUsage>(bufferDesc.usage);

    desc.bufId = packetHandle->getBufferId();
    desc.sourceTimeStampMillis = packetHandle->getTimeStamp();

    ScopedAStatus ret = mPacketHandlers[streamId]->deliverPacket(desc);
    if (!ret.isOk()) {
        LOG(ERROR) << "Unable to deliver packet. Dropping it and returning an error";
        return Status::INTERNAL_ERROR;
    }
    return Status::SUCCESS;
}

// Thread-safe function to deliver new packets to client.
Status AidlClientImpl::dispatchPacketToClient(int32_t streamId,
                                              const std::shared_ptr<MemHandle>& packetHandle) {
    // TODO(146464279) implement.
    if (!packetHandle) {
        LOG(ERROR) << "invalid packetHandle";
        return Status::INVALID_ARGUMENT;
    }
    proto::PacketType packetType = packetHandle->getType();
    switch (packetType) {
        case proto::SEMANTIC_DATA:
            return DispatchSemanticData(streamId, packetHandle);
        case proto::PIXEL_DATA:
            return DispatchPixelData(streamId, packetHandle);
        default:
            LOG(ERROR) << "Unsupported packet type " << packetHandle->getType();
            return Status::INVALID_ARGUMENT;
    }
    return Status::SUCCESS;
}

void AidlClientImpl::setPipeDebugger(
        const std::shared_ptr<aidl::android::automotive::computepipe::runner::IPipeDebugger>&
        pipeDebugger) {
    mPipeDebugger = pipeDebugger;
}

Status AidlClientImpl::stateUpdateNotification(const GraphState newState) {
    if (mClientStateChangeCallback) {
        (void)mClientStateChangeCallback->handleState(ToAidlState(newState));
    }
    return Status::SUCCESS;
}

ScopedAStatus AidlClientImpl::getPipeDescriptor(PipeDescriptor* _aidl_return) {
    if (_aidl_return == nullptr) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    *_aidl_return = OptionsToPipeDescriptor(mGraphOptions);
    return ScopedAStatus::ok();
}

ScopedAStatus AidlClientImpl::setPipeInputSource(int32_t configId) {
    if (!isClientInitDone()) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    proto::ConfigurationCommand configurationCommand;
    configurationCommand.mutable_set_input_source()->set_source_id(configId);

    Status status = mEngine->processClientConfigUpdate(configurationCommand);
    return ToNdkStatus(status);
}

ScopedAStatus AidlClientImpl::setPipeOffloadOptions(int32_t configId) {
    if (!isClientInitDone()) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    proto::ConfigurationCommand configurationCommand;
    configurationCommand.mutable_set_offload_offload()->set_offload_option_id(configId);

    Status status = mEngine->processClientConfigUpdate(configurationCommand);
    return ToNdkStatus(status);
}

ScopedAStatus AidlClientImpl::setPipeTermination(int32_t configId) {
    if (!isClientInitDone()) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    proto::ConfigurationCommand configurationCommand;
    configurationCommand.mutable_set_termination_option()->set_termination_option_id(configId);

    Status status = mEngine->processClientConfigUpdate(configurationCommand);
    return ToNdkStatus(status);
}

ScopedAStatus AidlClientImpl::init(const std::shared_ptr<IPipeStateCallback>& stateCb) {
    if (isClientInitDone()) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    AIBinder_DeathRecipient* recipient = AIBinder_DeathRecipient_new(&deathNotifier);
    AIBinder_linkToDeath(stateCb->asBinder().get(), recipient, this);

    mClientStateChangeCallback = stateCb;
    return ScopedAStatus::ok();
}

bool AidlClientImpl::isClientInitDone() {
    if (mClientStateChangeCallback == nullptr) {
        return false;
    }
    return true;
}

void AidlClientImpl::clientDied() {
    LOG(INFO) << "Client has died";
    releaseRunner();
}

ScopedAStatus AidlClientImpl::setPipeOutputConfig(int32_t streamId, int32_t maxInFlightCount,
                                                  const std::shared_ptr<IPipeStream>& handler) {
    if (!isClientInitDone()) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    if (mPacketHandlers.find(streamId) != mPacketHandlers.end()) {
        LOG(INFO) << "Handler for stream id " << streamId
                  << " has already"
                     " been registered.";
        return ToNdkStatus(INVALID_ARGUMENT);
    }

    mPacketHandlers.insert(std::pair<int, std::shared_ptr<IPipeStream>>(streamId, handler));

    proto::ConfigurationCommand configurationCommand;
    configurationCommand.mutable_set_output_stream()->set_stream_id(streamId);
    configurationCommand.mutable_set_output_stream()->set_max_inflight_packets_count(
            maxInFlightCount);
    Status status = mEngine->processClientConfigUpdate(configurationCommand);

    if (status != SUCCESS) {
        LOG(INFO) << "Failed to register handler for stream id " << streamId;
        mPacketHandlers.erase(streamId);
    }
    return ToNdkStatus(status);
}

ScopedAStatus AidlClientImpl::applyPipeConfigs() {
    if (!isClientInitDone()) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    proto::ControlCommand controlCommand;
    *controlCommand.mutable_apply_configs() = proto::ApplyConfigs();

    Status status = mEngine->processClientCommand(controlCommand);
    return ToNdkStatus(status);
}

ScopedAStatus AidlClientImpl::resetPipeConfigs() {
    if (!isClientInitDone()) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    proto::ControlCommand controlCommand;
    *controlCommand.mutable_reset_configs() = proto::ResetConfigs();

    Status status = mEngine->processClientCommand(controlCommand);
    return ToNdkStatus(status);
}

ScopedAStatus AidlClientImpl::startPipe() {
    proto::ControlCommand controlCommand;
    *controlCommand.mutable_start_graph() = proto::StartGraph();

    Status status = mEngine->processClientCommand(controlCommand);
    return ToNdkStatus(status);
}

ScopedAStatus AidlClientImpl::stopPipe() {
    proto::ControlCommand controlCommand;
    *controlCommand.mutable_stop_graph() = proto::StopGraph();

    Status status = mEngine->processClientCommand(controlCommand);
    return ToNdkStatus(status);
}

ScopedAStatus AidlClientImpl::doneWithPacket(int32_t bufferId, int32_t streamId) {
    auto it = mPacketHandlers.find(streamId);
    if (it == mPacketHandlers.end()) {
        LOG(ERROR) << "Bad stream id provided for doneWithPacket call";
        return ToNdkStatus(Status::INVALID_ARGUMENT);
    }

    return ToNdkStatus(mEngine->freePacket(bufferId, streamId));
}

ScopedAStatus AidlClientImpl::getPipeDebugger(
    std::shared_ptr<aidl::android::automotive::computepipe::runner::IPipeDebugger>*
    _aidl_return) {
    if (_aidl_return == nullptr) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (mPipeDebugger == nullptr) {
        return ScopedAStatus::fromExceptionCode(EX_TRANSACTION_FAILED);
    }
    *_aidl_return = mPipeDebugger;
    return ScopedAStatus::ok();
}

ScopedAStatus AidlClientImpl::releaseRunner() {
    proto::ControlCommand controlCommand;
    *controlCommand.mutable_death_notification() = proto::DeathNotification();

    Status status = mEngine->processClientCommand(controlCommand);

    mClientStateChangeCallback = nullptr;
    mPacketHandlers.clear();
    return ToNdkStatus(status);
}

}  // namespace aidl_client
}  // namespace client_interface
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
