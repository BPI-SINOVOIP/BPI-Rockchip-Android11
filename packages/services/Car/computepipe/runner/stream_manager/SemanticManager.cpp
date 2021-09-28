#include "SemanticManager.h"

#include <android-base/logging.h>

#include <cstdlib>
#include <thread>

#include "InputFrame.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace stream_manager {

proto::PacketType SemanticHandle::getType() const {
    return mType;
}

uint64_t SemanticHandle::getTimeStamp() const {
    return mTimestamp;
}

uint32_t SemanticHandle::getSize() const {
    return mSize;
}

const char* SemanticHandle::getData() const {
    return mData;
}

AHardwareBuffer* SemanticHandle::getHardwareBuffer() const {
    return nullptr;
}

int SemanticHandle::getStreamId() const {
    return mStreamId;
}

// Buffer id is not tracked for semantic handle as they do not need a
// doneWithPacket() call.
int SemanticHandle::getBufferId() const {
    return -1;
}

Status SemanticHandle::setMemInfo(int streamId, const char* data, uint32_t size, uint64_t timestamp,
                                  const proto::PacketType& type) {
    if (data == nullptr || size == 0 || size > kMaxSemanticDataSize) {
        return INVALID_ARGUMENT;
    }
    mStreamId = streamId;
    mData = (char*)malloc(size);
    if (!mData) {
        return NO_MEMORY;
    }
    memcpy(mData, data, size);
    mType = type;
    mTimestamp = timestamp;
    mSize = size;
    return SUCCESS;
}

/* Destroy local copy */
SemanticHandle::~SemanticHandle() {
    free(mData);
}

void SemanticManager::setEngineInterface(std::shared_ptr<StreamEngineInterface> engine) {
    mEngine = engine;
    std::lock_guard<std::mutex> lock(mStateLock);
    mState = RESET;
}

void SemanticManager::notifyEndOfStream() {
    mEngine->notifyEndOfStream();
}

// TODO: b/146495240 Add support for batching
Status SemanticManager::setMaxInFlightPackets(uint32_t /* maxPackets */) {
    if (!mEngine) {
        return ILLEGAL_STATE;
    }
    mState = CONFIG_DONE;
    return SUCCESS;
}

Status SemanticManager::handleExecutionPhase(const RunnerEvent& e) {
    std::lock_guard<std::mutex> lock(mStateLock);
    if (mState == CONFIG_DONE && e.isPhaseEntry()) {
        mState = RUNNING;
        return SUCCESS;
    }
    if (mState == RESET) {
        /* Cannot get to running phase from reset state without config phase*/
        return ILLEGAL_STATE;
    }
    if (mState == RUNNING && e.isAborted()) {
        /* Transition back to config completed */
        mState = CONFIG_DONE;
        return SUCCESS;
    }
    if (mState == RUNNING) {
        return ILLEGAL_STATE;
    }
    return SUCCESS;
}

Status SemanticManager::handleStopWithFlushPhase(const RunnerEvent& e) {
    std::lock_guard<std::mutex> lock(mStateLock);
    if (mState == CONFIG_DONE || mState == RESET) {
        return ILLEGAL_STATE;
    }
    /* Cannot have stop completed if we never entered stop state */
    if (mState == RUNNING && (e.isAborted() || e.isTransitionComplete())) {
        return ILLEGAL_STATE;
    }
    /* We are being asked to stop */
    if (mState == RUNNING && e.isPhaseEntry()) {
        mState = STOPPED;
        std::thread t(&SemanticManager::notifyEndOfStream, this);
        t.detach();
        return SUCCESS;
    }
    /* Other Components have stopped, we can transition back to CONFIG_DONE */
    if (mState == STOPPED && e.isTransitionComplete()) {
        mState = CONFIG_DONE;
        return SUCCESS;
    }
    /* We were stopped, but stop was aborted. */
    if (mState == STOPPED && e.isAborted()) {
        mState = RUNNING;
        return SUCCESS;
    }
    return SUCCESS;
}

Status SemanticManager::handleStopImmediatePhase(const RunnerEvent& e) {
    return handleStopWithFlushPhase(e);
}

Status SemanticManager::freePacket(int /* bufferId */) {
    return SUCCESS;
}

Status SemanticManager::queuePacket(const char* data, const uint32_t size, uint64_t timestamp) {
    std::lock_guard<std::mutex> lock(mStateLock);
    // We drop the packet since we have received the stop notifications.
    if (mState != RUNNING) {
        return SUCCESS;
    }
    // Invalid state.
    if (mEngine == nullptr) {
        return INTERNAL_ERROR;
    }
    auto memHandle = std::make_shared<SemanticHandle>();
    auto status = memHandle->setMemInfo(mStreamId, data, size, timestamp, mType);
    if (status != SUCCESS) {
        return status;
    }
    mEngine->dispatchPacket(memHandle);
    return SUCCESS;
}

Status SemanticManager::queuePacket(const InputFrame& /*inputData*/, uint64_t /*timestamp*/) {
    LOG(ERROR) << "Unexpected call to queue a pixel packet from a semantic stream manager.";
    return Status::ILLEGAL_STATE;
}

std::shared_ptr<MemHandle> SemanticManager::clonePacket(std::shared_ptr<MemHandle> handle) {
    return handle;
}

SemanticManager::SemanticManager(std::string name, int streamId, const proto::PacketType& type)
    : StreamManager(name, type), mStreamId(streamId) {
}
}  // namespace stream_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
