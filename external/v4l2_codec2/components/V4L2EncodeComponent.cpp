// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//#define LOG_NDEBUG 0
#define LOG_TAG "V4L2EncodeComponent"

#include <v4l2_codec2/components/V4L2EncodeComponent.h>

#include <inttypes.h>

#include <algorithm>
#include <utility>

#include <C2AllocatorGralloc.h>
#include <C2PlatformSupport.h>
#include <C2Work.h>
#include <android/hardware/graphics/common/1.0/types.h>
#include <base/bind.h>
#include <base/bind_helpers.h>
#include <log/log.h>
#include <media/stagefright/MediaDefs.h>
#include <ui/GraphicBuffer.h>

#include <fourcc.h>
#include <h264_parser.h>
#include <rect.h>
#include <v4l2_codec2/common/Common.h>
#include <v4l2_codec2/common/EncodeHelpers.h>
#include <v4l2_device.h>
#include <video_pixel_format.h>

using android::hardware::graphics::common::V1_0::BufferUsage;

namespace android {

namespace {

const media::VideoPixelFormat kInputPixelFormat = media::VideoPixelFormat::PIXEL_FORMAT_NV12;

// Get the video frame layout from the specified |inputBlock|.
// TODO(dstaessens): Clean up code extracting layout from a C2GraphicBlock.
std::optional<std::vector<VideoFramePlane>> getVideoFrameLayout(const C2ConstGraphicBlock& block,
                                                                media::VideoPixelFormat* format) {
    ALOGV("%s()", __func__);

    // Get the C2PlanarLayout from the graphics block. The C2GraphicView returned by block.map()
    // needs to be released before calling getGraphicBlockInfo(), or the lockYCbCr() call will block
    // Indefinitely.
    C2PlanarLayout layout = block.map().get().layout();

    // The above layout() cannot fill layout information and memset 0 instead if the input format is
    // IMPLEMENTATION_DEFINED and its backed format is RGB. We fill the layout by using
    // ImplDefinedToRGBXMap in the case.
    if (layout.type == C2PlanarLayout::TYPE_UNKNOWN) {
        std::unique_ptr<ImplDefinedToRGBXMap> idMap = ImplDefinedToRGBXMap::Create(block);
        if (idMap == nullptr) {
            ALOGE("Unable to parse RGBX_8888 from IMPLEMENTATION_DEFINED");
            return std::nullopt;
        }
        layout.type = C2PlanarLayout::TYPE_RGB;
        // These parameters would be used in TYPE_GRB case below.
        layout.numPlanes = 3;   // same value as in C2AllocationGralloc::map()
        layout.rootPlanes = 1;  // same value as in C2AllocationGralloc::map()
        layout.planes[C2PlanarLayout::PLANE_R].offset = idMap->offset();
        layout.planes[C2PlanarLayout::PLANE_R].rowInc = idMap->rowInc();
    }

    std::vector<uint32_t> offsets(layout.numPlanes, 0u);
    std::vector<uint32_t> strides(layout.numPlanes, 0u);
    switch (layout.type) {
    case C2PlanarLayout::TYPE_YUV: {
        android_ycbcr ycbcr = getGraphicBlockInfo(block);
        offsets[C2PlanarLayout::PLANE_Y] =
                static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ycbcr.y));
        offsets[C2PlanarLayout::PLANE_U] =
                static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ycbcr.cb));
        offsets[C2PlanarLayout::PLANE_V] =
                static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ycbcr.cr));
        strides[C2PlanarLayout::PLANE_Y] = static_cast<uint32_t>(ycbcr.ystride);
        strides[C2PlanarLayout::PLANE_U] = static_cast<uint32_t>(ycbcr.cstride);
        strides[C2PlanarLayout::PLANE_V] = static_cast<uint32_t>(ycbcr.cstride);

        bool crcb = false;
        if (offsets[C2PlanarLayout::PLANE_U] > offsets[C2PlanarLayout::PLANE_V]) {
            // Swap offsets, no need to swap strides as they are identical for both chroma planes.
            std::swap(offsets[C2PlanarLayout::PLANE_U], offsets[C2PlanarLayout::PLANE_V]);
            crcb = true;
        }

        bool semiplanar = false;
        if (ycbcr.chroma_step >
            offsets[C2PlanarLayout::PLANE_V] - offsets[C2PlanarLayout::PLANE_U]) {
            semiplanar = true;
        }

        if (!crcb && !semiplanar) {
            *format = media::VideoPixelFormat::PIXEL_FORMAT_I420;
        } else if (!crcb && semiplanar) {
            *format = media::VideoPixelFormat::PIXEL_FORMAT_NV12;
        } else if (crcb && !semiplanar) {
            // HACK: pretend YV12 is I420 now since VEA only accepts I420. (YV12 will be used
            //       for input byte-buffer mode).
            // TODO(dstaessens): Is this hack still necessary now we're not using the VEA directly?
            //format = media::VideoPixelFormat::PIXEL_FORMAT_YV12;
            *format = media::VideoPixelFormat::PIXEL_FORMAT_I420;
        } else {
            *format = media::VideoPixelFormat::PIXEL_FORMAT_NV21;
        }
        break;
    }
    case C2PlanarLayout::TYPE_RGB: {
        offsets[C2PlanarLayout::PLANE_R] = layout.planes[C2PlanarLayout::PLANE_R].offset;
        strides[C2PlanarLayout::PLANE_R] =
                static_cast<uint32_t>(layout.planes[C2PlanarLayout::PLANE_R].rowInc);
        *format = media::VideoPixelFormat::PIXEL_FORMAT_ARGB;
        break;
    }
    default:
        ALOGW("Unknown layout type: %u", static_cast<uint32_t>(layout.type));
        return std::nullopt;
    }

    std::vector<VideoFramePlane> planes;
    for (uint32_t i = 0; i < layout.rootPlanes; ++i) {
        planes.push_back({offsets[i], strides[i]});
    }
    return planes;
}

// The maximum size for output buffer, which is chosen empirically for a 1080p video.
constexpr size_t kMaxBitstreamBufferSizeInBytes = 2 * 1024 * 1024;  // 2MB
// The frame size for 1080p (FHD) video in pixels.
constexpr int k1080PSizeInPixels = 1920 * 1080;
// The frame size for 1440p (QHD) video in pixels.
constexpr int k1440PSizeInPixels = 2560 * 1440;

// Use quadruple size of kMaxBitstreamBufferSizeInBytes when the input frame size is larger than
// 1440p, double if larger than 1080p. This is chosen empirically for some 4k encoding use cases and
// the Android CTS VideoEncoderTest (crbug.com/927284).
size_t GetMaxOutputBufferSize(const media::Size& size) {
    if (size.GetArea() > k1440PSizeInPixels) return kMaxBitstreamBufferSizeInBytes * 4;
    if (size.GetArea() > k1080PSizeInPixels) return kMaxBitstreamBufferSizeInBytes * 2;
    return kMaxBitstreamBufferSizeInBytes;
}

// These are rather subjectively tuned.
constexpr size_t kInputBufferCount = 2;
constexpr size_t kOutputBufferCount = 2;

// Define V4L2_CID_MPEG_VIDEO_H264_SPS_PPS_BEFORE_IDR control code if not present in header files.
#ifndef V4L2_CID_MPEG_VIDEO_H264_SPS_PPS_BEFORE_IDR
#define V4L2_CID_MPEG_VIDEO_H264_SPS_PPS_BEFORE_IDR (V4L2_CID_MPEG_BASE + 388)
#endif

}  // namespace

// static
std::unique_ptr<V4L2EncodeComponent::InputFrame> V4L2EncodeComponent::InputFrame::Create(
        const C2ConstGraphicBlock& block) {
    std::vector<int> fds;
    const C2Handle* const handle = block.handle();
    for (int i = 0; i < handle->numFds; i++) {
        fds.emplace_back(handle->data[i]);
    }

    return std::unique_ptr<InputFrame>(new InputFrame(std::move(fds)));
}

// static
std::shared_ptr<C2Component> V4L2EncodeComponent::create(
        C2String name, c2_node_id_t id, std::shared_ptr<C2ReflectorHelper> helper,
        C2ComponentFactory::ComponentDeleter deleter) {
    ALOGV("%s(%s)", __func__, name.c_str());

    auto interface = std::make_shared<V4L2EncodeInterface>(name, std::move(helper));
    if (interface->status() != C2_OK) {
        ALOGE("Component interface initialization failed (error code %d)", interface->status());
        return nullptr;
    }

    return std::shared_ptr<C2Component>(new V4L2EncodeComponent(name, id, std::move(interface)),
                                        deleter);
}

V4L2EncodeComponent::V4L2EncodeComponent(C2String name, c2_node_id_t id,
                                         std::shared_ptr<V4L2EncodeInterface> interface)
      : mName(name),
        mId(id),
        mInterface(std::move(interface)),
        mComponentState(ComponentState::LOADED) {
    ALOGV("%s(%s)", __func__, name.c_str());
}

V4L2EncodeComponent::~V4L2EncodeComponent() {
    ALOGV("%s()", __func__);

    // Stop encoder thread and invalidate pointers if component wasn't stopped before destroying.
    if (mEncoderThread.IsRunning()) {
        mEncoderTaskRunner->PostTask(
                FROM_HERE, ::base::BindOnce(
                                   [](::base::WeakPtrFactory<V4L2EncodeComponent>* weakPtrFactory) {
                                       weakPtrFactory->InvalidateWeakPtrs();
                                   },
                                   &mWeakThisFactory));
        mEncoderThread.Stop();
    }
    ALOGV("%s(): done", __func__);
}

c2_status_t V4L2EncodeComponent::start() {
    ALOGV("%s()", __func__);

    // Lock while starting, to synchronize start/stop/reset/release calls.
    std::lock_guard<std::mutex> lock(mComponentLock);

    // According to the specification start() should only be called in the LOADED state.
    if (mComponentState != ComponentState::LOADED) {
        return C2_BAD_STATE;
    }

    if (!mEncoderThread.Start()) {
        ALOGE("Failed to start encoder thread");
        return C2_CORRUPTED;
    }
    mEncoderTaskRunner = mEncoderThread.task_runner();
    mWeakThis = mWeakThisFactory.GetWeakPtr();

    // Initialize the encoder on the encoder thread.
    ::base::WaitableEvent done;
    bool success = false;
    mEncoderTaskRunner->PostTask(
            FROM_HERE, ::base::Bind(&V4L2EncodeComponent::startTask, mWeakThis, &success, &done));
    done.Wait();

    if (!success) {
        ALOGE("Failed to initialize encoder");
        return C2_CORRUPTED;
    }

    setComponentState(ComponentState::RUNNING);
    return C2_OK;
}

c2_status_t V4L2EncodeComponent::stop() {
    ALOGV("%s()", __func__);

    // Lock while stopping, to synchronize start/stop/reset/release calls.
    std::lock_guard<std::mutex> lock(mComponentLock);

    if (mComponentState != ComponentState::RUNNING && mComponentState != ComponentState::ERROR) {
        return C2_BAD_STATE;
    }

    // Return immediately if the component is already stopped.
    if (!mEncoderThread.IsRunning()) {
        return C2_OK;
    }

    // Wait for the component to stop.
    ::base::WaitableEvent done;
    mEncoderTaskRunner->PostTask(
            FROM_HERE, ::base::BindOnce(&V4L2EncodeComponent::stopTask, mWeakThis, &done));
    done.Wait();
    mEncoderThread.Stop();

    setComponentState(ComponentState::LOADED);

    ALOGV("%s() - done", __func__);
    return C2_OK;
}

c2_status_t V4L2EncodeComponent::reset() {
    ALOGV("%s()", __func__);

    // The interface specification says: "This method MUST be supported in all (including tripped)
    // states other than released".
    if (mComponentState == ComponentState::UNLOADED) {
        return C2_BAD_STATE;
    }

    // TODO(dstaessens): Reset the component's interface to default values.
    stop();

    return C2_OK;
}

c2_status_t V4L2EncodeComponent::release() {
    ALOGV("%s()", __func__);

    // The interface specification says: "This method MUST be supported in stopped state.", but the
    // release method seems to be called in other states as well.
    reset();

    setComponentState(ComponentState::UNLOADED);
    return C2_OK;
}

c2_status_t V4L2EncodeComponent::queue_nb(std::list<std::unique_ptr<C2Work>>* const items) {
    ALOGV("%s()", __func__);

    if (mComponentState != ComponentState::RUNNING) {
        ALOGE("Trying to queue work item while component is not running");
        return C2_BAD_STATE;
    }

    while (!items->empty()) {
        mEncoderTaskRunner->PostTask(FROM_HERE,
                                     ::base::BindOnce(&V4L2EncodeComponent::queueTask, mWeakThis,
                                                      std::move(items->front())));
        items->pop_front();
    }

    return C2_OK;
}

c2_status_t V4L2EncodeComponent::drain_nb(drain_mode_t mode) {
    ALOGV("%s()", __func__);

    if (mode == DRAIN_CHAIN) {
        return C2_OMITTED;  // Tunneling is not supported for now.
    }

    if (mComponentState != ComponentState::RUNNING) {
        return C2_BAD_STATE;
    }

    mEncoderTaskRunner->PostTask(
            FROM_HERE, ::base::BindOnce(&V4L2EncodeComponent::drainTask, mWeakThis, mode));
    return C2_OK;
}

c2_status_t V4L2EncodeComponent::flush_sm(flush_mode_t mode,
                                          std::list<std::unique_ptr<C2Work>>* const flushedWork) {
    ALOGV("%s()", __func__);

    if (mode != FLUSH_COMPONENT) {
        return C2_OMITTED;  // Tunneling is not supported by now
    }

    if (mComponentState != ComponentState::RUNNING) {
        return C2_BAD_STATE;
    }

    // Work that can be immediately discarded should be returned in |flushedWork|. This method may
    // be momentarily blocking but must return within 5ms, which should give us enough time to
    // immediately abandon all non-started work on the encoder thread. We can return all work that
    // can't be immediately discarded using onWorkDone() later.
    ::base::WaitableEvent done;
    mEncoderTaskRunner->PostTask(FROM_HERE, ::base::BindOnce(&V4L2EncodeComponent::flushTask,
                                                             mWeakThis, &done, flushedWork));
    done.Wait();

    return C2_OK;
}

c2_status_t V4L2EncodeComponent::announce_nb(const std::vector<C2WorkOutline>& items) {
    return C2_OMITTED;  // Tunneling is not supported by now
}

c2_status_t V4L2EncodeComponent::setListener_vb(const std::shared_ptr<Listener>& listener,
                                                c2_blocking_t mayBlock) {
    ALOG_ASSERT(mComponentState != ComponentState::UNLOADED);

    // Lock so we're sure the component isn't currently starting or stopping.
    std::lock_guard<std::mutex> lock(mComponentLock);

    // If the encoder thread is not running it's safe to update the listener directly.
    if (!mEncoderThread.IsRunning()) {
        mListener = listener;
        return C2_OK;
    }

    // The listener should be updated before exiting this function. If called while the component is
    // currently running we should be allowed to block, as we can only change the listener on the
    // encoder thread.
    ALOG_ASSERT(mayBlock == c2_blocking_t::C2_MAY_BLOCK);

    ::base::WaitableEvent done;
    mEncoderTaskRunner->PostTask(FROM_HERE, ::base::BindOnce(&V4L2EncodeComponent::setListenerTask,
                                                             mWeakThis, listener, &done));
    done.Wait();

    return C2_OK;
}

std::shared_ptr<C2ComponentInterface> V4L2EncodeComponent::intf() {
    return std::make_shared<SimpleInterface<V4L2EncodeInterface>>(mName.c_str(), mId, mInterface);
}

void V4L2EncodeComponent::startTask(bool* success, ::base::WaitableEvent* done) {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(mEncoderState == EncoderState::UNINITIALIZED);

    *success = initializeEncoder();
    done->Signal();
}

void V4L2EncodeComponent::stopTask(::base::WaitableEvent* done) {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());

    // Flushing the encoder will abort all pending work and stop polling and streaming on the V4L2
    // device queues.
    flush();

    // Deallocate all V4L2 device input and output buffers.
    destroyInputBuffers();
    destroyOutputBuffers();

    // Invalidate all weak pointers so no more functions will be executed on the encoder thread.
    mWeakThisFactory.InvalidateWeakPtrs();

    setEncoderState(EncoderState::UNINITIALIZED);
    done->Signal();
}

void V4L2EncodeComponent::queueTask(std::unique_ptr<C2Work> work) {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(mEncoderState != EncoderState::UNINITIALIZED);

    // If we're in the error state we can immediately return, freeing all buffers in the work item.
    if (mEncoderState == EncoderState::ERROR) {
        return;
    }

    ALOGV("Queued work item (index: %llu, timestamp: %llu, EOS: %d)",
          work->input.ordinal.frameIndex.peekull(), work->input.ordinal.timestamp.peekull(),
          work->input.flags & C2FrameData::FLAG_END_OF_STREAM);

    mInputWorkQueue.push(std::move(work));

    // If we were waiting for work, start encoding again.
    if (mEncoderState == EncoderState::WAITING_FOR_INPUT) {
        setEncoderState(EncoderState::ENCODING);
        mEncoderTaskRunner->PostTask(
                FROM_HERE,
                ::base::BindOnce(&V4L2EncodeComponent::scheduleNextEncodeTask, mWeakThis));
    }
}

// TODO(dstaessens): Investigate improving drain logic after draining the virtio device is fixed.
void V4L2EncodeComponent::drainTask(drain_mode_t /*drainMode*/) {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());

    // We can only start draining if all the work in our input queue has been queued on the V4L2
    // device input queue, so we mark the last item in the input queue as EOS.
    if (!mInputWorkQueue.empty()) {
        ALOGV("Marking last item in input work queue as EOS");
        mInputWorkQueue.back()->input.flags = static_cast<C2FrameData::flags_t>(
                mInputWorkQueue.back()->input.flags | C2FrameData::FLAG_END_OF_STREAM);
        return;
    }

    // If the input queue is empty and there is only a single empty EOS work item in the output
    // queue we can immediately consider flushing done.
    if ((mOutputWorkQueue.size() == 1) && mOutputWorkQueue.back()->input.buffers.empty()) {
        ALOG_ASSERT(mOutputWorkQueue.back()->input.flags & C2FrameData::FLAG_END_OF_STREAM);
        setEncoderState(EncoderState::DRAINING);
        mEncoderTaskRunner->PostTask(
                FROM_HERE, ::base::BindOnce(&V4L2EncodeComponent::onDrainDone, mWeakThis, true));
        return;
    }

    // If the input queue is empty all work that needs to be drained has already been queued in the
    // V4L2 device, so we can immediately request a drain.
    if (!mOutputWorkQueue.empty()) {
        // Mark the last item in the output work queue as EOS, so we will only report it as
        // finished after draining has completed.
        ALOGV("Starting drain and marking last item in output work queue as EOS");
        mOutputWorkQueue.back()->input.flags = C2FrameData::FLAG_END_OF_STREAM;
        drain();
    }
}

void V4L2EncodeComponent::onDrainDone(bool done) {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(mEncoderState == EncoderState::DRAINING || mEncoderState == EncoderState::ERROR);

    if (mEncoderState == EncoderState::ERROR) {
        return;
    }

    if (!done) {
        ALOGE("draining the encoder failed");
        reportError(C2_CORRUPTED);
        return;
    }

    // The last work item in the output work queue should be an EOS request.
    if (mOutputWorkQueue.empty() ||
        !(mOutputWorkQueue.back()->input.flags & C2FrameData::FLAG_END_OF_STREAM)) {
        ALOGE("The last item in the output work queue should be marked EOS");
        reportError(C2_CORRUPTED);
        return;
    }

    // Mark the last item in the output work queue as EOS done.
    C2Work* eosWork = mOutputWorkQueue.back().get();
    eosWork->worklets.back()->output.flags = C2FrameData::FLAG_END_OF_STREAM;

    // Draining is done which means all buffers on the device output queue have been returned, but
    // not all buffers on the device input queue might have been returned yet.
    if ((mOutputWorkQueue.size() > 1) || !isWorkDone(*eosWork)) {
        ALOGV("Draining done, waiting for input buffers to be returned");
        return;
    }

    ALOGV("Draining done");
    reportWork(std::move(mOutputWorkQueue.front()));
    mOutputWorkQueue.pop_front();

    // Draining the encoder is now done, we can start encoding again.
    if (!mInputWorkQueue.empty()) {
        setEncoderState(EncoderState::ENCODING);
        mEncoderTaskRunner->PostTask(
                FROM_HERE,
                ::base::BindOnce(&V4L2EncodeComponent::scheduleNextEncodeTask, mWeakThis));
    } else {
        setEncoderState(EncoderState::WAITING_FOR_INPUT);
    }
}

void V4L2EncodeComponent::flushTask(::base::WaitableEvent* done,
                                    std::list<std::unique_ptr<C2Work>>* const flushedWork) {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());

    // Move all work that can immediately be aborted to flushedWork, and notify the caller.
    if (flushedWork) {
        while (!mInputWorkQueue.empty()) {
            std::unique_ptr<C2Work> work = std::move(mInputWorkQueue.front());
            work->input.buffers.clear();
            flushedWork->push_back(std::move(work));
            mInputWorkQueue.pop();
        }
    }
    done->Signal();

    flush();
}

void V4L2EncodeComponent::setListenerTask(const std::shared_ptr<Listener>& listener,
                                          ::base::WaitableEvent* done) {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());

    mListener = listener;
    done->Signal();
}

bool V4L2EncodeComponent::initializeEncoder() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(mEncoderState == EncoderState::UNINITIALIZED);

    mVisibleSize = mInterface->getInputVisibleSize();
    mKeyFramePeriod = mInterface->getKeyFramePeriod();
    mKeyFrameCounter = 0;
    mCSDSubmitted = false;

    // Open the V4L2 device for encoding to the requested output format.
    // TODO(dstaessens): Do we need to close the device first if already opened?
    // TODO(dstaessens): Avoid conversion to VideoCodecProfile and use C2Config::profile_t directly.
    media::VideoCodecProfile outputProfile =
            c2ProfileToVideoCodecProfile(mInterface->getOutputProfile());
    uint32_t outputPixelFormat =
            media::V4L2Device::VideoCodecProfileToV4L2PixFmt(outputProfile, false);
    if (!outputPixelFormat) {
        ALOGE("Invalid output profile %s", media::GetProfileName(outputProfile).c_str());
        return false;
    }

    mDevice = media::V4L2Device::Create();
    if (!mDevice) {
        ALOGE("Failed to create V4L2 device");
        return false;
    }

    if (!mDevice->Open(media::V4L2Device::Type::kEncoder, outputPixelFormat)) {
        ALOGE("Failed to open device for profile %s (%s)",
              media::GetProfileName(outputProfile).c_str(),
              media::FourccToString(outputPixelFormat).c_str());
        return false;
    }

    // Make sure the device has all required capabilities (multi-planar Memory-To-Memory and
    // streaming I/O), and whether flushing is supported.
    if (!mDevice->HasCapabilities(V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING)) {
        ALOGE("Device doesn't have the required capabilities");
        return false;
    }
    if (!mDevice->IsCommandSupported(V4L2_ENC_CMD_STOP)) {
        ALOGE("Device does not support flushing (V4L2_ENC_CMD_STOP)");
        return false;
    }

    // Get input/output queues so we can send encode request to the device and get back the results.
    mInputQueue = mDevice->GetQueue(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
    mOutputQueue = mDevice->GetQueue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    if (!mInputQueue || !mOutputQueue) {
        ALOGE("Failed to get V4L2 device queues");
        return false;
    }

    // First try to configure the specified output format, as changing the output format can affect
    // the configured input format.
    if (!configureOutputFormat(outputProfile)) return false;

    // Configure the input format. If the device doesn't support the specified format we'll use one
    // of the device's preferred formats in combination with an input format convertor.
    if (!configureInputFormat(kInputPixelFormat)) return false;

    // Create input and output buffers.
    // TODO(dstaessens): Avoid allocating output buffers, encode directly into blockpool buffers.
    if (!createInputBuffers() || !createOutputBuffers()) return false;

    // Configure the device, setting all required controls.
    uint8_t level = c2LevelToLevelIDC(mInterface->getOutputLevel());
    if (!configureDevice(outputProfile, level)) return false;

    // We're ready to start encoding now.
    setEncoderState(EncoderState::WAITING_FOR_INPUT);

    // As initialization is asynchronous work might have already be queued.
    if (!mInputWorkQueue.empty()) {
        setEncoderState(EncoderState::ENCODING);
        mEncoderTaskRunner->PostTask(
                FROM_HERE, ::base::Bind(&V4L2EncodeComponent::scheduleNextEncodeTask, mWeakThis));
    }
    return true;
}

bool V4L2EncodeComponent::configureInputFormat(media::VideoPixelFormat inputFormat) {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(mEncoderState == EncoderState::UNINITIALIZED);
    ALOG_ASSERT(!mInputQueue->IsStreaming());
    ALOG_ASSERT(!mVisibleSize.IsEmpty());
    ALOG_ASSERT(!mInputFormatConverter);

    // First try to use the requested pixel format directly.
    ::base::Optional<struct v4l2_format> format;
    auto fourcc = media::Fourcc::FromVideoPixelFormat(inputFormat, false);
    if (fourcc) {
        format = mInputQueue->SetFormat(fourcc->ToV4L2PixFmt(), mVisibleSize, 0);
    }

    // If the device doesn't support the requested input format we'll try the device's preferred
    // input pixel formats and use a format convertor. We need to try all formats as some formats
    // might not be supported for the configured output format.
    if (!format) {
        std::vector<uint32_t> preferredFormats =
                mDevice->PreferredInputFormat(media::V4L2Device::Type::kEncoder);
        for (uint32_t i = 0; !format && i < preferredFormats.size(); ++i) {
            format = mInputQueue->SetFormat(preferredFormats[i], mVisibleSize, 0);
        }
    }

    if (!format) {
        ALOGE("Failed to set input format to %s",
              media::VideoPixelFormatToString(inputFormat).c_str());
        return false;
    }

    // Check whether the negotiated input format is valid. The coded size might be adjusted to match
    // encoder minimums, maximums and alignment requirements of the currently selected formats.
    auto layout = media::V4L2Device::V4L2FormatToVideoFrameLayout(*format);
    if (!layout) {
        ALOGE("Invalid input layout");
        return false;
    }

    mInputLayout = layout.value();
    if (!media::Rect(mInputLayout->coded_size()).Contains(media::Rect(mVisibleSize))) {
        ALOGE("Input size %s exceeds encoder capability, encoder can handle %s",
              mVisibleSize.ToString().c_str(), mInputLayout->coded_size().ToString().c_str());
        return false;
    }

    // Calculate the input coded size from the format.
    // TODO(dstaessens): How is this different from mInputLayout->coded_size()?
    mInputCodedSize = media::V4L2Device::AllocatedSizeFromV4L2Format(*format);

    // Add an input format convertor if the device doesn't support the requested input format.
    // Note: The amount of input buffers in the convertor should match the amount of buffers on the
    // device input queue, to simplify logic.
    // TODO(dstaessens): Currently an input format convertor is always required. Mapping an input
    // buffer always seems to fail unless we copy it into a new a buffer first. As a temporary
    // workaround the line below is commented, but this should be undone once the issue is fixed.
    //if (mInputLayout->format() != inputFormat) {
    ALOGV("Creating input format convertor (%s)",
          media::VideoPixelFormatToString(mInputLayout->format()).c_str());
    mInputFormatConverter =
            FormatConverter::Create(inputFormat, mVisibleSize, kInputBufferCount, mInputCodedSize);
    if (!mInputFormatConverter) {
        ALOGE("Failed to created input format convertor");
        return false;
    }
    //}

    // The coded input size might be different from the visible size due to alignment requirements,
    // So we need to specify the visible rectangle. Note that this rectangle might still be adjusted
    // due to hardware limitations.
    // TODO(dstaessens): Overwrite mVisibleSize with the adapted visible size here?
    media::Rect visibleRectangle(mVisibleSize.width(), mVisibleSize.height());

    struct v4l2_rect rect;
    rect.left = visibleRectangle.x();
    rect.top = visibleRectangle.y();
    rect.width = visibleRectangle.width();
    rect.height = visibleRectangle.height();

    // Try to adjust the visible rectangle using the VIDIOC_S_SELECTION command. If this is not
    // supported we'll try to use the VIDIOC_S_CROP command instead. The visible rectangle might be
    // adjusted to conform to hardware limitations (e.g. round to closest horizontal and vertical
    // offsets, width and height).
    struct v4l2_selection selection_arg;
    memset(&selection_arg, 0, sizeof(selection_arg));
    selection_arg.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    selection_arg.target = V4L2_SEL_TGT_CROP;
    selection_arg.r = rect;
    if (mDevice->Ioctl(VIDIOC_S_SELECTION, &selection_arg) == 0) {
        visibleRectangle = media::Rect(selection_arg.r.left, selection_arg.r.top,
                                       selection_arg.r.width, selection_arg.r.height);
    } else {
        struct v4l2_crop crop;
        memset(&crop, 0, sizeof(v4l2_crop));
        crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        crop.c = rect;
        if (mDevice->Ioctl(VIDIOC_S_CROP, &crop) != 0 ||
            mDevice->Ioctl(VIDIOC_G_CROP, &crop) != 0) {
            ALOGE("Failed to crop to specified visible rectangle");
            return false;
        }
        visibleRectangle = media::Rect(crop.c.left, crop.c.top, crop.c.width, crop.c.height);
    }

    ALOGV("Input format set to %s (size: %s, adjusted size: %dx%d, coded size: %s)",
          media::VideoPixelFormatToString(mInputLayout->format()).c_str(),
          mVisibleSize.ToString().c_str(), visibleRectangle.width(), visibleRectangle.height(),
          mInputCodedSize.ToString().c_str());

    mVisibleSize.SetSize(visibleRectangle.width(), visibleRectangle.height());
    return true;
}

bool V4L2EncodeComponent::configureOutputFormat(media::VideoCodecProfile outputProfile) {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(mEncoderState == EncoderState::UNINITIALIZED);
    ALOG_ASSERT(!mOutputQueue->IsStreaming());
    ALOG_ASSERT(!mVisibleSize.IsEmpty());

    auto format = mOutputQueue->SetFormat(
            media::V4L2Device::VideoCodecProfileToV4L2PixFmt(outputProfile, false), mVisibleSize,
            GetMaxOutputBufferSize(mVisibleSize));
    if (!format) {
        ALOGE("Failed to set output format to %s", media::GetProfileName(outputProfile).c_str());
        return false;
    }

    // The device might adjust the requested output buffer size to match hardware requirements.
    mOutputBufferSize = ::base::checked_cast<size_t>(format->fmt.pix_mp.plane_fmt[0].sizeimage);

    ALOGV("Output format set to %s (buffer size: %u)", media::GetProfileName(outputProfile).c_str(),
          mOutputBufferSize);
    return true;
}

bool V4L2EncodeComponent::configureDevice(media::VideoCodecProfile outputProfile,
                                          std::optional<const uint8_t> outputH264Level) {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());

    // Enable frame-level bitrate control. This is the only mandatory general control.
    if (!mDevice->SetExtCtrls(V4L2_CTRL_CLASS_MPEG,
                              {media::V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE, 1)})) {
        ALOGW("Failed enabling bitrate control");
        // TODO(b/161508368): V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE is currently not supported yet,
        // assume the operation was successful for now.
    }

    // Additional optional controls:
    // - Enable macroblock-level bitrate control.
    // - Set GOP length to 0 to disable periodic key frames.
    mDevice->SetExtCtrls(V4L2_CTRL_CLASS_MPEG,
                         {media::V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE, 1),
                          media::V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_GOP_SIZE, 0)});

    // All controls below are H.264-specific, so we can return here if the profile is not H.264.
    if (outputProfile < media::H264PROFILE_MIN || outputProfile > media::H264PROFILE_MAX) {
        return true;
    }

    // When encoding H.264 we want to prepend SPS and PPS to each IDR for resilience. Some
    // devices support this through the V4L2_CID_MPEG_VIDEO_H264_SPS_PPS_BEFORE_IDR control.
    // TODO(b/161495502): V4L2_CID_MPEG_VIDEO_H264_SPS_PPS_BEFORE_IDR is currently not supported
    // yet, just log a warning if the operation was unsuccessful for now.
    if (mDevice->IsCtrlExposed(V4L2_CID_MPEG_VIDEO_H264_SPS_PPS_BEFORE_IDR)) {
        if (!mDevice->SetExtCtrls(
                    V4L2_CTRL_CLASS_MPEG,
                    {media::V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_H264_SPS_PPS_BEFORE_IDR, 1)})) {
            ALOGE("Failed to configure device to prepend SPS and PPS to each IDR");
            return false;
        }
        ALOGV("Device supports prepending SPS and PPS to each IDR");
    } else {
        ALOGW("Device doesn't support prepending SPS and PPS to IDR");
    }

    std::vector<media::V4L2ExtCtrl> h264Ctrls;

    // No B-frames, for lowest decoding latency.
    h264Ctrls.emplace_back(V4L2_CID_MPEG_VIDEO_B_FRAMES, 0);
    // Quantization parameter maximum value (for variable bitrate control).
    h264Ctrls.emplace_back(V4L2_CID_MPEG_VIDEO_H264_MAX_QP, 51);

    // Set H.264 profile.
    int32_t profile = media::V4L2Device::VideoCodecProfileToV4L2H264Profile(outputProfile);
    if (profile < 0) {
        ALOGE("Trying to set invalid H.264 profile");
        return false;
    }
    h264Ctrls.emplace_back(V4L2_CID_MPEG_VIDEO_H264_PROFILE, profile);

    // Set H.264 output level. Use Level 4.0 as fallback default.
    // TODO(dstaessens): Investigate code added by hiroh@ recently to select level in Chrome VEA.
    uint8_t h264Level = outputH264Level.value_or(media::H264SPS::kLevelIDC4p0);
    h264Ctrls.emplace_back(V4L2_CID_MPEG_VIDEO_H264_LEVEL,
                           media::V4L2Device::H264LevelIdcToV4L2H264Level(h264Level));

    // Ask not to put SPS and PPS into separate bitstream buffers.
    h264Ctrls.emplace_back(V4L2_CID_MPEG_VIDEO_HEADER_MODE,
                           V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME);

    // Ignore return value as these controls are optional.
    mDevice->SetExtCtrls(V4L2_CTRL_CLASS_MPEG, std::move(h264Ctrls));

    return true;
}

bool V4L2EncodeComponent::updateEncodingParameters() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());

    // Query the interface for the encoding parameters requested by the codec 2.0 framework.
    C2StreamBitrateInfo::output bitrateInfo;
    C2StreamFrameRateInfo::output framerateInfo;
    c2_status_t status =
            mInterface->query({&bitrateInfo, &framerateInfo}, {}, C2_DONT_BLOCK, nullptr);
    if (status != C2_OK) {
        ALOGE("Failed to query interface for encoding parameters (error code: %d)", status);
        reportError(status);
        return false;
    }

    // Ask device to change bitrate if it's different from the currently configured bitrate.
    uint32_t bitrate = bitrateInfo.value;
    if (mBitrate != bitrate) {
        ALOG_ASSERT(bitrate > 0u);
        ALOGV("Setting bitrate to %u", bitrate);
        if (!mDevice->SetExtCtrls(V4L2_CTRL_CLASS_MPEG,
                                  {media::V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_BITRATE, bitrate)})) {
            // TODO(b/161495749): V4L2_CID_MPEG_VIDEO_BITRATE is currently not supported yet, assume
            // the operation was successful for now.
            ALOGW("Requesting bitrate change failed");
        }
        mBitrate = bitrate;
    }

    // Ask device to change framerate if it's different from the currently configured framerate.
    // TODO(dstaessens): Move IOCTL to device and use helper function.
    uint32_t framerate = static_cast<uint32_t>(std::round(framerateInfo.value));
    if (mFramerate != framerate) {
        ALOG_ASSERT(framerate > 0u);
        ALOGV("Setting framerate to %u", framerate);
        struct v4l2_streamparm parms;
        memset(&parms, 0, sizeof(v4l2_streamparm));
        parms.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        parms.parm.output.timeperframe.numerator = 1;
        parms.parm.output.timeperframe.denominator = framerate;
        if (mDevice->Ioctl(VIDIOC_S_PARM, &parms) != 0) {
            // TODO(b/161499573): VIDIOC_S_PARM is currently not supported yet, assume the operation
            // was successful for now.
            ALOGW("Requesting framerate change failed");
        }
        mFramerate = framerate;
    }

    // Check whether an explicit key frame was requested, if so reset the key frame counter to
    // immediately request a key frame.
    C2StreamRequestSyncFrameTuning::output requestKeyFrame;
    status = mInterface->query({&requestKeyFrame}, {}, C2_DONT_BLOCK, nullptr);
    if (status != C2_OK) {
        ALOGE("Failed to query interface for key frame request (error code: %d)", status);
        reportError(status);
        return false;
    }
    if (requestKeyFrame.value == C2_TRUE) {
        mKeyFrameCounter = 0;
        requestKeyFrame.value = C2_FALSE;
        std::vector<std::unique_ptr<C2SettingResult>> failures;
        status = mInterface->config({&requestKeyFrame}, C2_MAY_BLOCK, &failures);
        if (status != C2_OK) {
            ALOGE("Failed to reset key frame request on interface (error code: %d)", status);
            reportError(status);
            return false;
        }
    }

    // Request the next frame to be a key frame each time the counter reaches 0.
    if (mKeyFrameCounter == 0) {
        if (!mDevice->SetExtCtrls(V4L2_CTRL_CLASS_MPEG,
                                  {media::V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME)})) {
            // TODO(b/161498590): V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME is currently not supported
            // yet, assume the operation was successful for now.
            ALOGW("Failed requesting key frame");
        }
    }

    return true;
}

void V4L2EncodeComponent::scheduleNextEncodeTask() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(mEncoderState == EncoderState::ENCODING || mEncoderState == EncoderState::ERROR);

    // If we're in the error state we can immediately return.
    if (mEncoderState == EncoderState::ERROR) {
        return;
    }

    // Get the next work item. Currently only a single worklet per work item is supported. An input
    // buffer should always be supplied unless this is a drain or CSD request.
    ALOG_ASSERT(!mInputWorkQueue.empty());
    C2Work* work = mInputWorkQueue.front().get();
    ALOG_ASSERT(work->input.buffers.size() <= 1u && work->worklets.size() == 1u);

    // Set the default values for the output worklet.
    work->worklets.front()->output.flags = static_cast<C2FrameData::flags_t>(0);
    work->worklets.front()->output.buffers.clear();
    work->worklets.front()->output.ordinal = work->input.ordinal;

    uint64_t index = work->input.ordinal.frameIndex.peeku();
    int64_t timestamp = static_cast<int64_t>(work->input.ordinal.timestamp.peeku());
    bool endOfStream = work->input.flags & C2FrameData::FLAG_END_OF_STREAM;
    ALOGV("Scheduling next encode (index: %" PRIu64 ", timestamp: %" PRId64 ", EOS: %d)", index,
          timestamp, endOfStream);

    if (!work->input.buffers.empty()) {
        // Check if the device has free input buffers available. If not we'll switch to the
        // WAITING_FOR_INPUT_BUFFERS state, and resume encoding once we're notified buffers are
        // available in the onInputBufferDone() task. Note: The input buffers are not copied into
        // the device's input buffers, but rather a memory pointer is imported. We still have to
        // throttle the number of enqueues queued simultaneously on the device however.
        if (mInputQueue->FreeBuffersCount() == 0) {
            ALOGV("Waiting for device to return input buffers");
            setEncoderState(EncoderState::WAITING_FOR_INPUT_BUFFERS);
            return;
        }

        C2ConstGraphicBlock inputBlock =
                work->input.buffers.front()->data().graphicBlocks().front();

        // If encoding fails, we'll wait for an event (e.g. input buffers available) to start
        // encoding again.
        if (!encode(inputBlock, index, timestamp)) {
            return;
        }
    }

    // The codec 2.0 framework might queue an empty CSD request, but this is currently not
    // supported. We will return the CSD with the first encoded buffer work.
    // TODO(dstaessens): Avoid doing this, store CSD request work at start of output queue.
    if (work->input.buffers.empty() && !endOfStream) {
        ALOGV("Discarding empty CSD request");
        reportWork(std::move(mInputWorkQueue.front()));
    } else {
        mOutputWorkQueue.push_back(std::move(mInputWorkQueue.front()));
    }
    mInputWorkQueue.pop();

    // Drain the encoder if required.
    if (endOfStream) {
        drainTask(C2Component::DRAIN_COMPONENT_WITH_EOS);
    }

    if (mEncoderState == EncoderState::DRAINING) {
        return;
    } else if (mInputWorkQueue.empty()) {
        setEncoderState(EncoderState::WAITING_FOR_INPUT);
        return;
    }

    // Queue the next work item to be encoded.
    mEncoderTaskRunner->PostTask(
            FROM_HERE, ::base::BindOnce(&V4L2EncodeComponent::scheduleNextEncodeTask, mWeakThis));
}

bool V4L2EncodeComponent::encode(C2ConstGraphicBlock block, uint64_t index, int64_t timestamp) {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(mEncoderState == EncoderState::ENCODING);

    // Update dynamic encoding parameters (bitrate, framerate, key frame) if requested.
    if (!updateEncodingParameters()) return false;

    mKeyFrameCounter = (mKeyFrameCounter + 1) % mKeyFramePeriod;

    // If required convert the data to the V4L2 device's configured input pixel format. We
    // allocate the same amount of buffers on the device input queue and the format convertor,
    // so we should never run out of conversion buffers if there are free buffers in the input
    // queue.
    if (mInputFormatConverter) {
        if (!mInputFormatConverter->isReady()) {
            ALOGE("Input format convertor ran out of buffers");
            reportError(C2_CORRUPTED);
            return false;
        }

        ALOGV("Converting input block (index: %" PRIu64 ")", index);
        c2_status_t status = C2_CORRUPTED;
        block = mInputFormatConverter->convertBlock(index, block, &status);
        if (status != C2_OK) {
            ALOGE("Failed to convert input block (index: %" PRIu64 ")", index);
            reportError(status);
            return false;
        }
    }

    ALOGV("Encoding input block (index: %" PRIu64 ", timestamp: %" PRId64 ", size: %dx%d)", index,
          timestamp, block.width(), block.height());

    // Create a video frame from the graphic block.
    std::unique_ptr<InputFrame> frame = InputFrame::Create(block);
    if (!frame) {
        ALOGE("Failed to create video frame from input block (index: %" PRIu64
              ", timestamp: %" PRId64 ")",
              index, timestamp);
        reportError(C2_CORRUPTED);
        return false;
    }

    // Get the video frame layout and pixel format from the graphic block.
    // TODO(dstaessens) Integrate getVideoFrameLayout() into InputFrame::Create()
    media::VideoPixelFormat format;
    std::optional<std::vector<VideoFramePlane>> planes = getVideoFrameLayout(block, &format);
    if (!planes) {
        ALOGE("Failed to get input block's layout");
        reportError(C2_CORRUPTED);
        return false;
    }

    if (!enqueueInputBuffer(std::move(frame), format, *planes, index, timestamp)) {
        ALOGE("Failed to enqueue video frame (index: %" PRIu64 ", timestamp: %" PRId64 ")", index,
              timestamp);
        reportError(C2_CORRUPTED);
        return false;
    }

    // Start streaming on the input and output queue if required.
    if (!mInputQueue->IsStreaming()) {
        ALOG_ASSERT(!mOutputQueue->IsStreaming());
        if (!mOutputQueue->Streamon() || !mInputQueue->Streamon()) {
            ALOGE("Failed to start streaming on input and output queue");
            reportError(C2_CORRUPTED);
            return false;
        }
        // Start polling on the V4L2 device.
        startDevicePoll();
    }

    // Queue all buffers on the output queue. These buffers will be used to store the encoded
    // bitstreams.
    while (mOutputQueue->FreeBuffersCount() > 0) {
        if (!enqueueOutputBuffer()) return false;
    }

    return true;
}

void V4L2EncodeComponent::drain() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());

    if (mEncoderState == EncoderState::DRAINING || mEncoderState == EncoderState::ERROR) {
        return;
    }

    ALOG_ASSERT(mInputQueue->IsStreaming() && mOutputQueue->IsStreaming());
    ALOG_ASSERT(!mOutputWorkQueue.empty());

    // TODO(dstaessens): Move IOCTL to device class.
    struct v4l2_encoder_cmd cmd;
    memset(&cmd, 0, sizeof(v4l2_encoder_cmd));
    cmd.cmd = V4L2_ENC_CMD_STOP;
    if (mDevice->Ioctl(VIDIOC_ENCODER_CMD, &cmd) != 0) {
        ALOGE("Failed to stop encoder");
        onDrainDone(false);
        return;
    }
    ALOGV("%s(): Sent STOP command to encoder", __func__);

    setEncoderState(EncoderState::DRAINING);
}

void V4L2EncodeComponent::flush() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());

    // Stop the device poll thread.
    stopDevicePoll();

    // Stop streaming on the V4L2 device, which stops all currently queued work and releases all
    // buffers currently in use by the device.
    // TODO(b/160540027): Calling streamoff currently results in a bug.
    for (auto& queue : {mInputQueue, mOutputQueue}) {
        if (queue && queue->IsStreaming() && !queue->Streamoff()) {
            ALOGE("Failed to stop streaming on the device queue");
            reportError(C2_CORRUPTED);
        }
    }

    // Return all buffers to the input format convertor and clear all references to graphic blocks
    // in the input queue. We don't need to clear the output map as those buffers will still be
    // used.
    for (auto& it : mInputBuffersMap) {
        if (mInputFormatConverter && it.second) {
            mInputFormatConverter->returnBlock(it.first);
        }
        it.second = nullptr;
    }

    // Report all queued work items as aborted.
    std::list<std::unique_ptr<C2Work>> abortedWorkItems;
    while (!mInputWorkQueue.empty()) {
        std::unique_ptr<C2Work> work = std::move(mInputWorkQueue.front());
        work->result = C2_NOT_FOUND;
        work->input.buffers.clear();
        abortedWorkItems.push_back(std::move(work));
        mInputWorkQueue.pop();
    }
    while (!mOutputWorkQueue.empty()) {
        std::unique_ptr<C2Work> work = std::move(mOutputWorkQueue.front());
        work->result = C2_NOT_FOUND;
        work->input.buffers.clear();
        abortedWorkItems.push_back(std::move(work));
        mOutputWorkQueue.pop_front();
    }
    if (!abortedWorkItems.empty())
        mListener->onWorkDone_nb(shared_from_this(), std::move(abortedWorkItems));

    // Streaming and polling on the V4L2 device input and output queues will be resumed once new
    // encode work is queued.
}

std::shared_ptr<C2LinearBlock> V4L2EncodeComponent::fetchOutputBlock() {
    // TODO(dstaessens): fetchLinearBlock() might be blocking.
    ALOGV("Fetching linear block (size: %u)", mOutputBufferSize);
    std::shared_ptr<C2LinearBlock> outputBlock;
    c2_status_t status = mOutputBlockPool->fetchLinearBlock(
            mOutputBufferSize,
            C2MemoryUsage(C2MemoryUsage::CPU_READ |
                          static_cast<uint64_t>(BufferUsage::VIDEO_ENCODER)),
            &outputBlock);
    if (status != C2_OK) {
        ALOGE("Failed to fetch linear block (error: %d)", status);
        reportError(status);
        return nullptr;
    }

    return outputBlock;
}

void V4L2EncodeComponent::onInputBufferDone(uint64_t index) {
    ALOGV("%s(): Input buffer done (index: %" PRIu64 ")", __func__, index);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(mEncoderState != EncoderState::UNINITIALIZED);

    // There are no guarantees the input buffers are returned in order, so we need to find the work
    // item which this buffer belongs to.
    C2Work* work = getWorkByIndex(index);
    if (!work) {
        ALOGE("Failed to find work associated with input buffer %" PRIu64, index);
        reportError(C2_CORRUPTED);
        return;
    }

    // We're done using the input block, release reference to return the block to the client. If
    // using an input format convertor, we also need to return the block to the convertor.
    LOG_ASSERT(!work->input.buffers.empty());
    work->input.buffers.front().reset();
    if (mInputFormatConverter) {
        c2_status_t status = mInputFormatConverter->returnBlock(index);
        if (status != C2_OK) {
            reportError(status);
            return;
        }
    }

    // Return all completed work items. The work item might have been waiting for it's input buffer
    // to be returned, in which case we can report it as completed now. As input buffers are not
    // necessarily returned in order we might be able to return multiple ready work items now.
    while (!mOutputWorkQueue.empty() && isWorkDone(*mOutputWorkQueue.front())) {
        reportWork(std::move(mOutputWorkQueue.front()));
        mOutputWorkQueue.pop_front();
    }

    // We might have been waiting for input buffers to be returned after draining finished.
    if (mEncoderState == EncoderState::DRAINING && mOutputWorkQueue.empty()) {
        ALOGV("Draining done");
        mEncoderState = EncoderState::WAITING_FOR_INPUT_BUFFERS;
    }

    // If we previously used up all input queue buffers we can start encoding again now.
    if ((mEncoderState == EncoderState::WAITING_FOR_INPUT_BUFFERS) && !mInputWorkQueue.empty()) {
        setEncoderState(EncoderState::ENCODING);
        mEncoderTaskRunner->PostTask(
                FROM_HERE,
                ::base::BindOnce(&V4L2EncodeComponent::scheduleNextEncodeTask, mWeakThis));
    }
}

void V4L2EncodeComponent::onOutputBufferDone(uint32_t payloadSize, bool keyFrame, int64_t timestamp,
                                             std::shared_ptr<C2LinearBlock> outputBlock) {
    ALOGV("%s(): output buffer done (timestamp: %" PRId64 ", size: %u, key frame: %d)", __func__,
          timestamp, payloadSize, keyFrame);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());

    if (mEncoderState == EncoderState::ERROR) {
        return;
    }

    C2ConstLinearBlock constBlock =
            outputBlock->share(outputBlock->offset(), payloadSize, C2Fence());

    // If no CSD (content-specific-data, e.g. SPS for H.264) has been submitted yet, we expect this
    // output block to contain CSD. We only submit the CSD once, even if it's attached to each key
    // frame.
    if (!mCSDSubmitted) {
        ALOGV("No CSD submitted yet, extracting CSD");
        std::unique_ptr<C2StreamInitDataInfo::output> csd;
        C2ReadView view = constBlock.map().get();
        extractCSDInfo(&csd, view.data(), view.capacity());
        if (!csd) {
            ALOGE("Failed to extract CSD");
            reportError(C2_CORRUPTED);
            return;
        }

        // Attach the CSD to the first item in our output work queue.
        LOG_ASSERT(!mOutputWorkQueue.empty());
        C2Work* work = mOutputWorkQueue.front().get();
        work->worklets.front()->output.configUpdate.push_back(std::move(csd));
        mCSDSubmitted = true;
    }

    // Get the work item associated with the timestamp.
    C2Work* work = getWorkByTimestamp(timestamp);
    if (!work) {
        // It's possible we got an empty CSD request with timestamp 0, which we currently just
        // discard.
        // TODO(dstaessens): Investigate handling empty CSD requests.
        if (timestamp != 0) {
            reportError(C2_CORRUPTED);
        }
        return;
    }

    std::shared_ptr<C2Buffer> buffer = C2Buffer::CreateLinearBuffer(std::move(constBlock));
    if (keyFrame) {
        buffer->setInfo(
                std::make_shared<C2StreamPictureTypeMaskInfo::output>(0u, C2Config::SYNC_FRAME));
    }
    work->worklets.front()->output.buffers.emplace_back(buffer);

    // We can report the work item as completed if its associated input buffer has also been
    // released. As output buffers are not necessarily returned in order we might be able to return
    // multiple ready work items now.
    while (!mOutputWorkQueue.empty() && isWorkDone(*mOutputWorkQueue.front())) {
        reportWork(std::move(mOutputWorkQueue.front()));
        mOutputWorkQueue.pop_front();
    }
}

C2Work* V4L2EncodeComponent::getWorkByIndex(uint64_t index) {
    ALOGV("%s(): getting work item (index: %" PRIu64 ")", __func__, index);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());

    auto it = std::find_if(mOutputWorkQueue.begin(), mOutputWorkQueue.end(),
                           [index](const std::unique_ptr<C2Work>& w) {
                               return w->input.ordinal.frameIndex.peeku() == index;
                           });
    if (it == mOutputWorkQueue.end()) {
        ALOGE("Failed to find work (index: %" PRIu64 ")", index);
        return nullptr;
    }
    return it->get();
}

C2Work* V4L2EncodeComponent::getWorkByTimestamp(int64_t timestamp) {
    ALOGV("%s(): getting work item (timestamp: %" PRId64 ")", __func__, timestamp);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(timestamp >= 0);

    // Find the work with specified timestamp by looping over the output work queue. This should be
    // very fast as the output work queue will never be longer then a few items. Ignore empty work
    // items that are marked as EOS, as their timestamp might clash with other work items.
    auto it = std::find_if(mOutputWorkQueue.begin(), mOutputWorkQueue.end(),
                           [timestamp](const std::unique_ptr<C2Work>& w) {
                               return !(w->input.flags & C2FrameData::FLAG_END_OF_STREAM) &&
                                      w->input.ordinal.timestamp.peeku() ==
                                              static_cast<uint64_t>(timestamp);
                           });
    if (it == mOutputWorkQueue.end()) {
        ALOGE("Failed to find work (timestamp: %" PRIu64 ")", timestamp);
        return nullptr;
    }
    return it->get();
}

bool V4L2EncodeComponent::isWorkDone(const C2Work& work) const {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());

    if ((work.input.flags & C2FrameData::FLAG_END_OF_STREAM) &&
        !(work.worklets.front()->output.flags & C2FrameData::FLAG_END_OF_STREAM)) {
        ALOGV("Work item %" PRIu64 " is marked as EOS but draining has not finished yet",
              work.input.ordinal.frameIndex.peeku());
        return false;
    }

    if (!work.input.buffers.empty() && work.input.buffers.front()) {
        ALOGV("Input buffer associated with work item %" PRIu64 " not returned yet",
              work.input.ordinal.frameIndex.peeku());
        return false;
    }

    // If the work item had an input buffer to be encoded, it should have an output buffer set.
    if (!work.input.buffers.empty() && work.worklets.front()->output.buffers.empty()) {
        ALOGV("Output buffer associated with work item %" PRIu64 " not returned yet",
              work.input.ordinal.frameIndex.peeku());
        return false;
    }

    return true;
}

void V4L2EncodeComponent::reportWork(std::unique_ptr<C2Work> work) {
    ALOG_ASSERT(work);
    ALOGV("%s(): Reporting work item as finished (index: %llu, timestamp: %llu)", __func__,
          work->input.ordinal.frameIndex.peekull(), work->input.ordinal.timestamp.peekull());
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());

    work->result = C2_OK;
    work->workletsProcessed = static_cast<uint32_t>(work->worklets.size());

    std::list<std::unique_ptr<C2Work>> finishedWorkList;
    finishedWorkList.emplace_back(std::move(work));
    mListener->onWorkDone_nb(shared_from_this(), std::move(finishedWorkList));
}

bool V4L2EncodeComponent::startDevicePoll() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());

    if (!mDevice->StartPolling(
                ::base::BindRepeating(&V4L2EncodeComponent::serviceDeviceTask, mWeakThis),
                ::base::BindRepeating(&V4L2EncodeComponent::onPollError, mWeakThis))) {
        ALOGE("Device poll thread failed to start");
        reportError(C2_CORRUPTED);
        return false;
    }

    ALOGV("Device poll started");
    return true;
}

bool V4L2EncodeComponent::stopDevicePoll() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());

    if (!mDevice->StopPolling()) {
        ALOGE("Failed to stop polling on the device");
        reportError(C2_CORRUPTED);
        return false;
    }

    ALOGV("Device poll stopped");
    return true;
}

void V4L2EncodeComponent::onPollError() {
    ALOGV("%s()", __func__);
    reportError(C2_CORRUPTED);
}

void V4L2EncodeComponent::serviceDeviceTask(bool /*event*/) {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(mEncoderState != EncoderState::UNINITIALIZED);

    if (mEncoderState == EncoderState::ERROR) {
        return;
    }

    // Dequeue completed input (VIDEO_OUTPUT) buffers, and recycle to the free list.
    while (mInputQueue->QueuedBuffersCount() > 0) {
        if (!dequeueInputBuffer()) break;
    }

    // Dequeue completed output (VIDEO_CAPTURE) buffers, and recycle to the free list.
    while (mOutputQueue->QueuedBuffersCount() > 0) {
        if (!dequeueOutputBuffer()) break;
    }

    ALOGV("%s() - done", __func__);
}

bool V4L2EncodeComponent::enqueueInputBuffer(std::unique_ptr<InputFrame> frame,
                                             media::VideoPixelFormat format,
                                             const std::vector<VideoFramePlane>& planes,
                                             int64_t index, int64_t timestamp) {
    ALOGV("%s(): queuing input buffer (index: %" PRId64 ")", __func__, index);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(mInputQueue->FreeBuffersCount() > 0);
    ALOG_ASSERT(mEncoderState == EncoderState::ENCODING);
    ALOG_ASSERT(mInputLayout->format() == format);
    ALOG_ASSERT(mInputLayout->planes().size() == planes.size());

    auto buffer = mInputQueue->GetFreeBuffer();
    if (!buffer) {
        ALOGE("Failed to get free buffer from device input queue");
        return false;
    }

    // Mark the buffer with the frame's timestamp so we can identify the associated output buffers.
    buffer->SetTimeStamp(
            {.tv_sec = static_cast<time_t>(timestamp / ::base::Time::kMicrosecondsPerSecond),
             .tv_usec = static_cast<time_t>(timestamp % ::base::Time::kMicrosecondsPerSecond)});
    size_t bufferId = buffer->BufferId();

    for (size_t i = 0; i < planes.size(); ++i) {
        // Single-buffer input format may have multiple color planes, so bytesUsed of the single
        // buffer should be sum of each color planes' size.
        size_t bytesUsed = 0;
        if (planes.size() == 1) {
            bytesUsed = media::VideoFrame::AllocationSize(format, mInputLayout->coded_size());
        } else {
            bytesUsed = ::base::checked_cast<size_t>(
                    media::VideoFrame::PlaneSize(format, i, mInputLayout->coded_size()).GetArea());
        }

        // TODO(crbug.com/901264): The way to pass an offset within a DMA-buf is not defined
        // in V4L2 specification, so we abuse data_offset for now. Fix it when we have the
        // right interface, including any necessary validation and potential alignment.
        buffer->SetPlaneDataOffset(i, planes[i].mOffset);
        bytesUsed += planes[i].mOffset;
        // Workaround: filling length should not be needed. This is a bug of videobuf2 library.
        buffer->SetPlaneSize(i, mInputLayout->planes()[i].size + planes[i].mOffset);
        buffer->SetPlaneBytesUsed(i, bytesUsed);
    }

    std::move(*buffer).QueueDMABuf(frame->getFDs());

    ALOGV("Queued buffer in input queue (index: %" PRId64 ", timestamp: %" PRId64
          ", bufferId: %zu)",
          index, timestamp, bufferId);

    mInputBuffersMap[bufferId] = {index, std::move(frame)};

    return true;
}

bool V4L2EncodeComponent::enqueueOutputBuffer() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(mOutputQueue->FreeBuffersCount() > 0);

    auto buffer = mOutputQueue->GetFreeBuffer();
    if (!buffer) {
        ALOGE("Failed to get free buffer from device output queue");
        reportError(C2_CORRUPTED);
        return false;
    }

    std::shared_ptr<C2LinearBlock> outputBlock = fetchOutputBlock();
    if (!outputBlock) {
        ALOGE("Failed to fetch output block");
        reportError(C2_CORRUPTED);
        return false;
    }

    size_t bufferId = buffer->BufferId();

    std::vector<int> fds;
    fds.push_back(outputBlock->handle()->data[0]);
    if (!std::move(*buffer).QueueDMABuf(fds)) {
        ALOGE("Failed to queue output buffer using QueueDMABuf");
        reportError(C2_CORRUPTED);
        return false;
    }

    ALOG_ASSERT(!mOutputBuffersMap[bufferId]);
    mOutputBuffersMap[bufferId] = std::move(outputBlock);
    ALOGV("%s(): Queued buffer in output queue (bufferId: %zu)", __func__, bufferId);
    return true;
}

bool V4L2EncodeComponent::dequeueInputBuffer() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(mEncoderState != EncoderState::UNINITIALIZED);
    ALOG_ASSERT(mInputQueue->QueuedBuffersCount() > 0);

    std::pair<bool, media::V4L2ReadableBufferRef> result = mInputQueue->DequeueBuffer();
    if (!result.first) {
        ALOGE("Failed to dequeue buffer from input queue");
        reportError(C2_CORRUPTED);
        return false;
    }
    if (!result.second) {
        // No more buffers ready to be dequeued in input queue.
        return false;
    }

    const media::V4L2ReadableBufferRef buffer = std::move(result.second);
    uint64_t index = mInputBuffersMap[buffer->BufferId()].first;
    int64_t timestamp = buffer->GetTimeStamp().tv_usec +
                        buffer->GetTimeStamp().tv_sec * ::base::Time::kMicrosecondsPerSecond;
    ALOGV("Dequeued buffer from input queue (index: %" PRId64 ", timestamp: %" PRId64
          ", bufferId: %zu)",
          index, timestamp, buffer->BufferId());

    mInputBuffersMap[buffer->BufferId()].second = nullptr;
    onInputBufferDone(index);

    return true;
}

bool V4L2EncodeComponent::dequeueOutputBuffer() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(mEncoderState != EncoderState::UNINITIALIZED);
    ALOG_ASSERT(mOutputQueue->QueuedBuffersCount() > 0);

    std::pair<bool, media::V4L2ReadableBufferRef> result = mOutputQueue->DequeueBuffer();
    if (!result.first) {
        ALOGE("Failed to dequeue buffer from output queue");
        reportError(C2_CORRUPTED);
        return false;
    }
    if (!result.second) {
        // No more buffers ready to be dequeued in output queue.
        return false;
    }

    media::V4L2ReadableBufferRef buffer = std::move(result.second);
    size_t encodedDataSize = buffer->GetPlaneBytesUsed(0) - buffer->GetPlaneDataOffset(0);
    ::base::TimeDelta timestamp = ::base::TimeDelta::FromMicroseconds(
            buffer->GetTimeStamp().tv_usec +
            buffer->GetTimeStamp().tv_sec * ::base::Time::kMicrosecondsPerSecond);

    ALOGV("Dequeued buffer from output queue (timestamp: %" PRId64
          ", bufferId: %zu, data size: %zu, EOS: %d)",
          timestamp.InMicroseconds(), buffer->BufferId(), encodedDataSize, buffer->IsLast());

    if (!mOutputBuffersMap[buffer->BufferId()]) {
        ALOGE("Failed to find output block associated with output buffer");
        reportError(C2_CORRUPTED);
        return false;
    }

    std::shared_ptr<C2LinearBlock> block = std::move(mOutputBuffersMap[buffer->BufferId()]);
    if (encodedDataSize > 0) {
        onOutputBufferDone(encodedDataSize, buffer->IsKeyframe(), timestamp.InMicroseconds(),
                           std::move(block));
    }

    // If the buffer is marked as last and we were flushing the encoder, flushing is now done.
    if ((mEncoderState == EncoderState::DRAINING) && buffer->IsLast()) {
        onDrainDone(true);

        // Start the encoder again.
        struct v4l2_encoder_cmd cmd;
        memset(&cmd, 0, sizeof(v4l2_encoder_cmd));
        cmd.cmd = V4L2_ENC_CMD_START;
        if (mDevice->Ioctl(VIDIOC_ENCODER_CMD, &cmd) != 0) {
            ALOGE("Failed to restart encoder after flushing (V4L2_ENC_CMD_START)");
            reportError(C2_CORRUPTED);
            return false;
        }
    }

    // Queue a new output buffer to replace the one we dequeued.
    buffer = nullptr;
    enqueueOutputBuffer();

    return true;
}

bool V4L2EncodeComponent::createInputBuffers() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(!mInputQueue->IsStreaming());
    ALOG_ASSERT(mInputBuffersMap.empty());

    // No memory is allocated here, we just generate a list of buffers on the input queue, which
    // will hold memory handles to the real buffers.
    if (mInputQueue->AllocateBuffers(kInputBufferCount, V4L2_MEMORY_DMABUF) < kInputBufferCount) {
        ALOGE("Failed to create V4L2 input buffers.");
        return false;
    }

    mInputBuffersMap.resize(mInputQueue->AllocatedBuffersCount());
    return true;
}

bool V4L2EncodeComponent::createOutputBuffers() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(!mOutputQueue->IsStreaming());
    ALOG_ASSERT(mOutputBuffersMap.empty());

    // Fetch the output block pool.
    C2BlockPool::local_id_t poolId = mInterface->getBlockPoolId();
    c2_status_t status = GetCodec2BlockPool(poolId, shared_from_this(), &mOutputBlockPool);
    if (status != C2_OK || !mOutputBlockPool) {
        ALOGE("Failed to get output block pool, error: %d", status);
        return false;
    }

    // No memory is allocated here, we just generate a list of buffers on the output queue, which
    // will hold memory handles to the real buffers.
    if (mOutputQueue->AllocateBuffers(kOutputBufferCount, V4L2_MEMORY_DMABUF) <
        kOutputBufferCount) {
        ALOGE("Failed to create V4L2 output buffers.");
        return false;
    }

    mOutputBuffersMap.resize(mOutputQueue->AllocatedBuffersCount());
    return true;
}

void V4L2EncodeComponent::destroyInputBuffers() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(!mInputQueue->IsStreaming());

    if (!mInputQueue || mInputQueue->AllocatedBuffersCount() == 0) return;
    mInputQueue->DeallocateBuffers();
    mInputBuffersMap.clear();
}

void V4L2EncodeComponent::destroyOutputBuffers() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());
    ALOG_ASSERT(!mOutputQueue->IsStreaming());

    if (!mOutputQueue || mOutputQueue->AllocatedBuffersCount() == 0) return;
    mOutputQueue->DeallocateBuffers();
    mOutputBuffersMap.clear();
    mOutputBlockPool.reset();
}

void V4L2EncodeComponent::reportError(c2_status_t error) {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());

    {
        std::lock_guard<std::mutex> lock(mComponentLock);
        setComponentState(ComponentState::ERROR);
    }

    // TODO(dstaessens): Report all pending work items as finished upon failure.
    if (mEncoderState != EncoderState::ERROR) {
        setEncoderState(EncoderState::ERROR);
        mListener->onError_nb(shared_from_this(), static_cast<uint32_t>(error));
    }
}

void V4L2EncodeComponent::setComponentState(ComponentState state) {
    // Check whether the state change is valid.
    switch (state) {
    case ComponentState::UNLOADED:
        ALOG_ASSERT(mComponentState == ComponentState::LOADED);
        break;
    case ComponentState::LOADED:
        ALOG_ASSERT(mComponentState == ComponentState::UNLOADED ||
                    mComponentState == ComponentState::RUNNING ||
                    mComponentState == ComponentState::ERROR);
        break;
    case ComponentState::RUNNING:
        ALOG_ASSERT(mComponentState == ComponentState::LOADED);
        break;
    case ComponentState::ERROR:
        break;
    }

    ALOGV("Changed component state from %s to %s", componentStateToString(mComponentState),
          componentStateToString(state));
    mComponentState = state;
}

void V4L2EncodeComponent::setEncoderState(EncoderState state) {
    ALOG_ASSERT(mEncoderTaskRunner->RunsTasksInCurrentSequence());

    // Check whether the state change is valid.
    switch (state) {
    case EncoderState::UNINITIALIZED:
        // TODO(dstaessens): Check all valid state changes.
        break;
    case EncoderState::WAITING_FOR_INPUT:
        ALOG_ASSERT(mEncoderState == EncoderState::UNINITIALIZED ||
                    mEncoderState == EncoderState::ENCODING ||
                    mEncoderState == EncoderState::DRAINING);
        break;
    case EncoderState::WAITING_FOR_INPUT_BUFFERS:
        ALOG_ASSERT(mEncoderState == EncoderState::ENCODING);
        break;
    case EncoderState::ENCODING:
        ALOG_ASSERT(mEncoderState == EncoderState::WAITING_FOR_INPUT ||
                    mEncoderState == EncoderState::WAITING_FOR_INPUT_BUFFERS ||
                    mEncoderState == EncoderState::DRAINING);
        break;
    case EncoderState::DRAINING:
        ALOG_ASSERT(mEncoderState == EncoderState::ENCODING);
        break;
    case EncoderState::ERROR:
        break;
    }

    ALOGV("Changed encoder state from %s to %s", encoderStateToString(mEncoderState),
          encoderStateToString(state));
    mEncoderState = state;
}

const char* V4L2EncodeComponent::componentStateToString(V4L2EncodeComponent::ComponentState state) {
    switch (state) {
    case ComponentState::UNLOADED:
        return "UNLOADED";
    case ComponentState::LOADED:
        return "LOADED";
    case ComponentState::RUNNING:
        return "RUNNING";
    case ComponentState::ERROR:
        return "ERROR";
    }
}

const char* V4L2EncodeComponent::encoderStateToString(V4L2EncodeComponent::EncoderState state) {
    switch (state) {
    case EncoderState::UNINITIALIZED:
        return "UNINITIALIZED";
    case EncoderState::WAITING_FOR_INPUT:
        return "WAITING_FOR_INPUT";
    case EncoderState::WAITING_FOR_INPUT_BUFFERS:
        return "WAITING_FOR_INPUT_BUFFERS";
    case EncoderState::ENCODING:
        return "ENCODING";
    case EncoderState::DRAINING:
        return "Draining";
    case EncoderState::ERROR:
        return "ERROR";
    }
}

}  // namespace android
