// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_V4L2_CODEC2_COMPONENTS_V4L2_ENCODE_COMPONENT_H
#define ANDROID_V4L2_CODEC2_COMPONENTS_V4L2_ENCODE_COMPONENT_H

#include <atomic>
#include <memory>
#include <optional>

#include <C2Component.h>
#include <C2ComponentFactory.h>
#include <C2Config.h>
#include <C2Enum.h>
#include <C2Param.h>
#include <C2ParamDef.h>
#include <SimpleC2Interface.h>
#include <base/memory/scoped_refptr.h>
#include <base/single_thread_task_runner.h>
#include <base/synchronization/waitable_event.h>
#include <base/threading/thread.h>
#include <util/C2InterfaceHelper.h>

#include <size.h>
#include <v4l2_codec2/common/FormatConverter.h>
#include <v4l2_codec2/components/V4L2EncodeInterface.h>
#include <video_frame_layout.h>

namespace media {
class V4L2Device;
class V4L2ReadableBuffer;
class V4L2Queue;
}  // namespace media

namespace android {

struct VideoFramePlane;

class V4L2EncodeComponent : public C2Component,
                            public std::enable_shared_from_this<V4L2EncodeComponent> {
public:
    // Create a new instance of the V4L2EncodeComponent.
    static std::shared_ptr<C2Component> create(C2String name, c2_node_id_t id,
                                               std::shared_ptr<C2ReflectorHelper> helper,
                                               C2ComponentFactory::ComponentDeleter deleter);

    virtual ~V4L2EncodeComponent() override;

    // Implementation of the C2Component interface.
    c2_status_t start() override;
    c2_status_t stop() override;
    c2_status_t reset() override;
    c2_status_t release() override;
    c2_status_t queue_nb(std::list<std::unique_ptr<C2Work>>* const items) override;
    c2_status_t drain_nb(drain_mode_t mode) override;
    c2_status_t flush_sm(flush_mode_t mode,
                         std::list<std::unique_ptr<C2Work>>* const flushedWork) override;
    c2_status_t announce_nb(const std::vector<C2WorkOutline>& items) override;
    c2_status_t setListener_vb(const std::shared_ptr<Listener>& listener,
                               c2_blocking_t mayBlock) override;
    std::shared_ptr<C2ComponentInterface> intf() override;

private:
    class InputFrame {
    public:
        // Create an input frame from a C2GraphicBlock.
        static std::unique_ptr<InputFrame> Create(const C2ConstGraphicBlock& block);
        ~InputFrame() = default;

        const std::vector<int>& getFDs() const { return mFds; }

    private:
        InputFrame(std::vector<int> fds) : mFds(std::move(fds)) {}
        const std::vector<int> mFds;
    };

    // Possible component states.
    enum class ComponentState {
        UNLOADED,  // Initial state of component.
        LOADED,    // The component is stopped, ready to start running.
        RUNNING,   // The component is currently running.
        ERROR,     // An error occurred.
    };

    // Possible encoder states.
    enum class EncoderState {
        UNINITIALIZED,              // Not initialized yet or initialization failed.
        WAITING_FOR_INPUT,          // Waiting for work to be queued.
        WAITING_FOR_INPUT_BUFFERS,  // Waiting for V4L2 input queue buffers.
        ENCODING,                   // Queuing input buffers.
        DRAINING,                   // Flushing encoder.
        ERROR,                      // encoder encountered an error.
    };

    V4L2EncodeComponent(C2String name, c2_node_id_t id,
                        std::shared_ptr<V4L2EncodeInterface> interface);

    V4L2EncodeComponent(const V4L2EncodeComponent&) = delete;
    V4L2EncodeComponent& operator=(const V4L2EncodeComponent&) = delete;

    // Initialize the encoder on the encoder thread.
    void startTask(bool* success, ::base::WaitableEvent* done);
    // Destroy the encoder on the encoder thread.
    void stopTask(::base::WaitableEvent* done);
    // Queue a new encode work item on the encoder thread.
    void queueTask(std::unique_ptr<C2Work> work);
    // Drain all currently scheduled work on the encoder thread. The encoder will process all
    // scheduled work and mark the last item as EOS, before processing any new work.
    void drainTask(drain_mode_t drainMode);
    // Called on the encoder thread when a drain is completed.
    void onDrainDone(bool done);
    // Flush all currently scheduled work on the encoder thread. The encoder will abort all
    // scheduled work items, work that can be immediately aborted will be placed in |flushedWork|.
    void flushTask(::base::WaitableEvent* done,
                   std::list<std::unique_ptr<C2Work>>* const flushedWork);
    // Set the component listener on the encoder thread.
    void setListenerTask(const std::shared_ptr<Listener>& listener, ::base::WaitableEvent* done);

    // Initialize the V4L2 device for encoding with the requested configuration.
    bool initializeEncoder();
    // Configure input format on the V4L2 device.
    bool configureInputFormat(media::VideoPixelFormat inputFormat);
    // Configure output format on the V4L2 device.
    bool configureOutputFormat(media::VideoCodecProfile outputProfile);
    // Configure required and optional controls on the V4L2 device.
    bool configureDevice(media::VideoCodecProfile outputProfile,
                         std::optional<const uint8_t> outputH264Level);
    // Update the |mBitrate| and |mFramerate| currently configured on the V4L2 device, to match the
    // values requested by the codec 2.0 framework.
    bool updateEncodingParameters();

    // Schedule the next encode operation on the V4L2 device.
    void scheduleNextEncodeTask();
    // Encode the specified |block| with corresponding |index| and |timestamp|.
    bool encode(C2ConstGraphicBlock block, uint64_t index, int64_t timestamp);
    // Drain the encoder.
    void drain();
    // Flush the encoder.
    void flush();

    // Fetch a new output buffer from the output block pool.
    std::shared_ptr<C2LinearBlock> fetchOutputBlock();

    // Called on the encoder thread when the encoder is done using an input buffer.
    void onInputBufferDone(uint64_t index);
    // Called on the encoder thread when an output buffer is ready.
    void onOutputBufferDone(uint32_t payloadSize, bool keyFrame, int64_t timestamp,
                            std::shared_ptr<C2LinearBlock> outputBlock);

    // Helper function to find a work item in the output work queue by index.
    C2Work* getWorkByIndex(uint64_t index);
    // Helper function to find a work item in the output work queue by timestamp.
    C2Work* getWorkByTimestamp(int64_t timestamp);
    // Helper function to determine if the specified |work| item is finished.
    bool isWorkDone(const C2Work& work) const;
    // Notify the listener the specified |work| item is finished.
    void reportWork(std::unique_ptr<C2Work> work);

    // Attempt to start the V4L2 device poller.
    bool startDevicePoll();
    // Attempt to stop the V4L2 device poller.
    bool stopDevicePoll();
    // Called by the V4L2 device poller on the |mEncoderTaskRunner| whenever an error occurred.
    void onPollError();
    // Service I/O on the V4L2 device, called by the V4L2 device poller on the |mEncoderTaskRunner|.
    void serviceDeviceTask(bool event);

    // Enqueue an input buffer to be encoded on the device input queue. Returns whether the
    // operation was successful.
    bool enqueueInputBuffer(std::unique_ptr<InputFrame> frame, media::VideoPixelFormat format,
                            const std::vector<VideoFramePlane>& planes, int64_t index,
                            int64_t timestamp);
    // Enqueue an output buffer to store the encoded bitstream on the device output queue. Returns
    // whether the operation was successful.
    bool enqueueOutputBuffer();
    // Dequeue an input buffer the V4L2 device has finished encoding on the device input queue.
    // Returns whether a buffer could be dequeued.
    bool dequeueInputBuffer();
    // Dequeue an output buffer containing the encoded bitstream from the device output queue. The
    // bitstream is copied into another buffer that is sent to the client, after which the output
    // buffer is returned to the queue. Returns whether the operation was successful.
    bool dequeueOutputBuffer();

    // Create input buffers on the V4L2 device input queue.
    bool createInputBuffers();
    // Create output buffers on the V4L2 device output queue.
    bool createOutputBuffers();
    // Destroy the input buffers on the V4L2 device input queue.
    void destroyInputBuffers();
    // Destroy the output buffers on the V4L2 device output queue.
    void destroyOutputBuffers();

    // Notify the client an error occurred and switch to the error state.
    void reportError(c2_status_t error);

    // Change the state of the component.
    void setComponentState(ComponentState state);
    // Change the state of the encoder, only called on the encoder thread.
    void setEncoderState(EncoderState state);
    // Get the specified component |state| as string.
    static const char* componentStateToString(ComponentState state);
    // Get the specified encoder |state| as string.
    static const char* encoderStateToString(EncoderState state);

    // The component's registered name.
    const C2String mName;
    // The component's id, provided by the C2 framework upon initialization.
    const c2_node_id_t mId = 0;
    // The component's interface implementation.
    const std::shared_ptr<V4L2EncodeInterface> mInterface;

    // Mutex used by the component to synchronize start/stop/reset/release calls, as the codec 2.0
    // API can be accessed from any thread.
    std::mutex mComponentLock;

    // The component's listener to be notified when events occur, only accessed on encoder thread.
    std::shared_ptr<Listener> mListener;

    // The V4L2 device used to interact with the driver, only accessed on encoder thread.
    scoped_refptr<media::V4L2Device> mDevice;
    scoped_refptr<media::V4L2Queue> mInputQueue;
    scoped_refptr<media::V4L2Queue> mOutputQueue;

    // The video stream's visible size.
    media::Size mVisibleSize;
    // The video stream's coded size.
    media::Size mInputCodedSize;
    // The input layout configured on the V4L2 device.
    std::optional<media::VideoFrameLayout> mInputLayout;
    // An input format convertor will be used if the device doesn't support the video's format.
    std::unique_ptr<FormatConverter> mInputFormatConverter;
    // Required output buffer byte size.
    uint32_t mOutputBufferSize = 0;

    // The bitrate currently configured on the v4l2 device.
    uint32_t mBitrate = 0;
    // The framerate currently configured on the v4l2 device.
    uint32_t mFramerate = 0;

    // How often we want to request the V4L2 device to create a key frame.
    uint32_t mKeyFramePeriod = 0;
    // Key frame counter, a key frame will be requested each time it reaches zero.
    uint32_t mKeyFrameCounter = 0;

    // Whether we extracted and submitted CSD (codec-specific data, e.g. H.264 SPS) to the framework.
    bool mCSDSubmitted = false;

    // The queue of encode work items to be processed.
    std::queue<std::unique_ptr<C2Work>> mInputWorkQueue;
    // The queue of encode work items currently being processed.
    std::deque<std::unique_ptr<C2Work>> mOutputWorkQueue;

    // List of work item indices and frames associated with each buffer in the device input queue.
    std::vector<std::pair<int64_t, std::unique_ptr<InputFrame>>> mInputBuffersMap;

    // Map of buffer indices and output blocks associated with each buffer in the output queue. This
    // map keeps the C2LinearBlock buffers alive so we can avoid duplicated fds.
    std::vector<std::shared_ptr<C2LinearBlock>> mOutputBuffersMap;
    // The output block pool.
    std::shared_ptr<C2BlockPool> mOutputBlockPool;

    // The component state, accessible from any thread as C2Component interface is not thread-safe.
    std::atomic<ComponentState> mComponentState;
    // The current state of the encoder, only accessed on the encoder thread.
    EncoderState mEncoderState = EncoderState::UNINITIALIZED;

    // The encoder thread on which all interaction with the V4L2 device is performed.
    ::base::Thread mEncoderThread{"V4L2EncodeComponentThread"};
    // The task runner on the encoder thread.
    scoped_refptr<::base::SequencedTaskRunner> mEncoderTaskRunner;

    // The WeakPtrFactory used to get weak pointers of this.
    ::base::WeakPtr<V4L2EncodeComponent> mWeakThis;
    ::base::WeakPtrFactory<V4L2EncodeComponent> mWeakThisFactory{this};
};

}  // namespace android

#endif  // ANDROID_V4L2_CODEC2_COMPONENTS_V4L2_ENCODE_COMPONENT_H
