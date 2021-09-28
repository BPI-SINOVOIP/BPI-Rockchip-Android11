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

#ifndef ANDROID_FRAMEWORKS_ML_NN_COMMON_EXECUTION_BURST_CONTROLLER_H
#define ANDROID_FRAMEWORKS_ML_NN_COMMON_EXECUTION_BURST_CONTROLLER_H

#include <android-base/macros.h>
#include <android/hardware/neuralnetworks/1.0/types.h>
#include <android/hardware/neuralnetworks/1.1/types.h>
#include <android/hardware/neuralnetworks/1.2/IBurstCallback.h>
#include <android/hardware/neuralnetworks/1.2/IBurstContext.h>
#include <android/hardware/neuralnetworks/1.2/IPreparedModel.h>
#include <android/hardware/neuralnetworks/1.2/types.h>
#include <fmq/MessageQueue.h>
#include <hidl/MQDescriptor.h>

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <stack>
#include <tuple>
#include <utility>
#include <vector>

namespace android::nn {

/**
 * Number of elements in the FMQ.
 */
constexpr const size_t kExecutionBurstChannelLength = 1024;

/**
 * Function to serialize a request.
 *
 * Prefer calling RequestChannelSender::send.
 *
 * @param request Request object without the pool information.
 * @param measure Whether to collect timing information for the execution.
 * @param memoryIds Slot identifiers corresponding to memory resources for the
 *     request.
 * @return Serialized FMQ request data.
 */
std::vector<hardware::neuralnetworks::V1_2::FmqRequestDatum> serialize(
        const hardware::neuralnetworks::V1_0::Request& request,
        hardware::neuralnetworks::V1_2::MeasureTiming measure, const std::vector<int32_t>& slots);

/**
 * Deserialize the FMQ result data.
 *
 * The three resulting fields are the status of the execution, the dynamic
 * shapes of the output tensors, and the timing information of the execution.
 *
 * @param data Serialized FMQ result data.
 * @return Result object if successfully deserialized, std::nullopt otherwise.
 */
std::optional<std::tuple<hardware::neuralnetworks::V1_0::ErrorStatus,
                         std::vector<hardware::neuralnetworks::V1_2::OutputShape>,
                         hardware::neuralnetworks::V1_2::Timing>>
deserialize(const std::vector<hardware::neuralnetworks::V1_2::FmqResultDatum>& data);

/**
 * Convert result code to error status.
 *
 * @param resultCode Result code to be converted.
 * @return ErrorStatus Resultant error status.
 */
hardware::neuralnetworks::V1_0::ErrorStatus legacyConvertResultCodeToErrorStatus(int resultCode);

/**
 * ResultChannelReceiver is responsible for waiting on the channel until the
 * packet is available, extracting the packet from the channel, and
 * deserializing the packet.
 *
 * Because the receiver can wait on a packet that may never come (e.g., because
 * the sending side of the packet has been closed), this object can be
 * invalidated, unblocking the receiver.
 */
class ResultChannelReceiver {
    using FmqResultDescriptor =
            hardware::MQDescriptorSync<hardware::neuralnetworks::V1_2::FmqResultDatum>;
    using FmqResultChannel = hardware::MessageQueue<hardware::neuralnetworks::V1_2::FmqResultDatum,
                                                    hardware::kSynchronizedReadWrite>;

   public:
    /**
     * Create the receiving end of a result channel.
     *
     * Prefer this call over the constructor.
     *
     * @param channelLength Number of elements in the FMQ.
     * @param pollingTimeWindow How much time (in microseconds) the
     *     ResultChannelReceiver is allowed to poll the FMQ before waiting on
     *     the blocking futex. Polling may result in lower latencies at the
     *     potential cost of more power usage.
     * @return A pair of ResultChannelReceiver and the FMQ descriptor on
     *     successful creation, both nullptr otherwise.
     */
    static std::pair<std::unique_ptr<ResultChannelReceiver>, const FmqResultDescriptor*> create(
            size_t channelLength, std::chrono::microseconds pollingTimeWindow);

    /**
     * Get the result from the channel.
     *
     * This method will block until either:
     * 1) The packet has been retrieved, or
     * 2) The receiver has been invalidated
     *
     * @return Result object if successfully received, std::nullopt if error or
     *     if the receiver object was invalidated.
     */
    std::optional<std::tuple<hardware::neuralnetworks::V1_0::ErrorStatus,
                             std::vector<hardware::neuralnetworks::V1_2::OutputShape>,
                             hardware::neuralnetworks::V1_2::Timing>>
    getBlocking();

    /**
     * Method to mark the channel as invalid, unblocking any current or future
     * calls to ResultChannelReceiver::getBlocking.
     */
    void invalidate();

    // prefer calling ResultChannelReceiver::getBlocking
    std::optional<std::vector<hardware::neuralnetworks::V1_2::FmqResultDatum>> getPacketBlocking();

    ResultChannelReceiver(std::unique_ptr<FmqResultChannel> fmqResultChannel,
                          std::chrono::microseconds pollingTimeWindow);

   private:
    const std::unique_ptr<FmqResultChannel> mFmqResultChannel;
    std::atomic<bool> mValid{true};
    const std::chrono::microseconds kPollingTimeWindow;
};

/**
 * RequestChannelSender is responsible for serializing the result packet of
 * information, sending it on the result channel, and signaling that the data is
 * available.
 */
class RequestChannelSender {
    using FmqRequestDescriptor =
            hardware::MQDescriptorSync<hardware::neuralnetworks::V1_2::FmqRequestDatum>;
    using FmqRequestChannel =
            hardware::MessageQueue<hardware::neuralnetworks::V1_2::FmqRequestDatum,
                                   hardware::kSynchronizedReadWrite>;

   public:
    /**
     * Create the sending end of a request channel.
     *
     * Prefer this call over the constructor.
     *
     * @param channelLength Number of elements in the FMQ.
     * @return A pair of ResultChannelReceiver and the FMQ descriptor on
     *     successful creation, both nullptr otherwise.
     */
    static std::pair<std::unique_ptr<RequestChannelSender>, const FmqRequestDescriptor*> create(
            size_t channelLength);

    /**
     * Send the request to the channel.
     *
     * @param request Request object without the pool information.
     * @param measure Whether to collect timing information for the execution.
     * @param memoryIds Slot identifiers corresponding to memory resources for
     *     the request.
     * @return 'true' on successful send, 'false' otherwise.
     */
    bool send(const hardware::neuralnetworks::V1_0::Request& request,
              hardware::neuralnetworks::V1_2::MeasureTiming measure,
              const std::vector<int32_t>& slots);

    /**
     * Method to mark the channel as invalid, causing all future calls to
     * RequestChannelSender::send to immediately return false without attempting
     * to send a message across the FMQ.
     */
    void invalidate();

    // prefer calling RequestChannelSender::send
    bool sendPacket(const std::vector<hardware::neuralnetworks::V1_2::FmqRequestDatum>& packet);

    RequestChannelSender(std::unique_ptr<FmqRequestChannel> fmqRequestChannel);

   private:
    const std::unique_ptr<FmqRequestChannel> mFmqRequestChannel;
    std::atomic<bool> mValid{true};
};

/**
 * The ExecutionBurstController class manages both the serialization and
 * deserialization of data across FMQ, making it appear to the runtime as a
 * regular synchronous inference. Additionally, this class manages the burst's
 * memory cache.
 */
class ExecutionBurstController {
    DISALLOW_IMPLICIT_CONSTRUCTORS(ExecutionBurstController);

   public:
    /**
     * NN runtime burst callback object and memory cache.
     *
     * ExecutionBurstCallback associates a hidl_memory object with a slot number
     * to be passed across FMQ. The ExecutionBurstServer can use this callback
     * to retrieve this hidl_memory corresponding to the slot via HIDL.
     *
     * Whenever a hidl_memory object is copied, it will duplicate the underlying
     * file descriptor. Because the NN runtime currently copies the hidl_memory
     * on each execution, it is difficult to associate hidl_memory objects with
     * previously cached hidl_memory objects. For this reason, callers of this
     * class must pair each hidl_memory object with an associated key. For
     * efficiency, if two hidl_memory objects represent the same underlying
     * buffer, they must use the same key.
     */
    class ExecutionBurstCallback : public hardware::neuralnetworks::V1_2::IBurstCallback {
        DISALLOW_COPY_AND_ASSIGN(ExecutionBurstCallback);

       public:
        ExecutionBurstCallback() = default;

        hardware::Return<void> getMemories(const hardware::hidl_vec<int32_t>& slots,
                                           getMemories_cb cb) override;

        /**
         * This function performs one of two different actions:
         * 1) If a key corresponding to a memory resource is unrecognized by the
         *    ExecutionBurstCallback object, the ExecutionBurstCallback object
         *    will allocate a slot, bind the memory to the slot, and return the
         *    slot identifier.
         * 2) If a key corresponding to a memory resource is recognized by the
         *    ExecutionBurstCallback object, the ExecutionBurstCallback object
         *    will return the existing slot identifier.
         *
         * @param memories Memory resources used in an inference.
         * @param keys Unique identifiers where each element corresponds to a
         *     memory resource element in "memories".
         * @return Unique slot identifiers where each returned slot element
         *     corresponds to a memory resource element in "memories".
         */
        std::vector<int32_t> getSlots(const hardware::hidl_vec<hardware::hidl_memory>& memories,
                                      const std::vector<intptr_t>& keys);

        /*
         * This function performs two different actions:
         * 1) Removes an entry from the cache (if present), including the local
         *    storage of the hidl_memory object. Note that this call does not
         *    free any corresponding hidl_memory object in ExecutionBurstServer,
         *    which is separately freed via IBurstContext::freeMemory.
         * 2) Return whether a cache entry was removed and which slot was removed if
         *    found. If the key did not to correspond to any entry in the cache, a
         *    slot number of 0 is returned. The slot number and whether the entry
         *    existed is useful so the same slot can be freed in the
         *    ExecutionBurstServer's cache via IBurstContext::freeMemory.
         */
        std::pair<bool, int32_t> freeMemory(intptr_t key);

       private:
        int32_t getSlotLocked(const hardware::hidl_memory& memory, intptr_t key);
        int32_t allocateSlotLocked();

        std::mutex mMutex;
        std::stack<int32_t, std::vector<int32_t>> mFreeSlots;
        std::map<intptr_t, int32_t> mMemoryIdToSlot;
        std::vector<hardware::hidl_memory> mMemoryCache;
    };

    /**
     * Creates a burst controller on a prepared model.
     *
     * Prefer this over ExecutionBurstController's constructor.
     *
     * @param preparedModel Model prepared for execution to execute on.
     * @param pollingTimeWindow How much time (in microseconds) the
     *     ExecutionBurstController is allowed to poll the FMQ before waiting on
     *     the blocking futex. Polling may result in lower latencies at the
     *     potential cost of more power usage.
     * @return ExecutionBurstController Execution burst controller object.
     */
    static std::unique_ptr<ExecutionBurstController> create(
            const sp<hardware::neuralnetworks::V1_2::IPreparedModel>& preparedModel,
            std::chrono::microseconds pollingTimeWindow);

    // prefer calling ExecutionBurstController::create
    ExecutionBurstController(const std::shared_ptr<RequestChannelSender>& requestChannelSender,
                             const std::shared_ptr<ResultChannelReceiver>& resultChannelReceiver,
                             const sp<hardware::neuralnetworks::V1_2::IBurstContext>& burstContext,
                             const sp<ExecutionBurstCallback>& callback,
                             const sp<hardware::hidl_death_recipient>& deathHandler = nullptr);

    // explicit destructor to unregister the death recipient
    ~ExecutionBurstController();

    /**
     * Execute a request on a model.
     *
     * @param request Arguments to be executed on a model.
     * @param measure Whether to collect timing measurements, either YES or NO
     * @param memoryIds Identifiers corresponding to each memory object in the
     *     request's pools.
     * @return A tuple of:
     *     - result code of the execution
     *     - dynamic output shapes from the execution
     *     - any execution time measurements of the execution
     *     - whether or not a failed burst execution should be re-run using a
     *       different path (e.g., IPreparedModel::executeSynchronously)
     */
    std::tuple<int, std::vector<hardware::neuralnetworks::V1_2::OutputShape>,
               hardware::neuralnetworks::V1_2::Timing, bool>
    compute(const hardware::neuralnetworks::V1_0::Request& request,
            hardware::neuralnetworks::V1_2::MeasureTiming measure,
            const std::vector<intptr_t>& memoryIds);

    /**
     * Propagate a user's freeing of memory to the service.
     *
     * @param key Key corresponding to the memory object.
     */
    void freeMemory(intptr_t key);

   private:
    std::mutex mMutex;
    const std::shared_ptr<RequestChannelSender> mRequestChannelSender;
    const std::shared_ptr<ResultChannelReceiver> mResultChannelReceiver;
    const sp<hardware::neuralnetworks::V1_2::IBurstContext> mBurstContext;
    const sp<ExecutionBurstCallback> mMemoryCache;
    const sp<hardware::hidl_death_recipient> mDeathHandler;
};

}  // namespace android::nn

#endif  // ANDROID_FRAMEWORKS_ML_NN_COMMON_EXECUTION_BURST_CONTROLLER_H
