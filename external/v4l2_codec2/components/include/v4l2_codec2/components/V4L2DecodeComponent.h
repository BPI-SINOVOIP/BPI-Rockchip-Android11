// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_V4L2_CODEC2_COMPONENTS_V4L2_DECODE_COMPONENT_H
#define ANDROID_V4L2_CODEC2_COMPONENTS_V4L2_DECODE_COMPONENT_H

#include <memory>

#include <C2Component.h>
#include <C2ComponentFactory.h>
#include <C2Work.h>
#include <base/memory/scoped_refptr.h>
#include <base/memory/weak_ptr.h>
#include <base/sequenced_task_runner.h>
#include <base/synchronization/waitable_event.h>
#include <base/threading/thread.h>

#include <v4l2_codec2/components/V4L2DecodeInterface.h>
#include <v4l2_codec2/components/VideoDecoder.h>
#include <v4l2_codec2/components/VideoFramePool.h>
#include <v4l2_device.h>

namespace android {

class V4L2DecodeComponent : public C2Component,
                            public std::enable_shared_from_this<V4L2DecodeComponent> {
public:
    static std::shared_ptr<C2Component> create(const std::string& name, c2_node_id_t id,
                                               const std::shared_ptr<C2ReflectorHelper>& helper,
                                               C2ComponentFactory::ComponentDeleter deleter);
    V4L2DecodeComponent(const std::string& name, c2_node_id_t id,
                        const std::shared_ptr<C2ReflectorHelper>& helper,
                        const std::shared_ptr<V4L2DecodeInterface>& intfImpl);
    ~V4L2DecodeComponent() override;

    // Implementation of C2Component.
    c2_status_t start() override;
    c2_status_t stop() override;
    c2_status_t reset() override;
    c2_status_t release() override;
    c2_status_t setListener_vb(const std::shared_ptr<Listener>& listener,
                               c2_blocking_t mayBlock) override;
    c2_status_t queue_nb(std::list<std::unique_ptr<C2Work>>* const items) override;
    c2_status_t announce_nb(const std::vector<C2WorkOutline>& items) override;
    c2_status_t flush_sm(flush_mode_t mode,
                         std::list<std::unique_ptr<C2Work>>* const flushedWork) override;
    c2_status_t drain_nb(drain_mode_t mode) override;
    std::shared_ptr<C2ComponentInterface> intf() override;

private:
    // The C2Component state machine.
    enum class ComponentState {
        STOPPED,
        RUNNING,
        RELEASED,
        ERROR,
    };
    static const char* ComponentStateToString(ComponentState state);

    // Handle C2Component's public methods on |mDecoderTaskRunner|.
    void destroyTask();
    void startTask(c2_status_t* status);
    void stopTask();
    void queueTask(std::unique_ptr<C2Work> work);
    void flushTask();
    void drainTask();
    void setListenerTask(const std::shared_ptr<Listener>& listener, ::base::WaitableEvent* done);

    // Try to process pending works at |mPendingWorks|. Paused when |mIsDraining| is set.
    void pumpPendingWorks();
    // Get the buffer pool.
    void getVideoFramePool(std::unique_ptr<VideoFramePool>* pool, const media::Size& size,
                           HalPixelFormat pixelFormat, size_t numBuffers);
    // Detect and report works with no-show frame, only used at VP8 and VP9.
    void detectNoShowFrameWorksAndReportIfFinished(const C2WorkOrdinalStruct& currOrdinal);

    // Finish callbacks of each method.
    void onOutputFrameReady(std::unique_ptr<VideoFrame> frame);
    void onDecodeDone(int32_t bitstreamId, VideoDecoder::DecodeStatus status);
    void onDrainDone(VideoDecoder::DecodeStatus status);
    void onFlushDone();

    // Try to process decoding works at |mPendingWorks|.
    void pumpReportWork();
    // Report finished work.
    bool reportWorkIfFinished(int32_t bitstreamId);
    bool reportEOSWork();
    void reportAbandonedWorks();
    bool reportWork(std::unique_ptr<C2Work> work);
    // Report error when any error occurs.
    void reportError(c2_status_t error);

    // The pointer of component interface implementation.
    std::shared_ptr<V4L2DecodeInterface> mIntfImpl;
    // The pointer of component interface.
    const std::shared_ptr<C2ComponentInterface> mIntf;
    // The pointer of component listener.
    std::shared_ptr<Listener> mListener;

    std::unique_ptr<VideoDecoder> mDecoder;
    // The queue of works that haven't processed and sent to |mDecoder|.
    std::queue<std::unique_ptr<C2Work>> mPendingWorks;
    // The works whose input buffers are sent to |mDecoder|. The key is the
    // bitstream ID of work's input buffer.
    std::map<int32_t, std::unique_ptr<C2Work>> mWorksAtDecoder;
    // The bitstream ID of the works that output frames have been returned from |mDecoder|.
    // The order is display order.
    std::queue<int32_t> mOutputBitstreamIds;

    // Set to true when decoding the protected playback.
    bool mIsSecure = false;
    // The component state.
    std::atomic<ComponentState> mComponentState{ComponentState::STOPPED};
    // Whether we are currently draining the component. This is set when the component is processing
    // the drain request, and unset either after reportEOSWork() (EOS is outputted), or
    // reportAbandonedWorks() (drain is cancelled and works are abandoned).
    bool mIsDraining = false;

    // The mutex lock to synchronize start/stop/reset/release calls.
    std::mutex mStartStopLock;
    ::base::WaitableEvent mStartStopDone;

    // The color aspects parameter for current decoded output buffers.
    std::shared_ptr<C2StreamColorAspectsInfo::output> mCurrentColorAspects;
    // The flag of pending color aspects change. This should be set once we have parsed color
    // aspects from bitstream by parseCodedColorAspects(), at the same time recorded input frame
    // index into |mPendingColorAspectsChangeFrameIndex|.
    // When this flag is true and the corresponding frame index is not less than
    // |mPendingColorAspectsChangeFrameIndex| for the output buffer in onOutputBufferDone(), update
    // |mCurrentColorAspects| from component interface and reset the flag.
    bool mPendingColorAspectsChange = false;
    // The record of frame index to update color aspects. Details as above.
    uint64_t mPendingColorAspectsChangeFrameIndex;

    // The device task runner and its sequence checker. We should interact with
    // |mDevice| on this.
    ::base::Thread mDecoderThread{"V4L2DecodeComponentDecoderThread"};
    scoped_refptr<::base::SequencedTaskRunner> mDecoderTaskRunner;

    ::base::WeakPtrFactory<V4L2DecodeComponent> mWeakThisFactory{this};
    ::base::WeakPtr<V4L2DecodeComponent> mWeakThis;
};

}  // namespace android

#endif  // ANDROID_V4L2_CODEC2_COMPONENTS_V4L2_DECODE_COMPONENT_H
