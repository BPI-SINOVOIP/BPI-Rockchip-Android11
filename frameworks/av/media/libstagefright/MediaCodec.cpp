/*
 * Copyright 2012, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#include "hidl/HidlSupport.h"
#define LOG_TAG "MediaCodec"
#include <utils/Log.h>

#include <set>

#include <inttypes.h>
#include <stdlib.h>

#include <C2Buffer.h>

#include "include/SoftwareRenderer.h"

#include <android/hardware/cas/native/1.0/IDescrambler.h>
#include <android/hardware/media/omx/1.0/IGraphicBufferSource.h>

#include <aidl/android/media/BnResourceManagerClient.h>
#include <aidl/android/media/IResourceManagerService.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <binder/IMemory.h>
#include <binder/MemoryDealer.h>
#include <cutils/properties.h>
#include <gui/BufferQueue.h>
#include <gui/Surface.h>
#include <hidlmemory/FrameworkUtils.h>
#include <mediadrm/ICrypto.h>
#include <media/IOMX.h>
#include <media/MediaCodecBuffer.h>
#include <media/MediaCodecInfo.h>
#include <media/MediaMetricsItem.h>
#include <media/MediaResource.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/foundation/AUtils.h>
#include <media/stagefright/foundation/avc_utils.h>
#include <media/stagefright/foundation/hexdump.h>
#include <media/stagefright/ACodec.h>
#include <media/stagefright/BatteryChecker.h>
#include <media/stagefright/BufferProducerWrapper.h>
#include <media/stagefright/CCodec.h>
#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/MediaCodecConstants.h>
#include <media/stagefright/MediaCodecList.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaFilter.h>
#include <media/stagefright/OMXClient.h>
#include <media/stagefright/PersistentSurface.h>
#include <media/stagefright/SurfaceUtils.h>
#include <private/android_filesystem_config.h>
#include <utils/Singleton.h>

namespace android {

using Status = ::ndk::ScopedAStatus;
using aidl::android::media::BnResourceManagerClient;
using aidl::android::media::IResourceManagerClient;
using aidl::android::media::IResourceManagerService;

// key for media statistics
static const char *kCodecKeyName = "codec";
// attrs for media statistics
// NB: these are matched with public Java API constants defined
// in frameworks/base/media/java/android/media/MediaCodec.java
// These must be kept synchronized with the constants there.
static const char *kCodecCodec = "android.media.mediacodec.codec";  /* e.g. OMX.google.aac.decoder */
static const char *kCodecMime = "android.media.mediacodec.mime";    /* e.g. audio/mime */
static const char *kCodecMode = "android.media.mediacodec.mode";    /* audio, video */
static const char *kCodecModeVideo = "video";            /* values returned for kCodecMode */
static const char *kCodecModeAudio = "audio";
static const char *kCodecEncoder = "android.media.mediacodec.encoder"; /* 0,1 */
static const char *kCodecSecure = "android.media.mediacodec.secure";   /* 0, 1 */
static const char *kCodecWidth = "android.media.mediacodec.width";     /* 0..n */
static const char *kCodecHeight = "android.media.mediacodec.height";   /* 0..n */
static const char *kCodecRotation = "android.media.mediacodec.rotation-degrees";  /* 0/90/180/270 */

// NB: These are not yet exposed as public Java API constants.
static const char *kCodecCrypto = "android.media.mediacodec.crypto";   /* 0,1 */
static const char *kCodecProfile = "android.media.mediacodec.profile";  /* 0..n */
static const char *kCodecLevel = "android.media.mediacodec.level";  /* 0..n */
static const char *kCodecBitrateMode = "android.media.mediacodec.bitrate_mode";  /* CQ/VBR/CBR */
static const char *kCodecBitrate = "android.media.mediacodec.bitrate";  /* 0..n */
static const char *kCodecMaxWidth = "android.media.mediacodec.maxwidth";  /* 0..n */
static const char *kCodecMaxHeight = "android.media.mediacodec.maxheight";  /* 0..n */
static const char *kCodecError = "android.media.mediacodec.errcode";
static const char *kCodecLifetimeMs = "android.media.mediacodec.lifetimeMs";   /* 0..n ms*/
static const char *kCodecErrorState = "android.media.mediacodec.errstate";
static const char *kCodecLatencyMax = "android.media.mediacodec.latency.max";   /* in us */
static const char *kCodecLatencyMin = "android.media.mediacodec.latency.min";   /* in us */
static const char *kCodecLatencyAvg = "android.media.mediacodec.latency.avg";   /* in us */
static const char *kCodecLatencyCount = "android.media.mediacodec.latency.n";
static const char *kCodecLatencyHist = "android.media.mediacodec.latency.hist"; /* in us */
static const char *kCodecLatencyUnknown = "android.media.mediacodec.latency.unknown";
static const char *kCodecQueueSecureInputBufferError = "android.media.mediacodec.queueSecureInputBufferError";
static const char *kCodecQueueInputBufferError = "android.media.mediacodec.queueInputBufferError";

static const char *kCodecNumLowLatencyModeOn = "android.media.mediacodec.low-latency.on";  /* 0..n */
static const char *kCodecNumLowLatencyModeOff = "android.media.mediacodec.low-latency.off";  /* 0..n */
static const char *kCodecFirstFrameIndexLowLatencyModeOn = "android.media.mediacodec.low-latency.first-frame";  /* 0..n */

// the kCodecRecent* fields appear only in getMetrics() results
static const char *kCodecRecentLatencyMax = "android.media.mediacodec.recent.max";      /* in us */
static const char *kCodecRecentLatencyMin = "android.media.mediacodec.recent.min";      /* in us */
static const char *kCodecRecentLatencyAvg = "android.media.mediacodec.recent.avg";      /* in us */
static const char *kCodecRecentLatencyCount = "android.media.mediacodec.recent.n";
static const char *kCodecRecentLatencyHist = "android.media.mediacodec.recent.hist";    /* in us */

// XXX suppress until we get our representation right
static bool kEmitHistogram = false;


static int64_t getId(const std::shared_ptr<IResourceManagerClient> &client) {
    return (int64_t) client.get();
}

static bool isResourceError(status_t err) {
    return (err == NO_MEMORY);
}

static const int kMaxRetry = 2;
static const int kMaxReclaimWaitTimeInUs = 500000;  // 0.5s
static const int kNumBuffersAlign = 16;

static const C2MemoryUsage kDefaultReadWriteUsage{
    C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE};

////////////////////////////////////////////////////////////////////////////////

struct ResourceManagerClient : public BnResourceManagerClient {
    explicit ResourceManagerClient(MediaCodec* codec) : mMediaCodec(codec) {}

    Status reclaimResource(bool* _aidl_return) override {
        sp<MediaCodec> codec = mMediaCodec.promote();
        if (codec == NULL) {
            // codec is already gone.
            *_aidl_return = true;
            return Status::ok();
        }
        status_t err = codec->reclaim();
        if (err == WOULD_BLOCK) {
            ALOGD("Wait for the client to release codec.");
            usleep(kMaxReclaimWaitTimeInUs);
            ALOGD("Try to reclaim again.");
            err = codec->reclaim(true /* force */);
        }
        if (err != OK) {
            ALOGW("ResourceManagerClient failed to release codec with err %d", err);
        }
        *_aidl_return = (err == OK);
        return Status::ok();
    }

    Status getName(::std::string* _aidl_return) override {
        _aidl_return->clear();
        sp<MediaCodec> codec = mMediaCodec.promote();
        if (codec == NULL) {
            // codec is already gone.
            return Status::ok();
        }

        AString name;
        if (codec->getName(&name) == OK) {
            *_aidl_return = name.c_str();
        }
        return Status::ok();
    }

    virtual ~ResourceManagerClient() {}

private:
    wp<MediaCodec> mMediaCodec;

    DISALLOW_EVIL_CONSTRUCTORS(ResourceManagerClient);
};

struct MediaCodec::ResourceManagerServiceProxy : public RefBase {
    ResourceManagerServiceProxy(pid_t pid, uid_t uid,
            const std::shared_ptr<IResourceManagerClient> &client);
    virtual ~ResourceManagerServiceProxy();

    void init();

    // implements DeathRecipient
    static void BinderDiedCallback(void* cookie);
    void binderDied();
    static Mutex sLockCookies;
    static std::set<void*> sCookies;
    static void addCookie(void* cookie);
    static void removeCookie(void* cookie);

    void addResource(const MediaResourceParcel &resource);
    void removeResource(const MediaResourceParcel &resource);
    void removeClient();
    void markClientForPendingRemoval();
    bool reclaimResource(const std::vector<MediaResourceParcel> &resources);

private:
    Mutex mLock;
    pid_t mPid;
    uid_t mUid;
    std::shared_ptr<IResourceManagerService> mService;
    std::shared_ptr<IResourceManagerClient> mClient;
    ::ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;
};

MediaCodec::ResourceManagerServiceProxy::ResourceManagerServiceProxy(
        pid_t pid, uid_t uid, const std::shared_ptr<IResourceManagerClient> &client)
        : mPid(pid), mUid(uid), mClient(client),
          mDeathRecipient(AIBinder_DeathRecipient_new(BinderDiedCallback)) {
    if (mPid == MediaCodec::kNoPid) {
        mPid = AIBinder_getCallingPid();
    }
}

MediaCodec::ResourceManagerServiceProxy::~ResourceManagerServiceProxy() {

    // remove the cookie, so any in-flight death notification will get dropped
    // by our handler.
    removeCookie(this);

    Mutex::Autolock _l(mLock);
    if (mService != nullptr) {
        AIBinder_unlinkToDeath(mService->asBinder().get(), mDeathRecipient.get(), this);
        mService = nullptr;
    }
}

void MediaCodec::ResourceManagerServiceProxy::init() {
    ::ndk::SpAIBinder binder(AServiceManager_getService("media.resource_manager"));
    mService = IResourceManagerService::fromBinder(binder);
    if (mService == nullptr) {
        ALOGE("Failed to get ResourceManagerService");
        return;
    }

    // so our handler will process the death notifications
    addCookie(this);

    // after this, require mLock whenever using mService
    AIBinder_linkToDeath(mService->asBinder().get(), mDeathRecipient.get(), this);

    // Kill clients pending removal.
    mService->reclaimResourcesFromClientsPendingRemoval(mPid);
}

//static
Mutex MediaCodec::ResourceManagerServiceProxy::sLockCookies;
std::set<void*> MediaCodec::ResourceManagerServiceProxy::sCookies;

//static
void MediaCodec::ResourceManagerServiceProxy::addCookie(void* cookie) {
    Mutex::Autolock _l(sLockCookies);
    sCookies.insert(cookie);
}

//static
void MediaCodec::ResourceManagerServiceProxy::removeCookie(void* cookie) {
    Mutex::Autolock _l(sLockCookies);
    sCookies.erase(cookie);
}

//static
void MediaCodec::ResourceManagerServiceProxy::BinderDiedCallback(void* cookie) {
    Mutex::Autolock _l(sLockCookies);
    if (sCookies.find(cookie) != sCookies.end()) {
        auto thiz = static_cast<ResourceManagerServiceProxy*>(cookie);
        thiz->binderDied();
    }
}

void MediaCodec::ResourceManagerServiceProxy::binderDied() {
    ALOGW("ResourceManagerService died.");
    Mutex::Autolock _l(mLock);
    mService = nullptr;
}

void MediaCodec::ResourceManagerServiceProxy::addResource(
        const MediaResourceParcel &resource) {
    std::vector<MediaResourceParcel> resources;
    resources.push_back(resource);

    Mutex::Autolock _l(mLock);
    if (mService == nullptr) {
        return;
    }
    mService->addResource(mPid, mUid, getId(mClient), mClient, resources);
}

void MediaCodec::ResourceManagerServiceProxy::removeResource(
        const MediaResourceParcel &resource) {
    std::vector<MediaResourceParcel> resources;
    resources.push_back(resource);

    Mutex::Autolock _l(mLock);
    if (mService == nullptr) {
        return;
    }
    mService->removeResource(mPid, getId(mClient), resources);
}

void MediaCodec::ResourceManagerServiceProxy::removeClient() {
    Mutex::Autolock _l(mLock);
    if (mService == nullptr) {
        return;
    }
    mService->removeClient(mPid, getId(mClient));
}

void MediaCodec::ResourceManagerServiceProxy::markClientForPendingRemoval() {
    Mutex::Autolock _l(mLock);
    if (mService == nullptr) {
        return;
    }
    mService->markClientForPendingRemoval(mPid, getId(mClient));
}

bool MediaCodec::ResourceManagerServiceProxy::reclaimResource(
        const std::vector<MediaResourceParcel> &resources) {
    Mutex::Autolock _l(mLock);
    if (mService == NULL) {
        return false;
    }
    bool success;
    Status status = mService->reclaimResource(mPid, resources, &success);
    return status.isOk() && success;
}

////////////////////////////////////////////////////////////////////////////////

MediaCodec::BufferInfo::BufferInfo() : mOwnedByClient(false) {}

////////////////////////////////////////////////////////////////////////////////

class MediaCodec::ReleaseSurface {
public:
    explicit ReleaseSurface(uint64_t usage) {
        BufferQueue::createBufferQueue(&mProducer, &mConsumer);
        mSurface = new Surface(mProducer, false /* controlledByApp */);
        struct ConsumerListener : public BnConsumerListener {
            void onFrameAvailable(const BufferItem&) override {}
            void onBuffersReleased() override {}
            void onSidebandStreamChanged() override {}
        };
        sp<ConsumerListener> listener{new ConsumerListener};
        mConsumer->consumerConnect(listener, false);
        mConsumer->setConsumerName(String8{"MediaCodec.release"});
        mConsumer->setConsumerUsageBits(usage);
    }

    const sp<Surface> &getSurface() {
        return mSurface;
    }

private:
    sp<IGraphicBufferProducer> mProducer;
    sp<IGraphicBufferConsumer> mConsumer;
    sp<Surface> mSurface;
};

////////////////////////////////////////////////////////////////////////////////

namespace {

enum {
    kWhatFillThisBuffer      = 'fill',
    kWhatDrainThisBuffer     = 'drai',
    kWhatEOS                 = 'eos ',
    kWhatStartCompleted      = 'Scom',
    kWhatStopCompleted       = 'scom',
    kWhatReleaseCompleted    = 'rcom',
    kWhatFlushCompleted      = 'fcom',
    kWhatError               = 'erro',
    kWhatComponentAllocated  = 'cAll',
    kWhatComponentConfigured = 'cCon',
    kWhatInputSurfaceCreated = 'isfc',
    kWhatInputSurfaceAccepted = 'isfa',
    kWhatSignaledInputEOS    = 'seos',
    kWhatOutputFramesRendered = 'outR',
    kWhatOutputBuffersChanged = 'outC',
};

class BufferCallback : public CodecBase::BufferCallback {
public:
    explicit BufferCallback(const sp<AMessage> &notify);
    virtual ~BufferCallback() = default;

    virtual void onInputBufferAvailable(
            size_t index, const sp<MediaCodecBuffer> &buffer) override;
    virtual void onOutputBufferAvailable(
            size_t index, const sp<MediaCodecBuffer> &buffer) override;
private:
    const sp<AMessage> mNotify;
};

BufferCallback::BufferCallback(const sp<AMessage> &notify)
    : mNotify(notify) {}

void BufferCallback::onInputBufferAvailable(
        size_t index, const sp<MediaCodecBuffer> &buffer) {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatFillThisBuffer);
    notify->setSize("index", index);
    notify->setObject("buffer", buffer);
    notify->post();
}

void BufferCallback::onOutputBufferAvailable(
        size_t index, const sp<MediaCodecBuffer> &buffer) {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatDrainThisBuffer);
    notify->setSize("index", index);
    notify->setObject("buffer", buffer);
    notify->post();
}

class CodecCallback : public CodecBase::CodecCallback {
public:
    explicit CodecCallback(const sp<AMessage> &notify);
    virtual ~CodecCallback() = default;

    virtual void onEos(status_t err) override;
    virtual void onStartCompleted() override;
    virtual void onStopCompleted() override;
    virtual void onReleaseCompleted() override;
    virtual void onFlushCompleted() override;
    virtual void onError(status_t err, enum ActionCode actionCode) override;
    virtual void onComponentAllocated(const char *componentName) override;
    virtual void onComponentConfigured(
            const sp<AMessage> &inputFormat, const sp<AMessage> &outputFormat) override;
    virtual void onInputSurfaceCreated(
            const sp<AMessage> &inputFormat,
            const sp<AMessage> &outputFormat,
            const sp<BufferProducerWrapper> &inputSurface) override;
    virtual void onInputSurfaceCreationFailed(status_t err) override;
    virtual void onInputSurfaceAccepted(
            const sp<AMessage> &inputFormat,
            const sp<AMessage> &outputFormat) override;
    virtual void onInputSurfaceDeclined(status_t err) override;
    virtual void onSignaledInputEOS(status_t err) override;
    virtual void onOutputFramesRendered(const std::list<FrameRenderTracker::Info> &done) override;
    virtual void onOutputBuffersChanged() override;
private:
    const sp<AMessage> mNotify;
};

CodecCallback::CodecCallback(const sp<AMessage> &notify) : mNotify(notify) {}

void CodecCallback::onEos(status_t err) {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatEOS);
    notify->setInt32("err", err);
    notify->post();
}

void CodecCallback::onStartCompleted() {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatStartCompleted);
    notify->post();
}

void CodecCallback::onStopCompleted() {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatStopCompleted);
    notify->post();
}

void CodecCallback::onReleaseCompleted() {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatReleaseCompleted);
    notify->post();
}

void CodecCallback::onFlushCompleted() {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatFlushCompleted);
    notify->post();
}

void CodecCallback::onError(status_t err, enum ActionCode actionCode) {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatError);
    notify->setInt32("err", err);
    notify->setInt32("actionCode", actionCode);
    notify->post();
}

void CodecCallback::onComponentAllocated(const char *componentName) {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatComponentAllocated);
    notify->setString("componentName", componentName);
    notify->post();
}

void CodecCallback::onComponentConfigured(
        const sp<AMessage> &inputFormat, const sp<AMessage> &outputFormat) {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatComponentConfigured);
    notify->setMessage("input-format", inputFormat);
    notify->setMessage("output-format", outputFormat);
    notify->post();
}

void CodecCallback::onInputSurfaceCreated(
        const sp<AMessage> &inputFormat,
        const sp<AMessage> &outputFormat,
        const sp<BufferProducerWrapper> &inputSurface) {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatInputSurfaceCreated);
    notify->setMessage("input-format", inputFormat);
    notify->setMessage("output-format", outputFormat);
    notify->setObject("input-surface", inputSurface);
    notify->post();
}

void CodecCallback::onInputSurfaceCreationFailed(status_t err) {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatInputSurfaceCreated);
    notify->setInt32("err", err);
    notify->post();
}

void CodecCallback::onInputSurfaceAccepted(
        const sp<AMessage> &inputFormat,
        const sp<AMessage> &outputFormat) {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatInputSurfaceAccepted);
    notify->setMessage("input-format", inputFormat);
    notify->setMessage("output-format", outputFormat);
    notify->post();
}

void CodecCallback::onInputSurfaceDeclined(status_t err) {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatInputSurfaceAccepted);
    notify->setInt32("err", err);
    notify->post();
}

void CodecCallback::onSignaledInputEOS(status_t err) {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatSignaledInputEOS);
    if (err != OK) {
        notify->setInt32("err", err);
    }
    notify->post();
}

void CodecCallback::onOutputFramesRendered(const std::list<FrameRenderTracker::Info> &done) {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatOutputFramesRendered);
    if (MediaCodec::CreateFramesRenderedMessage(done, notify)) {
        notify->post();
    }
}

void CodecCallback::onOutputBuffersChanged() {
    sp<AMessage> notify(mNotify->dup());
    notify->setInt32("what", kWhatOutputBuffersChanged);
    notify->post();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

// static
sp<MediaCodec> MediaCodec::CreateByType(
        const sp<ALooper> &looper, const AString &mime, bool encoder, status_t *err, pid_t pid,
        uid_t uid) {
    Vector<AString> matchingCodecs;

    MediaCodecList::findMatchingCodecs(
            mime.c_str(),
            encoder,
            0,
            &matchingCodecs);

    if (err != NULL) {
        *err = NAME_NOT_FOUND;
    }
    for (size_t i = 0; i < matchingCodecs.size(); ++i) {
        sp<MediaCodec> codec = new MediaCodec(looper, pid, uid);
        AString componentName = matchingCodecs[i];
        status_t ret = codec->init(componentName);
        if (err != NULL) {
            *err = ret;
        }
        if (ret == OK) {
            return codec;
        }
        ALOGD("Allocating component '%s' failed (%d), try next one.",
                componentName.c_str(), ret);
    }
    return NULL;
}

// static
sp<MediaCodec> MediaCodec::CreateByComponentName(
        const sp<ALooper> &looper, const AString &name, status_t *err, pid_t pid, uid_t uid) {
    sp<MediaCodec> codec = new MediaCodec(looper, pid, uid);

    const status_t ret = codec->init(name);
    if (err != NULL) {
        *err = ret;
    }
    return ret == OK ? codec : NULL; // NULL deallocates codec.
}

// static
sp<PersistentSurface> MediaCodec::CreatePersistentInputSurface() {
    sp<PersistentSurface> pluginSurface = CCodec::CreateInputSurface();
    if (pluginSurface != nullptr) {
        return pluginSurface;
    }

    OMXClient client;
    if (client.connect() != OK) {
        ALOGE("Failed to connect to OMX to create persistent input surface.");
        return NULL;
    }

    sp<IOMX> omx = client.interface();

    sp<IGraphicBufferProducer> bufferProducer;
    sp<hardware::media::omx::V1_0::IGraphicBufferSource> bufferSource;

    status_t err = omx->createInputSurface(&bufferProducer, &bufferSource);

    if (err != OK) {
        ALOGE("Failed to create persistent input surface.");
        return NULL;
    }

    return new PersistentSurface(bufferProducer, bufferSource);
}

MediaCodec::MediaCodec(
        const sp<ALooper> &looper, pid_t pid, uid_t uid,
        std::function<sp<CodecBase>(const AString &, const char *)> getCodecBase,
        std::function<status_t(const AString &, sp<MediaCodecInfo> *)> getCodecInfo)
    : mState(UNINITIALIZED),
      mReleasedByResourceManager(false),
      mLooper(looper),
      mCodec(NULL),
      mReplyID(0),
      mFlags(0),
      mStickyError(OK),
      mSoftRenderer(NULL),
      mIsVideo(false),
      mVideoWidth(0),
      mVideoHeight(0),
      mRotationDegrees(0),
      mDequeueInputTimeoutGeneration(0),
      mDequeueInputReplyID(0),
      mDequeueOutputTimeoutGeneration(0),
      mDequeueOutputReplyID(0),
      mHaveInputSurface(false),
      mHavePendingInputBuffers(false),
      mCpuBoostRequested(false),
      mLatencyUnknown(0),
      mNumLowLatencyEnables(0),
      mNumLowLatencyDisables(0),
      mIsLowLatencyModeOn(false),
      mIndexOfFirstFrameWhenLowLatencyOn(-1),
      mInputBufferCounter(0),
      mGetCodecBase(getCodecBase),
      mGetCodecInfo(getCodecInfo) {
    if (uid == kNoUid) {
        mUid = AIBinder_getCallingUid();
    } else {
        mUid = uid;
    }
    mResourceManagerProxy = new ResourceManagerServiceProxy(pid, mUid,
            ::ndk::SharedRefBase::make<ResourceManagerClient>(this));
    if (!mGetCodecBase) {
        mGetCodecBase = [](const AString &name, const char *owner) {
            return GetCodecBase(name, owner);
        };
    }
    if (!mGetCodecInfo) {
        mGetCodecInfo = [](const AString &name, sp<MediaCodecInfo> *info) -> status_t {
            *info = nullptr;
            const sp<IMediaCodecList> mcl = MediaCodecList::getInstance();
            if (!mcl) {
                return NO_INIT;  // if called from Java should raise IOException
            }
            AString tmp = name;
            if (tmp.endsWith(".secure")) {
                tmp.erase(tmp.size() - 7, 7);
            }
            for (const AString &codecName : { name, tmp }) {
                ssize_t codecIdx = mcl->findCodecByName(codecName.c_str());
                if (codecIdx < 0) {
                    continue;
                }
                *info = mcl->getCodecInfo(codecIdx);
                return OK;
            }
            return NAME_NOT_FOUND;
        };
    }

    initMediametrics();
}

MediaCodec::~MediaCodec() {
    CHECK_EQ(mState, UNINITIALIZED);
    mResourceManagerProxy->removeClient();

    flushMediametrics();
}

void MediaCodec::initMediametrics() {
    if (mMetricsHandle == 0) {
        mMetricsHandle = mediametrics_create(kCodecKeyName);
    }

    mLatencyHist.setup(kLatencyHistBuckets, kLatencyHistWidth, kLatencyHistFloor);

    {
        Mutex::Autolock al(mRecentLock);
        for (int i = 0; i<kRecentLatencyFrames; i++) {
            mRecentSamples[i] = kRecentSampleInvalid;
        }
        mRecentHead = 0;
    }

    {
        Mutex::Autolock al(mLatencyLock);
        mBuffersInFlight.clear();
        mNumLowLatencyEnables = 0;
        mNumLowLatencyDisables = 0;
        mIsLowLatencyModeOn = false;
        mIndexOfFirstFrameWhenLowLatencyOn = -1;
        mInputBufferCounter = 0;
    }

    mLifetimeStartNs = systemTime(SYSTEM_TIME_MONOTONIC);
}

void MediaCodec::updateMediametrics() {
    ALOGV("MediaCodec::updateMediametrics");
    if (mMetricsHandle == 0) {
        return;
    }

    if (mLatencyHist.getCount() != 0 ) {
        mediametrics_setInt64(mMetricsHandle, kCodecLatencyMax, mLatencyHist.getMax());
        mediametrics_setInt64(mMetricsHandle, kCodecLatencyMin, mLatencyHist.getMin());
        mediametrics_setInt64(mMetricsHandle, kCodecLatencyAvg, mLatencyHist.getAvg());
        mediametrics_setInt64(mMetricsHandle, kCodecLatencyCount, mLatencyHist.getCount());

        if (kEmitHistogram) {
            // and the histogram itself
            std::string hist = mLatencyHist.emit();
            mediametrics_setCString(mMetricsHandle, kCodecLatencyHist, hist.c_str());
        }
    }
    if (mLatencyUnknown > 0) {
        mediametrics_setInt64(mMetricsHandle, kCodecLatencyUnknown, mLatencyUnknown);
    }
    if (mLifetimeStartNs > 0) {
        nsecs_t lifetime = systemTime(SYSTEM_TIME_MONOTONIC) - mLifetimeStartNs;
        lifetime = lifetime / (1000 * 1000);    // emitted in ms, truncated not rounded
        mediametrics_setInt64(mMetricsHandle, kCodecLifetimeMs, lifetime);
    }

    {
        Mutex::Autolock al(mLatencyLock);
        mediametrics_setInt64(mMetricsHandle, kCodecNumLowLatencyModeOn, mNumLowLatencyEnables);
        mediametrics_setInt64(mMetricsHandle, kCodecNumLowLatencyModeOff, mNumLowLatencyDisables);
        mediametrics_setInt64(mMetricsHandle, kCodecFirstFrameIndexLowLatencyModeOn,
                              mIndexOfFirstFrameWhenLowLatencyOn);
    }
#if 0
    // enable for short term, only while debugging
    updateEphemeralMediametrics(mMetricsHandle);
#endif
}

void MediaCodec::updateEphemeralMediametrics(mediametrics_handle_t item) {
    ALOGD("MediaCodec::updateEphemeralMediametrics()");

    if (item == 0) {
        return;
    }

    Histogram recentHist;

    // build an empty histogram
    recentHist.setup(kLatencyHistBuckets, kLatencyHistWidth, kLatencyHistFloor);

    // stuff it with the samples in the ring buffer
    {
        Mutex::Autolock al(mRecentLock);

        for (int i=0; i<kRecentLatencyFrames; i++) {
            if (mRecentSamples[i] != kRecentSampleInvalid) {
                recentHist.insert(mRecentSamples[i]);
            }
        }
    }

    // spit the data (if any) into the supplied analytics record
    if (recentHist.getCount()!= 0 ) {
        mediametrics_setInt64(item, kCodecRecentLatencyMax, recentHist.getMax());
        mediametrics_setInt64(item, kCodecRecentLatencyMin, recentHist.getMin());
        mediametrics_setInt64(item, kCodecRecentLatencyAvg, recentHist.getAvg());
        mediametrics_setInt64(item, kCodecRecentLatencyCount, recentHist.getCount());

        if (kEmitHistogram) {
            // and the histogram itself
            std::string hist = recentHist.emit();
            mediametrics_setCString(item, kCodecRecentLatencyHist, hist.c_str());
        }
    }
}

void MediaCodec::flushMediametrics() {
    updateMediametrics();
    if (mMetricsHandle != 0) {
        if (mediametrics_count(mMetricsHandle) > 0) {
            mediametrics_selfRecord(mMetricsHandle);
        }
        mediametrics_delete(mMetricsHandle);
        mMetricsHandle = 0;
    }
}

void MediaCodec::updateLowLatency(const sp<AMessage> &msg) {
    int32_t lowLatency = 0;
    if (msg->findInt32("low-latency", &lowLatency)) {
        Mutex::Autolock al(mLatencyLock);
        if (lowLatency > 0) {
            ++mNumLowLatencyEnables;
            // This is just an estimate since low latency mode change happens ONLY at key frame
            mIsLowLatencyModeOn = true;
        } else if (lowLatency == 0) {
            ++mNumLowLatencyDisables;
            // This is just an estimate since low latency mode change happens ONLY at key frame
            mIsLowLatencyModeOn = false;
        }
    }
}

bool MediaCodec::Histogram::setup(int nbuckets, int64_t width, int64_t floor)
{
    if (nbuckets <= 0 || width <= 0) {
        return false;
    }

    // get histogram buckets
    if (nbuckets == mBucketCount && mBuckets != NULL) {
        // reuse our existing buffer
        memset(mBuckets, 0, sizeof(*mBuckets) * mBucketCount);
    } else {
        // get a new pre-zeroed buffer
        int64_t *newbuckets = (int64_t *)calloc(nbuckets, sizeof (*mBuckets));
        if (newbuckets == NULL) {
            goto bad;
        }
        if (mBuckets != NULL)
            free(mBuckets);
        mBuckets = newbuckets;
    }

    mWidth = width;
    mFloor = floor;
    mCeiling = floor + nbuckets * width;
    mBucketCount = nbuckets;

    mMin = INT64_MAX;
    mMax = INT64_MIN;
    mSum = 0;
    mCount = 0;
    mBelow = mAbove = 0;

    return true;

  bad:
    if (mBuckets != NULL) {
        free(mBuckets);
        mBuckets = NULL;
    }

    return false;
}

void MediaCodec::Histogram::insert(int64_t sample)
{
    // histogram is not set up
    if (mBuckets == NULL) {
        return;
    }

    mCount++;
    mSum += sample;
    if (mMin > sample) mMin = sample;
    if (mMax < sample) mMax = sample;

    if (sample < mFloor) {
        mBelow++;
    } else if (sample >= mCeiling) {
        mAbove++;
    } else {
        int64_t slot = (sample - mFloor) / mWidth;
        CHECK(slot < mBucketCount);
        mBuckets[slot]++;
    }
    return;
}

std::string MediaCodec::Histogram::emit()
{
    std::string value;
    char buffer[64];

    // emits:  width,Below{bucket0,bucket1,...., bucketN}above
    // unconfigured will emit: 0,0{}0
    // XXX: is this best representation?
    snprintf(buffer, sizeof(buffer), "%" PRId64 ",%" PRId64 ",%" PRId64 "{",
             mFloor, mWidth, mBelow);
    value = buffer;
    for (int i = 0; i < mBucketCount; i++) {
        if (i != 0) {
            value = value + ",";
        }
        snprintf(buffer, sizeof(buffer), "%" PRId64, mBuckets[i]);
        value = value + buffer;
    }
    snprintf(buffer, sizeof(buffer), "}%" PRId64 , mAbove);
    value = value + buffer;
    return value;
}

// when we send a buffer to the codec;
void MediaCodec::statsBufferSent(int64_t presentationUs) {

    // only enqueue if we have a legitimate time
    if (presentationUs <= 0) {
        ALOGV("presentation time: %" PRId64, presentationUs);
        return;
    }

    if (mBatteryChecker != nullptr) {
        mBatteryChecker->onCodecActivity([this] () {
            mResourceManagerProxy->addResource(MediaResource::VideoBatteryResource());
        });
    }

    const int64_t nowNs = systemTime(SYSTEM_TIME_MONOTONIC);
    BufferFlightTiming_t startdata = { presentationUs, nowNs };

    {
        // mutex access to mBuffersInFlight and other stats
        Mutex::Autolock al(mLatencyLock);


        // XXX: we *could* make sure that the time is later than the end of queue
        // as part of a consistency check...
        mBuffersInFlight.push_back(startdata);

        if (mIsLowLatencyModeOn && mIndexOfFirstFrameWhenLowLatencyOn < 0) {
            mIndexOfFirstFrameWhenLowLatencyOn = mInputBufferCounter;
        }
        ++mInputBufferCounter;
    }
}

// when we get a buffer back from the codec
void MediaCodec::statsBufferReceived(int64_t presentationUs) {

    CHECK_NE(mState, UNINITIALIZED);

    // mutex access to mBuffersInFlight and other stats
    Mutex::Autolock al(mLatencyLock);

    // how long this buffer took for the round trip through the codec
    // NB: pipelining can/will make these times larger. e.g., if each packet
    // is always 2 msec and we have 3 in flight at any given time, we're going to
    // see "6 msec" as an answer.

    // ignore stuff with no presentation time
    if (presentationUs <= 0) {
        ALOGV("-- returned buffer timestamp %" PRId64 " <= 0, ignore it", presentationUs);
        mLatencyUnknown++;
        return;
    }

    if (mBatteryChecker != nullptr) {
        mBatteryChecker->onCodecActivity([this] () {
            mResourceManagerProxy->addResource(MediaResource::VideoBatteryResource());
        });
    }

    BufferFlightTiming_t startdata;
    bool valid = false;
    while (mBuffersInFlight.size() > 0) {
        startdata = *mBuffersInFlight.begin();
        ALOGV("-- Looking at startdata. presentation %" PRId64 ", start %" PRId64,
              startdata.presentationUs, startdata.startedNs);
        if (startdata.presentationUs == presentationUs) {
            // a match
            ALOGV("-- match entry for %" PRId64 ", hits our frame of %" PRId64,
                  startdata.presentationUs, presentationUs);
            mBuffersInFlight.pop_front();
            valid = true;
            break;
        } else if (startdata.presentationUs < presentationUs) {
            // we must have missed the match for this, drop it and keep looking
            ALOGV("--  drop entry for %" PRId64 ", before our frame of %" PRId64,
                  startdata.presentationUs, presentationUs);
            mBuffersInFlight.pop_front();
            continue;
        } else {
            // head is after, so we don't have a frame for ourselves
            ALOGV("--  found entry for %" PRId64 ", AFTER our frame of %" PRId64
                  " we have nothing to pair with",
                  startdata.presentationUs, presentationUs);
            mLatencyUnknown++;
            return;
        }
    }
    if (!valid) {
        ALOGV("-- empty queue, so ignore that.");
        mLatencyUnknown++;
        return;
    }

    // nowNs start our calculations
    const int64_t nowNs = systemTime(SYSTEM_TIME_MONOTONIC);
    int64_t latencyUs = (nowNs - startdata.startedNs + 500) / 1000;

    mLatencyHist.insert(latencyUs);

    // push into the recent samples
    {
        Mutex::Autolock al(mRecentLock);

        if (mRecentHead >= kRecentLatencyFrames) {
            mRecentHead = 0;
        }
        mRecentSamples[mRecentHead++] = latencyUs;
    }
}

// static
status_t MediaCodec::PostAndAwaitResponse(
        const sp<AMessage> &msg, sp<AMessage> *response) {
    status_t err = msg->postAndAwaitResponse(response);

    if (err != OK) {
        return err;
    }

    if (!(*response)->findInt32("err", &err)) {
        err = OK;
    }

    return err;
}

void MediaCodec::PostReplyWithError(const sp<AMessage> &msg, int32_t err) {
    sp<AReplyToken> replyID;
    CHECK(msg->senderAwaitsResponse(&replyID));
    PostReplyWithError(replyID, err);
}

void MediaCodec::PostReplyWithError(const sp<AReplyToken> &replyID, int32_t err) {
    int32_t finalErr = err;
    if (mReleasedByResourceManager) {
        // override the err code if MediaCodec has been released by ResourceManager.
        finalErr = DEAD_OBJECT;
    }

    sp<AMessage> response = new AMessage;
    response->setInt32("err", finalErr);
    response->postReply(replyID);
}

static CodecBase *CreateCCodec() {
    return new CCodec;
}

//static
sp<CodecBase> MediaCodec::GetCodecBase(const AString &name, const char *owner) {
    if (owner) {
        if (strcmp(owner, "default") == 0) {
            return new ACodec;
        } else if (strncmp(owner, "codec2", 6) == 0) {
            return CreateCCodec();
        }
    }

    if (name.startsWithIgnoreCase("c2.")) {
        return CreateCCodec();
    } else if (name.startsWithIgnoreCase("omx.")) {
        // at this time only ACodec specifies a mime type.
        return new ACodec;
    } else if (name.startsWithIgnoreCase("android.filter.")) {
        return new MediaFilter;
    } else {
        return NULL;
    }
}

struct CodecListCache {
    CodecListCache()
        : mCodecInfoMap{[] {
              const sp<IMediaCodecList> mcl = MediaCodecList::getInstance();
              size_t count = mcl->countCodecs();
              std::map<std::string, sp<MediaCodecInfo>> codecInfoMap;
              for (size_t i = 0; i < count; ++i) {
                  sp<MediaCodecInfo> info = mcl->getCodecInfo(i);
                  codecInfoMap.emplace(info->getCodecName(), info);
              }
              return codecInfoMap;
          }()} {
    }

    const std::map<std::string, sp<MediaCodecInfo>> mCodecInfoMap;
};

static const CodecListCache &GetCodecListCache() {
    static CodecListCache sCache{};
    return sCache;
}

status_t MediaCodec::init(const AString &name) {
    mResourceManagerProxy->init();

    // save init parameters for reset
    mInitName = name;

    // Current video decoders do not return from OMX_FillThisBuffer
    // quickly, violating the OpenMAX specs, until that is remedied
    // we need to invest in an extra looper to free the main event
    // queue.

    mCodecInfo.clear();

    bool secureCodec = false;
    const char *owner = "";
    if (!name.startsWith("android.filter.")) {
        status_t err = mGetCodecInfo(name, &mCodecInfo);
        if (err != OK) {
            mCodec = NULL;  // remove the codec.
            return err;
        }
        if (mCodecInfo == nullptr) {
            ALOGE("Getting codec info with name '%s' failed", name.c_str());
            return NAME_NOT_FOUND;
        }
        secureCodec = name.endsWith(".secure");
        Vector<AString> mediaTypes;
        mCodecInfo->getSupportedMediaTypes(&mediaTypes);
        for (size_t i = 0; i < mediaTypes.size(); ++i) {
            if (mediaTypes[i].startsWith("video/")) {
                mIsVideo = true;
                break;
            }
        }
        owner = mCodecInfo->getOwnerName();
    }

    mCodec = mGetCodecBase(name, owner);
    if (mCodec == NULL) {
        ALOGE("Getting codec base with name '%s' (owner='%s') failed", name.c_str(), owner);
        return NAME_NOT_FOUND;
    }

    if (mIsVideo) {
        // video codec needs dedicated looper
        if (mCodecLooper == NULL) {
            mCodecLooper = new ALooper;
            mCodecLooper->setName("CodecLooper");
            mCodecLooper->start(false, false, ANDROID_PRIORITY_AUDIO);
        }

        mCodecLooper->registerHandler(mCodec);
    } else {
        mLooper->registerHandler(mCodec);
    }

    mLooper->registerHandler(this);

    mCodec->setCallback(
            std::unique_ptr<CodecBase::CodecCallback>(
                    new CodecCallback(new AMessage(kWhatCodecNotify, this))));
    mBufferChannel = mCodec->getBufferChannel();
    mBufferChannel->setCallback(
            std::unique_ptr<CodecBase::BufferCallback>(
                    new BufferCallback(new AMessage(kWhatCodecNotify, this))));

    sp<AMessage> msg = new AMessage(kWhatInit, this);
    if (mCodecInfo) {
        msg->setObject("codecInfo", mCodecInfo);
        // name may be different from mCodecInfo->getCodecName() if we stripped
        // ".secure"
    }
    msg->setString("name", name);

    if (mMetricsHandle != 0) {
        mediametrics_setCString(mMetricsHandle, kCodecCodec, name.c_str());
        mediametrics_setCString(mMetricsHandle, kCodecMode,
                                mIsVideo ? kCodecModeVideo : kCodecModeAudio);
    }

    if (mIsVideo) {
        mBatteryChecker = new BatteryChecker(new AMessage(kWhatCheckBatteryStats, this));
    }

    status_t err;
    std::vector<MediaResourceParcel> resources;
    resources.push_back(MediaResource::CodecResource(secureCodec, mIsVideo));
    for (int i = 0; i <= kMaxRetry; ++i) {
        if (i > 0) {
            // Don't try to reclaim resource for the first time.
            if (!mResourceManagerProxy->reclaimResource(resources)) {
                break;
            }
        }

        sp<AMessage> response;
        err = PostAndAwaitResponse(msg, &response);
        if (!isResourceError(err)) {
            break;
        }
    }
    return err;
}

status_t MediaCodec::setCallback(const sp<AMessage> &callback) {
    sp<AMessage> msg = new AMessage(kWhatSetCallback, this);
    msg->setMessage("callback", callback);

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t MediaCodec::setOnFrameRenderedNotification(const sp<AMessage> &notify) {
    sp<AMessage> msg = new AMessage(kWhatSetNotification, this);
    msg->setMessage("on-frame-rendered", notify);
    return msg->post();
}

status_t MediaCodec::configure(
        const sp<AMessage> &format,
        const sp<Surface> &nativeWindow,
        const sp<ICrypto> &crypto,
        uint32_t flags) {
    return configure(format, nativeWindow, crypto, NULL, flags);
}

status_t MediaCodec::configure(
        const sp<AMessage> &format,
        const sp<Surface> &surface,
        const sp<ICrypto> &crypto,
        const sp<IDescrambler> &descrambler,
        uint32_t flags) {
    sp<AMessage> msg = new AMessage(kWhatConfigure, this);

    if (mMetricsHandle != 0) {
        int32_t profile = 0;
        if (format->findInt32("profile", &profile)) {
            mediametrics_setInt32(mMetricsHandle, kCodecProfile, profile);
        }
        int32_t level = 0;
        if (format->findInt32("level", &level)) {
            mediametrics_setInt32(mMetricsHandle, kCodecLevel, level);
        }
        mediametrics_setInt32(mMetricsHandle, kCodecEncoder,
                              (flags & CONFIGURE_FLAG_ENCODE) ? 1 : 0);
    }

    if (mIsVideo) {
        format->findInt32("width", &mVideoWidth);
        format->findInt32("height", &mVideoHeight);
        if (!format->findInt32("rotation-degrees", &mRotationDegrees)) {
            mRotationDegrees = 0;
        }

        if (mMetricsHandle != 0) {
            mediametrics_setInt32(mMetricsHandle, kCodecWidth, mVideoWidth);
            mediametrics_setInt32(mMetricsHandle, kCodecHeight, mVideoHeight);
            mediametrics_setInt32(mMetricsHandle, kCodecRotation, mRotationDegrees);
            int32_t maxWidth = 0;
            if (format->findInt32("max-width", &maxWidth)) {
                mediametrics_setInt32(mMetricsHandle, kCodecMaxWidth, maxWidth);
            }
            int32_t maxHeight = 0;
            if (format->findInt32("max-height", &maxHeight)) {
                mediametrics_setInt32(mMetricsHandle, kCodecMaxHeight, maxHeight);
            }
        }

        // Prevent possible integer overflow in downstream code.
        if (mVideoWidth < 0 || mVideoHeight < 0 ||
               (uint64_t)mVideoWidth * mVideoHeight > (uint64_t)INT32_MAX / 4) {
            ALOGE("Invalid size(s), width=%d, height=%d", mVideoWidth, mVideoHeight);
            return BAD_VALUE;
        }
    }

    updateLowLatency(format);

    msg->setMessage("format", format);
    msg->setInt32("flags", flags);
    msg->setObject("surface", surface);

    if (crypto != NULL || descrambler != NULL) {
        if (crypto != NULL) {
            msg->setPointer("crypto", crypto.get());
        } else {
            msg->setPointer("descrambler", descrambler.get());
        }
        if (mMetricsHandle != 0) {
            mediametrics_setInt32(mMetricsHandle, kCodecCrypto, 1);
        }
    } else if (mFlags & kFlagIsSecure) {
        ALOGW("Crypto or descrambler should be given for secure codec");
    }

    // save msg for reset
    mConfigureMsg = msg;

    status_t err;
    std::vector<MediaResourceParcel> resources;
    resources.push_back(MediaResource::CodecResource(mFlags & kFlagIsSecure, mIsVideo));
    // Don't know the buffer size at this point, but it's fine to use 1 because
    // the reclaimResource call doesn't consider the requester's buffer size for now.
    resources.push_back(MediaResource::GraphicMemoryResource(1));
    for (int i = 0; i <= kMaxRetry; ++i) {
        if (i > 0) {
            // Don't try to reclaim resource for the first time.
            if (!mResourceManagerProxy->reclaimResource(resources)) {
                break;
            }
        }

        sp<AMessage> response;
        err = PostAndAwaitResponse(msg, &response);
        if (err != OK && err != INVALID_OPERATION) {
            // MediaCodec now set state to UNINITIALIZED upon any fatal error.
            // To maintain backward-compatibility, do a reset() to put codec
            // back into INITIALIZED state.
            // But don't reset if the err is INVALID_OPERATION, which means
            // the configure failure is due to wrong state.

            ALOGE("configure failed with err 0x%08x, resetting...", err);
            reset();
        }
        if (!isResourceError(err)) {
            break;
        }
    }

    return err;
}

status_t MediaCodec::releaseCrypto()
{
    ALOGV("releaseCrypto");

    sp<AMessage> msg = new AMessage(kWhatDrmReleaseCrypto, this);

    sp<AMessage> response;
    status_t status = msg->postAndAwaitResponse(&response);

    if (status == OK && response != NULL) {
        CHECK(response->findInt32("status", &status));
        ALOGV("releaseCrypto ret: %d ", status);
    }
    else {
        ALOGE("releaseCrypto err: %d", status);
    }

    return status;
}

void MediaCodec::onReleaseCrypto(const sp<AMessage>& msg)
{
    status_t status = INVALID_OPERATION;
    if (mCrypto != NULL) {
        ALOGV("onReleaseCrypto: mCrypto: %p (%d)", mCrypto.get(), mCrypto->getStrongCount());
        mBufferChannel->setCrypto(NULL);
        // TODO change to ALOGV
        ALOGD("onReleaseCrypto: [before clear]  mCrypto: %p (%d)",
                mCrypto.get(), mCrypto->getStrongCount());
        mCrypto.clear();

        status = OK;
    }
    else {
        ALOGW("onReleaseCrypto: No mCrypto. err: %d", status);
    }

    sp<AMessage> response = new AMessage;
    response->setInt32("status", status);

    sp<AReplyToken> replyID;
    CHECK(msg->senderAwaitsResponse(&replyID));
    response->postReply(replyID);
}

status_t MediaCodec::setInputSurface(
        const sp<PersistentSurface> &surface) {
    sp<AMessage> msg = new AMessage(kWhatSetInputSurface, this);
    msg->setObject("input-surface", surface.get());

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t MediaCodec::setSurface(const sp<Surface> &surface) {
    sp<AMessage> msg = new AMessage(kWhatSetSurface, this);
    msg->setObject("surface", surface);

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t MediaCodec::createInputSurface(
        sp<IGraphicBufferProducer>* bufferProducer) {
    sp<AMessage> msg = new AMessage(kWhatCreateInputSurface, this);

    sp<AMessage> response;
    status_t err = PostAndAwaitResponse(msg, &response);
    if (err == NO_ERROR) {
        // unwrap the sp<IGraphicBufferProducer>
        sp<RefBase> obj;
        bool found = response->findObject("input-surface", &obj);
        CHECK(found);
        sp<BufferProducerWrapper> wrapper(
                static_cast<BufferProducerWrapper*>(obj.get()));
        *bufferProducer = wrapper->getBufferProducer();
    } else {
        ALOGW("createInputSurface failed, err=%d", err);
    }
    return err;
}

uint64_t MediaCodec::getGraphicBufferSize() {
    if (!mIsVideo) {
        return 0;
    }

    uint64_t size = 0;
    size_t portNum = sizeof(mPortBuffers) / sizeof((mPortBuffers)[0]);
    for (size_t i = 0; i < portNum; ++i) {
        // TODO: this is just an estimation, we should get the real buffer size from ACodec.
        size += mPortBuffers[i].size() * mVideoWidth * mVideoHeight * 3 / 2;
    }
    return size;
}

status_t MediaCodec::start() {
    sp<AMessage> msg = new AMessage(kWhatStart, this);

    status_t err;
    std::vector<MediaResourceParcel> resources;
    resources.push_back(MediaResource::CodecResource(mFlags & kFlagIsSecure, mIsVideo));
    // Don't know the buffer size at this point, but it's fine to use 1 because
    // the reclaimResource call doesn't consider the requester's buffer size for now.
    resources.push_back(MediaResource::GraphicMemoryResource(1));
    for (int i = 0; i <= kMaxRetry; ++i) {
        if (i > 0) {
            // Don't try to reclaim resource for the first time.
            if (!mResourceManagerProxy->reclaimResource(resources)) {
                break;
            }
            // Recover codec from previous error before retry start.
            err = reset();
            if (err != OK) {
                ALOGE("retrying start: failed to reset codec");
                break;
            }
            sp<AMessage> response;
            err = PostAndAwaitResponse(mConfigureMsg, &response);
            if (err != OK) {
                ALOGE("retrying start: failed to configure codec");
                break;
            }
        }

        sp<AMessage> response;
        err = PostAndAwaitResponse(msg, &response);
        if (!isResourceError(err)) {
            break;
        }
    }
    return err;
}

status_t MediaCodec::stop() {
    sp<AMessage> msg = new AMessage(kWhatStop, this);

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

bool MediaCodec::hasPendingBuffer(int portIndex) {
    return std::any_of(
            mPortBuffers[portIndex].begin(), mPortBuffers[portIndex].end(),
            [](const BufferInfo &info) { return info.mOwnedByClient; });
}

bool MediaCodec::hasPendingBuffer() {
    return hasPendingBuffer(kPortIndexInput) || hasPendingBuffer(kPortIndexOutput);
}

status_t MediaCodec::reclaim(bool force) {
    ALOGD("MediaCodec::reclaim(%p) %s", this, mInitName.c_str());
    sp<AMessage> msg = new AMessage(kWhatRelease, this);
    msg->setInt32("reclaimed", 1);
    msg->setInt32("force", force ? 1 : 0);

    sp<AMessage> response;
    status_t ret = PostAndAwaitResponse(msg, &response);
    if (ret == -ENOENT) {
        ALOGD("MediaCodec looper is gone, skip reclaim");
        ret = OK;
    }
    return ret;
}

status_t MediaCodec::release() {
    sp<AMessage> msg = new AMessage(kWhatRelease, this);
    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t MediaCodec::releaseAsync(const sp<AMessage> &notify) {
    sp<AMessage> msg = new AMessage(kWhatRelease, this);
    msg->setMessage("async", notify);
    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t MediaCodec::reset() {
    /* When external-facing MediaCodec object is created,
       it is already initialized.  Thus, reset is essentially
       release() followed by init(), plus clearing the state */

    status_t err = release();

    // unregister handlers
    if (mCodec != NULL) {
        if (mCodecLooper != NULL) {
            mCodecLooper->unregisterHandler(mCodec->id());
        } else {
            mLooper->unregisterHandler(mCodec->id());
        }
        mCodec = NULL;
    }
    mLooper->unregisterHandler(id());

    mFlags = 0;    // clear all flags
    mStickyError = OK;

    // reset state not reset by setState(UNINITIALIZED)
    mDequeueInputReplyID = 0;
    mDequeueOutputReplyID = 0;
    mDequeueInputTimeoutGeneration = 0;
    mDequeueOutputTimeoutGeneration = 0;
    mHaveInputSurface = false;

    if (err == OK) {
        err = init(mInitName);
    }
    return err;
}

status_t MediaCodec::queueInputBuffer(
        size_t index,
        size_t offset,
        size_t size,
        int64_t presentationTimeUs,
        uint32_t flags,
        AString *errorDetailMsg) {
    if (errorDetailMsg != NULL) {
        errorDetailMsg->clear();
    }

    sp<AMessage> msg = new AMessage(kWhatQueueInputBuffer, this);
    msg->setSize("index", index);
    msg->setSize("offset", offset);
    msg->setSize("size", size);
    msg->setInt64("timeUs", presentationTimeUs);
    msg->setInt32("flags", flags);
    msg->setPointer("errorDetailMsg", errorDetailMsg);

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t MediaCodec::queueSecureInputBuffer(
        size_t index,
        size_t offset,
        const CryptoPlugin::SubSample *subSamples,
        size_t numSubSamples,
        const uint8_t key[16],
        const uint8_t iv[16],
        CryptoPlugin::Mode mode,
        const CryptoPlugin::Pattern &pattern,
        int64_t presentationTimeUs,
        uint32_t flags,
        AString *errorDetailMsg) {
    if (errorDetailMsg != NULL) {
        errorDetailMsg->clear();
    }

    sp<AMessage> msg = new AMessage(kWhatQueueInputBuffer, this);
    msg->setSize("index", index);
    msg->setSize("offset", offset);
    msg->setPointer("subSamples", (void *)subSamples);
    msg->setSize("numSubSamples", numSubSamples);
    msg->setPointer("key", (void *)key);
    msg->setPointer("iv", (void *)iv);
    msg->setInt32("mode", mode);
    msg->setInt32("encryptBlocks", pattern.mEncryptBlocks);
    msg->setInt32("skipBlocks", pattern.mSkipBlocks);
    msg->setInt64("timeUs", presentationTimeUs);
    msg->setInt32("flags", flags);
    msg->setPointer("errorDetailMsg", errorDetailMsg);

    sp<AMessage> response;
    status_t err = PostAndAwaitResponse(msg, &response);

    return err;
}

status_t MediaCodec::queueBuffer(
        size_t index,
        const std::shared_ptr<C2Buffer> &buffer,
        int64_t presentationTimeUs,
        uint32_t flags,
        const sp<AMessage> &tunings,
        AString *errorDetailMsg) {
    if (errorDetailMsg != NULL) {
        errorDetailMsg->clear();
    }

    sp<AMessage> msg = new AMessage(kWhatQueueInputBuffer, this);
    msg->setSize("index", index);
    sp<WrapperObject<std::shared_ptr<C2Buffer>>> obj{
        new WrapperObject<std::shared_ptr<C2Buffer>>{buffer}};
    msg->setObject("c2buffer", obj);
    msg->setInt64("timeUs", presentationTimeUs);
    msg->setInt32("flags", flags);
    msg->setMessage("tunings", tunings);
    msg->setPointer("errorDetailMsg", errorDetailMsg);

    sp<AMessage> response;
    status_t err = PostAndAwaitResponse(msg, &response);

    return err;
}

status_t MediaCodec::queueEncryptedBuffer(
        size_t index,
        const sp<hardware::HidlMemory> &buffer,
        size_t offset,
        const CryptoPlugin::SubSample *subSamples,
        size_t numSubSamples,
        const uint8_t key[16],
        const uint8_t iv[16],
        CryptoPlugin::Mode mode,
        const CryptoPlugin::Pattern &pattern,
        int64_t presentationTimeUs,
        uint32_t flags,
        const sp<AMessage> &tunings,
        AString *errorDetailMsg) {
    if (errorDetailMsg != NULL) {
        errorDetailMsg->clear();
    }

    sp<AMessage> msg = new AMessage(kWhatQueueInputBuffer, this);
    msg->setSize("index", index);
    sp<WrapperObject<sp<hardware::HidlMemory>>> memory{
        new WrapperObject<sp<hardware::HidlMemory>>{buffer}};
    msg->setObject("memory", memory);
    msg->setSize("offset", offset);
    msg->setPointer("subSamples", (void *)subSamples);
    msg->setSize("numSubSamples", numSubSamples);
    msg->setPointer("key", (void *)key);
    msg->setPointer("iv", (void *)iv);
    msg->setInt32("mode", mode);
    msg->setInt32("encryptBlocks", pattern.mEncryptBlocks);
    msg->setInt32("skipBlocks", pattern.mSkipBlocks);
    msg->setInt64("timeUs", presentationTimeUs);
    msg->setInt32("flags", flags);
    msg->setMessage("tunings", tunings);
    msg->setPointer("errorDetailMsg", errorDetailMsg);

    sp<AMessage> response;
    status_t err = PostAndAwaitResponse(msg, &response);

    return err;
}

status_t MediaCodec::dequeueInputBuffer(size_t *index, int64_t timeoutUs) {
    sp<AMessage> msg = new AMessage(kWhatDequeueInputBuffer, this);
    msg->setInt64("timeoutUs", timeoutUs);

    sp<AMessage> response;
    status_t err;
    if ((err = PostAndAwaitResponse(msg, &response)) != OK) {
        return err;
    }

    CHECK(response->findSize("index", index));

    return OK;
}

status_t MediaCodec::dequeueOutputBuffer(
        size_t *index,
        size_t *offset,
        size_t *size,
        int64_t *presentationTimeUs,
        uint32_t *flags,
        int64_t timeoutUs) {
    sp<AMessage> msg = new AMessage(kWhatDequeueOutputBuffer, this);
    msg->setInt64("timeoutUs", timeoutUs);

    sp<AMessage> response;
    status_t err;
    if ((err = PostAndAwaitResponse(msg, &response)) != OK) {
        return err;
    }

    CHECK(response->findSize("index", index));
    CHECK(response->findSize("offset", offset));
    CHECK(response->findSize("size", size));
    CHECK(response->findInt64("timeUs", presentationTimeUs));
    CHECK(response->findInt32("flags", (int32_t *)flags));

    return OK;
}

status_t MediaCodec::renderOutputBufferAndRelease(size_t index) {
    sp<AMessage> msg = new AMessage(kWhatReleaseOutputBuffer, this);
    msg->setSize("index", index);
    msg->setInt32("render", true);

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t MediaCodec::renderOutputBufferAndRelease(size_t index, int64_t timestampNs) {
    sp<AMessage> msg = new AMessage(kWhatReleaseOutputBuffer, this);
    msg->setSize("index", index);
    msg->setInt32("render", true);
    msg->setInt64("timestampNs", timestampNs);

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t MediaCodec::releaseOutputBuffer(size_t index) {
    sp<AMessage> msg = new AMessage(kWhatReleaseOutputBuffer, this);
    msg->setSize("index", index);

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t MediaCodec::signalEndOfInputStream() {
    sp<AMessage> msg = new AMessage(kWhatSignalEndOfInputStream, this);

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t MediaCodec::getOutputFormat(sp<AMessage> *format) const {
    sp<AMessage> msg = new AMessage(kWhatGetOutputFormat, this);

    sp<AMessage> response;
    status_t err;
    if ((err = PostAndAwaitResponse(msg, &response)) != OK) {
        return err;
    }

    CHECK(response->findMessage("format", format));

    return OK;
}

status_t MediaCodec::getInputFormat(sp<AMessage> *format) const {
    sp<AMessage> msg = new AMessage(kWhatGetInputFormat, this);

    sp<AMessage> response;
    status_t err;
    if ((err = PostAndAwaitResponse(msg, &response)) != OK) {
        return err;
    }

    CHECK(response->findMessage("format", format));

    return OK;
}

status_t MediaCodec::getName(AString *name) const {
    sp<AMessage> msg = new AMessage(kWhatGetName, this);

    sp<AMessage> response;
    status_t err;
    if ((err = PostAndAwaitResponse(msg, &response)) != OK) {
        return err;
    }

    CHECK(response->findString("name", name));

    return OK;
}

status_t MediaCodec::getCodecInfo(sp<MediaCodecInfo> *codecInfo) const {
    sp<AMessage> msg = new AMessage(kWhatGetCodecInfo, this);

    sp<AMessage> response;
    status_t err;
    if ((err = PostAndAwaitResponse(msg, &response)) != OK) {
        return err;
    }

    sp<RefBase> obj;
    CHECK(response->findObject("codecInfo", &obj));
    *codecInfo = static_cast<MediaCodecInfo *>(obj.get());

    return OK;
}

status_t MediaCodec::getMetrics(mediametrics_handle_t &reply) {

    reply = 0;

    // shouldn't happen, but be safe
    if (mMetricsHandle == 0) {
        return UNKNOWN_ERROR;
    }

    // update any in-flight data that's not carried within the record
    updateMediametrics();

    // send it back to the caller.
    reply = mediametrics_dup(mMetricsHandle);

    updateEphemeralMediametrics(reply);

    return OK;
}

status_t MediaCodec::getInputBuffers(Vector<sp<MediaCodecBuffer> > *buffers) const {
    sp<AMessage> msg = new AMessage(kWhatGetBuffers, this);
    msg->setInt32("portIndex", kPortIndexInput);
    msg->setPointer("buffers", buffers);

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t MediaCodec::getOutputBuffers(Vector<sp<MediaCodecBuffer> > *buffers) const {
    sp<AMessage> msg = new AMessage(kWhatGetBuffers, this);
    msg->setInt32("portIndex", kPortIndexOutput);
    msg->setPointer("buffers", buffers);

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t MediaCodec::getOutputBuffer(size_t index, sp<MediaCodecBuffer> *buffer) {
    sp<AMessage> format;
    return getBufferAndFormat(kPortIndexOutput, index, buffer, &format);
}

status_t MediaCodec::getOutputFormat(size_t index, sp<AMessage> *format) {
    sp<MediaCodecBuffer> buffer;
    return getBufferAndFormat(kPortIndexOutput, index, &buffer, format);
}

status_t MediaCodec::getInputBuffer(size_t index, sp<MediaCodecBuffer> *buffer) {
    sp<AMessage> format;
    return getBufferAndFormat(kPortIndexInput, index, buffer, &format);
}

bool MediaCodec::isExecuting() const {
    return mState == STARTED || mState == FLUSHED;
}

status_t MediaCodec::getBufferAndFormat(
        size_t portIndex, size_t index,
        sp<MediaCodecBuffer> *buffer, sp<AMessage> *format) {
    // use mutex instead of a context switch
    if (mReleasedByResourceManager) {
        ALOGE("getBufferAndFormat - resource already released");
        return DEAD_OBJECT;
    }

    if (buffer == NULL) {
        ALOGE("getBufferAndFormat - null MediaCodecBuffer");
        return INVALID_OPERATION;
    }

    if (format == NULL) {
        ALOGE("getBufferAndFormat - null AMessage");
        return INVALID_OPERATION;
    }

    buffer->clear();
    format->clear();

    if (!isExecuting()) {
        ALOGE("getBufferAndFormat - not executing");
        return INVALID_OPERATION;
    }

    // we do not want mPortBuffers to change during this section
    // we also don't want mOwnedByClient to change during this
    Mutex::Autolock al(mBufferLock);

    std::vector<BufferInfo> &buffers = mPortBuffers[portIndex];
    if (index >= buffers.size()) {
        ALOGE("getBufferAndFormat - trying to get buffer with "
              "bad index (index=%zu buffer_size=%zu)", index, buffers.size());
        return INVALID_OPERATION;
    }

    const BufferInfo &info = buffers[index];
    if (!info.mOwnedByClient) {
        ALOGE("getBufferAndFormat - invalid operation "
              "(the index %zu is not owned by client)", index);
        return INVALID_OPERATION;
    }

    *buffer = info.mData;
    *format = info.mData->format();

    return OK;
}

status_t MediaCodec::flush() {
    sp<AMessage> msg = new AMessage(kWhatFlush, this);

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t MediaCodec::requestIDRFrame() {
    (new AMessage(kWhatRequestIDRFrame, this))->post();

    return OK;
}

void MediaCodec::requestActivityNotification(const sp<AMessage> &notify) {
    sp<AMessage> msg = new AMessage(kWhatRequestActivityNotification, this);
    msg->setMessage("notify", notify);
    msg->post();
}

void MediaCodec::requestCpuBoostIfNeeded() {
    if (mCpuBoostRequested) {
        return;
    }
    int32_t colorFormat;
    if (mOutputFormat->contains("hdr-static-info")
            && mOutputFormat->findInt32("color-format", &colorFormat)
            // check format for OMX only, for C2 the format is always opaque since the
            // software rendering doesn't go through client
            && ((mSoftRenderer != NULL && colorFormat == OMX_COLOR_FormatYUV420Planar16)
                    || mOwnerName.equalsIgnoreCase("codec2::software"))) {
        int32_t left, top, right, bottom, width, height;
        int64_t totalPixel = 0;
        if (mOutputFormat->findRect("crop", &left, &top, &right, &bottom)) {
            totalPixel = (right - left + 1) * (bottom - top + 1);
        } else if (mOutputFormat->findInt32("width", &width)
                && mOutputFormat->findInt32("height", &height)) {
            totalPixel = width * height;
        }
        if (totalPixel >= 1920 * 1080) {
            mResourceManagerProxy->addResource(MediaResource::CpuBoostResource());
            mCpuBoostRequested = true;
        }
    }
}

BatteryChecker::BatteryChecker(const sp<AMessage> &msg, int64_t timeoutUs)
    : mTimeoutUs(timeoutUs)
    , mLastActivityTimeUs(-1ll)
    , mBatteryStatNotified(false)
    , mBatteryCheckerGeneration(0)
    , mIsExecuting(false)
    , mBatteryCheckerMsg(msg) {}

void BatteryChecker::onCodecActivity(std::function<void()> batteryOnCb) {
    if (!isExecuting()) {
        // ignore if not executing
        return;
    }
    if (!mBatteryStatNotified) {
        batteryOnCb();
        mBatteryStatNotified = true;
        sp<AMessage> msg = mBatteryCheckerMsg->dup();
        msg->setInt32("generation", mBatteryCheckerGeneration);

        // post checker and clear last activity time
        msg->post(mTimeoutUs);
        mLastActivityTimeUs = -1ll;
    } else {
        // update last activity time
        mLastActivityTimeUs = ALooper::GetNowUs();
    }
}

void BatteryChecker::onCheckBatteryTimer(
        const sp<AMessage> &msg, std::function<void()> batteryOffCb) {
    // ignore if this checker already expired because the client resource was removed
    int32_t generation;
    if (!msg->findInt32("generation", &generation)
            || generation != mBatteryCheckerGeneration) {
        return;
    }

    if (mLastActivityTimeUs < 0ll) {
        // timed out inactive, do not repost checker
        batteryOffCb();
        mBatteryStatNotified = false;
    } else {
        // repost checker and clear last activity time
        msg->post(mTimeoutUs + mLastActivityTimeUs - ALooper::GetNowUs());
        mLastActivityTimeUs = -1ll;
    }
}

void BatteryChecker::onClientRemoved() {
    mBatteryStatNotified = false;
    mBatteryCheckerGeneration++;
}

////////////////////////////////////////////////////////////////////////////////

void MediaCodec::cancelPendingDequeueOperations() {
    if (mFlags & kFlagDequeueInputPending) {
        PostReplyWithError(mDequeueInputReplyID, INVALID_OPERATION);

        ++mDequeueInputTimeoutGeneration;
        mDequeueInputReplyID = 0;
        mFlags &= ~kFlagDequeueInputPending;
    }

    if (mFlags & kFlagDequeueOutputPending) {
        PostReplyWithError(mDequeueOutputReplyID, INVALID_OPERATION);

        ++mDequeueOutputTimeoutGeneration;
        mDequeueOutputReplyID = 0;
        mFlags &= ~kFlagDequeueOutputPending;
    }
}

bool MediaCodec::handleDequeueInputBuffer(const sp<AReplyToken> &replyID, bool newRequest) {
    if (!isExecuting() || (mFlags & kFlagIsAsync)
            || (newRequest && (mFlags & kFlagDequeueInputPending))) {
        PostReplyWithError(replyID, INVALID_OPERATION);
        return true;
    } else if (mFlags & kFlagStickyError) {
        PostReplyWithError(replyID, getStickyError());
        return true;
    }

    ssize_t index = dequeuePortBuffer(kPortIndexInput);

    if (index < 0) {
        CHECK_EQ(index, -EAGAIN);
        return false;
    }

    sp<AMessage> response = new AMessage;
    response->setSize("index", index);
    response->postReply(replyID);

    return true;
}

bool MediaCodec::handleDequeueOutputBuffer(const sp<AReplyToken> &replyID, bool newRequest) {
    if (!isExecuting() || (mFlags & kFlagIsAsync)
            || (newRequest && (mFlags & kFlagDequeueOutputPending))) {
        PostReplyWithError(replyID, INVALID_OPERATION);
    } else if (mFlags & kFlagStickyError) {
        PostReplyWithError(replyID, getStickyError());
    } else if (mFlags & kFlagOutputBuffersChanged) {
        PostReplyWithError(replyID, INFO_OUTPUT_BUFFERS_CHANGED);
        mFlags &= ~kFlagOutputBuffersChanged;
    } else {
        sp<AMessage> response = new AMessage;
        BufferInfo *info = peekNextPortBuffer(kPortIndexOutput);
        if (!info) {
            return false;
        }

        // In synchronous mode, output format change should be handled
        // at dequeue to put the event at the correct order.

        const sp<MediaCodecBuffer> &buffer = info->mData;
        handleOutputFormatChangeIfNeeded(buffer);
        if (mFlags & kFlagOutputFormatChanged) {
            PostReplyWithError(replyID, INFO_FORMAT_CHANGED);
            mFlags &= ~kFlagOutputFormatChanged;
            return true;
        }

        ssize_t index = dequeuePortBuffer(kPortIndexOutput);

        response->setSize("index", index);
        response->setSize("offset", buffer->offset());
        response->setSize("size", buffer->size());

        int64_t timeUs;
        CHECK(buffer->meta()->findInt64("timeUs", &timeUs));

        statsBufferReceived(timeUs);

        response->setInt64("timeUs", timeUs);

        int32_t flags;
        CHECK(buffer->meta()->findInt32("flags", &flags));

        response->setInt32("flags", flags);
        response->postReply(replyID);
    }

    return true;
}

void MediaCodec::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatCodecNotify:
        {
            int32_t what;
            CHECK(msg->findInt32("what", &what));

            switch (what) {
                case kWhatError:
                {
                    int32_t err, actionCode;
                    CHECK(msg->findInt32("err", &err));
                    CHECK(msg->findInt32("actionCode", &actionCode));

                    ALOGE("Codec reported err %#x, actionCode %d, while in state %d",
                            err, actionCode, mState);
                    if (err == DEAD_OBJECT) {
                        mFlags |= kFlagSawMediaServerDie;
                        mFlags &= ~kFlagIsComponentAllocated;
                    }

                    bool sendErrorResponse = true;
                    std::string origin{"kWhatError:"};
                    origin += stateString(mState);

                    switch (mState) {
                        case INITIALIZING:
                        {
                            setState(UNINITIALIZED);
                            break;
                        }

                        case CONFIGURING:
                        {
                            if (actionCode == ACTION_CODE_FATAL) {
                                mediametrics_setInt32(mMetricsHandle, kCodecError, err);
                                mediametrics_setCString(mMetricsHandle, kCodecErrorState,
                                                        stateString(mState).c_str());
                                flushMediametrics();
                                initMediametrics();
                            }
                            setState(actionCode == ACTION_CODE_FATAL ?
                                    UNINITIALIZED : INITIALIZED);
                            break;
                        }

                        case STARTING:
                        {
                            if (actionCode == ACTION_CODE_FATAL) {
                                mediametrics_setInt32(mMetricsHandle, kCodecError, err);
                                mediametrics_setCString(mMetricsHandle, kCodecErrorState,
                                                        stateString(mState).c_str());
                                flushMediametrics();
                                initMediametrics();
                            }
                            setState(actionCode == ACTION_CODE_FATAL ?
                                    UNINITIALIZED : CONFIGURED);
                            break;
                        }

                        case RELEASING:
                        {
                            // Ignore the error, assuming we'll still get
                            // the shutdown complete notification. If we
                            // don't, we'll timeout and force release.
                            sendErrorResponse = false;
                            FALLTHROUGH_INTENDED;
                        }
                        case STOPPING:
                        {
                            if (mFlags & kFlagSawMediaServerDie) {
                                // MediaServer died, there definitely won't
                                // be a shutdown complete notification after
                                // all.

                                // note that we may be directly going from
                                // STOPPING->UNINITIALIZED, instead of the
                                // usual STOPPING->INITIALIZED state.
                                setState(UNINITIALIZED);
                                if (mState == RELEASING) {
                                    mComponentName.clear();
                                }
                                postPendingRepliesAndDeferredMessages(origin + ":dead");
                                sendErrorResponse = false;
                            }
                            break;
                        }

                        case FLUSHING:
                        {
                            if (actionCode == ACTION_CODE_FATAL) {
                                mediametrics_setInt32(mMetricsHandle, kCodecError, err);
                                mediametrics_setCString(mMetricsHandle, kCodecErrorState,
                                                        stateString(mState).c_str());
                                flushMediametrics();
                                initMediametrics();

                                setState(UNINITIALIZED);
                            } else {
                                setState(
                                        (mFlags & kFlagIsAsync) ? FLUSHED : STARTED);
                            }
                            break;
                        }

                        case FLUSHED:
                        case STARTED:
                        {
                            sendErrorResponse = (mReplyID != nullptr);

                            setStickyError(err);
                            postActivityNotificationIfPossible();

                            cancelPendingDequeueOperations();

                            if (mFlags & kFlagIsAsync) {
                                onError(err, actionCode);
                            }
                            switch (actionCode) {
                            case ACTION_CODE_TRANSIENT:
                                break;
                            case ACTION_CODE_RECOVERABLE:
                                setState(INITIALIZED);
                                break;
                            default:
                                mediametrics_setInt32(mMetricsHandle, kCodecError, err);
                                mediametrics_setCString(mMetricsHandle, kCodecErrorState,
                                                        stateString(mState).c_str());
                                flushMediametrics();
                                initMediametrics();
                                setState(UNINITIALIZED);
                                break;
                            }
                            break;
                        }

                        default:
                        {
                            sendErrorResponse = (mReplyID != nullptr);

                            setStickyError(err);
                            postActivityNotificationIfPossible();

                            // actionCode in an uninitialized state is always fatal.
                            if (mState == UNINITIALIZED) {
                                actionCode = ACTION_CODE_FATAL;
                            }
                            if (mFlags & kFlagIsAsync) {
                                onError(err, actionCode);
                            }
                            switch (actionCode) {
                            case ACTION_CODE_TRANSIENT:
                                break;
                            case ACTION_CODE_RECOVERABLE:
                                setState(INITIALIZED);
                                break;
                            default:
                                setState(UNINITIALIZED);
                                break;
                            }
                            break;
                        }
                    }

                    if (sendErrorResponse) {
                        // TRICKY: replicate PostReplyWithError logic for
                        //         err code override
                        int32_t finalErr = err;
                        if (mReleasedByResourceManager) {
                            // override the err code if MediaCodec has been
                            // released by ResourceManager.
                            finalErr = DEAD_OBJECT;
                        }
                        postPendingRepliesAndDeferredMessages(origin, finalErr);
                    }
                    break;
                }

                case kWhatComponentAllocated:
                {
                    if (mState == RELEASING || mState == UNINITIALIZED) {
                        // In case a kWhatError or kWhatRelease message came in and replied,
                        // we log a warning and ignore.
                        ALOGW("allocate interrupted by error or release, current state %d",
                              mState);
                        break;
                    }
                    CHECK_EQ(mState, INITIALIZING);
                    setState(INITIALIZED);
                    mFlags |= kFlagIsComponentAllocated;

                    CHECK(msg->findString("componentName", &mComponentName));

                    if (mComponentName.c_str()) {
                        mediametrics_setCString(mMetricsHandle, kCodecCodec,
                                                mComponentName.c_str());
                    }

                    const char *owner = mCodecInfo ? mCodecInfo->getOwnerName() : "";
                    if (mComponentName.startsWith("OMX.google.")
                            && strncmp(owner, "default", 8) == 0) {
                        mFlags |= kFlagUsesSoftwareRenderer;
                    } else {
                        mFlags &= ~kFlagUsesSoftwareRenderer;
                    }
                    mOwnerName = owner;

                    if (mComponentName.endsWith(".secure")) {
                        mFlags |= kFlagIsSecure;
                        mediametrics_setInt32(mMetricsHandle, kCodecSecure, 1);
                    } else {
                        mFlags &= ~kFlagIsSecure;
                        mediametrics_setInt32(mMetricsHandle, kCodecSecure, 0);
                    }

                    if (mIsVideo) {
                        // audio codec is currently ignored.
                        mResourceManagerProxy->addResource(
                                MediaResource::CodecResource(mFlags & kFlagIsSecure, mIsVideo));
                    }

                    postPendingRepliesAndDeferredMessages("kWhatComponentAllocated");
                    break;
                }

                case kWhatComponentConfigured:
                {
                    if (mState == RELEASING || mState == UNINITIALIZED || mState == INITIALIZED) {
                        // In case a kWhatError or kWhatRelease message came in and replied,
                        // we log a warning and ignore.
                        ALOGW("configure interrupted by error or release, current state %d",
                              mState);
                        break;
                    }
                    CHECK_EQ(mState, CONFIGURING);

                    // reset input surface flag
                    mHaveInputSurface = false;

                    CHECK(msg->findMessage("input-format", &mInputFormat));
                    CHECK(msg->findMessage("output-format", &mOutputFormat));

                    // limit to confirming the opt-in behavior to minimize any behavioral change
                    if (mSurface != nullptr && !mAllowFrameDroppingBySurface) {
                        // signal frame dropping mode in the input format as this may also be
                        // meaningful and confusing for an encoder in a transcoder scenario
                        mInputFormat->setInt32("allow-frame-drop", mAllowFrameDroppingBySurface);
                    }
                    sp<AMessage> interestingFormat =
                            (mFlags & kFlagIsEncoder) ? mOutputFormat : mInputFormat;
                    ALOGV("[%s] configured as input format: %s, output format: %s",
                            mComponentName.c_str(),
                            mInputFormat->debugString(4).c_str(),
                            mOutputFormat->debugString(4).c_str());
                    int32_t usingSwRenderer;
                    if (mOutputFormat->findInt32("using-sw-renderer", &usingSwRenderer)
                            && usingSwRenderer) {
                        mFlags |= kFlagUsesSoftwareRenderer;
                    }
                    setState(CONFIGURED);
                    postPendingRepliesAndDeferredMessages("kWhatComponentConfigured");

                    // augment our media metrics info, now that we know more things
                    // such as what the codec extracted from any CSD passed in.
                    if (mMetricsHandle != 0) {
                        sp<AMessage> format;
                        if (mConfigureMsg != NULL &&
                            mConfigureMsg->findMessage("format", &format)) {
                                // format includes: mime
                                AString mime;
                                if (format->findString("mime", &mime)) {
                                    mediametrics_setCString(mMetricsHandle, kCodecMime,
                                                            mime.c_str());
                                }
                            }
                        // perhaps video only?
                        int32_t profile = 0;
                        if (interestingFormat->findInt32("profile", &profile)) {
                            mediametrics_setInt32(mMetricsHandle, kCodecProfile, profile);
                        }
                        int32_t level = 0;
                        if (interestingFormat->findInt32("level", &level)) {
                            mediametrics_setInt32(mMetricsHandle, kCodecLevel, level);
                        }
                        // bitrate and bitrate mode, encoder only
                        if (mFlags & kFlagIsEncoder) {
                            // encoder specific values
                            int32_t bitrate_mode = -1;
                            if (mOutputFormat->findInt32(KEY_BITRATE_MODE, &bitrate_mode)) {
                                    mediametrics_setCString(mMetricsHandle, kCodecBitrateMode,
                                          asString_BitrateMode(bitrate_mode));
                            }
                            int32_t bitrate = -1;
                            if (mOutputFormat->findInt32(KEY_BIT_RATE, &bitrate)) {
                                    mediametrics_setInt32(mMetricsHandle, kCodecBitrate, bitrate);
                            }
                        } else {
                            // decoder specific values
                        }
                    }
                    break;
                }

                case kWhatInputSurfaceCreated:
                {
                    if (mState != CONFIGURED) {
                        // state transitioned unexpectedly; we should have replied already.
                        ALOGD("received kWhatInputSurfaceCreated message in state %s",
                                stateString(mState).c_str());
                        break;
                    }
                    // response to initiateCreateInputSurface()
                    status_t err = NO_ERROR;
                    sp<AMessage> response = new AMessage;
                    if (!msg->findInt32("err", &err)) {
                        sp<RefBase> obj;
                        msg->findObject("input-surface", &obj);
                        CHECK(msg->findMessage("input-format", &mInputFormat));
                        CHECK(msg->findMessage("output-format", &mOutputFormat));
                        ALOGV("[%s] input surface created as input format: %s, output format: %s",
                                mComponentName.c_str(),
                                mInputFormat->debugString(4).c_str(),
                                mOutputFormat->debugString(4).c_str());
                        CHECK(obj != NULL);
                        response->setObject("input-surface", obj);
                        mHaveInputSurface = true;
                    } else {
                        response->setInt32("err", err);
                    }
                    postPendingRepliesAndDeferredMessages("kWhatInputSurfaceCreated", response);
                    break;
                }

                case kWhatInputSurfaceAccepted:
                {
                    if (mState != CONFIGURED) {
                        // state transitioned unexpectedly; we should have replied already.
                        ALOGD("received kWhatInputSurfaceAccepted message in state %s",
                                stateString(mState).c_str());
                        break;
                    }
                    // response to initiateSetInputSurface()
                    status_t err = NO_ERROR;
                    sp<AMessage> response = new AMessage();
                    if (!msg->findInt32("err", &err)) {
                        CHECK(msg->findMessage("input-format", &mInputFormat));
                        CHECK(msg->findMessage("output-format", &mOutputFormat));
                        mHaveInputSurface = true;
                    } else {
                        response->setInt32("err", err);
                    }
                    postPendingRepliesAndDeferredMessages("kWhatInputSurfaceAccepted", response);
                    break;
                }

                case kWhatSignaledInputEOS:
                {
                    if (!isExecuting()) {
                        // state transitioned unexpectedly; we should have replied already.
                        ALOGD("received kWhatSignaledInputEOS message in state %s",
                                stateString(mState).c_str());
                        break;
                    }
                    // response to signalEndOfInputStream()
                    sp<AMessage> response = new AMessage;
                    status_t err;
                    if (msg->findInt32("err", &err)) {
                        response->setInt32("err", err);
                    }
                    postPendingRepliesAndDeferredMessages("kWhatSignaledInputEOS", response);
                    break;
                }

                case kWhatStartCompleted:
                {
                    if (mState == RELEASING || mState == UNINITIALIZED) {
                        // In case a kWhatRelease message came in and replied,
                        // we log a warning and ignore.
                        ALOGW("start interrupted by release, current state %d", mState);
                        break;
                    }

                    CHECK_EQ(mState, STARTING);
                    if (mIsVideo) {
                        mResourceManagerProxy->addResource(
                                MediaResource::GraphicMemoryResource(getGraphicBufferSize()));
                    }
                    setState(STARTED);
                    postPendingRepliesAndDeferredMessages("kWhatStartCompleted");
                    break;
                }

                case kWhatOutputBuffersChanged:
                {
                    mFlags |= kFlagOutputBuffersChanged;
                    postActivityNotificationIfPossible();
                    break;
                }

                case kWhatOutputFramesRendered:
                {
                    // ignore these in all states except running, and check that we have a
                    // notification set
                    if (mState == STARTED && mOnFrameRenderedNotification != NULL) {
                        sp<AMessage> notify = mOnFrameRenderedNotification->dup();
                        notify->setMessage("data", msg);
                        notify->post();
                    }
                    break;
                }

                case kWhatFillThisBuffer:
                {
                    /* size_t index = */updateBuffers(kPortIndexInput, msg);

                    if (mState == FLUSHING
                            || mState == STOPPING
                            || mState == RELEASING) {
                        returnBuffersToCodecOnPort(kPortIndexInput);
                        break;
                    }

                    if (!mCSD.empty()) {
                        ssize_t index = dequeuePortBuffer(kPortIndexInput);
                        CHECK_GE(index, 0);

                        // If codec specific data had been specified as
                        // part of the format in the call to configure and
                        // if there's more csd left, we submit it here
                        // clients only get access to input buffers once
                        // this data has been exhausted.

                        status_t err = queueCSDInputBuffer(index);

                        if (err != OK) {
                            ALOGE("queueCSDInputBuffer failed w/ error %d",
                                  err);

                            setStickyError(err);
                            postActivityNotificationIfPossible();

                            cancelPendingDequeueOperations();
                        }
                        break;
                    }
                    if (!mLeftover.empty()) {
                        ssize_t index = dequeuePortBuffer(kPortIndexInput);
                        CHECK_GE(index, 0);

                        status_t err = handleLeftover(index);
                        if (err != OK) {
                            setStickyError(err);
                            postActivityNotificationIfPossible();
                            cancelPendingDequeueOperations();
                        }
                        break;
                    }

                    if (mFlags & kFlagIsAsync) {
                        if (!mHaveInputSurface) {
                            if (mState == FLUSHED) {
                                mHavePendingInputBuffers = true;
                            } else {
                                onInputBufferAvailable();
                            }
                        }
                    } else if (mFlags & kFlagDequeueInputPending) {
                        CHECK(handleDequeueInputBuffer(mDequeueInputReplyID));

                        ++mDequeueInputTimeoutGeneration;
                        mFlags &= ~kFlagDequeueInputPending;
                        mDequeueInputReplyID = 0;
                    } else {
                        postActivityNotificationIfPossible();
                    }
                    break;
                }

                case kWhatDrainThisBuffer:
                {
                    /* size_t index = */updateBuffers(kPortIndexOutput, msg);

                    if (mState == FLUSHING
                            || mState == STOPPING
                            || mState == RELEASING) {
                        returnBuffersToCodecOnPort(kPortIndexOutput);
                        break;
                    }

                    if (mFlags & kFlagIsAsync) {
                        sp<RefBase> obj;
                        CHECK(msg->findObject("buffer", &obj));
                        sp<MediaCodecBuffer> buffer = static_cast<MediaCodecBuffer *>(obj.get());

                        // In asynchronous mode, output format change is processed immediately.
                        handleOutputFormatChangeIfNeeded(buffer);
                        onOutputBufferAvailable();
                    } else if (mFlags & kFlagDequeueOutputPending) {
                        CHECK(handleDequeueOutputBuffer(mDequeueOutputReplyID));

                        ++mDequeueOutputTimeoutGeneration;
                        mFlags &= ~kFlagDequeueOutputPending;
                        mDequeueOutputReplyID = 0;
                    } else {
                        postActivityNotificationIfPossible();
                    }

                    break;
                }

                case kWhatEOS:
                {
                    // We already notify the client of this by using the
                    // corresponding flag in "onOutputBufferReady".
                    break;
                }

                case kWhatStopCompleted:
                {
                    if (mState != STOPPING) {
                        ALOGW("Received kWhatStopCompleted in state %d", mState);
                        break;
                    }
                    setState(INITIALIZED);
                    if (mReplyID) {
                        postPendingRepliesAndDeferredMessages("kWhatStopCompleted");
                    } else {
                        ALOGW("kWhatStopCompleted: presumably an error occurred earlier, "
                              "but the operation completed anyway. (last reply origin=%s)",
                              mLastReplyOrigin.c_str());
                    }
                    break;
                }

                case kWhatReleaseCompleted:
                {
                    if (mState != RELEASING) {
                        ALOGW("Received kWhatReleaseCompleted in state %d", mState);
                        break;
                    }
                    setState(UNINITIALIZED);
                    mComponentName.clear();

                    mFlags &= ~kFlagIsComponentAllocated;

                    // off since we're removing all resources including the battery on
                    if (mBatteryChecker != nullptr) {
                        mBatteryChecker->onClientRemoved();
                    }

                    mResourceManagerProxy->removeClient();
                    mReleaseSurface.reset();

                    if (mReplyID != nullptr) {
                        postPendingRepliesAndDeferredMessages("kWhatReleaseCompleted");
                    }
                    if (mAsyncReleaseCompleteNotification != nullptr) {
                        flushMediametrics();
                        mAsyncReleaseCompleteNotification->post();
                        mAsyncReleaseCompleteNotification.clear();
                    }
                    break;
                }

                case kWhatFlushCompleted:
                {
                    if (mState != FLUSHING) {
                        ALOGW("received FlushCompleted message in state %d",
                                mState);
                        break;
                    }

                    if (mFlags & kFlagIsAsync) {
                        setState(FLUSHED);
                    } else {
                        setState(STARTED);
                        mCodec->signalResume();
                    }

                    postPendingRepliesAndDeferredMessages("kWhatFlushCompleted");
                    break;
                }

                default:
                    TRESPASS();
            }
            break;
        }

        case kWhatInit:
        {
            if (mState != UNINITIALIZED) {
                PostReplyWithError(msg, INVALID_OPERATION);
                break;
            }

            if (mReplyID) {
                mDeferredMessages.push_back(msg);
                break;
            }
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            mReplyID = replyID;
            setState(INITIALIZING);

            sp<RefBase> codecInfo;
            (void)msg->findObject("codecInfo", &codecInfo);
            AString name;
            CHECK(msg->findString("name", &name));

            sp<AMessage> format = new AMessage;
            if (codecInfo) {
                format->setObject("codecInfo", codecInfo);
            }
            format->setString("componentName", name);

            mCodec->initiateAllocateComponent(format);
            break;
        }

        case kWhatSetNotification:
        {
            sp<AMessage> notify;
            if (msg->findMessage("on-frame-rendered", &notify)) {
                mOnFrameRenderedNotification = notify;
            }
            break;
        }

        case kWhatSetCallback:
        {
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            if (mState == UNINITIALIZED
                    || mState == INITIALIZING
                    || isExecuting()) {
                // callback can't be set after codec is executing,
                // or before it's initialized (as the callback
                // will be cleared when it goes to INITIALIZED)
                PostReplyWithError(replyID, INVALID_OPERATION);
                break;
            }

            sp<AMessage> callback;
            CHECK(msg->findMessage("callback", &callback));

            mCallback = callback;

            if (mCallback != NULL) {
                ALOGI("MediaCodec will operate in async mode");
                mFlags |= kFlagIsAsync;
            } else {
                mFlags &= ~kFlagIsAsync;
            }

            sp<AMessage> response = new AMessage;
            response->postReply(replyID);
            break;
        }

        case kWhatConfigure:
        {
            if (mState != INITIALIZED) {
                PostReplyWithError(msg, INVALID_OPERATION);
                break;
            }

            if (mReplyID) {
                mDeferredMessages.push_back(msg);
                break;
            }
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            sp<RefBase> obj;
            CHECK(msg->findObject("surface", &obj));

            sp<AMessage> format;
            CHECK(msg->findMessage("format", &format));

            int32_t push;
            if (msg->findInt32("push-blank-buffers-on-shutdown", &push) && push != 0) {
                mFlags |= kFlagPushBlankBuffersOnShutdown;
            }

            if (obj != NULL) {
                if (!format->findInt32("allow-frame-drop", &mAllowFrameDroppingBySurface)) {
                    // allow frame dropping by surface by default
                    mAllowFrameDroppingBySurface = true;
                }

                format->setObject("native-window", obj);
                status_t err = handleSetSurface(static_cast<Surface *>(obj.get()));
                if (err != OK) {
                    PostReplyWithError(replyID, err);
                    break;
                }
            } else {
                // we are not using surface so this variable is not used, but initialize sensibly anyway
                mAllowFrameDroppingBySurface = false;

                handleSetSurface(NULL);
            }

            uint32_t flags;
            CHECK(msg->findInt32("flags", (int32_t *)&flags));
            if (flags & CONFIGURE_FLAG_USE_BLOCK_MODEL) {
                if (!(mFlags & kFlagIsAsync)) {
                    PostReplyWithError(replyID, INVALID_OPERATION);
                    break;
                }
                mFlags |= kFlagUseBlockModel;
            }
            mReplyID = replyID;
            setState(CONFIGURING);

            void *crypto;
            if (!msg->findPointer("crypto", &crypto)) {
                crypto = NULL;
            }

            ALOGV("kWhatConfigure: Old mCrypto: %p (%d)",
                    mCrypto.get(), (mCrypto != NULL ? mCrypto->getStrongCount() : 0));

            mCrypto = static_cast<ICrypto *>(crypto);
            mBufferChannel->setCrypto(mCrypto);

            ALOGV("kWhatConfigure: New mCrypto: %p (%d)",
                    mCrypto.get(), (mCrypto != NULL ? mCrypto->getStrongCount() : 0));

            void *descrambler;
            if (!msg->findPointer("descrambler", &descrambler)) {
                descrambler = NULL;
            }

            mDescrambler = static_cast<IDescrambler *>(descrambler);
            mBufferChannel->setDescrambler(mDescrambler);

            format->setInt32("flags", flags);
            if (flags & CONFIGURE_FLAG_ENCODE) {
                format->setInt32("encoder", true);
                mFlags |= kFlagIsEncoder;
            }

            extractCSD(format);

            mCodec->initiateConfigureComponent(format);
            break;
        }

        case kWhatSetSurface:
        {
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            status_t err = OK;

            switch (mState) {
                case CONFIGURED:
                case STARTED:
                case FLUSHED:
                {
                    sp<RefBase> obj;
                    (void)msg->findObject("surface", &obj);
                    sp<Surface> surface = static_cast<Surface *>(obj.get());
                    if (mSurface == NULL) {
                        // do not support setting surface if it was not set
                        err = INVALID_OPERATION;
                    } else if (obj == NULL) {
                        // do not support unsetting surface
                        err = BAD_VALUE;
                    } else {
                        err = connectToSurface(surface);
                        if (err == ALREADY_EXISTS) {
                            // reconnecting to same surface
                            err = OK;
                        } else {
                            if (err == OK) {
                                if (mFlags & kFlagUsesSoftwareRenderer) {
                                    if (mSoftRenderer != NULL
                                            && (mFlags & kFlagPushBlankBuffersOnShutdown)) {
                                        pushBlankBuffersToNativeWindow(mSurface.get());
                                    }
                                    surface->setDequeueTimeout(-1);
                                    mSoftRenderer = new SoftwareRenderer(surface);
                                    // TODO: check if this was successful
                                } else {
                                    err = mCodec->setSurface(surface);
                                }
                            }
                            if (err == OK) {
                                (void)disconnectFromSurface();
                                mSurface = surface;
                            }
                        }
                    }
                    break;
                }

                default:
                    err = INVALID_OPERATION;
                    break;
            }

            PostReplyWithError(replyID, err);
            break;
        }

        case kWhatCreateInputSurface:
        case kWhatSetInputSurface:
        {
            // Must be configured, but can't have been started yet.
            if (mState != CONFIGURED) {
                PostReplyWithError(msg, INVALID_OPERATION);
                break;
            }

            if (mReplyID) {
                mDeferredMessages.push_back(msg);
                break;
            }
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            mReplyID = replyID;
            if (msg->what() == kWhatCreateInputSurface) {
                mCodec->initiateCreateInputSurface();
            } else {
                sp<RefBase> obj;
                CHECK(msg->findObject("input-surface", &obj));

                mCodec->initiateSetInputSurface(
                        static_cast<PersistentSurface *>(obj.get()));
            }
            break;
        }
        case kWhatStart:
        {
            if (mState == FLUSHED) {
                setState(STARTED);
                if (mHavePendingInputBuffers) {
                    onInputBufferAvailable();
                    mHavePendingInputBuffers = false;
                }
                mCodec->signalResume();
                PostReplyWithError(msg, OK);
                break;
            } else if (mState != CONFIGURED) {
                PostReplyWithError(msg, INVALID_OPERATION);
                break;
            }

            if (mReplyID) {
                mDeferredMessages.push_back(msg);
                break;
            }
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            mReplyID = replyID;
            setState(STARTING);

            mCodec->initiateStart();
            break;
        }

        case kWhatStop: {
            if (mReplyID) {
                mDeferredMessages.push_back(msg);
                break;
            }
            [[fallthrough]];
        }
        case kWhatRelease:
        {
            State targetState =
                (msg->what() == kWhatStop) ? INITIALIZED : UNINITIALIZED;

            if ((mState == RELEASING && targetState == UNINITIALIZED)
                    || (mState == STOPPING && targetState == INITIALIZED)) {
                mDeferredMessages.push_back(msg);
                break;
            }

            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            sp<AMessage> asyncNotify;
            (void)msg->findMessage("async", &asyncNotify);
            // post asyncNotify if going out of scope.
            struct AsyncNotifyPost {
                AsyncNotifyPost(const sp<AMessage> &asyncNotify) : mAsyncNotify(asyncNotify) {}
                ~AsyncNotifyPost() {
                    if (mAsyncNotify) {
                        mAsyncNotify->post();
                    }
                }
                void clear() { mAsyncNotify.clear(); }
            private:
                sp<AMessage> mAsyncNotify;
            } asyncNotifyPost{asyncNotify};

            // already stopped/released
            if (mState == UNINITIALIZED && mReleasedByResourceManager) {
                sp<AMessage> response = new AMessage;
                response->setInt32("err", OK);
                response->postReply(replyID);
                break;
            }

            int32_t reclaimed = 0;
            msg->findInt32("reclaimed", &reclaimed);
            if (reclaimed) {
                mReleasedByResourceManager = true;

                int32_t force = 0;
                msg->findInt32("force", &force);
                if (!force && hasPendingBuffer()) {
                    ALOGW("Can't reclaim codec right now due to pending buffers.");

                    // return WOULD_BLOCK to ask resource manager to retry later.
                    sp<AMessage> response = new AMessage;
                    response->setInt32("err", WOULD_BLOCK);
                    response->postReply(replyID);

                    // notify the async client
                    if (mFlags & kFlagIsAsync) {
                        onError(DEAD_OBJECT, ACTION_CODE_FATAL);
                    }
                    break;
                }
            }

            bool isReleasingAllocatedComponent =
                    (mFlags & kFlagIsComponentAllocated) && targetState == UNINITIALIZED;
            if (!isReleasingAllocatedComponent // See 1
                    && mState != INITIALIZED
                    && mState != CONFIGURED && !isExecuting()) {
                // 1) Permit release to shut down the component if allocated.
                //
                // 2) We may be in "UNINITIALIZED" state already and
                // also shutdown the encoder/decoder without the
                // client being aware of this if media server died while
                // we were being stopped. The client would assume that
                // after stop() returned, it would be safe to call release()
                // and it should be in this case, no harm to allow a release()
                // if we're already uninitialized.
                sp<AMessage> response = new AMessage;
                // TODO: we shouldn't throw an exception for stop/release. Change this to wait until
                // the previous stop/release completes and then reply with OK.
                status_t err = mState == targetState ? OK : INVALID_OPERATION;
                response->setInt32("err", err);
                if (err == OK && targetState == UNINITIALIZED) {
                    mComponentName.clear();
                }
                response->postReply(replyID);
                break;
            }

            // If we're flushing, stopping, configuring or starting  but
            // received a release request, post the reply for the pending call
            // first, and consider it done. The reply token will be replaced
            // after this, and we'll no longer be able to reply.
            if (mState == FLUSHING || mState == STOPPING
                    || mState == CONFIGURING || mState == STARTING) {
                // mReply is always set if in these states.
                postPendingRepliesAndDeferredMessages(
                        std::string("kWhatRelease:") + stateString(mState));
            }

            if (mFlags & kFlagSawMediaServerDie) {
                // It's dead, Jim. Don't expect initiateShutdown to yield
                // any useful results now...
                // Any pending reply would have been handled at kWhatError.
                setState(UNINITIALIZED);
                if (targetState == UNINITIALIZED) {
                    mComponentName.clear();
                }
                (new AMessage)->postReply(replyID);
                break;
            }

            // If we already have an error, component may not be able to
            // complete the shutdown properly. If we're stopping, post the
            // reply now with an error to unblock the client, client can
            // release after the failure (instead of ANR).
            if (msg->what() == kWhatStop && (mFlags & kFlagStickyError)) {
                // Any pending reply would have been handled at kWhatError.
                PostReplyWithError(replyID, getStickyError());
                break;
            }

            if (asyncNotify != nullptr) {
                if (mSurface != NULL) {
                    if (!mReleaseSurface) {
                        uint64_t usage = 0;
                        if (mSurface->getConsumerUsage(&usage) != OK) {
                            usage = 0;
                        }
                        mReleaseSurface.reset(new ReleaseSurface(usage));
                    }
                    if (mSurface != mReleaseSurface->getSurface()) {
                        status_t err = connectToSurface(mReleaseSurface->getSurface());
                        ALOGW_IF(err != OK, "error connecting to release surface: err = %d", err);
                        if (err == OK && !(mFlags & kFlagUsesSoftwareRenderer)) {
                            err = mCodec->setSurface(mReleaseSurface->getSurface());
                            ALOGW_IF(err != OK, "error setting release surface: err = %d", err);
                        }
                        if (err == OK) {
                            (void)disconnectFromSurface();
                            mSurface = mReleaseSurface->getSurface();
                        }
                    }
                }
            }

            if (mReplyID) {
                // State transition replies are handled above, so this reply
                // would not be related to state transition. As we are
                // shutting down the component, just fail the operation.
                postPendingRepliesAndDeferredMessages("kWhatRelease:reply", UNKNOWN_ERROR);
            }
            mReplyID = replyID;
            setState(msg->what() == kWhatStop ? STOPPING : RELEASING);

            mCodec->initiateShutdown(
                    msg->what() == kWhatStop /* keepComponentAllocated */);

            returnBuffersToCodec(reclaimed);

            if (mSoftRenderer != NULL && (mFlags & kFlagPushBlankBuffersOnShutdown)) {
                pushBlankBuffersToNativeWindow(mSurface.get());
            }

            if (asyncNotify != nullptr) {
                mResourceManagerProxy->markClientForPendingRemoval();
                postPendingRepliesAndDeferredMessages("kWhatRelease:async");
                asyncNotifyPost.clear();
                mAsyncReleaseCompleteNotification = asyncNotify;
            }

            break;
        }

        case kWhatDequeueInputBuffer:
        {
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            if (mFlags & kFlagIsAsync) {
                ALOGE("dequeueInputBuffer can't be used in async mode");
                PostReplyWithError(replyID, INVALID_OPERATION);
                break;
            }

            if (mHaveInputSurface) {
                ALOGE("dequeueInputBuffer can't be used with input surface");
                PostReplyWithError(replyID, INVALID_OPERATION);
                break;
            }

            if (handleDequeueInputBuffer(replyID, true /* new request */)) {
                break;
            }

            int64_t timeoutUs;
            CHECK(msg->findInt64("timeoutUs", &timeoutUs));

            if (timeoutUs == 0LL) {
                PostReplyWithError(replyID, -EAGAIN);
                break;
            }

            mFlags |= kFlagDequeueInputPending;
            mDequeueInputReplyID = replyID;

            if (timeoutUs > 0LL) {
                sp<AMessage> timeoutMsg =
                    new AMessage(kWhatDequeueInputTimedOut, this);
                timeoutMsg->setInt32(
                        "generation", ++mDequeueInputTimeoutGeneration);
                timeoutMsg->post(timeoutUs);
            }
            break;
        }

        case kWhatDequeueInputTimedOut:
        {
            int32_t generation;
            CHECK(msg->findInt32("generation", &generation));

            if (generation != mDequeueInputTimeoutGeneration) {
                // Obsolete
                break;
            }

            CHECK(mFlags & kFlagDequeueInputPending);

            PostReplyWithError(mDequeueInputReplyID, -EAGAIN);

            mFlags &= ~kFlagDequeueInputPending;
            mDequeueInputReplyID = 0;
            break;
        }

        case kWhatQueueInputBuffer:
        {
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            if (!isExecuting()) {
                PostReplyWithError(replyID, INVALID_OPERATION);
                break;
            } else if (mFlags & kFlagStickyError) {
                PostReplyWithError(replyID, getStickyError());
                break;
            }

            status_t err = UNKNOWN_ERROR;
            if (!mLeftover.empty()) {
                mLeftover.push_back(msg);
                size_t index;
                msg->findSize("index", &index);
                err = handleLeftover(index);
            } else {
                err = onQueueInputBuffer(msg);
            }

            PostReplyWithError(replyID, err);
            break;
        }

        case kWhatDequeueOutputBuffer:
        {
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            if (mFlags & kFlagIsAsync) {
                ALOGE("dequeueOutputBuffer can't be used in async mode");
                PostReplyWithError(replyID, INVALID_OPERATION);
                break;
            }

            if (handleDequeueOutputBuffer(replyID, true /* new request */)) {
                break;
            }

            int64_t timeoutUs;
            CHECK(msg->findInt64("timeoutUs", &timeoutUs));

            if (timeoutUs == 0LL) {
                PostReplyWithError(replyID, -EAGAIN);
                break;
            }

            mFlags |= kFlagDequeueOutputPending;
            mDequeueOutputReplyID = replyID;

            if (timeoutUs > 0LL) {
                sp<AMessage> timeoutMsg =
                    new AMessage(kWhatDequeueOutputTimedOut, this);
                timeoutMsg->setInt32(
                        "generation", ++mDequeueOutputTimeoutGeneration);
                timeoutMsg->post(timeoutUs);
            }
            break;
        }

        case kWhatDequeueOutputTimedOut:
        {
            int32_t generation;
            CHECK(msg->findInt32("generation", &generation));

            if (generation != mDequeueOutputTimeoutGeneration) {
                // Obsolete
                break;
            }

            CHECK(mFlags & kFlagDequeueOutputPending);

            PostReplyWithError(mDequeueOutputReplyID, -EAGAIN);

            mFlags &= ~kFlagDequeueOutputPending;
            mDequeueOutputReplyID = 0;
            break;
        }

        case kWhatReleaseOutputBuffer:
        {
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            if (!isExecuting()) {
                PostReplyWithError(replyID, INVALID_OPERATION);
                break;
            } else if (mFlags & kFlagStickyError) {
                PostReplyWithError(replyID, getStickyError());
                break;
            }

            status_t err = onReleaseOutputBuffer(msg);

            PostReplyWithError(replyID, err);
            break;
        }

        case kWhatSignalEndOfInputStream:
        {
            if (!isExecuting() || !mHaveInputSurface) {
                PostReplyWithError(msg, INVALID_OPERATION);
                break;
            } else if (mFlags & kFlagStickyError) {
                PostReplyWithError(msg, getStickyError());
                break;
            }

            if (mReplyID) {
                mDeferredMessages.push_back(msg);
                break;
            }
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            mReplyID = replyID;
            mCodec->signalEndOfInputStream();
            break;
        }

        case kWhatGetBuffers:
        {
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));
            if (!isExecuting() || (mFlags & kFlagIsAsync)) {
                PostReplyWithError(replyID, INVALID_OPERATION);
                break;
            } else if (mFlags & kFlagStickyError) {
                PostReplyWithError(replyID, getStickyError());
                break;
            }

            int32_t portIndex;
            CHECK(msg->findInt32("portIndex", &portIndex));

            Vector<sp<MediaCodecBuffer> > *dstBuffers;
            CHECK(msg->findPointer("buffers", (void **)&dstBuffers));

            dstBuffers->clear();
            // If we're using input surface (either non-persistent created by
            // createInputSurface(), or persistent set by setInputSurface()),
            // give the client an empty input buffers array.
            if (portIndex != kPortIndexInput || !mHaveInputSurface) {
                if (portIndex == kPortIndexInput) {
                    mBufferChannel->getInputBufferArray(dstBuffers);
                } else {
                    mBufferChannel->getOutputBufferArray(dstBuffers);
                }
            }

            (new AMessage)->postReply(replyID);
            break;
        }

        case kWhatFlush:
        {
            if (!isExecuting()) {
                PostReplyWithError(msg, INVALID_OPERATION);
                break;
            } else if (mFlags & kFlagStickyError) {
                PostReplyWithError(msg, getStickyError());
                break;
            }

            if (mReplyID) {
                mDeferredMessages.push_back(msg);
                break;
            }
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            mReplyID = replyID;
            // TODO: skip flushing if already FLUSHED
            setState(FLUSHING);

            mCodec->signalFlush();
            returnBuffersToCodec();
            break;
        }

        case kWhatGetInputFormat:
        case kWhatGetOutputFormat:
        {
            sp<AMessage> format =
                (msg->what() == kWhatGetOutputFormat ? mOutputFormat : mInputFormat);

            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            if ((mState != CONFIGURED && mState != STARTING &&
                 mState != STARTED && mState != FLUSHING &&
                 mState != FLUSHED)
                    || format == NULL) {
                PostReplyWithError(replyID, INVALID_OPERATION);
                break;
            } else if (mFlags & kFlagStickyError) {
                PostReplyWithError(replyID, getStickyError());
                break;
            }

            sp<AMessage> response = new AMessage;
            response->setMessage("format", format);
            response->postReply(replyID);
            break;
        }

        case kWhatRequestIDRFrame:
        {
            mCodec->signalRequestIDRFrame();
            break;
        }

        case kWhatRequestActivityNotification:
        {
            CHECK(mActivityNotify == NULL);
            CHECK(msg->findMessage("notify", &mActivityNotify));

            postActivityNotificationIfPossible();
            break;
        }

        case kWhatGetName:
        {
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            if (mComponentName.empty()) {
                PostReplyWithError(replyID, INVALID_OPERATION);
                break;
            }

            sp<AMessage> response = new AMessage;
            response->setString("name", mComponentName.c_str());
            response->postReply(replyID);
            break;
        }

        case kWhatGetCodecInfo:
        {
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            sp<AMessage> response = new AMessage;
            response->setObject("codecInfo", mCodecInfo);
            response->postReply(replyID);
            break;
        }

        case kWhatSetParameters:
        {
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            sp<AMessage> params;
            CHECK(msg->findMessage("params", &params));

            status_t err = onSetParameters(params);

            PostReplyWithError(replyID, err);
            break;
        }

        case kWhatDrmReleaseCrypto:
        {
            onReleaseCrypto(msg);
            break;
        }

        case kWhatCheckBatteryStats:
        {
            if (mBatteryChecker != nullptr) {
                mBatteryChecker->onCheckBatteryTimer(msg, [this] () {
                    mResourceManagerProxy->removeResource(
                            MediaResource::VideoBatteryResource());
                });
            }
            break;
        }

        default:
            TRESPASS();
    }
}

void MediaCodec::handleOutputFormatChangeIfNeeded(const sp<MediaCodecBuffer> &buffer) {
    sp<AMessage> format = buffer->format();
    if (mOutputFormat == format) {
        return;
    }
    if (mFlags & kFlagUseBlockModel) {
        sp<AMessage> diff1 = mOutputFormat->changesFrom(format);
        sp<AMessage> diff2 = format->changesFrom(mOutputFormat);
        std::set<std::string> keys;
        size_t numEntries = diff1->countEntries();
        AMessage::Type type;
        for (size_t i = 0; i < numEntries; ++i) {
            keys.emplace(diff1->getEntryNameAt(i, &type));
        }
        numEntries = diff2->countEntries();
        for (size_t i = 0; i < numEntries; ++i) {
            keys.emplace(diff2->getEntryNameAt(i, &type));
        }
        sp<WrapperObject<std::set<std::string>>> changedKeys{
            new WrapperObject<std::set<std::string>>{std::move(keys)}};
        buffer->meta()->setObject("changedKeys", changedKeys);
    }
    mOutputFormat = format;
    ALOGV("[%s] output format changed to: %s",
            mComponentName.c_str(), mOutputFormat->debugString(4).c_str());

    if (mSoftRenderer == NULL &&
            mSurface != NULL &&
            (mFlags & kFlagUsesSoftwareRenderer)) {
        AString mime;
        CHECK(mOutputFormat->findString("mime", &mime));

        // TODO: propagate color aspects to software renderer to allow better
        // color conversion to RGB. For now, just mark dataspace for YUV
        // rendering.
        int32_t dataSpace;
        if (mOutputFormat->findInt32("android._dataspace", &dataSpace)) {
            ALOGD("[%s] setting dataspace on output surface to #%x",
                    mComponentName.c_str(), dataSpace);
            int err = native_window_set_buffers_data_space(
                    mSurface.get(), (android_dataspace)dataSpace);
            ALOGW_IF(err != 0, "failed to set dataspace on surface (%d)", err);
        }
        if (mOutputFormat->contains("hdr-static-info")) {
            HDRStaticInfo info;
            if (ColorUtils::getHDRStaticInfoFromFormat(mOutputFormat, &info)) {
                setNativeWindowHdrMetadata(mSurface.get(), &info);
            }
        }

        sp<ABuffer> hdr10PlusInfo;
        if (mOutputFormat->findBuffer("hdr10-plus-info", &hdr10PlusInfo)
                && hdr10PlusInfo != nullptr && hdr10PlusInfo->size() > 0) {
            native_window_set_buffers_hdr10_plus_metadata(mSurface.get(),
                    hdr10PlusInfo->size(), hdr10PlusInfo->data());
        }

        if (mime.startsWithIgnoreCase("video/")) {
            mSurface->setDequeueTimeout(-1);
            mSoftRenderer = new SoftwareRenderer(mSurface, mRotationDegrees);
        }
    }

    requestCpuBoostIfNeeded();

    if (mFlags & kFlagIsEncoder) {
        // Before we announce the format change we should
        // collect codec specific data and amend the output
        // format as necessary.
        int32_t flags = 0;
        (void) buffer->meta()->findInt32("flags", &flags);
        if ((flags & BUFFER_FLAG_CODECCONFIG) && !(mFlags & kFlagIsSecure)) {
            status_t err =
                amendOutputFormatWithCodecSpecificData(buffer);

            if (err != OK) {
                ALOGE("Codec spit out malformed codec "
                      "specific data!");
            }
        }
    }
    if (mFlags & kFlagIsAsync) {
        onOutputFormatChanged();
    } else {
        mFlags |= kFlagOutputFormatChanged;
        postActivityNotificationIfPossible();
    }

    // Notify mCrypto of video resolution changes
    if (mCrypto != NULL) {
        int32_t left, top, right, bottom, width, height;
        if (mOutputFormat->findRect("crop", &left, &top, &right, &bottom)) {
            mCrypto->notifyResolution(right - left + 1, bottom - top + 1);
        } else if (mOutputFormat->findInt32("width", &width)
                && mOutputFormat->findInt32("height", &height)) {
            mCrypto->notifyResolution(width, height);
        }
    }
}

void MediaCodec::extractCSD(const sp<AMessage> &format) {
    mCSD.clear();

    size_t i = 0;
    for (;;) {
        sp<ABuffer> csd;
        if (!format->findBuffer(AStringPrintf("csd-%u", i).c_str(), &csd)) {
            break;
        }
        if (csd->size() == 0) {
            ALOGW("csd-%zu size is 0", i);
        }

        mCSD.push_back(csd);
        ++i;
    }

    ALOGV("Found %zu pieces of codec specific data.", mCSD.size());
}

status_t MediaCodec::queueCSDInputBuffer(size_t bufferIndex) {
    CHECK(!mCSD.empty());

    sp<ABuffer> csd = *mCSD.begin();
    mCSD.erase(mCSD.begin());
    std::shared_ptr<C2Buffer> c2Buffer;
    sp<hardware::HidlMemory> memory;
    size_t offset = 0;

    if (mFlags & kFlagUseBlockModel) {
        if (hasCryptoOrDescrambler()) {
            constexpr size_t kInitialDealerCapacity = 1048576;  // 1MB
            thread_local sp<MemoryDealer> sDealer = new MemoryDealer(
                    kInitialDealerCapacity, "CSD(1MB)");
            sp<IMemory> mem = sDealer->allocate(csd->size());
            if (mem == nullptr) {
                size_t newDealerCapacity = sDealer->getMemoryHeap()->getSize() * 2;
                while (csd->size() * 2 > newDealerCapacity) {
                    newDealerCapacity *= 2;
                }
                sDealer = new MemoryDealer(
                        newDealerCapacity,
                        AStringPrintf("CSD(%dMB)", newDealerCapacity / 1048576).c_str());
                mem = sDealer->allocate(csd->size());
            }
            memcpy(mem->unsecurePointer(), csd->data(), csd->size());
            ssize_t heapOffset;
            memory = hardware::fromHeap(mem->getMemory(&heapOffset, nullptr));
            offset += heapOffset;
        } else {
            std::shared_ptr<C2LinearBlock> block =
                FetchLinearBlock(csd->size(), {std::string{mComponentName.c_str()}});
            C2WriteView view{block->map().get()};
            if (view.error() != C2_OK) {
                return -EINVAL;
            }
            if (csd->size() > view.capacity()) {
                return -EINVAL;
            }
            memcpy(view.base(), csd->data(), csd->size());
            c2Buffer = C2Buffer::CreateLinearBuffer(block->share(0, csd->size(), C2Fence{}));
        }
    } else {
        const BufferInfo &info = mPortBuffers[kPortIndexInput][bufferIndex];
        const sp<MediaCodecBuffer> &codecInputData = info.mData;

        if (csd->size() > codecInputData->capacity()) {
            return -EINVAL;
        }
        if (codecInputData->data() == NULL) {
            ALOGV("Input buffer %zu is not properly allocated", bufferIndex);
            return -EINVAL;
        }

        memcpy(codecInputData->data(), csd->data(), csd->size());
    }

    AString errorDetailMsg;

    sp<AMessage> msg = new AMessage(kWhatQueueInputBuffer, this);
    msg->setSize("index", bufferIndex);
    msg->setSize("offset", 0);
    msg->setSize("size", csd->size());
    msg->setInt64("timeUs", 0LL);
    msg->setInt32("flags", BUFFER_FLAG_CODECCONFIG);
    msg->setPointer("errorDetailMsg", &errorDetailMsg);
    if (c2Buffer) {
        sp<WrapperObject<std::shared_ptr<C2Buffer>>> obj{
            new WrapperObject<std::shared_ptr<C2Buffer>>{c2Buffer}};
        msg->setObject("c2buffer", obj);
        msg->setMessage("tunings", new AMessage);
    } else if (memory) {
        sp<WrapperObject<sp<hardware::HidlMemory>>> obj{
            new WrapperObject<sp<hardware::HidlMemory>>{memory}};
        msg->setObject("memory", obj);
        msg->setMessage("tunings", new AMessage);
    }

    return onQueueInputBuffer(msg);
}

void MediaCodec::setState(State newState) {
    if (newState == INITIALIZED || newState == UNINITIALIZED) {
        delete mSoftRenderer;
        mSoftRenderer = NULL;

        if ( mCrypto != NULL ) {
            ALOGV("setState: ~mCrypto: %p (%d)",
                    mCrypto.get(), (mCrypto != NULL ? mCrypto->getStrongCount() : 0));
        }
        mCrypto.clear();
        mDescrambler.clear();
        handleSetSurface(NULL);

        mInputFormat.clear();
        mOutputFormat.clear();
        mFlags &= ~kFlagOutputFormatChanged;
        mFlags &= ~kFlagOutputBuffersChanged;
        mFlags &= ~kFlagStickyError;
        mFlags &= ~kFlagIsEncoder;
        mFlags &= ~kFlagIsAsync;
        mStickyError = OK;

        mActivityNotify.clear();
        mCallback.clear();
    }

    if (newState == UNINITIALIZED) {
        // return any straggling buffers, e.g. if we got here on an error
        returnBuffersToCodec();

        // The component is gone, mediaserver's probably back up already
        // but should definitely be back up should we try to instantiate
        // another component.. and the cycle continues.
        mFlags &= ~kFlagSawMediaServerDie;
    }

    mState = newState;

    if (mBatteryChecker != nullptr) {
        mBatteryChecker->setExecuting(isExecuting());
    }

    cancelPendingDequeueOperations();
}

void MediaCodec::returnBuffersToCodec(bool isReclaim) {
    returnBuffersToCodecOnPort(kPortIndexInput, isReclaim);
    returnBuffersToCodecOnPort(kPortIndexOutput, isReclaim);
}

void MediaCodec::returnBuffersToCodecOnPort(int32_t portIndex, bool isReclaim) {
    CHECK(portIndex == kPortIndexInput || portIndex == kPortIndexOutput);
    Mutex::Autolock al(mBufferLock);

    if (portIndex == kPortIndexInput) {
        mLeftover.clear();
    }
    for (size_t i = 0; i < mPortBuffers[portIndex].size(); ++i) {
        BufferInfo *info = &mPortBuffers[portIndex][i];

        if (info->mData != nullptr) {
            sp<MediaCodecBuffer> buffer = info->mData;
            if (isReclaim && info->mOwnedByClient) {
                ALOGD("port %d buffer %zu still owned by client when codec is reclaimed",
                        portIndex, i);
            } else {
                info->mOwnedByClient = false;
                info->mData.clear();
            }
            mBufferChannel->discardBuffer(buffer);
        }
    }

    mAvailPortBuffers[portIndex].clear();
}

size_t MediaCodec::updateBuffers(
        int32_t portIndex, const sp<AMessage> &msg) {
    CHECK(portIndex == kPortIndexInput || portIndex == kPortIndexOutput);
    size_t index;
    CHECK(msg->findSize("index", &index));
    sp<RefBase> obj;
    CHECK(msg->findObject("buffer", &obj));
    sp<MediaCodecBuffer> buffer = static_cast<MediaCodecBuffer *>(obj.get());

    {
        Mutex::Autolock al(mBufferLock);
        if (mPortBuffers[portIndex].size() <= index) {
            mPortBuffers[portIndex].resize(align(index + 1, kNumBuffersAlign));
        }
        mPortBuffers[portIndex][index].mData = buffer;
    }
    mAvailPortBuffers[portIndex].push_back(index);

    return index;
}

status_t MediaCodec::onQueueInputBuffer(const sp<AMessage> &msg) {
    size_t index;
    size_t offset;
    size_t size;
    int64_t timeUs;
    uint32_t flags;
    CHECK(msg->findSize("index", &index));
    CHECK(msg->findInt64("timeUs", &timeUs));
    CHECK(msg->findInt32("flags", (int32_t *)&flags));
    std::shared_ptr<C2Buffer> c2Buffer;
    sp<hardware::HidlMemory> memory;
    sp<RefBase> obj;
    if (msg->findObject("c2buffer", &obj)) {
        CHECK(obj);
        c2Buffer = static_cast<WrapperObject<std::shared_ptr<C2Buffer>> *>(obj.get())->value;
    } else if (msg->findObject("memory", &obj)) {
        CHECK(obj);
        memory = static_cast<WrapperObject<sp<hardware::HidlMemory>> *>(obj.get())->value;
        CHECK(msg->findSize("offset", &offset));
    } else {
        CHECK(msg->findSize("offset", &offset));
    }
    const CryptoPlugin::SubSample *subSamples;
    size_t numSubSamples;
    const uint8_t *key;
    const uint8_t *iv;
    CryptoPlugin::Mode mode = CryptoPlugin::kMode_Unencrypted;

    // We allow the simpler queueInputBuffer API to be used even in
    // secure mode, by fabricating a single unencrypted subSample.
    CryptoPlugin::SubSample ss;
    CryptoPlugin::Pattern pattern;

    if (msg->findSize("size", &size)) {
        if (hasCryptoOrDescrambler()) {
            ss.mNumBytesOfClearData = size;
            ss.mNumBytesOfEncryptedData = 0;

            subSamples = &ss;
            numSubSamples = 1;
            key = NULL;
            iv = NULL;
            pattern.mEncryptBlocks = 0;
            pattern.mSkipBlocks = 0;
        }
    } else if (!c2Buffer) {
        if (!hasCryptoOrDescrambler()) {
            ALOGE("[%s] queuing secure buffer without mCrypto or mDescrambler!",
                    mComponentName.c_str());
            return -EINVAL;
        }

        CHECK(msg->findPointer("subSamples", (void **)&subSamples));
        CHECK(msg->findSize("numSubSamples", &numSubSamples));
        CHECK(msg->findPointer("key", (void **)&key));
        CHECK(msg->findPointer("iv", (void **)&iv));
        CHECK(msg->findInt32("encryptBlocks", (int32_t *)&pattern.mEncryptBlocks));
        CHECK(msg->findInt32("skipBlocks", (int32_t *)&pattern.mSkipBlocks));

        int32_t tmp;
        CHECK(msg->findInt32("mode", &tmp));

        mode = (CryptoPlugin::Mode)tmp;

        size = 0;
        for (size_t i = 0; i < numSubSamples; ++i) {
            size += subSamples[i].mNumBytesOfClearData;
            size += subSamples[i].mNumBytesOfEncryptedData;
        }
    }

    if (index >= mPortBuffers[kPortIndexInput].size()) {
        return -ERANGE;
    }

    BufferInfo *info = &mPortBuffers[kPortIndexInput][index];
    sp<MediaCodecBuffer> buffer = info->mData;

    if (c2Buffer || memory) {
        sp<AMessage> tunings;
        CHECK(msg->findMessage("tunings", &tunings));
        onSetParameters(tunings);

        status_t err = OK;
        if (c2Buffer) {
            err = mBufferChannel->attachBuffer(c2Buffer, buffer);
        } else if (memory) {
            err = mBufferChannel->attachEncryptedBuffer(
                    memory, (mFlags & kFlagIsSecure), key, iv, mode, pattern,
                    offset, subSamples, numSubSamples, buffer);
        } else {
            err = UNKNOWN_ERROR;
        }

        if (err == OK && !buffer->asC2Buffer()
                && c2Buffer && c2Buffer->data().type() == C2BufferData::LINEAR) {
            C2ConstLinearBlock block{c2Buffer->data().linearBlocks().front()};
            if (block.size() > buffer->size()) {
                C2ConstLinearBlock leftover = block.subBlock(
                        block.offset() + buffer->size(), block.size() - buffer->size());
                sp<WrapperObject<std::shared_ptr<C2Buffer>>> obj{
                    new WrapperObject<std::shared_ptr<C2Buffer>>{
                        C2Buffer::CreateLinearBuffer(leftover)}};
                msg->setObject("c2buffer", obj);
                mLeftover.push_front(msg);
                // Not sending EOS if we have leftovers
                flags &= ~BUFFER_FLAG_EOS;
            }
        }

        offset = buffer->offset();
        size = buffer->size();
        if (err != OK) {
            return err;
        }
    }

    if (buffer == nullptr || !info->mOwnedByClient) {
        return -EACCES;
    }

    if (offset + size > buffer->capacity()) {
        return -EINVAL;
    }

    buffer->setRange(offset, size);
    buffer->meta()->setInt64("timeUs", timeUs);
    if (flags & BUFFER_FLAG_EOS) {
        buffer->meta()->setInt32("eos", true);
    }

    if (flags & BUFFER_FLAG_CODECCONFIG) {
        buffer->meta()->setInt32("csd", true);
    }

    status_t err = OK;
    if (hasCryptoOrDescrambler() && !c2Buffer && !memory) {
        AString *errorDetailMsg;
        CHECK(msg->findPointer("errorDetailMsg", (void **)&errorDetailMsg));

        err = mBufferChannel->queueSecureInputBuffer(
                buffer,
                (mFlags & kFlagIsSecure),
                key,
                iv,
                mode,
                pattern,
                subSamples,
                numSubSamples,
                errorDetailMsg);
        if (err != OK) {
            mediametrics_setInt32(mMetricsHandle, kCodecQueueSecureInputBufferError, err);
            ALOGW("Log queueSecureInputBuffer error: %d", err);
        }
    } else {
        err = mBufferChannel->queueInputBuffer(buffer);
        if (err != OK) {
            mediametrics_setInt32(mMetricsHandle, kCodecQueueInputBufferError, err);
            ALOGW("Log queueInputBuffer error: %d", err);
        }
    }

    if (err == OK) {
        // synchronization boundary for getBufferAndFormat
        Mutex::Autolock al(mBufferLock);
        info->mOwnedByClient = false;
        info->mData.clear();

        statsBufferSent(timeUs);
    }

    return err;
}

status_t MediaCodec::handleLeftover(size_t index) {
    if (mLeftover.empty()) {
        return OK;
    }
    sp<AMessage> msg = mLeftover.front();
    mLeftover.pop_front();
    msg->setSize("index", index);
    return onQueueInputBuffer(msg);
}

//static
size_t MediaCodec::CreateFramesRenderedMessage(
        const std::list<FrameRenderTracker::Info> &done, sp<AMessage> &msg) {
    size_t index = 0;

    for (std::list<FrameRenderTracker::Info>::const_iterator it = done.cbegin();
            it != done.cend(); ++it) {
        if (it->getRenderTimeNs() < 0) {
            continue; // dropped frame from tracking
        }
        msg->setInt64(AStringPrintf("%zu-media-time-us", index).c_str(), it->getMediaTimeUs());
        msg->setInt64(AStringPrintf("%zu-system-nano", index).c_str(), it->getRenderTimeNs());
        ++index;
    }
    return index;
}

status_t MediaCodec::onReleaseOutputBuffer(const sp<AMessage> &msg) {
    size_t index;
    CHECK(msg->findSize("index", &index));

    int32_t render;
    if (!msg->findInt32("render", &render)) {
        render = 0;
    }

    if (!isExecuting()) {
        return -EINVAL;
    }

    if (index >= mPortBuffers[kPortIndexOutput].size()) {
        return -ERANGE;
    }

    BufferInfo *info = &mPortBuffers[kPortIndexOutput][index];

    if (info->mData == nullptr || !info->mOwnedByClient) {
        return -EACCES;
    }

    // synchronization boundary for getBufferAndFormat
    sp<MediaCodecBuffer> buffer;
    {
        Mutex::Autolock al(mBufferLock);
        info->mOwnedByClient = false;
        buffer = info->mData;
        info->mData.clear();
    }

    if (render && buffer->size() != 0) {
        int64_t mediaTimeUs = -1;
        buffer->meta()->findInt64("timeUs", &mediaTimeUs);

        int64_t renderTimeNs = 0;
        if (!msg->findInt64("timestampNs", &renderTimeNs)) {
            // use media timestamp if client did not request a specific render timestamp
            ALOGV("using buffer PTS of %lld", (long long)mediaTimeUs);
            renderTimeNs = mediaTimeUs * 1000;
        }

        if (mSoftRenderer != NULL) {
            std::list<FrameRenderTracker::Info> doneFrames = mSoftRenderer->render(
                    buffer->data(), buffer->size(), mediaTimeUs, renderTimeNs,
                    mPortBuffers[kPortIndexOutput].size(), buffer->format());

            // if we are running, notify rendered frames
            if (!doneFrames.empty() && mState == STARTED && mOnFrameRenderedNotification != NULL) {
                sp<AMessage> notify = mOnFrameRenderedNotification->dup();
                sp<AMessage> data = new AMessage;
                if (CreateFramesRenderedMessage(doneFrames, data)) {
                    notify->setMessage("data", data);
                    notify->post();
                }
            }
        }
        mBufferChannel->renderOutputBuffer(buffer, renderTimeNs);
    } else {
        mBufferChannel->discardBuffer(buffer);
    }

    return OK;
}

MediaCodec::BufferInfo *MediaCodec::peekNextPortBuffer(int32_t portIndex) {
    CHECK(portIndex == kPortIndexInput || portIndex == kPortIndexOutput);

    List<size_t> *availBuffers = &mAvailPortBuffers[portIndex];

    if (availBuffers->empty()) {
        return nullptr;
    }

    return &mPortBuffers[portIndex][*availBuffers->begin()];
}

ssize_t MediaCodec::dequeuePortBuffer(int32_t portIndex) {
    CHECK(portIndex == kPortIndexInput || portIndex == kPortIndexOutput);

    BufferInfo *info = peekNextPortBuffer(portIndex);
    if (!info) {
        return -EAGAIN;
    }

    List<size_t> *availBuffers = &mAvailPortBuffers[portIndex];
    size_t index = *availBuffers->begin();
    CHECK_EQ(info, &mPortBuffers[portIndex][index]);
    availBuffers->erase(availBuffers->begin());

    CHECK(!info->mOwnedByClient);
    {
        Mutex::Autolock al(mBufferLock);
        info->mOwnedByClient = true;

        // set image-data
        if (info->mData->format() != NULL) {
            sp<ABuffer> imageData;
            if (info->mData->format()->findBuffer("image-data", &imageData)) {
                info->mData->meta()->setBuffer("image-data", imageData);
            }
            int32_t left, top, right, bottom;
            if (info->mData->format()->findRect("crop", &left, &top, &right, &bottom)) {
                info->mData->meta()->setRect("crop-rect", left, top, right, bottom);
            }
        }
    }

    return index;
}

status_t MediaCodec::connectToSurface(const sp<Surface> &surface) {
    status_t err = OK;
    if (surface != NULL) {
        uint64_t oldId, newId;
        if (mSurface != NULL
                && surface->getUniqueId(&newId) == NO_ERROR
                && mSurface->getUniqueId(&oldId) == NO_ERROR
                && newId == oldId) {
            ALOGI("[%s] connecting to the same surface. Nothing to do.", mComponentName.c_str());
            return ALREADY_EXISTS;
        }

        err = nativeWindowConnect(surface.get(), "connectToSurface");
        if (err == OK) {
            // Require a fresh set of buffers after each connect by using a unique generation
            // number. Rely on the fact that max supported process id by Linux is 2^22.
            // PID is never 0 so we don't have to worry that we use the default generation of 0.
            // TODO: come up with a unique scheme if other producers also set the generation number.
            static uint32_t mSurfaceGeneration = 0;
            uint32_t generation = (getpid() << 10) | (++mSurfaceGeneration & ((1 << 10) - 1));
            surface->setGenerationNumber(generation);
            ALOGI("[%s] setting surface generation to %u", mComponentName.c_str(), generation);

            // HACK: clear any free buffers. Remove when connect will automatically do this.
            // This is needed as the consumer may be holding onto stale frames that it can reattach
            // to this surface after disconnect/connect, and those free frames would inherit the new
            // generation number. Disconnecting after setting a unique generation prevents this.
            nativeWindowDisconnect(surface.get(), "connectToSurface(reconnect)");
            err = nativeWindowConnect(surface.get(), "connectToSurface(reconnect)");
        }

        if (err != OK) {
            ALOGE("nativeWindowConnect returned an error: %s (%d)", strerror(-err), err);
        } else {
            if (!mAllowFrameDroppingBySurface) {
                disableLegacyBufferDropPostQ(surface);
            }
        }
    }
    // do not return ALREADY_EXISTS unless surfaces are the same
    return err == ALREADY_EXISTS ? BAD_VALUE : err;
}

status_t MediaCodec::disconnectFromSurface() {
    status_t err = OK;
    if (mSurface != NULL) {
        // Resetting generation is not technically needed, but there is no need to keep it either
        mSurface->setGenerationNumber(0);
        err = nativeWindowDisconnect(mSurface.get(), "disconnectFromSurface");
        if (err != OK) {
            ALOGW("nativeWindowDisconnect returned an error: %s (%d)", strerror(-err), err);
        }
        // assume disconnected even on error
        mSurface.clear();
    }
    return err;
}

status_t MediaCodec::handleSetSurface(const sp<Surface> &surface) {
    status_t err = OK;
    if (mSurface != NULL) {
        (void)disconnectFromSurface();
    }
    if (surface != NULL) {
        err = connectToSurface(surface);
        if (err == OK) {
            mSurface = surface;
        }
    }
    return err;
}

void MediaCodec::onInputBufferAvailable() {
    int32_t index;
    while ((index = dequeuePortBuffer(kPortIndexInput)) >= 0) {
        sp<AMessage> msg = mCallback->dup();
        msg->setInt32("callbackID", CB_INPUT_AVAILABLE);
        msg->setInt32("index", index);
        msg->post();
    }
}

void MediaCodec::onOutputBufferAvailable() {
    int32_t index;
    while ((index = dequeuePortBuffer(kPortIndexOutput)) >= 0) {
        const sp<MediaCodecBuffer> &buffer =
            mPortBuffers[kPortIndexOutput][index].mData;
        sp<AMessage> msg = mCallback->dup();
        msg->setInt32("callbackID", CB_OUTPUT_AVAILABLE);
        msg->setInt32("index", index);
        msg->setSize("offset", buffer->offset());
        msg->setSize("size", buffer->size());

        int64_t timeUs;
        CHECK(buffer->meta()->findInt64("timeUs", &timeUs));

        msg->setInt64("timeUs", timeUs);

        statsBufferReceived(timeUs);

        int32_t flags;
        CHECK(buffer->meta()->findInt32("flags", &flags));

        msg->setInt32("flags", flags);

        msg->post();
    }
}

void MediaCodec::onError(status_t err, int32_t actionCode, const char *detail) {
    if (mCallback != NULL) {
        sp<AMessage> msg = mCallback->dup();
        msg->setInt32("callbackID", CB_ERROR);
        msg->setInt32("err", err);
        msg->setInt32("actionCode", actionCode);

        if (detail != NULL) {
            msg->setString("detail", detail);
        }

        msg->post();
    }
}

void MediaCodec::onOutputFormatChanged() {
    if (mCallback != NULL) {
        sp<AMessage> msg = mCallback->dup();
        msg->setInt32("callbackID", CB_OUTPUT_FORMAT_CHANGED);
        msg->setMessage("format", mOutputFormat);
        msg->post();
    }
}

void MediaCodec::postActivityNotificationIfPossible() {
    if (mActivityNotify == NULL) {
        return;
    }

    bool isErrorOrOutputChanged =
            (mFlags & (kFlagStickyError
                    | kFlagOutputBuffersChanged
                    | kFlagOutputFormatChanged));

    if (isErrorOrOutputChanged
            || !mAvailPortBuffers[kPortIndexInput].empty()
            || !mAvailPortBuffers[kPortIndexOutput].empty()) {
        mActivityNotify->setInt32("input-buffers",
                mAvailPortBuffers[kPortIndexInput].size());

        if (isErrorOrOutputChanged) {
            // we want consumer to dequeue as many times as it can
            mActivityNotify->setInt32("output-buffers", INT32_MAX);
        } else {
            mActivityNotify->setInt32("output-buffers",
                    mAvailPortBuffers[kPortIndexOutput].size());
        }
        mActivityNotify->post();
        mActivityNotify.clear();
    }
}

status_t MediaCodec::setParameters(const sp<AMessage> &params) {
    sp<AMessage> msg = new AMessage(kWhatSetParameters, this);
    msg->setMessage("params", params);

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t MediaCodec::onSetParameters(const sp<AMessage> &params) {
    updateLowLatency(params);
    mCodec->signalSetParameters(params);

    return OK;
}

status_t MediaCodec::amendOutputFormatWithCodecSpecificData(
        const sp<MediaCodecBuffer> &buffer) {
    AString mime;
    CHECK(mOutputFormat->findString("mime", &mime));

    if (!strcasecmp(mime.c_str(), MEDIA_MIMETYPE_VIDEO_AVC)) {
        // Codec specific data should be SPS and PPS in a single buffer,
        // each prefixed by a startcode (0x00 0x00 0x00 0x01).
        // We separate the two and put them into the output format
        // under the keys "csd-0" and "csd-1".

        unsigned csdIndex = 0;

        const uint8_t *data = buffer->data();
        size_t size = buffer->size();

        const uint8_t *nalStart;
        size_t nalSize;
        while (getNextNALUnit(&data, &size, &nalStart, &nalSize, true) == OK) {
            sp<ABuffer> csd = new ABuffer(nalSize + 4);
            memcpy(csd->data(), "\x00\x00\x00\x01", 4);
            memcpy(csd->data() + 4, nalStart, nalSize);

            mOutputFormat->setBuffer(
                    AStringPrintf("csd-%u", csdIndex).c_str(), csd);

            ++csdIndex;
        }

        if (csdIndex != 2) {
            return ERROR_MALFORMED;
        }
    } else {
        // For everything else we just stash the codec specific data into
        // the output format as a single piece of csd under "csd-0".
        sp<ABuffer> csd = new ABuffer(buffer->size());
        memcpy(csd->data(), buffer->data(), buffer->size());
        csd->setRange(0, buffer->size());
        mOutputFormat->setBuffer("csd-0", csd);
    }

    return OK;
}

void MediaCodec::postPendingRepliesAndDeferredMessages(
        std::string origin, status_t err /* = OK */) {
    sp<AMessage> response{new AMessage};
    if (err != OK) {
        response->setInt32("err", err);
    }
    postPendingRepliesAndDeferredMessages(origin, response);
}

void MediaCodec::postPendingRepliesAndDeferredMessages(
        std::string origin, const sp<AMessage> &response) {
    LOG_ALWAYS_FATAL_IF(
            !mReplyID,
            "postPendingRepliesAndDeferredMessages: mReplyID == null, from %s following %s",
            origin.c_str(),
            mLastReplyOrigin.c_str());
    mLastReplyOrigin = origin;
    response->postReply(mReplyID);
    mReplyID.clear();
    ALOGV_IF(!mDeferredMessages.empty(),
            "posting %zu deferred messages", mDeferredMessages.size());
    for (sp<AMessage> msg : mDeferredMessages) {
        msg->post();
    }
    mDeferredMessages.clear();
}

std::string MediaCodec::stateString(State state) {
    const char *rval = NULL;
    char rawbuffer[16]; // room for "%d"

    switch (state) {
        case UNINITIALIZED: rval = "UNINITIALIZED"; break;
        case INITIALIZING: rval = "INITIALIZING"; break;
        case INITIALIZED: rval = "INITIALIZED"; break;
        case CONFIGURING: rval = "CONFIGURING"; break;
        case CONFIGURED: rval = "CONFIGURED"; break;
        case STARTING: rval = "STARTING"; break;
        case STARTED: rval = "STARTED"; break;
        case FLUSHING: rval = "FLUSHING"; break;
        case FLUSHED: rval = "FLUSHED"; break;
        case STOPPING: rval = "STOPPING"; break;
        case RELEASING: rval = "RELEASING"; break;
        default:
            snprintf(rawbuffer, sizeof(rawbuffer), "%d", state);
            rval = rawbuffer;
            break;
    }
    return rval;
}

// static
status_t MediaCodec::CanFetchLinearBlock(
        const std::vector<std::string> &names, bool *isCompatible) {
    *isCompatible = false;
    if (names.size() == 0) {
        *isCompatible = true;
        return OK;
    }
    const CodecListCache &cache = GetCodecListCache();
    for (const std::string &name : names) {
        auto it = cache.mCodecInfoMap.find(name);
        if (it == cache.mCodecInfoMap.end()) {
            return NAME_NOT_FOUND;
        }
        const char *owner = it->second->getOwnerName();
        if (owner == nullptr || strncmp(owner, "default", 8) == 0) {
            *isCompatible = false;
            return OK;
        } else if (strncmp(owner, "codec2::", 8) != 0) {
            return NAME_NOT_FOUND;
        }
    }
    return CCodec::CanFetchLinearBlock(names, kDefaultReadWriteUsage, isCompatible);
}

// static
std::shared_ptr<C2LinearBlock> MediaCodec::FetchLinearBlock(
        size_t capacity, const std::vector<std::string> &names) {
    return CCodec::FetchLinearBlock(capacity, kDefaultReadWriteUsage, names);
}

// static
status_t MediaCodec::CanFetchGraphicBlock(
        const std::vector<std::string> &names, bool *isCompatible) {
    *isCompatible = false;
    if (names.size() == 0) {
        *isCompatible = true;
        return OK;
    }
    const CodecListCache &cache = GetCodecListCache();
    for (const std::string &name : names) {
        auto it = cache.mCodecInfoMap.find(name);
        if (it == cache.mCodecInfoMap.end()) {
            return NAME_NOT_FOUND;
        }
        const char *owner = it->second->getOwnerName();
        if (owner == nullptr || strncmp(owner, "default", 8) == 0) {
            *isCompatible = false;
            return OK;
        } else if (strncmp(owner, "codec2.", 7) != 0) {
            return NAME_NOT_FOUND;
        }
    }
    return CCodec::CanFetchGraphicBlock(names, isCompatible);
}

// static
std::shared_ptr<C2GraphicBlock> MediaCodec::FetchGraphicBlock(
        int32_t width,
        int32_t height,
        int32_t format,
        uint64_t usage,
        const std::vector<std::string> &names) {
    return CCodec::FetchGraphicBlock(width, height, format, usage, names);
}

}  // namespace android
