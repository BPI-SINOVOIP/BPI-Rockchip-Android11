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

#ifndef ANDROID_FRAMEWORKS_ML_NN_COMMON_EXECUTION_BURST_SERVER_H
#define ANDROID_FRAMEWORKS_ML_NN_COMMON_EXECUTION_BURST_SERVER_H

#include <android-base/macros.h>
#include <android/hardware/neuralnetworks/1.0/types.h>
#include <android/hardware/neuralnetworks/1.1/types.h>
#include <android/hardware/neuralnetworks/1.2/IBurstCallback.h>
#include <android/hardware/neuralnetworks/1.2/IPreparedModel.h>
#include <android/hardware/neuralnetworks/1.2/types.h>
#include <fmq/MessageQueue.h>
#include <hidl/MQDescriptor.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <thread>
#include <tuple>
#include <vector>

namespace android::nn {

using FmqRequestDescriptor =
        hardware::MQDescriptorSync<hardware::neuralnetworks::V1_2::FmqRequestDatum>;
using FmqResultDescriptor =
        hardware::MQDescriptorSync<hardware::neuralnetworks::V1_2::FmqResultDatum>;

/**
 * Function to serialize results.
 *
 * Prefer calling ResultChannelSender::send.
 *
 * @param errorStatus Status of the execution.
 * @param outputShapes Dynamic shapes of the output tensors.
 * @param timing Timing information of the execution.
 * @return Serialized FMQ result data.
 */
std::vector<hardware::neuralnetworks::V1_2::FmqResultDatum> serialize(
        hardware::neuralnetworks::V1_0::ErrorStatus errorStatus,
        const std::vector<hardware::neuralnetworks::V1_2::OutputShape>& outputShapes,
        hardware::neuralnetworks::V1_2::Timing timing);

/**
 * Deserialize the FMQ request data.
 *
 * The three resulting fields are the Request object (where Request::pools is
 * empty), slot identifiers (which are stand-ins for Request::pools), and
 * whether timing information must be collected for the run.
 *
 * @param data Serialized FMQ request data.
 * @return Request object if successfully deserialized, std::nullopt otherwise.
 */
std::optional<std::tuple<hardware::neuralnetworks::V1_0::Request, std::vector<int32_t>,
                         hardware::neuralnetworks::V1_2::MeasureTiming>>
deserialize(const std::vector<hardware::neuralnetworks::V1_2::FmqRequestDatum>& data);

/**
 * RequestChannelReceiver is responsible for waiting on the channel until the
 * packet is available, extracting the packet from the channel, and
 * deserializing the packet.
 *
 * Because the receiver can wait on a packet that may never come (e.g., because
 * the sending side of the packet has been closed), this object can be
 * invalidated, unblocking the receiver.
 */
class RequestChannelReceiver {
    using FmqRequestChannel =
            hardware::MessageQueue<hardware::neuralnetworks::V1_2::FmqRequestDatum,
                                   hardware::kSynchronizedReadWrite>;

   public:
    /**
     * Create the receiving end of a request channel.
     *
     * Prefer this call over the constructor.
     *
     * @param requestChannel Descriptor for the request channel.
     * @param pollingTimeWindow How much time (in microseconds) the
     *     RequestChannelReceiver is allowed to poll the FMQ before waiting on
     *     the blocking futex. Polling may result in lower latencies at the
     *     potential cost of more power usage.
     * @return RequestChannelReceiver on successful creation, nullptr otherwise.
     */
    static std::unique_ptr<RequestChannelReceiver> create(
            const FmqRequestDescriptor& requestChannel,
            std::chrono::microseconds pollingTimeWindow);

    /**
     * Get the request from the channel.
     *
     * This method will block until either:
     * 1) The packet has been retrieved, or
     * 2) The receiver has been invalidated
     *
     * @return Request object if successfully received, std::nullopt if error or
     *     if the receiver object was invalidated.
     */
    std::optional<std::tuple<hardware::neuralnetworks::V1_0::Request, std::vector<int32_t>,
                             hardware::neuralnetworks::V1_2::MeasureTiming>>
    getBlocking();

    /**
     * Method to mark the channel as invalid, unblocking any current or future
     * calls to RequestChannelReceiver::getBlocking.
     */
    void invalidate();

    RequestChannelReceiver(std::unique_ptr<FmqRequestChannel> fmqRequestChannel,
                           std::chrono::microseconds pollingTimeWindow);

   private:
    std::optional<std::vector<hardware::neuralnetworks::V1_2::FmqRequestDatum>> getPacketBlocking();

    const std::unique_ptr<FmqRequestChannel> mFmqRequestChannel;
    std::atomic<bool> mTeardown{false};
    const std::chrono::microseconds kPollingTimeWindow;
};

/**
 * ResultChannelSender is responsible for serializing the result packet of
 * information, sending it on the result channel, and signaling that the data is
 * available.
 */
class ResultChannelSender {
    using FmqResultChannel = hardware::MessageQueue<hardware::neuralnetworks::V1_2::FmqResultDatum,
                                                    hardware::kSynchronizedReadWrite>;

   public:
    /**
     * Create the sending end of a result channel.
     *
     * Prefer this call over the constructor.
     *
     * @param resultChannel Descriptor for the result channel.
     * @return ResultChannelSender on successful creation, nullptr otherwise.
     */
    static std::unique_ptr<ResultChannelSender> create(const FmqResultDescriptor& resultChannel);

    /**
     * Send the result to the channel.
     *
     * @param errorStatus Status of the execution.
     * @param outputShapes Dynamic shapes of the output tensors.
     * @param timing Timing information of the execution.
     * @return 'true' on successful send, 'false' otherwise.
     */
    bool send(hardware::neuralnetworks::V1_0::ErrorStatus errorStatus,
              const std::vector<hardware::neuralnetworks::V1_2::OutputShape>& outputShapes,
              hardware::neuralnetworks::V1_2::Timing timing);

    // prefer calling ResultChannelSender::send
    bool sendPacket(const std::vector<hardware::neuralnetworks::V1_2::FmqResultDatum>& packet);

    ResultChannelSender(std::unique_ptr<FmqResultChannel> fmqResultChannel);

   private:
    const std::unique_ptr<FmqResultChannel> mFmqResultChannel;
};

/**
 * The ExecutionBurstServer class is responsible for waiting for and
 * deserializing a request object from a FMQ, performing the inference, and
 * serializing the result back across another FMQ.
 */
class ExecutionBurstServer : public hardware::neuralnetworks::V1_2::IBurstContext {
    DISALLOW_IMPLICIT_CONSTRUCTORS(ExecutionBurstServer);

   public:
    /**
     * IBurstExecutorWithCache is a callback object passed to
     * ExecutionBurstServer's factory function that is used to perform an
     * execution. Because some memory resources are needed across multiple
     * executions, this object also contains a local cache that can directly be
     * used in the execution.
     *
     * ExecutionBurstServer will never access its IBurstExecutorWithCache object
     * with concurrent calls.
     */
    class IBurstExecutorWithCache {
        DISALLOW_COPY_AND_ASSIGN(IBurstExecutorWithCache);

       public:
        IBurstExecutorWithCache() = default;
        virtual ~IBurstExecutorWithCache() = default;

        /**
         * Checks if a cache entry specified by a slot is present in the cache.
         *
         * @param slot Identifier of the cache entry.
         * @return 'true' if the cache entry is present in the cache, 'false'
         *     otherwise.
         */
        virtual bool isCacheEntryPresent(int32_t slot) const = 0;

        /**
         * Adds an entry specified by a slot to the cache.
         *
         * The caller of this function must ensure that the cache entry that is
         * being added is not already present in the cache. This can be checked
         * via isCacheEntryPresent.
         *
         * @param memory Memory resource to be cached.
         * @param slot Slot identifier corresponding to the memory resource.
         */
        virtual void addCacheEntry(const hardware::hidl_memory& memory, int32_t slot) = 0;

        /**
         * Removes an entry specified by a slot from the cache.
         *
         * If the cache entry corresponding to the slot number does not exist,
         * the call does nothing.
         *
         * @param slot Slot identifier corresponding to the memory resource.
         */
        virtual void removeCacheEntry(int32_t slot) = 0;

        /**
         * Perform an execution.
         *
         * @param request Request object with inputs and outputs specified.
         *     Request::pools is empty, and DataLocation::poolIndex instead
         *     refers to the 'slots' argument as if it were Request::pools.
         * @param slots Slots corresponding to the cached memory entries to be
         *     used.
         * @param measure Whether timing information is requested for the
         *     execution.
         * @return Result of the execution, including the status of the
         *     execution, dynamic output shapes, and any timing information.
         */
        virtual std::tuple<hardware::neuralnetworks::V1_0::ErrorStatus,
                           hardware::hidl_vec<hardware::neuralnetworks::V1_2::OutputShape>,
                           hardware::neuralnetworks::V1_2::Timing>
        execute(const hardware::neuralnetworks::V1_0::Request& request,
                const std::vector<int32_t>& slots,
                hardware::neuralnetworks::V1_2::MeasureTiming measure) = 0;
    };

    /**
     * Create automated context to manage FMQ-based executions.
     *
     * This function is intended to be used by a service to automatically:
     * 1) Receive data from a provided FMQ
     * 2) Execute a model with the given information
     * 3) Send the result to the created FMQ
     *
     * @param callback Callback used to retrieve memories corresponding to
     *     unrecognized slots.
     * @param requestChannel Input FMQ channel through which the client passes the
     *     request to the service.
     * @param resultChannel Output FMQ channel from which the client can retrieve
     *     the result of the execution.
     * @param executorWithCache Object which maintains a local cache of the
     *     memory pools and executes using the cached memory pools.
     * @param pollingTimeWindow How much time (in microseconds) the
     *     ExecutionBurstServer is allowed to poll the FMQ before waiting on
     *     the blocking futex. Polling may result in lower latencies at the
     *     potential cost of more power usage.
     * @result IBurstContext Handle to the burst context.
     */
    static sp<ExecutionBurstServer> create(
            const sp<hardware::neuralnetworks::V1_2::IBurstCallback>& callback,
            const FmqRequestDescriptor& requestChannel, const FmqResultDescriptor& resultChannel,
            std::shared_ptr<IBurstExecutorWithCache> executorWithCache,
            std::chrono::microseconds pollingTimeWindow = std::chrono::microseconds{0});

    /**
     * Create automated context to manage FMQ-based executions.
     *
     * This function is intended to be used by a service to automatically:
     * 1) Receive data from a provided FMQ
     * 2) Execute a model with the given information
     * 3) Send the result to the created FMQ
     *
     * @param callback Callback used to retrieve memories corresponding to
     *     unrecognized slots.
     * @param requestChannel Input FMQ channel through which the client passes the
     *     request to the service.
     * @param resultChannel Output FMQ channel from which the client can retrieve
     *     the result of the execution.
     * @param preparedModel PreparedModel that the burst object was created from.
     *     IPreparedModel::executeSynchronously will be used to perform the
     *     execution.
     * @param pollingTimeWindow How much time (in microseconds) the
     *     ExecutionBurstServer is allowed to poll the FMQ before waiting on
     *     the blocking futex. Polling may result in lower latencies at the
     *     potential cost of more power usage.
     * @result IBurstContext Handle to the burst context.
     */
    static sp<ExecutionBurstServer> create(
            const sp<hardware::neuralnetworks::V1_2::IBurstCallback>& callback,
            const FmqRequestDescriptor& requestChannel, const FmqResultDescriptor& resultChannel,
            hardware::neuralnetworks::V1_2::IPreparedModel* preparedModel,
            std::chrono::microseconds pollingTimeWindow = std::chrono::microseconds{0});

    ExecutionBurstServer(const sp<hardware::neuralnetworks::V1_2::IBurstCallback>& callback,
                         std::unique_ptr<RequestChannelReceiver> requestChannel,
                         std::unique_ptr<ResultChannelSender> resultChannel,
                         std::shared_ptr<IBurstExecutorWithCache> cachedExecutor);
    ~ExecutionBurstServer();

    // Used by the NN runtime to preemptively remove any stored memory.
    hardware::Return<void> freeMemory(int32_t slot) override;

   private:
    // Ensures all cache entries contained in mExecutorWithCache are present in
    // the cache. If they are not present, they are retrieved (via
    // IBurstCallback::getMemories) and added to mExecutorWithCache.
    //
    // This method is locked via mMutex when it is called.
    void ensureCacheEntriesArePresentLocked(const std::vector<int32_t>& slots);

    // Work loop that will continue processing execution requests until the
    // ExecutionBurstServer object is freed.
    void task();

    std::thread mWorker;
    std::mutex mMutex;
    std::atomic<bool> mTeardown{false};
    const sp<hardware::neuralnetworks::V1_2::IBurstCallback> mCallback;
    const std::unique_ptr<RequestChannelReceiver> mRequestChannelReceiver;
    const std::unique_ptr<ResultChannelSender> mResultChannelSender;
    const std::shared_ptr<IBurstExecutorWithCache> mExecutorWithCache;
};

}  // namespace android::nn

#endif  // ANDROID_FRAMEWORKS_ML_NN_COMMON_EXECUTION_BURST_SERVER_H
