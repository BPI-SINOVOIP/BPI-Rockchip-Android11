/*
 * Copyright 2019 The Android Open Source Project
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

#define LOG_TAG "TvTuner-JNI"
#include <utils/Log.h>

#include "android_media_MediaCodecLinearBlock.h"
#include "android_media_tv_Tuner.h"
#include "android_runtime/AndroidRuntime.h"

#include <android-base/logging.h>
#include <android/hardware/tv/tuner/1.0/ITuner.h>
#include <media/stagefright/foundation/ADebug.h>
#include <nativehelper/JNIHelp.h>
#include <nativehelper/ScopedLocalRef.h>
#include <utils/NativeHandle.h>

#pragma GCC diagnostic ignored "-Wunused-function"

using ::android::hardware::Void;
using ::android::hardware::hidl_bitfield;
using ::android::hardware::hidl_vec;
using ::android::hardware::tv::tuner::V1_0::AudioExtraMetaData;
using ::android::hardware::tv::tuner::V1_0::Constant;
using ::android::hardware::tv::tuner::V1_0::DataFormat;
using ::android::hardware::tv::tuner::V1_0::DemuxAlpFilterSettings;
using ::android::hardware::tv::tuner::V1_0::DemuxAlpFilterType;
using ::android::hardware::tv::tuner::V1_0::DemuxAlpLengthType;
using ::android::hardware::tv::tuner::V1_0::DemuxCapabilities;
using ::android::hardware::tv::tuner::V1_0::DemuxFilterAvSettings;
using ::android::hardware::tv::tuner::V1_0::DemuxFilterDownloadEvent;
using ::android::hardware::tv::tuner::V1_0::DemuxFilterDownloadSettings;
using ::android::hardware::tv::tuner::V1_0::DemuxFilterIpPayloadEvent;
using ::android::hardware::tv::tuner::V1_0::DemuxFilterMainType;
using ::android::hardware::tv::tuner::V1_0::DemuxFilterMediaEvent;
using ::android::hardware::tv::tuner::V1_0::DemuxFilterMmtpRecordEvent;
using ::android::hardware::tv::tuner::V1_0::DemuxFilterPesDataSettings;
using ::android::hardware::tv::tuner::V1_0::DemuxFilterPesEvent;
using ::android::hardware::tv::tuner::V1_0::DemuxFilterRecordSettings;
using ::android::hardware::tv::tuner::V1_0::DemuxFilterSectionBits;
using ::android::hardware::tv::tuner::V1_0::DemuxFilterSectionEvent;
using ::android::hardware::tv::tuner::V1_0::DemuxFilterSectionSettings;
using ::android::hardware::tv::tuner::V1_0::DemuxFilterSettings;
using ::android::hardware::tv::tuner::V1_0::DemuxFilterTemiEvent;
using ::android::hardware::tv::tuner::V1_0::DemuxFilterTsRecordEvent;
using ::android::hardware::tv::tuner::V1_0::DemuxIpAddress;
using ::android::hardware::tv::tuner::V1_0::DemuxIpFilterSettings;
using ::android::hardware::tv::tuner::V1_0::DemuxIpFilterType;
using ::android::hardware::tv::tuner::V1_0::DemuxMmtpFilterSettings;
using ::android::hardware::tv::tuner::V1_0::DemuxMmtpFilterType;
using ::android::hardware::tv::tuner::V1_0::DemuxMmtpPid;
using ::android::hardware::tv::tuner::V1_0::DemuxQueueNotifyBits;
using ::android::hardware::tv::tuner::V1_0::DemuxRecordScIndexType;
using ::android::hardware::tv::tuner::V1_0::DemuxScHevcIndex;
using ::android::hardware::tv::tuner::V1_0::DemuxScIndex;
using ::android::hardware::tv::tuner::V1_0::DemuxTlvFilterSettings;
using ::android::hardware::tv::tuner::V1_0::DemuxTlvFilterType;
using ::android::hardware::tv::tuner::V1_0::DemuxTpid;
using ::android::hardware::tv::tuner::V1_0::DemuxTsFilterSettings;
using ::android::hardware::tv::tuner::V1_0::DemuxTsFilterType;
using ::android::hardware::tv::tuner::V1_0::DemuxTsIndex;
using ::android::hardware::tv::tuner::V1_0::DvrSettings;
using ::android::hardware::tv::tuner::V1_0::FrontendAnalogSettings;
using ::android::hardware::tv::tuner::V1_0::FrontendAnalogSifStandard;
using ::android::hardware::tv::tuner::V1_0::FrontendAnalogType;
using ::android::hardware::tv::tuner::V1_0::FrontendAtsc3Bandwidth;
using ::android::hardware::tv::tuner::V1_0::FrontendAtsc3CodeRate;
using ::android::hardware::tv::tuner::V1_0::FrontendAtsc3DemodOutputFormat;
using ::android::hardware::tv::tuner::V1_0::FrontendAtsc3Fec;
using ::android::hardware::tv::tuner::V1_0::FrontendAtsc3Modulation;
using ::android::hardware::tv::tuner::V1_0::FrontendAtsc3PlpSettings;
using ::android::hardware::tv::tuner::V1_0::FrontendAtsc3Settings;
using ::android::hardware::tv::tuner::V1_0::FrontendAtsc3TimeInterleaveMode;
using ::android::hardware::tv::tuner::V1_0::FrontendAtscSettings;
using ::android::hardware::tv::tuner::V1_0::FrontendAtscModulation;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbcAnnex;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbcModulation;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbcOuterFec;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbcSettings;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbcSpectralInversion;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbsCodeRate;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbsModulation;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbsPilot;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbsRolloff;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbsSettings;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbsStandard;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbsVcmMode;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbtBandwidth;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbtCoderate;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbtConstellation;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbtGuardInterval;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbtHierarchy;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbtPlpMode;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbtSettings;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbtStandard;
using ::android::hardware::tv::tuner::V1_0::FrontendDvbtTransmissionMode;
using ::android::hardware::tv::tuner::V1_0::FrontendInnerFec;
using ::android::hardware::tv::tuner::V1_0::FrontendIsdbs3Coderate;
using ::android::hardware::tv::tuner::V1_0::FrontendIsdbs3Modulation;
using ::android::hardware::tv::tuner::V1_0::FrontendIsdbs3Rolloff;
using ::android::hardware::tv::tuner::V1_0::FrontendIsdbs3Settings;
using ::android::hardware::tv::tuner::V1_0::FrontendIsdbsCoderate;
using ::android::hardware::tv::tuner::V1_0::FrontendIsdbsModulation;
using ::android::hardware::tv::tuner::V1_0::FrontendIsdbsRolloff;
using ::android::hardware::tv::tuner::V1_0::FrontendIsdbsSettings;
using ::android::hardware::tv::tuner::V1_0::FrontendIsdbsStreamIdType;
using ::android::hardware::tv::tuner::V1_0::FrontendIsdbtBandwidth;
using ::android::hardware::tv::tuner::V1_0::FrontendIsdbtCoderate;
using ::android::hardware::tv::tuner::V1_0::FrontendIsdbtGuardInterval;
using ::android::hardware::tv::tuner::V1_0::FrontendIsdbtMode;
using ::android::hardware::tv::tuner::V1_0::FrontendIsdbtModulation;
using ::android::hardware::tv::tuner::V1_0::FrontendIsdbtSettings;
using ::android::hardware::tv::tuner::V1_0::FrontendModulationStatus;
using ::android::hardware::tv::tuner::V1_0::FrontendScanAtsc3PlpInfo;
using ::android::hardware::tv::tuner::V1_0::FrontendStatus;
using ::android::hardware::tv::tuner::V1_0::FrontendStatusAtsc3PlpInfo;
using ::android::hardware::tv::tuner::V1_0::FrontendStatusType;
using ::android::hardware::tv::tuner::V1_0::FrontendType;
using ::android::hardware::tv::tuner::V1_0::ITuner;
using ::android::hardware::tv::tuner::V1_0::LnbPosition;
using ::android::hardware::tv::tuner::V1_0::LnbTone;
using ::android::hardware::tv::tuner::V1_0::LnbVoltage;
using ::android::hardware::tv::tuner::V1_0::PlaybackSettings;
using ::android::hardware::tv::tuner::V1_0::RecordSettings;

struct fields_t {
    jfieldID tunerContext;
    jfieldID lnbContext;
    jfieldID filterContext;
    jfieldID timeFilterContext;
    jfieldID descramblerContext;
    jfieldID dvrRecorderContext;
    jfieldID dvrPlaybackContext;
    jfieldID mediaEventContext;
    jmethodID frontendInitID;
    jmethodID filterInitID;
    jmethodID timeFilterInitID;
    jmethodID dvrRecorderInitID;
    jmethodID dvrPlaybackInitID;
    jmethodID onFrontendEventID;
    jmethodID onFilterStatusID;
    jmethodID onFilterEventID;
    jmethodID lnbInitID;
    jmethodID onLnbEventID;
    jmethodID onLnbDiseqcMessageID;
    jmethodID onDvrRecordStatusID;
    jmethodID onDvrPlaybackStatusID;
    jmethodID descramblerInitID;
    jmethodID linearBlockInitID;
    jmethodID linearBlockSetInternalStateID;
};

static fields_t gFields;


static int IP_V4_LENGTH = 4;
static int IP_V6_LENGTH = 16;

void DestroyCallback(const C2Buffer * /* buf */, void *arg) {
    android::sp<android::MediaEvent> event = (android::MediaEvent *)arg;
    event->mAvHandleRefCnt--;
    event->finalize();
}

namespace android {
/////////////// LnbCallback ///////////////////////
LnbCallback::LnbCallback(jobject lnbObj, LnbId id) : mId(id) {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    mLnb = env->NewWeakGlobalRef(lnbObj);
}

Return<void> LnbCallback::onEvent(LnbEventType lnbEventType) {
    ALOGD("LnbCallback::onEvent, type=%d", lnbEventType);
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    env->CallVoidMethod(
            mLnb,
            gFields.onLnbEventID,
            (jint)lnbEventType);
    return Void();
}
Return<void> LnbCallback::onDiseqcMessage(const hidl_vec<uint8_t>& diseqcMessage) {
    ALOGD("LnbCallback::onDiseqcMessage");
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jbyteArray array = env->NewByteArray(diseqcMessage.size());
    env->SetByteArrayRegion(
            array, 0, diseqcMessage.size(), reinterpret_cast<jbyte*>(diseqcMessage[0]));

    env->CallVoidMethod(
            mLnb,
            gFields.onLnbDiseqcMessageID,
            array);
    return Void();
}

/////////////// Lnb ///////////////////////

Lnb::Lnb(sp<ILnb> sp, jobject obj) : mLnbSp(sp) {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    mLnbObj = env->NewWeakGlobalRef(obj);
}

Lnb::~Lnb() {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    env->DeleteWeakGlobalRef(mLnbObj);
    mLnbObj = NULL;
}

sp<ILnb> Lnb::getILnb() {
    return mLnbSp;
}

/////////////// DvrCallback ///////////////////////
Return<void> DvrCallback::onRecordStatus(RecordStatus status) {
    ALOGD("DvrCallback::onRecordStatus");
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    env->CallVoidMethod(
            mDvr,
            gFields.onDvrRecordStatusID,
            (jint) status);
    return Void();
}

Return<void> DvrCallback::onPlaybackStatus(PlaybackStatus status) {
    ALOGD("DvrCallback::onPlaybackStatus");
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    env->CallVoidMethod(
            mDvr,
            gFields.onDvrPlaybackStatusID,
            (jint) status);
    return Void();
}

void DvrCallback::setDvr(const jobject dvr) {
    ALOGD("DvrCallback::setDvr");
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    mDvr = env->NewWeakGlobalRef(dvr);
}

DvrCallback::~DvrCallback() {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    if (mDvr != NULL) {
        env->DeleteWeakGlobalRef(mDvr);
        mDvr = NULL;
    }
}

/////////////// Dvr ///////////////////////

Dvr::Dvr(sp<IDvr> sp, jobject obj) : mDvrSp(sp), mDvrMQEventFlag(nullptr) {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    mDvrObj = env->NewWeakGlobalRef(obj);
}

Dvr::~Dvr() {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    env->DeleteWeakGlobalRef(mDvrObj);
    mDvrObj = NULL;
}

jint Dvr::close() {
    Result r = mDvrSp->close();
    if (r == Result::SUCCESS) {
        EventFlag::deleteEventFlag(&mDvrMQEventFlag);
    }
    return (jint) r;
}

sp<IDvr> Dvr::getIDvr() {
    return mDvrSp;
}

MQ& Dvr::getDvrMQ() {
    return *mDvrMQ;
}

/////////////// C2DataIdInfo ///////////////////////

C2DataIdInfo::C2DataIdInfo(uint32_t index, uint64_t value) : C2Param(kParamSize, index) {
    CHECK(isGlobal());
    CHECK_EQ(C2Param::INFO, kind());
    DummyInfo info{value};
    memcpy(this + 1, static_cast<C2Param *>(&info) + 1, kParamSize - sizeof(C2Param));
}

/////////////// MediaEvent ///////////////////////

MediaEvent::MediaEvent(sp<IFilter> iFilter, hidl_handle avHandle,
        uint64_t dataId, uint64_t dataLength, jobject obj) : mIFilter(iFilter),
        mDataId(dataId), mDataLength(dataLength), mBuffer(nullptr),
        mDataIdRefCnt(0), mAvHandleRefCnt(0), mIonHandle(nullptr) {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    mMediaEventObj = env->NewWeakGlobalRef(obj);
    mAvHandle = native_handle_clone(avHandle.getNativeHandle());
}

MediaEvent::~MediaEvent() {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    env->DeleteWeakGlobalRef(mMediaEventObj);
    mMediaEventObj = NULL;
    native_handle_delete(mAvHandle);
    if (mIonHandle != NULL) {
        delete mIonHandle;
    }
    std::shared_ptr<C2Buffer> pC2Buffer = mC2Buffer.lock();
    if (pC2Buffer != NULL) {
        pC2Buffer->unregisterOnDestroyNotify(&DestroyCallback, this);
    }
}

void MediaEvent::finalize() {
    if (mAvHandleRefCnt == 0) {
        mIFilter->releaseAvHandle(hidl_handle(mAvHandle), mDataIdRefCnt == 0 ? mDataId : 0);
        native_handle_close(mAvHandle);
    }
}

jobject MediaEvent::getLinearBlock() {
    ALOGD("MediaEvent::getLinearBlock");
    if (mAvHandle == NULL) {
        return NULL;
    }
    if (mLinearBlockObj != NULL) {
        return mLinearBlockObj;
    }
    mIonHandle = new C2HandleIon(dup(mAvHandle->data[0]), mDataLength);
    std::shared_ptr<C2LinearBlock> block = _C2BlockFactory::CreateLinearBlock(mIonHandle);

    JNIEnv *env = AndroidRuntime::getJNIEnv();
    std::unique_ptr<JMediaCodecLinearBlock> context{new JMediaCodecLinearBlock};
    context->mBlock = block;
    std::shared_ptr<C2Buffer> pC2Buffer = context->toC2Buffer(0, mDataLength);
    context->mBuffer = pC2Buffer;
    mC2Buffer = pC2Buffer;
    if (mAvHandle->numInts > 0) {
        // use first int in the native_handle as the index
        int index = mAvHandle->data[mAvHandle->numFds];
        std::shared_ptr<C2Param> c2param = std::make_shared<C2DataIdInfo>(index, mDataId);
        std::shared_ptr<C2Info> info(std::static_pointer_cast<C2Info>(c2param));
        pC2Buffer->setInfo(info);
    }
    pC2Buffer->registerOnDestroyNotify(&DestroyCallback, this);
    jobject linearBlock =
            env->NewObject(
                    env->FindClass("android/media/MediaCodec$LinearBlock"),
                    gFields.linearBlockInitID);
    env->CallVoidMethod(
            linearBlock,
            gFields.linearBlockSetInternalStateID,
            (jlong)context.release(),
            true);
    mLinearBlockObj = env->NewWeakGlobalRef(linearBlock);
    mAvHandleRefCnt++;
    return mLinearBlockObj;
}

uint64_t MediaEvent::getAudioHandle() {
    mDataIdRefCnt++;
    return mDataId;
}

/////////////// FilterCallback ///////////////////////

jobjectArray FilterCallback::getSectionEvent(
        jobjectArray& arr, const std::vector<DemuxFilterEvent::Event>& events) {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jclass eventClazz = env->FindClass("android/media/tv/tuner/filter/SectionEvent");
    jmethodID eventInit = env->GetMethodID(eventClazz, "<init>", "(IIII)V");

    for (int i = 0; i < events.size(); i++) {
        auto event = events[i];
        DemuxFilterSectionEvent sectionEvent = event.section();

        jint tableId = static_cast<jint>(sectionEvent.tableId);
        jint version = static_cast<jint>(sectionEvent.version);
        jint sectionNum = static_cast<jint>(sectionEvent.sectionNum);
        jint dataLength = static_cast<jint>(sectionEvent.dataLength);

        jobject obj =
                env->NewObject(eventClazz, eventInit, tableId, version, sectionNum, dataLength);
        env->SetObjectArrayElement(arr, i, obj);
    }
    return arr;
}

jobjectArray FilterCallback::getMediaEvent(
        jobjectArray& arr, const std::vector<DemuxFilterEvent::Event>& events) {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jclass eventClazz = env->FindClass("android/media/tv/tuner/filter/MediaEvent");
    jmethodID eventInit = env->GetMethodID(eventClazz,
            "<init>",
            "(IZJJJLandroid/media/MediaCodec$LinearBlock;"
            "ZJIZLandroid/media/tv/tuner/filter/AudioDescriptor;)V");
    jfieldID eventContext = env->GetFieldID(eventClazz, "mNativeContext", "J");

    for (int i = 0; i < events.size(); i++) {
        auto event = events[i];
        DemuxFilterMediaEvent mediaEvent = event.media();

        jobject audioDescriptor = NULL;
        if (mediaEvent.extraMetaData.getDiscriminator()
                == DemuxFilterMediaEvent::ExtraMetaData::hidl_discriminator::audio) {
            jclass adClazz = env->FindClass("android/media/tv/tuner/filter/AudioDescriptor");
            jmethodID adInit = env->GetMethodID(adClazz, "<init>", "(BBCBBB)V");

            AudioExtraMetaData ad = mediaEvent.extraMetaData.audio();
            jbyte adFade = static_cast<jbyte>(ad.adFade);
            jbyte adPan = static_cast<jbyte>(ad.adPan);
            jchar versionTextTag = static_cast<jchar>(ad.versionTextTag);
            jbyte adGainCenter = static_cast<jbyte>(ad.adGainCenter);
            jbyte adGainFront = static_cast<jbyte>(ad.adGainFront);
            jbyte adGainSurround = static_cast<jbyte>(ad.adGainSurround);

            audioDescriptor =
                    env->NewObject(adClazz, adInit, adFade, adPan, versionTextTag, adGainCenter,
                            adGainFront, adGainSurround);
        }

        jlong dataLength = static_cast<jlong>(mediaEvent.dataLength);

        jint streamId = static_cast<jint>(mediaEvent.streamId);
        jboolean isPtsPresent = static_cast<jboolean>(mediaEvent.isPtsPresent);
        jlong pts = static_cast<jlong>(mediaEvent.pts);
        jlong offset = static_cast<jlong>(mediaEvent.offset);
        jboolean isSecureMemory = static_cast<jboolean>(mediaEvent.isSecureMemory);
        jlong avDataId = static_cast<jlong>(mediaEvent.avDataId);
        jint mpuSequenceNumber = static_cast<jint>(mediaEvent.mpuSequenceNumber);
        jboolean isPesPrivateData = static_cast<jboolean>(mediaEvent.isPesPrivateData);

        jobject obj =
                env->NewObject(eventClazz, eventInit, streamId, isPtsPresent, pts, dataLength,
                offset, NULL, isSecureMemory, avDataId, mpuSequenceNumber, isPesPrivateData,
                audioDescriptor);

        if (mediaEvent.avMemory.getNativeHandle() != NULL || mediaEvent.avDataId != 0) {
            sp<MediaEvent> mediaEventSp =
                           new MediaEvent(mIFilter, mediaEvent.avMemory,
                               mediaEvent.avDataId, dataLength, obj);
            mediaEventSp->mAvHandleRefCnt++;
            env->SetLongField(obj, eventContext, (jlong) mediaEventSp.get());
            mediaEventSp->incStrong(obj);
        }

        env->SetObjectArrayElement(arr, i, obj);
    }
    return arr;
}

jobjectArray FilterCallback::getPesEvent(
        jobjectArray& arr, const std::vector<DemuxFilterEvent::Event>& events) {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jclass eventClazz = env->FindClass("android/media/tv/tuner/filter/PesEvent");
    jmethodID eventInit = env->GetMethodID(eventClazz, "<init>", "(III)V");

    for (int i = 0; i < events.size(); i++) {
        auto event = events[i];
        DemuxFilterPesEvent pesEvent = event.pes();

        jint streamId = static_cast<jint>(pesEvent.streamId);
        jint dataLength = static_cast<jint>(pesEvent.dataLength);
        jint mpuSequenceNumber = static_cast<jint>(pesEvent.mpuSequenceNumber);

        jobject obj =
                env->NewObject(eventClazz, eventInit, streamId, dataLength, mpuSequenceNumber);
        env->SetObjectArrayElement(arr, i, obj);
    }
    return arr;
}

jobjectArray FilterCallback::getTsRecordEvent(
        jobjectArray& arr, const std::vector<DemuxFilterEvent::Event>& events) {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jclass eventClazz = env->FindClass("android/media/tv/tuner/filter/TsRecordEvent");
    jmethodID eventInit = env->GetMethodID(eventClazz, "<init>", "(IIIJ)V");

    for (int i = 0; i < events.size(); i++) {
        auto event = events[i];
        DemuxFilterTsRecordEvent tsRecordEvent = event.tsRecord();
        DemuxPid pid = tsRecordEvent.pid;

        jint jpid = static_cast<jint>(Constant::INVALID_TS_PID);

        if (pid.getDiscriminator() == DemuxPid::hidl_discriminator::tPid) {
            jpid = static_cast<jint>(pid.tPid());
        } else if (pid.getDiscriminator() == DemuxPid::hidl_discriminator::mmtpPid) {
            jpid = static_cast<jint>(pid.mmtpPid());
        }

        jint sc = 0;

        if (tsRecordEvent.scIndexMask.getDiscriminator()
                == DemuxFilterTsRecordEvent::ScIndexMask::hidl_discriminator::sc) {
            sc = static_cast<jint>(tsRecordEvent.scIndexMask.sc());
        } else if (tsRecordEvent.scIndexMask.getDiscriminator()
                == DemuxFilterTsRecordEvent::ScIndexMask::hidl_discriminator::scHevc) {
            sc = static_cast<jint>(tsRecordEvent.scIndexMask.scHevc());
        }

        jint ts = static_cast<jint>(tsRecordEvent.tsIndexMask);

        jlong byteNumber = static_cast<jlong>(tsRecordEvent.byteNumber);

        jobject obj =
                env->NewObject(eventClazz, eventInit, jpid, ts, sc, byteNumber);
        env->SetObjectArrayElement(arr, i, obj);
    }
    return arr;
}

jobjectArray FilterCallback::getMmtpRecordEvent(
        jobjectArray& arr, const std::vector<DemuxFilterEvent::Event>& events) {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jclass eventClazz = env->FindClass("android/media/tv/tuner/filter/MmtpRecordEvent");
    jmethodID eventInit = env->GetMethodID(eventClazz, "<init>", "(IJ)V");

    for (int i = 0; i < events.size(); i++) {
        auto event = events[i];
        DemuxFilterMmtpRecordEvent mmtpRecordEvent = event.mmtpRecord();

        jint scHevcIndexMask = static_cast<jint>(mmtpRecordEvent.scHevcIndexMask);
        jlong byteNumber = static_cast<jlong>(mmtpRecordEvent.byteNumber);

        jobject obj =
                env->NewObject(eventClazz, eventInit, scHevcIndexMask, byteNumber);
        env->SetObjectArrayElement(arr, i, obj);
    }
    return arr;
}

jobjectArray FilterCallback::getDownloadEvent(
        jobjectArray& arr, const std::vector<DemuxFilterEvent::Event>& events) {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jclass eventClazz = env->FindClass("android/media/tv/tuner/filter/DownloadEvent");
    jmethodID eventInit = env->GetMethodID(eventClazz, "<init>", "(IIIII)V");

    for (int i = 0; i < events.size(); i++) {
        auto event = events[i];
        DemuxFilterDownloadEvent downloadEvent = event.download();

        jint itemId = static_cast<jint>(downloadEvent.itemId);
        jint mpuSequenceNumber = static_cast<jint>(downloadEvent.mpuSequenceNumber);
        jint itemFragmentIndex = static_cast<jint>(downloadEvent.itemFragmentIndex);
        jint lastItemFragmentIndex = static_cast<jint>(downloadEvent.lastItemFragmentIndex);
        jint dataLength = static_cast<jint>(downloadEvent.dataLength);

        jobject obj =
                env->NewObject(eventClazz, eventInit, itemId, mpuSequenceNumber, itemFragmentIndex,
                        lastItemFragmentIndex, dataLength);
        env->SetObjectArrayElement(arr, i, obj);
    }
    return arr;
}

jobjectArray FilterCallback::getIpPayloadEvent(
        jobjectArray& arr, const std::vector<DemuxFilterEvent::Event>& events) {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jclass eventClazz = env->FindClass("android/media/tv/tuner/filter/IpPayloadEvent");
    jmethodID eventInit = env->GetMethodID(eventClazz, "<init>", "(I)V");

    for (int i = 0; i < events.size(); i++) {
        auto event = events[i];
        DemuxFilterIpPayloadEvent ipPayloadEvent = event.ipPayload();
        jint dataLength = static_cast<jint>(ipPayloadEvent.dataLength);
        jobject obj = env->NewObject(eventClazz, eventInit, dataLength);
        env->SetObjectArrayElement(arr, i, obj);
    }
    return arr;
}

jobjectArray FilterCallback::getTemiEvent(
        jobjectArray& arr, const std::vector<DemuxFilterEvent::Event>& events) {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jclass eventClazz = env->FindClass("android/media/tv/tuner/filter/TemiEvent");
    jmethodID eventInit = env->GetMethodID(eventClazz, "<init>", "(JB[B)V");

    for (int i = 0; i < events.size(); i++) {
        auto event = events[i];
        DemuxFilterTemiEvent temiEvent = event.temi();
        jlong pts = static_cast<jlong>(temiEvent.pts);
        jbyte descrTag = static_cast<jbyte>(temiEvent.descrTag);
        std::vector<uint8_t> descrData = temiEvent.descrData;

        jbyteArray array = env->NewByteArray(descrData.size());
        env->SetByteArrayRegion(
                array, 0, descrData.size(), reinterpret_cast<jbyte*>(&descrData[0]));

        jobject obj = env->NewObject(eventClazz, eventInit, pts, descrTag, array);
        env->SetObjectArrayElement(arr, i, obj);
    }
    return arr;
}

Return<void> FilterCallback::onFilterEvent(const DemuxFilterEvent& filterEvent) {
    ALOGD("FilterCallback::onFilterEvent");

    JNIEnv *env = AndroidRuntime::getJNIEnv();

    std::vector<DemuxFilterEvent::Event> events = filterEvent.events;
    jclass eventClazz = env->FindClass("android/media/tv/tuner/filter/FilterEvent");
    jobjectArray array = env->NewObjectArray(events.size(), eventClazz, NULL);

    if (!events.empty()) {
        auto event = events[0];
        switch (event.getDiscriminator()) {
            case DemuxFilterEvent::Event::hidl_discriminator::media: {
                array = getMediaEvent(array, events);
                break;
            }
            case DemuxFilterEvent::Event::hidl_discriminator::section: {
                array = getSectionEvent(array, events);
                break;
            }
            case DemuxFilterEvent::Event::hidl_discriminator::pes: {
                array = getPesEvent(array, events);
                break;
            }
            case DemuxFilterEvent::Event::hidl_discriminator::tsRecord: {
                array = getTsRecordEvent(array, events);
                break;
            }
            case DemuxFilterEvent::Event::hidl_discriminator::mmtpRecord: {
                array = getMmtpRecordEvent(array, events);
                break;
            }
            case DemuxFilterEvent::Event::hidl_discriminator::download: {
                array = getDownloadEvent(array, events);
                break;
            }
            case DemuxFilterEvent::Event::hidl_discriminator::ipPayload: {
                array = getIpPayloadEvent(array, events);
                break;
            }
            case DemuxFilterEvent::Event::hidl_discriminator::temi: {
                array = getTemiEvent(array, events);
                break;
            }
            default: {
                break;
            }
        }
    }
    env->CallVoidMethod(
            mFilter,
            gFields.onFilterEventID,
            array);
    return Void();
}


Return<void> FilterCallback::onFilterStatus(const DemuxFilterStatus status) {
    ALOGD("FilterCallback::onFilterStatus");
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    env->CallVoidMethod(
            mFilter,
            gFields.onFilterStatusID,
            (jint)status);
    return Void();
}

void FilterCallback::setFilter(const sp<Filter> filter) {
    ALOGD("FilterCallback::setFilter");
    mFilter = filter->mFilterObj;
    mIFilter = filter->mFilterSp;
}

FilterCallback::~FilterCallback() {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    if (mFilter != NULL) {
        env->DeleteWeakGlobalRef(mFilter);
        mFilter = NULL;
    }
}

/////////////// Filter ///////////////////////

Filter::Filter(sp<IFilter> sp, jobject obj) : mFilterSp(sp) {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    mFilterObj = env->NewWeakGlobalRef(obj);
}

Filter::~Filter() {
    JNIEnv *env = AndroidRuntime::getJNIEnv();

    env->DeleteWeakGlobalRef(mFilterObj);
    mFilterObj = NULL;
    EventFlag::deleteEventFlag(&mFilterMQEventFlag);
}

int Filter::close() {
    Result r = mFilterSp->close();
    if (r == Result::SUCCESS) {
        EventFlag::deleteEventFlag(&mFilterMQEventFlag);
    }
    return (int)r;
}

sp<IFilter> Filter::getIFilter() {
    return mFilterSp;
}

/////////////// TimeFilter ///////////////////////

TimeFilter::TimeFilter(sp<ITimeFilter> sp, jobject obj) : mTimeFilterSp(sp) {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    mTimeFilterObj = env->NewWeakGlobalRef(obj);
}

TimeFilter::~TimeFilter() {
    ALOGD("~TimeFilter");
    JNIEnv *env = AndroidRuntime::getJNIEnv();

    env->DeleteWeakGlobalRef(mTimeFilterObj);
    mTimeFilterObj = NULL;
}

sp<ITimeFilter> TimeFilter::getITimeFilter() {
    return mTimeFilterSp;
}

/////////////// FrontendCallback ///////////////////////

FrontendCallback::FrontendCallback(jweak tunerObj, FrontendId id) : mObject(tunerObj), mId(id) {}

Return<void> FrontendCallback::onEvent(FrontendEventType frontendEventType) {
    ALOGD("FrontendCallback::onEvent, type=%d", frontendEventType);
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    env->CallVoidMethod(
            mObject,
            gFields.onFrontendEventID,
            (jint)frontendEventType);
    return Void();
}

Return<void> FrontendCallback::onScanMessage(FrontendScanMessageType type, const FrontendScanMessage& message) {
    ALOGD("FrontendCallback::onScanMessage, type=%d", type);
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jclass clazz = env->FindClass("android/media/tv/tuner/Tuner");
    switch(type) {
        case FrontendScanMessageType::LOCKED: {
            if (message.isLocked()) {
                env->CallVoidMethod(
                        mObject,
                        env->GetMethodID(clazz, "onLocked", "()V"));
            }
            break;
        }
        case FrontendScanMessageType::END: {
            if (message.isEnd()) {
                env->CallVoidMethod(
                        mObject,
                        env->GetMethodID(clazz, "onScanStopped", "()V"));
            }
            break;
        }
        case FrontendScanMessageType::PROGRESS_PERCENT: {
            env->CallVoidMethod(
                    mObject,
                    env->GetMethodID(clazz, "onProgress", "(I)V"),
                    (jint) message.progressPercent());
            break;
        }
        case FrontendScanMessageType::FREQUENCY: {
            std::vector<uint32_t> v = message.frequencies();
            jintArray freqs = env->NewIntArray(v.size());
            env->SetIntArrayRegion(freqs, 0, v.size(), reinterpret_cast<jint*>(&v[0]));

            env->CallVoidMethod(
                    mObject,
                    env->GetMethodID(clazz, "onFrequenciesReport", "([I)V"),
                    freqs);
            break;
        }
        case FrontendScanMessageType::SYMBOL_RATE: {
            std::vector<uint32_t> v = message.symbolRates();
            jintArray symbolRates = env->NewIntArray(v.size());
            env->SetIntArrayRegion(symbolRates, 0, v.size(), reinterpret_cast<jint*>(&v[0]));

            env->CallVoidMethod(
                    mObject,
                    env->GetMethodID(clazz, "onSymbolRates", "([I)V"),
                    symbolRates);
            break;
        }
        case FrontendScanMessageType::HIERARCHY: {
            env->CallVoidMethod(
                    mObject,
                    env->GetMethodID(clazz, "onHierarchy", "(I)V"),
                    (jint) message.hierarchy());
            break;
        }
        case FrontendScanMessageType::ANALOG_TYPE: {
            env->CallVoidMethod(
                    mObject,
                    env->GetMethodID(clazz, "onSignalType", "(I)V"),
                    (jint) message.analogType());
            break;
        }
        case FrontendScanMessageType::PLP_IDS: {
            std::vector<uint8_t> v = message.plpIds();
            std::vector<jint> jintV(v.begin(), v.end());
            jintArray plpIds = env->NewIntArray(v.size());
            env->SetIntArrayRegion(plpIds, 0, jintV.size(), &jintV[0]);

            env->CallVoidMethod(
                    mObject,
                    env->GetMethodID(clazz, "onPlpIds", "([I)V"),
                    plpIds);
            break;
        }
        case FrontendScanMessageType::GROUP_IDS: {
            std::vector<uint8_t> v = message.groupIds();
            std::vector<jint> jintV(v.begin(), v.end());
            jintArray groupIds = env->NewIntArray(v.size());
            env->SetIntArrayRegion(groupIds, 0, jintV.size(), &jintV[0]);

            env->CallVoidMethod(
                    mObject,
                    env->GetMethodID(clazz, "onGroupIds", "([I)V"),
                    groupIds);
            break;
        }
        case FrontendScanMessageType::INPUT_STREAM_IDS: {
            std::vector<uint16_t> v = message.inputStreamIds();
            std::vector<jint> jintV(v.begin(), v.end());
            jintArray streamIds = env->NewIntArray(v.size());
            env->SetIntArrayRegion(streamIds, 0, jintV.size(), &jintV[0]);

            env->CallVoidMethod(
                    mObject,
                    env->GetMethodID(clazz, "onInputStreamIds", "([I)V"),
                    streamIds);
            break;
        }
        case FrontendScanMessageType::STANDARD: {
            FrontendScanMessage::Standard std = message.std();
            jint standard;
            if (std.getDiscriminator() == FrontendScanMessage::Standard::hidl_discriminator::sStd) {
                standard = (jint) std.sStd();
                env->CallVoidMethod(
                        mObject,
                        env->GetMethodID(clazz, "onDvbsStandard", "(I)V"),
                        standard);
            } else if (std.getDiscriminator() == FrontendScanMessage::Standard::hidl_discriminator::tStd) {
                standard = (jint) std.tStd();
                env->CallVoidMethod(
                        mObject,
                        env->GetMethodID(clazz, "onDvbtStandard", "(I)V"),
                        standard);
            } else if (std.getDiscriminator() == FrontendScanMessage::Standard::hidl_discriminator::sifStd) {
                standard = (jint) std.sifStd();
                env->CallVoidMethod(
                        mObject,
                        env->GetMethodID(clazz, "onAnalogSifStandard", "(I)V"),
                        standard);
            }
            break;
        }
        case FrontendScanMessageType::ATSC3_PLP_INFO: {
            jclass plpClazz = env->FindClass("android/media/tv/tuner/frontend/Atsc3PlpInfo");
            jmethodID init = env->GetMethodID(plpClazz, "<init>", "(IZ)V");
            std::vector<FrontendScanAtsc3PlpInfo> plpInfos = message.atsc3PlpInfos();
            jobjectArray array = env->NewObjectArray(plpInfos.size(), plpClazz, NULL);

            for (int i = 0; i < plpInfos.size(); i++) {
                auto info = plpInfos[i];
                jint plpId = (jint) info.plpId;
                jboolean lls = (jboolean) info.bLlsFlag;

                jobject obj = env->NewObject(plpClazz, init, plpId, lls);
                env->SetObjectArrayElement(array, i, obj);
            }
            env->CallVoidMethod(
                    mObject,
                    env->GetMethodID(clazz, "onAtsc3PlpInfos", "([Landroid/media/tv/tuner/frontend/Atsc3PlpInfo;)V"),
                    array);
            break;
        }
    }
    return Void();
}

/////////////// Tuner ///////////////////////

sp<ITuner> JTuner::mTuner;

JTuner::JTuner(JNIEnv *env, jobject thiz)
    : mClass(NULL) {
    jclass clazz = env->GetObjectClass(thiz);
    CHECK(clazz != NULL);

    mClass = (jclass)env->NewGlobalRef(clazz);
    mObject = env->NewWeakGlobalRef(thiz);
    if (mTuner == NULL) {
        mTuner = getTunerService();
    }
}

JTuner::~JTuner() {
    if (mFe != NULL) {
        mFe->close();
    }
    if (mDemux != NULL) {
        mDemux->close();
    }
    JNIEnv *env = AndroidRuntime::getJNIEnv();

    env->DeleteWeakGlobalRef(mObject);
    env->DeleteGlobalRef(mClass);
    mTuner = NULL;
    mClass = NULL;
    mObject = NULL;
}

sp<ITuner> JTuner::getTunerService() {
    if (mTuner == nullptr) {
        mTuner = ITuner::getService();

        if (mTuner == nullptr) {
            ALOGW("Failed to get tuner service.");
        }
    }
    return mTuner;
}

jobject JTuner::getFrontendIds() {
    ALOGD("JTuner::getFrontendIds()");
    mTuner->getFrontendIds([&](Result, const hidl_vec<FrontendId>& frontendIds) {
        mFeIds = frontendIds;
    });
    if (mFeIds.size() == 0) {
        ALOGW("Frontend isn't available");
        return NULL;
    }

    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jclass arrayListClazz = env->FindClass("java/util/ArrayList");
    jmethodID arrayListAdd = env->GetMethodID(arrayListClazz, "add", "(Ljava/lang/Object;)Z");
    jobject obj = env->NewObject(arrayListClazz, env->GetMethodID(arrayListClazz, "<init>", "()V"));

    jclass integerClazz = env->FindClass("java/lang/Integer");
    jmethodID intInit = env->GetMethodID(integerClazz, "<init>", "(I)V");

    for (int i=0; i < mFeIds.size(); i++) {
       jobject idObj = env->NewObject(integerClazz, intInit, mFeIds[i]);
       env->CallBooleanMethod(obj, arrayListAdd, idObj);
    }
    return obj;
}

jobject JTuner::openFrontendById(int id) {
    sp<IFrontend> fe;
    Result res;
    mTuner->openFrontendById(id, [&](Result r, const sp<IFrontend>& frontend) {
        fe = frontend;
        res = r;
    });
    if (res != Result::SUCCESS || fe == nullptr) {
        ALOGE("Failed to open frontend");
        return NULL;
    }
    mFe = fe;
    mFeId = id;
    if (mDemux != NULL) {
        mDemux->setFrontendDataSource(mFeId);
    }
    sp<FrontendCallback> feCb = new FrontendCallback(mObject, id);
    fe->setCallback(feCb);

    jint jId = (jint) id;

    JNIEnv *env = AndroidRuntime::getJNIEnv();
    // TODO: add more fields to frontend
    return env->NewObject(
            env->FindClass("android/media/tv/tuner/Tuner$Frontend"),
            gFields.frontendInitID,
            mObject,
            (jint) jId);
}

jint JTuner::closeFrontendById(int id) {
    if (mFe != NULL && mFeId == id) {
        Result r = mFe->close();
        return (jint) r;
    }
    return (jint) Result::SUCCESS;
}

jobject JTuner::getAnalogFrontendCaps(JNIEnv *env, FrontendInfo::FrontendCapabilities& caps) {
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/AnalogFrontendCapabilities");
    jmethodID capsInit = env->GetMethodID(clazz, "<init>", "(II)V");

    jint typeCap = caps.analogCaps().typeCap;
    jint sifStandardCap = caps.analogCaps().sifStandardCap;
    return env->NewObject(clazz, capsInit, typeCap, sifStandardCap);
}

jobject JTuner::getAtsc3FrontendCaps(JNIEnv *env, FrontendInfo::FrontendCapabilities& caps) {
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/Atsc3FrontendCapabilities");
    jmethodID capsInit = env->GetMethodID(clazz, "<init>", "(IIIIII)V");

    jint bandwidthCap = caps.atsc3Caps().bandwidthCap;
    jint modulationCap = caps.atsc3Caps().modulationCap;
    jint timeInterleaveModeCap = caps.atsc3Caps().timeInterleaveModeCap;
    jint codeRateCap = caps.atsc3Caps().codeRateCap;
    jint fecCap = caps.atsc3Caps().fecCap;
    jint demodOutputFormatCap = caps.atsc3Caps().demodOutputFormatCap;

    return env->NewObject(clazz, capsInit, bandwidthCap, modulationCap, timeInterleaveModeCap,
            codeRateCap, fecCap, demodOutputFormatCap);
}

jobject JTuner::getAtscFrontendCaps(JNIEnv *env, FrontendInfo::FrontendCapabilities& caps) {
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/AtscFrontendCapabilities");
    jmethodID capsInit = env->GetMethodID(clazz, "<init>", "(I)V");

    jint modulationCap = caps.atscCaps().modulationCap;

    return env->NewObject(clazz, capsInit, modulationCap);
}

jobject JTuner::getDvbcFrontendCaps(JNIEnv *env, FrontendInfo::FrontendCapabilities& caps) {
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/DvbcFrontendCapabilities");
    jmethodID capsInit = env->GetMethodID(clazz, "<init>", "(III)V");

    jint modulationCap = caps.dvbcCaps().modulationCap;
    jint fecCap = caps.dvbcCaps().fecCap;
    jint annexCap = caps.dvbcCaps().annexCap;

    return env->NewObject(clazz, capsInit, modulationCap, fecCap, annexCap);
}

jobject JTuner::getDvbsFrontendCaps(JNIEnv *env, FrontendInfo::FrontendCapabilities& caps) {
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/DvbsFrontendCapabilities");
    jmethodID capsInit = env->GetMethodID(clazz, "<init>", "(IJI)V");

    jint modulationCap = caps.dvbsCaps().modulationCap;
    jlong innerfecCap = caps.dvbsCaps().innerfecCap;
    jint standard = caps.dvbsCaps().standard;

    return env->NewObject(clazz, capsInit, modulationCap, innerfecCap, standard);
}

jobject JTuner::getDvbtFrontendCaps(JNIEnv *env, FrontendInfo::FrontendCapabilities& caps) {
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/DvbtFrontendCapabilities");
    jmethodID capsInit = env->GetMethodID(clazz, "<init>", "(IIIIIIZZ)V");

    jint transmissionModeCap = caps.dvbtCaps().transmissionModeCap;
    jint bandwidthCap = caps.dvbtCaps().bandwidthCap;
    jint constellationCap = caps.dvbtCaps().constellationCap;
    jint coderateCap = caps.dvbtCaps().coderateCap;
    jint hierarchyCap = caps.dvbtCaps().hierarchyCap;
    jint guardIntervalCap = caps.dvbtCaps().guardIntervalCap;
    jboolean isT2Supported = caps.dvbtCaps().isT2Supported;
    jboolean isMisoSupported = caps.dvbtCaps().isMisoSupported;

    return env->NewObject(clazz, capsInit, transmissionModeCap, bandwidthCap, constellationCap,
            coderateCap, hierarchyCap, guardIntervalCap, isT2Supported, isMisoSupported);
}

jobject JTuner::getIsdbs3FrontendCaps(JNIEnv *env, FrontendInfo::FrontendCapabilities& caps) {
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/Isdbs3FrontendCapabilities");
    jmethodID capsInit = env->GetMethodID(clazz, "<init>", "(II)V");

    jint modulationCap = caps.isdbs3Caps().modulationCap;
    jint coderateCap = caps.isdbs3Caps().coderateCap;

    return env->NewObject(clazz, capsInit, modulationCap, coderateCap);
}

jobject JTuner::getIsdbsFrontendCaps(JNIEnv *env, FrontendInfo::FrontendCapabilities& caps) {
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/IsdbsFrontendCapabilities");
    jmethodID capsInit = env->GetMethodID(clazz, "<init>", "(II)V");

    jint modulationCap = caps.isdbsCaps().modulationCap;
    jint coderateCap = caps.isdbsCaps().coderateCap;

    return env->NewObject(clazz, capsInit, modulationCap, coderateCap);
}

jobject JTuner::getIsdbtFrontendCaps(JNIEnv *env, FrontendInfo::FrontendCapabilities& caps) {
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/IsdbtFrontendCapabilities");
    jmethodID capsInit = env->GetMethodID(clazz, "<init>", "(IIIII)V");

    jint modeCap = caps.isdbtCaps().modeCap;
    jint bandwidthCap = caps.isdbtCaps().bandwidthCap;
    jint modulationCap = caps.isdbtCaps().modulationCap;
    jint coderateCap = caps.isdbtCaps().coderateCap;
    jint guardIntervalCap = caps.isdbtCaps().guardIntervalCap;

    return env->NewObject(clazz, capsInit, modeCap, bandwidthCap, modulationCap, coderateCap,
            guardIntervalCap);
}

jobject JTuner::getFrontendInfo(int id) {
    FrontendInfo feInfo;
    Result res;
    mTuner->getFrontendInfo(id, [&](Result r, const FrontendInfo& info) {
        feInfo = info;
        res = r;
    });
    if (res != Result::SUCCESS) {
        return NULL;
    }

    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/FrontendInfo");
    jmethodID infoInit = env->GetMethodID(clazz, "<init>",
            "(IIIIIIII[ILandroid/media/tv/tuner/frontend/FrontendCapabilities;)V");

    jint type = (jint) feInfo.type;
    jint minFrequency = feInfo.minFrequency;
    jint maxFrequency = feInfo.maxFrequency;
    jint minSymbolRate = feInfo.minSymbolRate;
    jint maxSymbolRate = feInfo.maxSymbolRate;
    jint acquireRange = feInfo.acquireRange;
    jint exclusiveGroupId = feInfo.exclusiveGroupId;
    jintArray statusCaps = env->NewIntArray(feInfo.statusCaps.size());
    env->SetIntArrayRegion(
            statusCaps, 0, feInfo.statusCaps.size(),
            reinterpret_cast<jint*>(&feInfo.statusCaps[0]));
    FrontendInfo::FrontendCapabilities caps = feInfo.frontendCaps;

    jobject jcaps = NULL;
    switch(feInfo.type) {
        case FrontendType::ANALOG:
            if (FrontendInfo::FrontendCapabilities::hidl_discriminator::analogCaps
                    == caps.getDiscriminator()) {
                jcaps = getAnalogFrontendCaps(env, caps);
            }
            break;
        case FrontendType::ATSC3:
            if (FrontendInfo::FrontendCapabilities::hidl_discriminator::atsc3Caps
                    == caps.getDiscriminator()) {
                jcaps = getAtsc3FrontendCaps(env, caps);
            }
            break;
        case FrontendType::ATSC:
            if (FrontendInfo::FrontendCapabilities::hidl_discriminator::atscCaps
                    == caps.getDiscriminator()) {
                jcaps = getAtscFrontendCaps(env, caps);
            }
            break;
        case FrontendType::DVBC:
            if (FrontendInfo::FrontendCapabilities::hidl_discriminator::dvbcCaps
                    == caps.getDiscriminator()) {
                jcaps = getDvbcFrontendCaps(env, caps);
            }
            break;
        case FrontendType::DVBS:
            if (FrontendInfo::FrontendCapabilities::hidl_discriminator::dvbsCaps
                    == caps.getDiscriminator()) {
                jcaps = getDvbsFrontendCaps(env, caps);
            }
            break;
        case FrontendType::DVBT:
            if (FrontendInfo::FrontendCapabilities::hidl_discriminator::dvbtCaps
                    == caps.getDiscriminator()) {
                jcaps = getDvbtFrontendCaps(env, caps);
            }
            break;
        case FrontendType::ISDBS:
            if (FrontendInfo::FrontendCapabilities::hidl_discriminator::isdbsCaps
                    == caps.getDiscriminator()) {
                jcaps = getIsdbsFrontendCaps(env, caps);
            }
            break;
        case FrontendType::ISDBS3:
            if (FrontendInfo::FrontendCapabilities::hidl_discriminator::isdbs3Caps
                    == caps.getDiscriminator()) {
                jcaps = getIsdbs3FrontendCaps(env, caps);
            }
            break;
        case FrontendType::ISDBT:
            if (FrontendInfo::FrontendCapabilities::hidl_discriminator::isdbtCaps
                    == caps.getDiscriminator()) {
                jcaps = getIsdbtFrontendCaps(env, caps);
            }
            break;
        default:
            break;
    }

    return env->NewObject(
            clazz, infoInit, (jint) id, type, minFrequency, maxFrequency, minSymbolRate,
            maxSymbolRate, acquireRange, exclusiveGroupId, statusCaps, jcaps);
}

jintArray JTuner::getLnbIds() {
    ALOGD("JTuner::getLnbIds()");
    Result res;
    hidl_vec<LnbId> lnbIds;
    mTuner->getLnbIds([&](Result r, const hidl_vec<LnbId>& ids) {
        lnbIds = ids;
        res = r;
    });
    if (res != Result::SUCCESS || lnbIds.size() == 0) {
        ALOGW("Lnb isn't available");
        return NULL;
    }

    mLnbIds = lnbIds;
    JNIEnv *env = AndroidRuntime::getJNIEnv();

    jintArray ids = env->NewIntArray(mLnbIds.size());
    env->SetIntArrayRegion(ids, 0, mLnbIds.size(), reinterpret_cast<jint*>(&mLnbIds[0]));

    return ids;
}

jobject JTuner::openLnbById(int id) {
    sp<ILnb> iLnbSp;
    Result r;
    mTuner->openLnbById(id, [&](Result res, const sp<ILnb>& lnb) {
        r = res;
        iLnbSp = lnb;
    });
    if (r != Result::SUCCESS || iLnbSp == nullptr) {
        ALOGE("Failed to open lnb");
        return NULL;
    }
    mLnb = iLnbSp;

    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jobject lnbObj = env->NewObject(
            env->FindClass("android/media/tv/tuner/Lnb"),
            gFields.lnbInitID,
            (jint) id);

    sp<LnbCallback> lnbCb = new LnbCallback(lnbObj, id);
    mLnb->setCallback(lnbCb);

    sp<Lnb> lnbSp = new Lnb(iLnbSp, lnbObj);
    lnbSp->incStrong(lnbObj);
    env->SetLongField(lnbObj, gFields.lnbContext, (jlong) lnbSp.get());

    return lnbObj;
}

jobject JTuner::openLnbByName(jstring name) {
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    std::string lnbName(env->GetStringUTFChars(name, nullptr));
    sp<ILnb> iLnbSp;
    Result res;
    LnbId id;
    mTuner->openLnbByName(lnbName, [&](Result r, LnbId lnbId, const sp<ILnb>& lnb) {
        res = r;
        iLnbSp = lnb;
        id = lnbId;
    });
    if (res != Result::SUCCESS || iLnbSp == nullptr) {
        ALOGE("Failed to open lnb");
        return NULL;
    }
    mLnb = iLnbSp;

    jobject lnbObj = env->NewObject(
            env->FindClass("android/media/tv/tuner/Lnb"),
            gFields.lnbInitID,
            id);

    sp<LnbCallback> lnbCb = new LnbCallback(lnbObj, id);
    mLnb->setCallback(lnbCb);

    sp<Lnb> lnbSp = new Lnb(iLnbSp, lnbObj);
    lnbSp->incStrong(lnbObj);
    env->SetLongField(lnbObj, gFields.lnbContext, (jlong) lnbSp.get());

    return lnbObj;
}

int JTuner::tune(const FrontendSettings& settings) {
    if (mFe == NULL) {
        ALOGE("frontend is not initialized");
        return (int)Result::INVALID_STATE;
    }
    Result result = mFe->tune(settings);
    return (int)result;
}

int JTuner::stopTune() {
    if (mFe == NULL) {
        ALOGE("frontend is not initialized");
        return (int)Result::INVALID_STATE;
    }
    Result result = mFe->stopTune();
    return (int)result;
}

int JTuner::scan(const FrontendSettings& settings, FrontendScanType scanType) {
    if (mFe == NULL) {
        ALOGE("frontend is not initialized");
        return (int)Result::INVALID_STATE;
    }
    Result result = mFe->scan(settings, scanType);
    return (int)result;
}

int JTuner::stopScan() {
    if (mFe == NULL) {
        ALOGE("frontend is not initialized");
        return (int)Result::INVALID_STATE;
    }
    Result result = mFe->stopScan();
    return (int)result;
}

int JTuner::setLnb(int id) {
    if (mFe == NULL) {
        ALOGE("frontend is not initialized");
        return (int)Result::INVALID_STATE;
    }
    Result result = mFe->setLnb(id);
    return (int)result;
}

int JTuner::setLna(bool enable) {
    if (mFe == NULL) {
        ALOGE("frontend is not initialized");
        return (int)Result::INVALID_STATE;
    }
    Result result = mFe->setLna(enable);
    return (int)result;
}

Result JTuner::openDemux() {
    if (mTuner == nullptr) {
        return Result::NOT_INITIALIZED;
    }
    if (mDemux != nullptr) {
        return Result::SUCCESS;
    }
    Result res;
    uint32_t id;
    sp<IDemux> demuxSp;
    mTuner->openDemux([&](Result r, uint32_t demuxId, const sp<IDemux>& demux) {
        demuxSp = demux;
        id = demuxId;
        res = r;
        ALOGD("open demux, id = %d", demuxId);
    });
    if (res == Result::SUCCESS) {
        mDemux = demuxSp;
        mDemuxId = id;
        if (mFe != NULL) {
            mDemux->setFrontendDataSource(mFeId);
        }
    }
    return res;
}

jint JTuner::close() {
    Result res = Result::SUCCESS;
    if (mFe != NULL) {
        res = mFe->close();
        if (res != Result::SUCCESS) {
            return (jint) res;
        }
    }
    if (mDemux != NULL) {
        res = mDemux->close();
        if (res != Result::SUCCESS) {
            return (jint) res;
        }
    }
    return (jint) res;
}

jobject JTuner::getAvSyncHwId(sp<Filter> filter) {
    if (mDemux == NULL) {
        return NULL;
    }

    uint32_t avSyncHwId;
    Result res;
    sp<IFilter> iFilterSp = filter->getIFilter();
    mDemux->getAvSyncHwId(iFilterSp,
            [&](Result r, uint32_t id) {
                res = r;
                avSyncHwId = id;
            });
    if (res == Result::SUCCESS) {
        JNIEnv *env = AndroidRuntime::getJNIEnv();
        jclass integerClazz = env->FindClass("java/lang/Integer");
        jmethodID intInit = env->GetMethodID(integerClazz, "<init>", "(I)V");
        return env->NewObject(integerClazz, intInit, avSyncHwId);
    }
    return NULL;
}

jobject JTuner::getAvSyncTime(jint id) {
    if (mDemux == NULL) {
        return NULL;
    }
    uint64_t time;
    Result res;
    mDemux->getAvSyncTime(static_cast<uint32_t>(id),
            [&](Result r, uint64_t ts) {
                res = r;
                time = ts;
            });
    if (res == Result::SUCCESS) {
        JNIEnv *env = AndroidRuntime::getJNIEnv();
        jclass longClazz = env->FindClass("java/lang/Long");
        jmethodID longInit = env->GetMethodID(longClazz, "<init>", "(J)V");
        return env->NewObject(longClazz, longInit, static_cast<jlong>(time));
    }
    return NULL;
}

int JTuner::connectCiCam(jint id) {
    if (mDemux == NULL) {
        Result r = openDemux();
        if (r != Result::SUCCESS) {
            return (int) r;
        }
    }
    Result r = mDemux->connectCiCam(static_cast<uint32_t>(id));
    return (int) r;
}

int JTuner::disconnectCiCam() {
    if (mDemux == NULL) {
        Result r = openDemux();
        if (r != Result::SUCCESS) {
            return (int) r;
        }
    }
    Result r = mDemux->disconnectCiCam();
    return (int) r;
}

jobject JTuner::openDescrambler() {
    ALOGD("JTuner::openDescrambler");
    if (mTuner == nullptr || mDemux == nullptr) {
        return NULL;
    }
    sp<IDescrambler> descramblerSp;
    Result res;
    mTuner->openDescrambler([&](Result r, const sp<IDescrambler>& descrambler) {
        res = r;
        descramblerSp = descrambler;
    });

    if (res != Result::SUCCESS || descramblerSp == NULL) {
        return NULL;
    }

    descramblerSp->setDemuxSource(mDemuxId);

    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jobject descramblerObj =
            env->NewObject(
                    env->FindClass("android/media/tv/tuner/Descrambler"),
                    gFields.descramblerInitID);

    descramblerSp->incStrong(descramblerObj);
    env->SetLongField(descramblerObj, gFields.descramblerContext, (jlong)descramblerSp.get());

    return descramblerObj;
}

jobject JTuner::openFilter(DemuxFilterType type, int bufferSize) {
    if (mDemux == NULL) {
        if (openDemux() != Result::SUCCESS) {
            return NULL;
        }
    }

    sp<IFilter> iFilterSp;
    sp<FilterCallback> callback = new FilterCallback();
    Result res;
    mDemux->openFilter(type, bufferSize, callback,
            [&](Result r, const sp<IFilter>& filter) {
                iFilterSp = filter;
                res = r;
            });
    if (res != Result::SUCCESS || iFilterSp == NULL) {
        ALOGD("Failed to open filter, type = %d", type.mainType);
        return NULL;
    }
    int fId;
    iFilterSp->getId([&](Result, uint32_t filterId) {
        fId = filterId;
    });

    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jobject filterObj =
            env->NewObject(
                    env->FindClass("android/media/tv/tuner/filter/Filter"),
                    gFields.filterInitID,
                    (jint) fId);

    sp<Filter> filterSp = new Filter(iFilterSp, filterObj);
    filterSp->incStrong(filterObj);
    env->SetLongField(filterObj, gFields.filterContext, (jlong)filterSp.get());

    callback->setFilter(filterSp);

    return filterObj;
}

jobject JTuner::openTimeFilter() {
    if (mDemux == NULL) {
        if (openDemux() != Result::SUCCESS) {
            return NULL;
        }
    }
    sp<ITimeFilter> iTimeFilterSp;
    Result res;
    mDemux->openTimeFilter(
            [&](Result r, const sp<ITimeFilter>& filter) {
                iTimeFilterSp = filter;
                res = r;
            });

    if (res != Result::SUCCESS || iTimeFilterSp == NULL) {
        return NULL;
    }

    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jobject timeFilterObj =
            env->NewObject(
                    env->FindClass("android/media/tv/tuner/filter/TimeFilter"),
                    gFields.timeFilterInitID);
    sp<TimeFilter> timeFilterSp = new TimeFilter(iTimeFilterSp, timeFilterObj);
    timeFilterSp->incStrong(timeFilterObj);
    env->SetLongField(timeFilterObj, gFields.timeFilterContext, (jlong)timeFilterSp.get());

    return timeFilterObj;
}

jobject JTuner::openDvr(DvrType type, jlong bufferSize) {
    ALOGD("JTuner::openDvr");
    if (mDemux == NULL) {
        if (openDemux() != Result::SUCCESS) {
            return NULL;
        }
    }
    sp<IDvr> iDvrSp;
    sp<DvrCallback> callback = new DvrCallback();
    Result res;
    mDemux->openDvr(type, (uint32_t) bufferSize, callback,
            [&](Result r, const sp<IDvr>& dvr) {
                res = r;
                iDvrSp = dvr;
            });

    if (res != Result::SUCCESS || iDvrSp == NULL) {
        return NULL;
    }

    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jobject dvrObj;
    if (type == DvrType::RECORD) {
        dvrObj =
                env->NewObject(
                        env->FindClass("android/media/tv/tuner/dvr/DvrRecorder"),
                        gFields.dvrRecorderInitID,
                        mObject);
        sp<Dvr> dvrSp = new Dvr(iDvrSp, dvrObj);
        dvrSp->incStrong(dvrObj);
        env->SetLongField(dvrObj, gFields.dvrRecorderContext, (jlong)dvrSp.get());
    } else {
        dvrObj =
                env->NewObject(
                        env->FindClass("android/media/tv/tuner/dvr/DvrPlayback"),
                        gFields.dvrPlaybackInitID,
                        mObject);
        sp<Dvr> dvrSp = new Dvr(iDvrSp, dvrObj);
        dvrSp->incStrong(dvrObj);
        env->SetLongField(dvrObj, gFields.dvrPlaybackContext, (jlong)dvrSp.get());
    }

    callback->setDvr(dvrObj);

    return dvrObj;
}

jobject JTuner::getDemuxCaps() {
    DemuxCapabilities caps;
    Result res;
    mTuner->getDemuxCaps([&](Result r, const DemuxCapabilities& demuxCaps) {
        caps = demuxCaps;
        res = r;
    });
    if (res != Result::SUCCESS) {
        return NULL;
    }
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jclass clazz = env->FindClass("android/media/tv/tuner/DemuxCapabilities");
    jmethodID capsInit = env->GetMethodID(clazz, "<init>", "(IIIIIIIIIJI[IZ)V");

    jint numDemux = caps.numDemux;
    jint numRecord = caps.numRecord;
    jint numPlayback = caps.numPlayback;
    jint numTsFilter = caps.numTsFilter;
    jint numSectionFilter = caps.numSectionFilter;
    jint numAudioFilter = caps.numAudioFilter;
    jint numVideoFilter = caps.numVideoFilter;
    jint numPesFilter = caps.numPesFilter;
    jint numPcrFilter = caps.numPcrFilter;
    jlong numBytesInSectionFilter = caps.numBytesInSectionFilter;
    jint filterCaps = static_cast<jint>(caps.filterCaps);
    jboolean bTimeFilter = caps.bTimeFilter;

    jintArray linkCaps = env->NewIntArray(caps.linkCaps.size());
    env->SetIntArrayRegion(
            linkCaps, 0, caps.linkCaps.size(), reinterpret_cast<jint*>(&caps.linkCaps[0]));

    return env->NewObject(clazz, capsInit, numDemux, numRecord, numPlayback, numTsFilter,
            numSectionFilter, numAudioFilter, numVideoFilter, numPesFilter, numPcrFilter,
            numBytesInSectionFilter, filterCaps, linkCaps, bTimeFilter);
}

jobject JTuner::getFrontendStatus(jintArray types) {
    if (mFe == NULL) {
        return NULL;
    }
    JNIEnv *env = AndroidRuntime::getJNIEnv();
    jsize size = env->GetArrayLength(types);
    std::vector<FrontendStatusType> v(size);
    env->GetIntArrayRegion(types, 0, size, reinterpret_cast<jint*>(&v[0]));

    Result res;
    hidl_vec<FrontendStatus> status;
    mFe->getStatus(v,
            [&](Result r, const hidl_vec<FrontendStatus>& s) {
                res = r;
                status = s;
            });
    if (res != Result::SUCCESS) {
        return NULL;
    }

    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/FrontendStatus");
    jmethodID init = env->GetMethodID(clazz, "<init>", "()V");
    jobject statusObj = env->NewObject(clazz, init);

    jclass intClazz = env->FindClass("java/lang/Integer");
    jmethodID initInt = env->GetMethodID(intClazz, "<init>", "(I)V");
    jclass booleanClazz = env->FindClass("java/lang/Boolean");
    jmethodID initBoolean = env->GetMethodID(booleanClazz, "<init>", "(Z)V");

    for (auto s : status) {
        switch(s.getDiscriminator()) {
            case FrontendStatus::hidl_discriminator::isDemodLocked: {
                jfieldID field = env->GetFieldID(clazz, "mIsDemodLocked", "Ljava/lang/Boolean;");
                jobject newBooleanObj = env->NewObject(
                        booleanClazz, initBoolean, static_cast<jboolean>(s.isDemodLocked()));
                env->SetObjectField(statusObj, field, newBooleanObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::snr: {
                jfieldID field = env->GetFieldID(clazz, "mSnr", "Ljava/lang/Integer;");
                jobject newIntegerObj = env->NewObject(
                        intClazz, initInt, static_cast<jint>(s.snr()));
                env->SetObjectField(statusObj, field, newIntegerObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::ber: {
                jfieldID field = env->GetFieldID(clazz, "mBer", "Ljava/lang/Integer;");
                jobject newIntegerObj = env->NewObject(
                        intClazz, initInt, static_cast<jint>(s.ber()));
                env->SetObjectField(statusObj, field, newIntegerObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::per: {
                jfieldID field = env->GetFieldID(clazz, "mPer", "Ljava/lang/Integer;");
                jobject newIntegerObj = env->NewObject(
                        intClazz, initInt, static_cast<jint>(s.per()));
                env->SetObjectField(statusObj, field, newIntegerObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::preBer: {
                jfieldID field = env->GetFieldID(clazz, "mPerBer", "Ljava/lang/Integer;");
                jobject newIntegerObj = env->NewObject(
                        intClazz, initInt, static_cast<jint>(s.preBer()));
                env->SetObjectField(statusObj, field, newIntegerObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::signalQuality: {
                jfieldID field = env->GetFieldID(clazz, "mSignalQuality", "Ljava/lang/Integer;");
                jobject newIntegerObj = env->NewObject(
                        intClazz, initInt, static_cast<jint>(s.signalQuality()));
                env->SetObjectField(statusObj, field, newIntegerObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::signalStrength: {
                jfieldID field = env->GetFieldID(clazz, "mSignalStrength", "Ljava/lang/Integer;");
                jobject newIntegerObj = env->NewObject(
                        intClazz, initInt, static_cast<jint>(s.signalStrength()));
                env->SetObjectField(statusObj, field, newIntegerObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::symbolRate: {
                jfieldID field = env->GetFieldID(clazz, "mSymbolRate", "Ljava/lang/Integer;");
                jobject newIntegerObj = env->NewObject(
                        intClazz, initInt, static_cast<jint>(s.symbolRate()));
                env->SetObjectField(statusObj, field, newIntegerObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::innerFec: {
                jfieldID field = env->GetFieldID(clazz, "mInnerFec", "Ljava/lang/Long;");
                jclass longClazz = env->FindClass("java/lang/Long");
                jmethodID initLong = env->GetMethodID(longClazz, "<init>", "(J)V");
                jobject newLongObj = env->NewObject(
                        longClazz, initLong, static_cast<jlong>(s.innerFec()));
                env->SetObjectField(statusObj, field, newLongObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::modulation: {
                jfieldID field = env->GetFieldID(clazz, "mModulation", "Ljava/lang/Integer;");
                FrontendModulationStatus modulation = s.modulation();
                jint intModulation;
                bool valid = true;
                switch(modulation.getDiscriminator()) {
                    case FrontendModulationStatus::hidl_discriminator::dvbc: {
                        intModulation = static_cast<jint>(modulation.dvbc());
                        break;
                    }
                    case FrontendModulationStatus::hidl_discriminator::dvbs: {
                        intModulation = static_cast<jint>(modulation.dvbs());
                        break;
                    }
                    case FrontendModulationStatus::hidl_discriminator::isdbs: {
                        intModulation = static_cast<jint>(modulation.isdbs());
                        break;
                    }
                    case FrontendModulationStatus::hidl_discriminator::isdbs3: {
                        intModulation = static_cast<jint>(modulation.isdbs3());
                        break;
                    }
                    case FrontendModulationStatus::hidl_discriminator::isdbt: {
                        intModulation = static_cast<jint>(modulation.isdbt());
                        break;
                    }
                    default: {
                        valid = false;
                        break;
                    }
                }
                if (valid) {
                    jobject newIntegerObj = env->NewObject(intClazz, initInt, intModulation);
                    env->SetObjectField(statusObj, field, newIntegerObj);
                }
                break;
            }
            case FrontendStatus::hidl_discriminator::inversion: {
                jfieldID field = env->GetFieldID(clazz, "mInversion", "Ljava/lang/Integer;");
                jobject newIntegerObj = env->NewObject(
                        intClazz, initInt, static_cast<jint>(s.inversion()));
                env->SetObjectField(statusObj, field, newIntegerObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::lnbVoltage: {
                jfieldID field = env->GetFieldID(clazz, "mLnbVoltage", "Ljava/lang/Integer;");
                jobject newIntegerObj = env->NewObject(
                        intClazz, initInt, static_cast<jint>(s.lnbVoltage()));
                env->SetObjectField(statusObj, field, newIntegerObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::plpId: {
                jfieldID field = env->GetFieldID(clazz, "mPlpId", "Ljava/lang/Integer;");
                jobject newIntegerObj = env->NewObject(
                        intClazz, initInt, static_cast<jint>(s.plpId()));
                env->SetObjectField(statusObj, field, newIntegerObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::isEWBS: {
                jfieldID field = env->GetFieldID(clazz, "mIsEwbs", "Ljava/lang/Boolean;");
                jobject newBooleanObj = env->NewObject(
                        booleanClazz, initBoolean, static_cast<jboolean>(s.isEWBS()));
                env->SetObjectField(statusObj, field, newBooleanObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::agc: {
                jfieldID field = env->GetFieldID(clazz, "mAgc", "Ljava/lang/Integer;");
                jobject newIntegerObj = env->NewObject(
                        intClazz, initInt, static_cast<jint>(s.agc()));
                env->SetObjectField(statusObj, field, newIntegerObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::isLnaOn: {
                jfieldID field = env->GetFieldID(clazz, "mIsLnaOn", "Ljava/lang/Boolean;");
                jobject newBooleanObj = env->NewObject(
                        booleanClazz, initBoolean, static_cast<jboolean>(s.isLnaOn()));
                env->SetObjectField(statusObj, field, newBooleanObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::isLayerError: {
                jfieldID field = env->GetFieldID(clazz, "mIsLayerErrors", "[Z");
                hidl_vec<bool> layerErr = s.isLayerError();

                jbooleanArray valObj = env->NewBooleanArray(layerErr.size());

                for (size_t i = 0; i < layerErr.size(); i++) {
                    jboolean x = layerErr[i];
                    env->SetBooleanArrayRegion(valObj, i, 1, &x);
                }
                env->SetObjectField(statusObj, field, valObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::mer: {
                jfieldID field = env->GetFieldID(clazz, "mMer", "Ljava/lang/Integer;");
                jobject newIntegerObj = env->NewObject(
                        intClazz, initInt, static_cast<jint>(s.mer()));
                env->SetObjectField(statusObj, field, newIntegerObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::freqOffset: {
                jfieldID field = env->GetFieldID(clazz, "mFreqOffset", "Ljava/lang/Integer;");
                jobject newIntegerObj = env->NewObject(
                        intClazz, initInt, static_cast<jint>(s.freqOffset()));
                env->SetObjectField(statusObj, field, newIntegerObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::hierarchy: {
                jfieldID field = env->GetFieldID(clazz, "mHierarchy", "Ljava/lang/Integer;");
                jobject newIntegerObj = env->NewObject(
                        intClazz, initInt, static_cast<jint>(s.hierarchy()));
                env->SetObjectField(statusObj, field, newIntegerObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::isRfLocked: {
                jfieldID field = env->GetFieldID(clazz, "mIsRfLocked", "Ljava/lang/Boolean;");
                jobject newBooleanObj = env->NewObject(
                        booleanClazz, initBoolean, static_cast<jboolean>(s.isRfLocked()));
                env->SetObjectField(statusObj, field, newBooleanObj);
                break;
            }
            case FrontendStatus::hidl_discriminator::plpInfo: {
                jfieldID field = env->GetFieldID(clazz, "mPlpInfo",
                        "[Landroid/media/tv/tuner/frontend/FrontendStatus$Atsc3PlpTuningInfo;");
                jclass plpClazz = env->FindClass(
                        "android/media/tv/tuner/frontend/FrontendStatus$Atsc3PlpTuningInfo");
                jmethodID initPlp = env->GetMethodID(plpClazz, "<init>", "(IZI)V");

                hidl_vec<FrontendStatusAtsc3PlpInfo> plpInfos = s.plpInfo();

                jobjectArray valObj = env->NewObjectArray(plpInfos.size(), plpClazz, NULL);
                for (int i = 0; i < plpInfos.size(); i++) {
                    auto info = plpInfos[i];
                    jint plpId = (jint) info.plpId;
                    jboolean isLocked = (jboolean) info.isLocked;
                    jint uec = (jint) info.uec;

                    jobject plpObj = env->NewObject(plpClazz, initPlp, plpId, isLocked, uec);
                    env->SetObjectArrayElement(valObj, i, plpObj);
                }

                env->SetObjectField(statusObj, field, valObj);
                break;
            }
            default: {
                break;
            }
        }
    }

    return statusObj;
}

jint JTuner::closeFrontend() {
    Result r = Result::SUCCESS;
    if (mFe != NULL) {
        r = mFe->close();
    }
    return (jint) r;
}

jint JTuner::closeDemux() {
    Result r = Result::SUCCESS;
    if (mDemux != NULL) {
        r = mDemux->close();
    }
    return (jint) r;
}

}  // namespace android

////////////////////////////////////////////////////////////////////////////////

using namespace android;

static sp<JTuner> setTuner(JNIEnv *env, jobject thiz, const sp<JTuner> &tuner) {
    sp<JTuner> old = (JTuner *)env->GetLongField(thiz, gFields.tunerContext);

    if (tuner != NULL) {
        tuner->incStrong(thiz);
    }
    if (old != NULL) {
        old->decStrong(thiz);
    }
    env->SetLongField(thiz, gFields.tunerContext, (jlong)tuner.get());

    return old;
}

static sp<JTuner> getTuner(JNIEnv *env, jobject thiz) {
    return (JTuner *)env->GetLongField(thiz, gFields.tunerContext);
}

static sp<IDescrambler> getDescrambler(JNIEnv *env, jobject descrambler) {
    return (IDescrambler *)env->GetLongField(descrambler, gFields.descramblerContext);
}

static uint32_t getResourceIdFromHandle(jint handle) {
    return (handle & 0x00ff0000) >> 16;
}

static DemuxPid getDemuxPid(int pidType, int pid) {
    DemuxPid demuxPid;
    if ((int)pidType == 1) {
        demuxPid.tPid(static_cast<DemuxTpid>(pid));
    } else if ((int)pidType == 2) {
        demuxPid.mmtpPid(static_cast<DemuxMmtpPid>(pid));
    }
    return demuxPid;
}

static uint32_t getFrontendSettingsFreq(JNIEnv *env, const jobject& settings) {
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/FrontendSettings");
    jfieldID freqField = env->GetFieldID(clazz, "mFrequency", "I");
    uint32_t freq = static_cast<uint32_t>(env->GetIntField(settings, freqField));
    return freq;
}

static FrontendSettings getAnalogFrontendSettings(JNIEnv *env, const jobject& settings) {
    FrontendSettings frontendSettings;
    uint32_t freq = getFrontendSettingsFreq(env, settings);
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/AnalogFrontendSettings");
    FrontendAnalogType analogType =
            static_cast<FrontendAnalogType>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mSignalType", "I")));
    FrontendAnalogSifStandard sifStandard =
            static_cast<FrontendAnalogSifStandard>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mSifStandard", "I")));
    FrontendAnalogSettings frontendAnalogSettings {
            .frequency = freq,
            .type = analogType,
            .sifStandard = sifStandard,
    };
    frontendSettings.analog(frontendAnalogSettings);
    return frontendSettings;
}

static hidl_vec<FrontendAtsc3PlpSettings> getAtsc3PlpSettings(
        JNIEnv *env, const jobject& settings) {
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/Atsc3FrontendSettings");
    jobjectArray plpSettings =
            reinterpret_cast<jobjectArray>(
                    env->GetObjectField(settings,
                            env->GetFieldID(
                                    clazz,
                                    "mPlpSettings",
                                    "[Landroid/media/tv/tuner/frontend/Atsc3PlpSettings;")));
    int len = env->GetArrayLength(plpSettings);

    jclass plpClazz = env->FindClass("android/media/tv/tuner/frontend/Atsc3PlpSettings");
    hidl_vec<FrontendAtsc3PlpSettings> plps = hidl_vec<FrontendAtsc3PlpSettings>(len);
    // parse PLP settings
    for (int i = 0; i < len; i++) {
        jobject plp = env->GetObjectArrayElement(plpSettings, i);
        uint8_t plpId =
                static_cast<uint8_t>(
                        env->GetIntField(plp, env->GetFieldID(plpClazz, "mPlpId", "I")));
        FrontendAtsc3Modulation modulation =
                static_cast<FrontendAtsc3Modulation>(
                        env->GetIntField(plp, env->GetFieldID(plpClazz, "mModulation", "I")));
        FrontendAtsc3TimeInterleaveMode interleaveMode =
                static_cast<FrontendAtsc3TimeInterleaveMode>(
                        env->GetIntField(
                                plp, env->GetFieldID(plpClazz, "mInterleaveMode", "I")));
        FrontendAtsc3CodeRate codeRate =
                static_cast<FrontendAtsc3CodeRate>(
                        env->GetIntField(plp, env->GetFieldID(plpClazz, "mCodeRate", "I")));
        FrontendAtsc3Fec fec =
                static_cast<FrontendAtsc3Fec>(
                        env->GetIntField(plp, env->GetFieldID(plpClazz, "mFec", "I")));
        FrontendAtsc3PlpSettings frontendAtsc3PlpSettings {
                .plpId = plpId,
                .modulation = modulation,
                .interleaveMode = interleaveMode,
                .codeRate = codeRate,
                .fec = fec,
        };
        plps[i] = frontendAtsc3PlpSettings;
    }
    return plps;
}

static FrontendSettings getAtsc3FrontendSettings(JNIEnv *env, const jobject& settings) {
    FrontendSettings frontendSettings;
    uint32_t freq = getFrontendSettingsFreq(env, settings);
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/Atsc3FrontendSettings");

    FrontendAtsc3Bandwidth bandwidth =
            static_cast<FrontendAtsc3Bandwidth>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mBandwidth", "I")));
    FrontendAtsc3DemodOutputFormat demod =
            static_cast<FrontendAtsc3DemodOutputFormat>(
                    env->GetIntField(
                            settings, env->GetFieldID(clazz, "mDemodOutputFormat", "I")));
    hidl_vec<FrontendAtsc3PlpSettings> plps = getAtsc3PlpSettings(env, settings);
    FrontendAtsc3Settings frontendAtsc3Settings {
            .frequency = freq,
            .bandwidth = bandwidth,
            .demodOutputFormat = demod,
            .plpSettings = plps,
    };
    frontendSettings.atsc3(frontendAtsc3Settings);
    return frontendSettings;
}

static FrontendSettings getAtscFrontendSettings(JNIEnv *env, const jobject& settings) {
    FrontendSettings frontendSettings;
    uint32_t freq = getFrontendSettingsFreq(env, settings);
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/AtscFrontendSettings");
    FrontendAtscModulation modulation =
            static_cast<FrontendAtscModulation>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mModulation", "I")));
    FrontendAtscSettings frontendAtscSettings {
            .frequency = freq,
            .modulation = modulation,
    };
    frontendSettings.atsc(frontendAtscSettings);
    return frontendSettings;
}

static FrontendSettings getDvbcFrontendSettings(JNIEnv *env, const jobject& settings) {
    FrontendSettings frontendSettings;
    uint32_t freq = getFrontendSettingsFreq(env, settings);
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/DvbcFrontendSettings");
    FrontendDvbcModulation modulation =
            static_cast<FrontendDvbcModulation>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mModulation", "I")));
    FrontendInnerFec innerFec =
            static_cast<FrontendInnerFec>(
                    env->GetLongField(settings, env->GetFieldID(clazz, "mFec", "J")));
    uint32_t symbolRate =
            static_cast<uint32_t>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mSymbolRate", "I")));
    FrontendDvbcOuterFec outerFec =
            static_cast<FrontendDvbcOuterFec>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mOuterFec", "I")));
    FrontendDvbcAnnex annex =
            static_cast<FrontendDvbcAnnex>(
                    env->GetByteField(settings, env->GetFieldID(clazz, "mAnnex", "B")));
    FrontendDvbcSpectralInversion spectralInversion =
            static_cast<FrontendDvbcSpectralInversion>(
                    env->GetIntField(
                            settings, env->GetFieldID(clazz, "mSpectralInversion", "I")));
    FrontendDvbcSettings frontendDvbcSettings {
            .frequency = freq,
            .modulation = modulation,
            .fec = innerFec,
            .symbolRate = symbolRate,
            .outerFec = outerFec,
            .annex = annex,
            .spectralInversion = spectralInversion,
    };
    frontendSettings.dvbc(frontendDvbcSettings);
    return frontendSettings;
}

static FrontendDvbsCodeRate getDvbsCodeRate(JNIEnv *env, const jobject& settings) {
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/DvbsFrontendSettings");
    jobject jcodeRate =
            env->GetObjectField(settings,
                    env->GetFieldID(
                            clazz,
                            "mCodeRate",
                            "Landroid/media/tv/tuner/frontend/DvbsCodeRate;"));

    jclass codeRateClazz = env->FindClass("android/media/tv/tuner/frontend/DvbsCodeRate");
    FrontendInnerFec innerFec =
            static_cast<FrontendInnerFec>(
                    env->GetLongField(
                            jcodeRate, env->GetFieldID(codeRateClazz, "mInnerFec", "J")));
    bool isLinear =
            static_cast<bool>(
                    env->GetBooleanField(
                            jcodeRate, env->GetFieldID(codeRateClazz, "mIsLinear", "Z")));
    bool isShortFrames =
            static_cast<bool>(
                    env->GetBooleanField(
                            jcodeRate, env->GetFieldID(codeRateClazz, "mIsShortFrames", "Z")));
    uint32_t bitsPer1000Symbol =
            static_cast<uint32_t>(
                    env->GetIntField(
                            jcodeRate, env->GetFieldID(
                                    codeRateClazz, "mBitsPer1000Symbol", "I")));
    FrontendDvbsCodeRate coderate {
            .fec = innerFec,
            .isLinear = isLinear,
            .isShortFrames = isShortFrames,
            .bitsPer1000Symbol = bitsPer1000Symbol,
    };
    return coderate;
}

static FrontendSettings getDvbsFrontendSettings(JNIEnv *env, const jobject& settings) {
    FrontendSettings frontendSettings;
    uint32_t freq = getFrontendSettingsFreq(env, settings);
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/DvbsFrontendSettings");


    FrontendDvbsModulation modulation =
            static_cast<FrontendDvbsModulation>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mModulation", "I")));
    uint32_t symbolRate =
            static_cast<uint32_t>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mSymbolRate", "I")));
    FrontendDvbsRolloff rolloff =
            static_cast<FrontendDvbsRolloff>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mRolloff", "I")));
    FrontendDvbsPilot pilot =
            static_cast<FrontendDvbsPilot>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mPilot", "I")));
    uint32_t inputStreamId =
            static_cast<uint32_t>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mInputStreamId", "I")));
    FrontendDvbsStandard standard =
            static_cast<FrontendDvbsStandard>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mStandard", "I")));
    FrontendDvbsVcmMode vcmMode =
            static_cast<FrontendDvbsVcmMode>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mVcmMode", "I")));
    FrontendDvbsCodeRate coderate = getDvbsCodeRate(env, settings);

    FrontendDvbsSettings frontendDvbsSettings {
            .frequency = freq,
            .modulation = modulation,
            .coderate = coderate,
            .symbolRate = symbolRate,
            .rolloff = rolloff,
            .pilot = pilot,
            .inputStreamId = inputStreamId,
            .standard = standard,
            .vcmMode = vcmMode,
    };
    frontendSettings.dvbs(frontendDvbsSettings);
    return frontendSettings;
}

static FrontendSettings getDvbtFrontendSettings(JNIEnv *env, const jobject& settings) {
    FrontendSettings frontendSettings;
    uint32_t freq = getFrontendSettingsFreq(env, settings);
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/DvbtFrontendSettings");
    FrontendDvbtTransmissionMode transmissionMode =
            static_cast<FrontendDvbtTransmissionMode>(
                    env->GetIntField(
                            settings, env->GetFieldID(clazz, "mTransmissionMode", "I")));
    FrontendDvbtBandwidth bandwidth =
            static_cast<FrontendDvbtBandwidth>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mBandwidth", "I")));
    FrontendDvbtConstellation constellation =
            static_cast<FrontendDvbtConstellation>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mConstellation", "I")));
    FrontendDvbtHierarchy hierarchy =
            static_cast<FrontendDvbtHierarchy>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mHierarchy", "I")));
    FrontendDvbtCoderate hpCoderate =
            static_cast<FrontendDvbtCoderate>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mHpCodeRate", "I")));
    FrontendDvbtCoderate lpCoderate =
            static_cast<FrontendDvbtCoderate>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mLpCodeRate", "I")));
    FrontendDvbtGuardInterval guardInterval =
            static_cast<FrontendDvbtGuardInterval>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mGuardInterval", "I")));
    bool isHighPriority =
            static_cast<bool>(
                    env->GetBooleanField(
                            settings, env->GetFieldID(clazz, "mIsHighPriority", "Z")));
    FrontendDvbtStandard standard =
            static_cast<FrontendDvbtStandard>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mStandard", "I")));
    bool isMiso =
            static_cast<bool>(
                    env->GetBooleanField(settings, env->GetFieldID(clazz, "mIsMiso", "Z")));
    FrontendDvbtPlpMode plpMode =
            static_cast<FrontendDvbtPlpMode>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mPlpMode", "I")));
    uint8_t plpId =
            static_cast<uint8_t>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mPlpId", "I")));
    uint8_t plpGroupId =
            static_cast<uint8_t>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mPlpGroupId", "I")));

    FrontendDvbtSettings frontendDvbtSettings {
            .frequency = freq,
            .transmissionMode = transmissionMode,
            .bandwidth = bandwidth,
            .constellation = constellation,
            .hierarchy = hierarchy,
            .hpCoderate = hpCoderate,
            .lpCoderate = lpCoderate,
            .guardInterval = guardInterval,
            .isHighPriority = isHighPriority,
            .standard = standard,
            .isMiso = isMiso,
            .plpMode = plpMode,
            .plpId = plpId,
            .plpGroupId = plpGroupId,
    };
    frontendSettings.dvbt(frontendDvbtSettings);
    return frontendSettings;
}

static FrontendSettings getIsdbsFrontendSettings(JNIEnv *env, const jobject& settings) {
    FrontendSettings frontendSettings;
    uint32_t freq = getFrontendSettingsFreq(env, settings);
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/IsdbsFrontendSettings");
    uint16_t streamId =
            static_cast<uint16_t>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mStreamId", "I")));
    FrontendIsdbsStreamIdType streamIdType =
            static_cast<FrontendIsdbsStreamIdType>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mStreamIdType", "I")));
    FrontendIsdbsModulation modulation =
            static_cast<FrontendIsdbsModulation>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mModulation", "I")));
    FrontendIsdbsCoderate coderate =
            static_cast<FrontendIsdbsCoderate>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mCodeRate", "I")));
    uint32_t symbolRate =
            static_cast<uint32_t>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mSymbolRate", "I")));
    FrontendIsdbsRolloff rolloff =
            static_cast<FrontendIsdbsRolloff>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mRolloff", "I")));

    FrontendIsdbsSettings frontendIsdbsSettings {
            .frequency = freq,
            .streamId = streamId,
            .streamIdType = streamIdType,
            .modulation = modulation,
            .coderate = coderate,
            .symbolRate = symbolRate,
            .rolloff = rolloff,
    };
    frontendSettings.isdbs(frontendIsdbsSettings);
    return frontendSettings;
}

static FrontendSettings getIsdbs3FrontendSettings(JNIEnv *env, const jobject& settings) {
    FrontendSettings frontendSettings;
    uint32_t freq = getFrontendSettingsFreq(env, settings);
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/Isdbs3FrontendSettings");
    uint16_t streamId =
            static_cast<uint16_t>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mStreamId", "I")));
    FrontendIsdbsStreamIdType streamIdType =
            static_cast<FrontendIsdbsStreamIdType>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mStreamIdType", "I")));
    FrontendIsdbs3Modulation modulation =
            static_cast<FrontendIsdbs3Modulation>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mModulation", "I")));
    FrontendIsdbs3Coderate coderate =
            static_cast<FrontendIsdbs3Coderate>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mCodeRate", "I")));
    uint32_t symbolRate =
            static_cast<uint32_t>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mSymbolRate", "I")));
    FrontendIsdbs3Rolloff rolloff =
            static_cast<FrontendIsdbs3Rolloff>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mRolloff", "I")));

    FrontendIsdbs3Settings frontendIsdbs3Settings {
            .frequency = freq,
            .streamId = streamId,
            .streamIdType = streamIdType,
            .modulation = modulation,
            .coderate = coderate,
            .symbolRate = symbolRate,
            .rolloff = rolloff,
    };
    frontendSettings.isdbs3(frontendIsdbs3Settings);
    return frontendSettings;
}

static FrontendSettings getIsdbtFrontendSettings(JNIEnv *env, const jobject& settings) {
    FrontendSettings frontendSettings;
    uint32_t freq = getFrontendSettingsFreq(env, settings);
    jclass clazz = env->FindClass("android/media/tv/tuner/frontend/IsdbtFrontendSettings");
    FrontendIsdbtModulation modulation =
            static_cast<FrontendIsdbtModulation>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mModulation", "I")));
    FrontendIsdbtBandwidth bandwidth =
            static_cast<FrontendIsdbtBandwidth>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mBandwidth", "I")));
    FrontendIsdbtMode mode =
            static_cast<FrontendIsdbtMode>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mMode", "I")));
    FrontendIsdbtCoderate coderate =
            static_cast<FrontendIsdbtCoderate>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mCodeRate", "I")));
    FrontendIsdbtGuardInterval guardInterval =
            static_cast<FrontendIsdbtGuardInterval>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mGuardInterval", "I")));
    uint32_t serviceAreaId =
            static_cast<uint32_t>(
                    env->GetIntField(settings, env->GetFieldID(clazz, "mServiceAreaId", "I")));

    FrontendIsdbtSettings frontendIsdbtSettings {
            .frequency = freq,
            .modulation = modulation,
            .bandwidth = bandwidth,
            .mode = mode,
            .coderate = coderate,
            .guardInterval = guardInterval,
            .serviceAreaId = serviceAreaId,
    };
    frontendSettings.isdbt(frontendIsdbtSettings);
    return frontendSettings;
}

static FrontendSettings getFrontendSettings(JNIEnv *env, int type, jobject settings) {
    ALOGD("getFrontendSettings %d", type);

    FrontendType feType = static_cast<FrontendType>(type);
    switch(feType) {
        case FrontendType::ANALOG:
            return getAnalogFrontendSettings(env, settings);
        case FrontendType::ATSC3:
            return getAtsc3FrontendSettings(env, settings);
        case FrontendType::ATSC:
            return getAtscFrontendSettings(env, settings);
        case FrontendType::DVBC:
            return getDvbcFrontendSettings(env, settings);
        case FrontendType::DVBS:
            return getDvbsFrontendSettings(env, settings);
        case FrontendType::DVBT:
            return getDvbtFrontendSettings(env, settings);
        case FrontendType::ISDBS:
            return getIsdbsFrontendSettings(env, settings);
        case FrontendType::ISDBS3:
            return getIsdbs3FrontendSettings(env, settings);
        case FrontendType::ISDBT:
            return getIsdbtFrontendSettings(env, settings);
        default:
            // should never happen because a type is associated with a subclass of
            // FrontendSettings and not set by users
            jniThrowExceptionFmt(env, "java/lang/IllegalArgumentException",
                "Unsupported frontend type %d", type);
            return FrontendSettings();
    }
}

static sp<Filter> getFilter(JNIEnv *env, jobject filter) {
    return (Filter *)env->GetLongField(filter, gFields.filterContext);
}

static DvrSettings getDvrSettings(JNIEnv *env, jobject settings, bool isRecorder) {
    DvrSettings dvrSettings;
    jclass clazz = env->FindClass("android/media/tv/tuner/dvr/DvrSettings");
    uint32_t statusMask =
            static_cast<uint32_t>(env->GetIntField(
                    settings, env->GetFieldID(clazz, "mStatusMask", "I")));
    uint32_t lowThreshold =
            static_cast<uint32_t>(env->GetLongField(
                    settings, env->GetFieldID(clazz, "mLowThreshold", "J")));
    uint32_t highThreshold =
            static_cast<uint32_t>(env->GetLongField(
                    settings, env->GetFieldID(clazz, "mHighThreshold", "J")));
    uint8_t packetSize =
            static_cast<uint8_t>(env->GetLongField(
                    settings, env->GetFieldID(clazz, "mPacketSize", "J")));
    DataFormat dataFormat =
            static_cast<DataFormat>(env->GetIntField(
                    settings, env->GetFieldID(clazz, "mDataFormat", "I")));
    if (isRecorder) {
        RecordSettings recordSettings {
                .statusMask = static_cast<unsigned char>(statusMask),
                .lowThreshold = lowThreshold,
                .highThreshold = highThreshold,
                .dataFormat = dataFormat,
                .packetSize = packetSize,
        };
        dvrSettings.record(recordSettings);
    } else {
        PlaybackSettings PlaybackSettings {
                .statusMask = statusMask,
                .lowThreshold = lowThreshold,
                .highThreshold = highThreshold,
                .dataFormat = dataFormat,
                .packetSize = packetSize,
        };
        dvrSettings.playback(PlaybackSettings);
    }
    return dvrSettings;
}

static sp<Dvr> getDvr(JNIEnv *env, jobject dvr) {
    bool isRecorder =
            env->IsInstanceOf(dvr, env->FindClass("android/media/tv/tuner/dvr/DvrRecorder"));
    jfieldID fieldId =
            isRecorder ? gFields.dvrRecorderContext : gFields.dvrPlaybackContext;
    return (Dvr *)env->GetLongField(dvr, fieldId);
}

static void android_media_tv_Tuner_native_init(JNIEnv *env) {
    jclass clazz = env->FindClass("android/media/tv/tuner/Tuner");
    CHECK(clazz != NULL);

    gFields.tunerContext = env->GetFieldID(clazz, "mNativeContext", "J");
    CHECK(gFields.tunerContext != NULL);

    gFields.onFrontendEventID = env->GetMethodID(clazz, "onFrontendEvent", "(I)V");

    jclass frontendClazz = env->FindClass("android/media/tv/tuner/Tuner$Frontend");
    gFields.frontendInitID =
            env->GetMethodID(frontendClazz, "<init>", "(Landroid/media/tv/tuner/Tuner;I)V");

    jclass lnbClazz = env->FindClass("android/media/tv/tuner/Lnb");
    gFields.lnbContext = env->GetFieldID(lnbClazz, "mNativeContext", "J");
    gFields.lnbInitID = env->GetMethodID(lnbClazz, "<init>", "(I)V");
    gFields.onLnbEventID = env->GetMethodID(lnbClazz, "onEvent", "(I)V");
    gFields.onLnbDiseqcMessageID = env->GetMethodID(lnbClazz, "onDiseqcMessage", "([B)V");

    jclass filterClazz = env->FindClass("android/media/tv/tuner/filter/Filter");
    gFields.filterContext = env->GetFieldID(filterClazz, "mNativeContext", "J");
    gFields.filterInitID =
            env->GetMethodID(filterClazz, "<init>", "(I)V");
    gFields.onFilterStatusID =
            env->GetMethodID(filterClazz, "onFilterStatus", "(I)V");
    gFields.onFilterEventID =
            env->GetMethodID(filterClazz, "onFilterEvent",
                    "([Landroid/media/tv/tuner/filter/FilterEvent;)V");

    jclass timeFilterClazz = env->FindClass("android/media/tv/tuner/filter/TimeFilter");
    gFields.timeFilterContext = env->GetFieldID(timeFilterClazz, "mNativeContext", "J");
    gFields.timeFilterInitID = env->GetMethodID(timeFilterClazz, "<init>", "()V");

    jclass descramblerClazz = env->FindClass("android/media/tv/tuner/Descrambler");
    gFields.descramblerContext = env->GetFieldID(descramblerClazz, "mNativeContext", "J");
    gFields.descramblerInitID = env->GetMethodID(descramblerClazz, "<init>", "()V");

    jclass dvrRecorderClazz = env->FindClass("android/media/tv/tuner/dvr/DvrRecorder");
    gFields.dvrRecorderContext = env->GetFieldID(dvrRecorderClazz, "mNativeContext", "J");
    gFields.dvrRecorderInitID = env->GetMethodID(dvrRecorderClazz, "<init>", "()V");
    gFields.onDvrRecordStatusID =
            env->GetMethodID(dvrRecorderClazz, "onRecordStatusChanged", "(I)V");

    jclass dvrPlaybackClazz = env->FindClass("android/media/tv/tuner/dvr/DvrPlayback");
    gFields.dvrPlaybackContext = env->GetFieldID(dvrPlaybackClazz, "mNativeContext", "J");
    gFields.dvrPlaybackInitID = env->GetMethodID(dvrPlaybackClazz, "<init>", "()V");
    gFields.onDvrPlaybackStatusID =
            env->GetMethodID(dvrPlaybackClazz, "onPlaybackStatusChanged", "(I)V");

    jclass mediaEventClazz = env->FindClass("android/media/tv/tuner/filter/MediaEvent");
    gFields.mediaEventContext = env->GetFieldID(mediaEventClazz, "mNativeContext", "J");

    jclass linearBlockClazz = env->FindClass("android/media/MediaCodec$LinearBlock");
    gFields.linearBlockInitID = env->GetMethodID(linearBlockClazz, "<init>", "()V");
    gFields.linearBlockSetInternalStateID =
            env->GetMethodID(linearBlockClazz, "setInternalStateLocked", "(JZ)V");
}

static void android_media_tv_Tuner_native_setup(JNIEnv *env, jobject thiz) {
    sp<JTuner> tuner = new JTuner(env, thiz);
    setTuner(env,thiz, tuner);
}

static jobject android_media_tv_Tuner_get_frontend_ids(JNIEnv *env, jobject thiz) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->getFrontendIds();
}

static jobject android_media_tv_Tuner_open_frontend_by_handle(
        JNIEnv *env, jobject thiz, jint handle) {
    sp<JTuner> tuner = getTuner(env, thiz);
    uint32_t id = getResourceIdFromHandle(handle);
    return tuner->openFrontendById(id);
}

static jint android_media_tv_Tuner_close_frontend_by_handle(
        JNIEnv *env, jobject thiz, jint handle) {
    sp<JTuner> tuner = getTuner(env, thiz);
    uint32_t id = getResourceIdFromHandle(handle);
    return tuner->closeFrontendById(id);
}

static int android_media_tv_Tuner_tune(JNIEnv *env, jobject thiz, jint type, jobject settings) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->tune(getFrontendSettings(env, type, settings));
}

static int android_media_tv_Tuner_stop_tune(JNIEnv *env, jobject thiz) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->stopTune();
}

static int android_media_tv_Tuner_scan(
        JNIEnv *env, jobject thiz, jint settingsType, jobject settings, jint scanType) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->scan(getFrontendSettings(
            env, settingsType, settings), static_cast<FrontendScanType>(scanType));
}

static int android_media_tv_Tuner_stop_scan(JNIEnv *env, jobject thiz) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->stopScan();
}

static int android_media_tv_Tuner_set_lnb(JNIEnv *env, jobject thiz, jint id) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->setLnb(id);
}

static int android_media_tv_Tuner_set_lna(JNIEnv *env, jobject thiz, jboolean enable) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->setLna(enable);
}

static jobject android_media_tv_Tuner_get_frontend_status(
        JNIEnv* env, jobject thiz, jintArray types) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->getFrontendStatus(types);
}

static jobject android_media_tv_Tuner_get_av_sync_hw_id(
        JNIEnv *env, jobject thiz, jobject filter) {
    sp<Filter> filterSp = getFilter(env, filter);
    if (filterSp == NULL) {
        ALOGD("Failed to get sync ID. Filter not found");
        return NULL;
    }
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->getAvSyncHwId(filterSp);
}

static jobject android_media_tv_Tuner_get_av_sync_time(JNIEnv *env, jobject thiz, jint id) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->getAvSyncTime(id);
}

static int android_media_tv_Tuner_connect_cicam(JNIEnv *env, jobject thiz, jint id) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->connectCiCam(id);
}

static int android_media_tv_Tuner_disconnect_cicam(JNIEnv *env, jobject thiz) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->disconnectCiCam();
}

static jobject android_media_tv_Tuner_get_frontend_info(JNIEnv *env, jobject thiz, jint id) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->getFrontendInfo(id);
}

static jintArray android_media_tv_Tuner_get_lnb_ids(JNIEnv *env, jobject thiz) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->getLnbIds();
}

static jobject android_media_tv_Tuner_open_lnb_by_handle(JNIEnv *env, jobject thiz, jint handle) {
    sp<JTuner> tuner = getTuner(env, thiz);
    uint32_t id = getResourceIdFromHandle(handle);
    return tuner->openLnbById(id);
}

static jobject android_media_tv_Tuner_open_lnb_by_name(JNIEnv *env, jobject thiz, jstring name) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->openLnbByName(name);
}


static jobject android_media_tv_Tuner_open_filter(
        JNIEnv *env, jobject thiz, jint type, jint subType, jlong bufferSize) {
    sp<JTuner> tuner = getTuner(env, thiz);
    DemuxFilterMainType mainType = static_cast<DemuxFilterMainType>(type);
    DemuxFilterType filterType {
        .mainType = mainType,
    };

    switch(mainType) {
        case DemuxFilterMainType::TS:
            filterType.subType.tsFilterType(static_cast<DemuxTsFilterType>(subType));
            break;
        case DemuxFilterMainType::MMTP:
            filterType.subType.mmtpFilterType(static_cast<DemuxMmtpFilterType>(subType));
            break;
        case DemuxFilterMainType::IP:
            filterType.subType.ipFilterType(static_cast<DemuxIpFilterType>(subType));
            break;
        case DemuxFilterMainType::TLV:
            filterType.subType.tlvFilterType(static_cast<DemuxTlvFilterType>(subType));
            break;
        case DemuxFilterMainType::ALP:
            filterType.subType.alpFilterType(static_cast<DemuxAlpFilterType>(subType));
            break;
    }

    return tuner->openFilter(filterType, bufferSize);
}

static jobject android_media_tv_Tuner_open_time_filter(JNIEnv *env, jobject thiz) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->openTimeFilter();
}

static DemuxFilterSectionBits getFilterSectionBits(JNIEnv *env, const jobject& settings) {
    jclass clazz = env->FindClass("android/media/tv/tuner/filter/SectionSettingsWithSectionBits");
    jbyteArray jfilterBytes = static_cast<jbyteArray>(
            env->GetObjectField(settings, env->GetFieldID(clazz, "mFilter", "[B")));
    jsize size = env->GetArrayLength(jfilterBytes);
    std::vector<uint8_t> filterBytes(size);
    env->GetByteArrayRegion(
            jfilterBytes, 0, size, reinterpret_cast<jbyte*>(&filterBytes[0]));

    jbyteArray jmask = static_cast<jbyteArray>(
            env->GetObjectField(settings, env->GetFieldID(clazz, "mMask", "[B")));
    size = env->GetArrayLength(jmask);
    std::vector<uint8_t> mask(size);
    env->GetByteArrayRegion(jmask, 0, size, reinterpret_cast<jbyte*>(&mask[0]));

    jbyteArray jmode = static_cast<jbyteArray>(
            env->GetObjectField(settings, env->GetFieldID(clazz, "mMode", "[B")));
    size = env->GetArrayLength(jmode);
    std::vector<uint8_t> mode(size);
    env->GetByteArrayRegion(jmode, 0, size, reinterpret_cast<jbyte*>(&mode[0]));

    DemuxFilterSectionBits filterSectionBits {
        .filter = filterBytes,
        .mask = mask,
        .mode = mode,
    };
    return filterSectionBits;
}

static DemuxFilterSectionSettings::Condition::TableInfo getFilterTableInfo(
        JNIEnv *env, const jobject& settings) {
    jclass clazz = env->FindClass("android/media/tv/tuner/filter/SectionSettingsWithTableInfo");
    uint16_t tableId = static_cast<uint16_t>(
            env->GetIntField(settings, env->GetFieldID(clazz, "mTableId", "I")));
    uint16_t version = static_cast<uint16_t>(
            env->GetIntField(settings, env->GetFieldID(clazz, "mVersion", "I")));
    DemuxFilterSectionSettings::Condition::TableInfo tableInfo {
        .tableId = tableId,
        .version = version,
    };
    return tableInfo;
}

static DemuxFilterSectionSettings getFilterSectionSettings(JNIEnv *env, const jobject& settings) {
    jclass clazz = env->FindClass("android/media/tv/tuner/filter/SectionSettings");
    bool isCheckCrc = static_cast<bool>(
            env->GetBooleanField(settings, env->GetFieldID(clazz, "mCrcEnabled", "Z")));
    bool isRepeat = static_cast<bool>(
            env->GetBooleanField(settings, env->GetFieldID(clazz, "mIsRepeat", "Z")));
    bool isRaw = static_cast<bool>(
            env->GetBooleanField(settings, env->GetFieldID(clazz, "mIsRaw", "Z")));

    DemuxFilterSectionSettings filterSectionSettings {
        .isCheckCrc = isCheckCrc,
        .isRepeat = isRepeat,
        .isRaw = isRaw,
    };
    if (env->IsInstanceOf(
            settings,
            env->FindClass("android/media/tv/tuner/filter/SectionSettingsWithSectionBits"))) {
        filterSectionSettings.condition.sectionBits(getFilterSectionBits(env, settings));
    } else if (env->IsInstanceOf(
            settings,
            env->FindClass("android/media/tv/tuner/filter/SectionSettingsWithTableInfo"))) {
        filterSectionSettings.condition.tableInfo(getFilterTableInfo(env, settings));
    }
    return filterSectionSettings;
}

static DemuxFilterAvSettings getFilterAvSettings(JNIEnv *env, const jobject& settings) {
    jclass clazz = env->FindClass("android/media/tv/tuner/filter/AvSettings");
    bool isPassthrough = static_cast<bool>(
            env->GetBooleanField(settings, env->GetFieldID(clazz, "mIsPassthrough", "Z")));
    DemuxFilterAvSettings filterAvSettings {
        .isPassthrough = isPassthrough,
    };
    return filterAvSettings;
}

static DemuxFilterPesDataSettings getFilterPesDataSettings(JNIEnv *env, const jobject& settings) {
    jclass clazz = env->FindClass("android/media/tv/tuner/filter/PesSettings");
    uint16_t streamId = static_cast<uint16_t>(
            env->GetIntField(settings, env->GetFieldID(clazz, "mStreamId", "I")));
    bool isRaw = static_cast<bool>(
            env->GetBooleanField(settings, env->GetFieldID(clazz, "mIsRaw", "Z")));
    DemuxFilterPesDataSettings filterPesDataSettings {
        .streamId = streamId,
        .isRaw = isRaw,
    };
    return filterPesDataSettings;
}

static DemuxFilterRecordSettings getFilterRecordSettings(JNIEnv *env, const jobject& settings) {
    jclass clazz = env->FindClass("android/media/tv/tuner/filter/RecordSettings");
    hidl_bitfield<DemuxTsIndex> tsIndexMask = static_cast<hidl_bitfield<DemuxTsIndex>>(
            env->GetIntField(settings, env->GetFieldID(clazz, "mTsIndexMask", "I")));
    DemuxRecordScIndexType scIndexType = static_cast<DemuxRecordScIndexType>(
            env->GetIntField(settings, env->GetFieldID(clazz, "mScIndexType", "I")));
    jint scIndexMask = env->GetIntField(settings, env->GetFieldID(clazz, "mScIndexMask", "I"));

    DemuxFilterRecordSettings filterRecordSettings {
        .tsIndexMask = tsIndexMask,
        .scIndexType = scIndexType,
    };
    if (scIndexType == DemuxRecordScIndexType::SC) {
        filterRecordSettings.scIndexMask.sc(static_cast<hidl_bitfield<DemuxScIndex>>(scIndexMask));
    } else if (scIndexType == DemuxRecordScIndexType::SC_HEVC) {
        filterRecordSettings.scIndexMask.scHevc(
                static_cast<hidl_bitfield<DemuxScHevcIndex>>(scIndexMask));
    }
    return filterRecordSettings;
}

static DemuxFilterDownloadSettings getFilterDownloadSettings(JNIEnv *env, const jobject& settings) {
    jclass clazz = env->FindClass("android/media/tv/tuner/filter/DownloadSettings");
    uint32_t downloadId = static_cast<uint32_t>(
            env->GetIntField(settings, env->GetFieldID(clazz, "mDownloadId", "I")));

    DemuxFilterDownloadSettings filterDownloadSettings {
        .downloadId = downloadId,
    };
    return filterDownloadSettings;
}

static DemuxIpAddress getDemuxIpAddress(JNIEnv *env, const jobject& config) {
    jclass clazz = env->FindClass("android/media/tv/tuner/filter/IpFilterConfiguration");

    jbyteArray jsrcIpAddress = static_cast<jbyteArray>(
            env->GetObjectField(config, env->GetFieldID(clazz, "mSrcIpAddress", "[B")));
    jsize srcSize = env->GetArrayLength(jsrcIpAddress);
    jbyteArray jdstIpAddress = static_cast<jbyteArray>(
            env->GetObjectField(config, env->GetFieldID(clazz, "mDstIpAddress", "[B")));
    jsize dstSize = env->GetArrayLength(jdstIpAddress);

    DemuxIpAddress res;

    if (srcSize != dstSize) {
        // should never happen. Validated on Java size.
        jniThrowExceptionFmt(env, "java/lang/IllegalArgumentException",
            "IP address lengths don't match. srcLength=%d, dstLength=%d", srcSize, dstSize);
        return res;
    }

    if (srcSize == IP_V4_LENGTH) {
        uint8_t srcAddr[IP_V4_LENGTH];
        uint8_t dstAddr[IP_V4_LENGTH];
        env->GetByteArrayRegion(
                jsrcIpAddress, 0, srcSize, reinterpret_cast<jbyte*>(srcAddr));
        env->GetByteArrayRegion(
                jdstIpAddress, 0, dstSize, reinterpret_cast<jbyte*>(dstAddr));
        res.srcIpAddress.v4(srcAddr);
        res.dstIpAddress.v4(dstAddr);
    } else if (srcSize == IP_V6_LENGTH) {
        uint8_t srcAddr[IP_V6_LENGTH];
        uint8_t dstAddr[IP_V6_LENGTH];
        env->GetByteArrayRegion(
                jsrcIpAddress, 0, srcSize, reinterpret_cast<jbyte*>(srcAddr));
        env->GetByteArrayRegion(
                jdstIpAddress, 0, dstSize, reinterpret_cast<jbyte*>(dstAddr));
        res.srcIpAddress.v6(srcAddr);
        res.dstIpAddress.v6(dstAddr);
    } else {
        // should never happen. Validated on Java size.
        jniThrowExceptionFmt(env, "java/lang/IllegalArgumentException",
            "Invalid IP address length %d", srcSize);
        return res;
    }

    uint16_t srcPort = static_cast<uint16_t>(
            env->GetIntField(config, env->GetFieldID(clazz, "mSrcPort", "I")));
    uint16_t dstPort = static_cast<uint16_t>(
            env->GetIntField(config, env->GetFieldID(clazz, "mDstPort", "I")));

    res.srcPort = srcPort;
    res.dstPort = dstPort;

    return res;
}

static DemuxFilterSettings getFilterConfiguration(
        JNIEnv *env, int type, int subtype, jobject filterConfigObj) {
    DemuxFilterSettings filterSettings;
    jobject settingsObj =
            env->GetObjectField(
                    filterConfigObj,
                    env->GetFieldID(
                            env->FindClass("android/media/tv/tuner/filter/FilterConfiguration"),
                            "mSettings",
                            "Landroid/media/tv/tuner/filter/Settings;"));
    DemuxFilterMainType mainType = static_cast<DemuxFilterMainType>(type);
    switch (mainType) {
        case DemuxFilterMainType::TS: {
            jclass clazz = env->FindClass("android/media/tv/tuner/filter/TsFilterConfiguration");
            uint16_t tpid = static_cast<uint16_t>(
                    env->GetIntField(filterConfigObj, env->GetFieldID(clazz, "mTpid", "I")));
            DemuxTsFilterSettings tsFilterSettings {
                .tpid = tpid,
            };

            DemuxTsFilterType tsType = static_cast<DemuxTsFilterType>(subtype);
            switch (tsType) {
                case DemuxTsFilterType::SECTION:
                    tsFilterSettings.filterSettings.section(
                            getFilterSectionSettings(env, settingsObj));
                    break;
                case DemuxTsFilterType::AUDIO:
                case DemuxTsFilterType::VIDEO:
                    tsFilterSettings.filterSettings.av(getFilterAvSettings(env, settingsObj));
                    break;
                case DemuxTsFilterType::PES:
                    tsFilterSettings.filterSettings.pesData(
                            getFilterPesDataSettings(env, settingsObj));
                    break;
                case DemuxTsFilterType::RECORD:
                    tsFilterSettings.filterSettings.record(
                            getFilterRecordSettings(env, settingsObj));
                    break;
                default:
                    break;
            }
            filterSettings.ts(tsFilterSettings);
            break;
        }
        case DemuxFilterMainType::MMTP: {
            jclass clazz = env->FindClass("android/media/tv/tuner/filter/MmtpFilterConfiguration");
            uint16_t mmtpPid = static_cast<uint16_t>(
                    env->GetIntField(filterConfigObj, env->GetFieldID(clazz, "mMmtpPid", "I")));
            DemuxMmtpFilterSettings mmtpFilterSettings {
                .mmtpPid = mmtpPid,
            };
            DemuxMmtpFilterType mmtpType = static_cast<DemuxMmtpFilterType>(subtype);
            switch (mmtpType) {
                case DemuxMmtpFilterType::SECTION:
                    mmtpFilterSettings.filterSettings.section(
                            getFilterSectionSettings(env, settingsObj));
                    break;
                case DemuxMmtpFilterType::AUDIO:
                case DemuxMmtpFilterType::VIDEO:
                    mmtpFilterSettings.filterSettings.av(getFilterAvSettings(env, settingsObj));
                    break;
                case DemuxMmtpFilterType::PES:
                    mmtpFilterSettings.filterSettings.pesData(
                            getFilterPesDataSettings(env, settingsObj));
                    break;
                case DemuxMmtpFilterType::RECORD:
                    mmtpFilterSettings.filterSettings.record(
                            getFilterRecordSettings(env, settingsObj));
                    break;
                case DemuxMmtpFilterType::DOWNLOAD:
                    mmtpFilterSettings.filterSettings.download(
                            getFilterDownloadSettings(env, settingsObj));
                    break;
                default:
                    break;
            }
            filterSettings.mmtp(mmtpFilterSettings);
            break;
        }
        case DemuxFilterMainType::IP: {
            DemuxIpAddress ipAddr = getDemuxIpAddress(env, filterConfigObj);

            DemuxIpFilterSettings ipFilterSettings {
                .ipAddr = ipAddr,
            };
            DemuxIpFilterType ipType = static_cast<DemuxIpFilterType>(subtype);
            switch (ipType) {
                case DemuxIpFilterType::SECTION: {
                    ipFilterSettings.filterSettings.section(
                            getFilterSectionSettings(env, settingsObj));
                    break;
                }
                case DemuxIpFilterType::IP: {
                    jclass clazz = env->FindClass(
                            "android/media/tv/tuner/filter/IpFilterConfiguration");
                    bool bPassthrough = static_cast<bool>(
                            env->GetBooleanField(
                                    filterConfigObj, env->GetFieldID(
                                            clazz, "mPassthrough", "Z")));
                    ipFilterSettings.filterSettings.bPassthrough(bPassthrough);
                    break;
                }
                default: {
                    break;
                }
            }
            filterSettings.ip(ipFilterSettings);
            break;
        }
        case DemuxFilterMainType::TLV: {
            jclass clazz = env->FindClass("android/media/tv/tuner/filter/TlvFilterConfiguration");
            uint8_t packetType = static_cast<uint8_t>(
                    env->GetIntField(filterConfigObj, env->GetFieldID(clazz, "mPacketType", "I")));
            bool isCompressedIpPacket = static_cast<bool>(
                    env->GetBooleanField(
                            filterConfigObj, env->GetFieldID(clazz, "mIsCompressedIpPacket", "Z")));

            DemuxTlvFilterSettings tlvFilterSettings {
                .packetType = packetType,
                .isCompressedIpPacket = isCompressedIpPacket,
            };
            DemuxTlvFilterType tlvType = static_cast<DemuxTlvFilterType>(subtype);
            switch (tlvType) {
                case DemuxTlvFilterType::SECTION: {
                    tlvFilterSettings.filterSettings.section(
                            getFilterSectionSettings(env, settingsObj));
                    break;
                }
                case DemuxTlvFilterType::TLV: {
                    bool bPassthrough = static_cast<bool>(
                            env->GetBooleanField(
                                    filterConfigObj, env->GetFieldID(
                                            clazz, "mPassthrough", "Z")));
                    tlvFilterSettings.filterSettings.bPassthrough(bPassthrough);
                    break;
                }
                default: {
                    break;
                }
            }
            filterSettings.tlv(tlvFilterSettings);
            break;
        }
        case DemuxFilterMainType::ALP: {
            jclass clazz = env->FindClass("android/media/tv/tuner/filter/AlpFilterConfiguration");
            uint8_t packetType = static_cast<uint8_t>(
                    env->GetIntField(filterConfigObj, env->GetFieldID(clazz, "mPacketType", "I")));
            DemuxAlpLengthType lengthType = static_cast<DemuxAlpLengthType>(
                    env->GetIntField(filterConfigObj, env->GetFieldID(clazz, "mLengthType", "I")));
            DemuxAlpFilterSettings alpFilterSettings {
                .packetType = packetType,
                .lengthType = lengthType,
            };
            DemuxAlpFilterType alpType = static_cast<DemuxAlpFilterType>(subtype);
            switch (alpType) {
                case DemuxAlpFilterType::SECTION:
                    alpFilterSettings.filterSettings.section(
                            getFilterSectionSettings(env, settingsObj));
                    break;
                default:
                    break;
            }
            filterSettings.alp(alpFilterSettings);
            break;
        }
        default: {
            break;
        }
    }
    return filterSettings;
}

static jint copyData(JNIEnv *env, std::unique_ptr<MQ>& mq, EventFlag* flag, jbyteArray buffer,
        jlong offset, jlong size) {
    ALOGD("copyData, size=%ld, offset=%ld", (long) size, (long) offset);

    jlong available = mq->availableToRead();
    ALOGD("copyData, available=%ld", (long) available);
    size = std::min(size, available);

    jboolean isCopy;
    jbyte *dst = env->GetByteArrayElements(buffer, &isCopy);
    ALOGD("copyData, isCopy=%d", isCopy);
    if (dst == nullptr) {
        jniThrowRuntimeException(env, "Failed to GetByteArrayElements");
        return 0;
    }

    if (mq->read(reinterpret_cast<unsigned char*>(dst) + offset, size)) {
        env->ReleaseByteArrayElements(buffer, dst, 0);
        flag->wake(static_cast<uint32_t>(DemuxQueueNotifyBits::DATA_CONSUMED));
    } else {
        jniThrowRuntimeException(env, "Failed to read FMQ");
        env->ReleaseByteArrayElements(buffer, dst, 0);
        return 0;
    }
    return size;
}

static jint android_media_tv_Tuner_configure_filter(
        JNIEnv *env, jobject filter, int type, int subtype, jobject settings) {
    ALOGD("configure filter type=%d, subtype=%d", type, subtype);
    sp<Filter> filterSp = getFilter(env, filter);
    sp<IFilter> iFilterSp = filterSp->getIFilter();
    if (iFilterSp == NULL) {
        ALOGD("Failed to configure filter: filter not found");
        return (jint) Result::NOT_INITIALIZED;
    }
    DemuxFilterSettings filterSettings = getFilterConfiguration(env, type, subtype, settings);
    Result res = iFilterSp->configure(filterSettings);

    if (res != Result::SUCCESS) {
        return (jint) res;
    }

    MQDescriptorSync<uint8_t> filterMQDesc;
    Result getQueueDescResult = Result::UNKNOWN_ERROR;
    if (filterSp->mFilterMQ == NULL) {
        iFilterSp->getQueueDesc(
                [&](Result r, const MQDescriptorSync<uint8_t>& desc) {
                    filterMQDesc = desc;
                    getQueueDescResult = r;
                    ALOGD("getFilterQueueDesc");
                });
        if (getQueueDescResult == Result::SUCCESS) {
            filterSp->mFilterMQ = std::make_unique<MQ>(filterMQDesc, true);
            EventFlag::createEventFlag(
                    filterSp->mFilterMQ->getEventFlagWord(), &(filterSp->mFilterMQEventFlag));
        }
    }
    return (jint) getQueueDescResult;
}

static jint android_media_tv_Tuner_get_filter_id(JNIEnv* env, jobject filter) {
    sp<IFilter> iFilterSp = getFilter(env, filter)->getIFilter();
    if (iFilterSp == NULL) {
        ALOGD("Failed to get filter ID: filter not found");
        return (int) Result::NOT_INITIALIZED;
    }
    Result res;
    uint32_t id;
    iFilterSp->getId(
            [&](Result r, uint32_t filterId) {
                res = r;
                id = filterId;
            });
    if (res != Result::SUCCESS) {
        return (jint) Constant::INVALID_FILTER_ID;
    }
    return (jint) id;
}

static jint android_media_tv_Tuner_set_filter_data_source(
        JNIEnv* env, jobject filter, jobject srcFilter) {
    sp<IFilter> iFilterSp = getFilter(env, filter)->getIFilter();
    if (iFilterSp == NULL) {
        ALOGD("Failed to set filter data source: filter not found");
        return (jint) Result::NOT_INITIALIZED;
    }
    Result r;
    if (srcFilter == NULL) {
        r = iFilterSp->setDataSource(NULL);
    } else {
        sp<IFilter> srcSp = getFilter(env, srcFilter)->getIFilter();
        if (iFilterSp == NULL) {
            ALOGD("Failed to set filter data source: src filter not found");
            return (jint) Result::INVALID_ARGUMENT;
        }
        r = iFilterSp->setDataSource(srcSp);
    }
    return (jint) r;
}

static jint android_media_tv_Tuner_start_filter(JNIEnv *env, jobject filter) {
    sp<IFilter> iFilterSp = getFilter(env, filter)->getIFilter();
    if (iFilterSp == NULL) {
        ALOGD("Failed to start filter: filter not found");
        return (jint) Result::NOT_INITIALIZED;
    }
    Result r = iFilterSp->start();
    return (jint) r;
}

static jint android_media_tv_Tuner_stop_filter(JNIEnv *env, jobject filter) {
    sp<IFilter> iFilterSp = getFilter(env, filter)->getIFilter();
    if (iFilterSp == NULL) {
        ALOGD("Failed to stop filter: filter not found");
        return (jint) Result::NOT_INITIALIZED;
    }
    Result r = iFilterSp->stop();
    return (jint) r;
}

static jint android_media_tv_Tuner_flush_filter(JNIEnv *env, jobject filter) {
    sp<IFilter> iFilterSp = getFilter(env, filter)->getIFilter();
    if (iFilterSp == NULL) {
        ALOGD("Failed to flush filter: filter not found");
        return (jint) Result::NOT_INITIALIZED;
    }
    Result r = iFilterSp->flush();
    return (jint) r;
}

static jint android_media_tv_Tuner_read_filter_fmq(
        JNIEnv *env, jobject filter, jbyteArray buffer, jlong offset, jlong size) {
    sp<Filter> filterSp = getFilter(env, filter);
    if (filterSp == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException",
                "Failed to read filter FMQ: filter not found");
        return 0;
    }
    return copyData(env, filterSp->mFilterMQ, filterSp->mFilterMQEventFlag, buffer, offset, size);
}

static jint android_media_tv_Tuner_close_filter(JNIEnv *env, jobject filter) {
    sp<IFilter> iFilterSp = getFilter(env, filter)->getIFilter();
    if (iFilterSp == NULL) {
        ALOGD("Failed to close filter: filter not found");
        return (jint) Result::NOT_INITIALIZED;
    }
    Result r = iFilterSp->close();
    return (jint) r;
}

static sp<TimeFilter> getTimeFilter(JNIEnv *env, jobject filter) {
    return (TimeFilter *)env->GetLongField(filter, gFields.timeFilterContext);
}

static int android_media_tv_Tuner_time_filter_set_timestamp(
        JNIEnv *env, jobject filter, jlong timestamp) {
    sp<TimeFilter> filterSp = getTimeFilter(env, filter);
    if (filterSp == NULL) {
        ALOGD("Failed set timestamp: time filter not found");
        return (int) Result::INVALID_STATE;
    }
    sp<ITimeFilter> iFilterSp = filterSp->getITimeFilter();
    Result r = iFilterSp->setTimeStamp(static_cast<uint64_t>(timestamp));
    return (int) r;
}

static int android_media_tv_Tuner_time_filter_clear_timestamp(JNIEnv *env, jobject filter) {
    sp<TimeFilter> filterSp = getTimeFilter(env, filter);
    if (filterSp == NULL) {
        ALOGD("Failed clear timestamp: time filter not found");
        return (int) Result::INVALID_STATE;
    }
    sp<ITimeFilter> iFilterSp = filterSp->getITimeFilter();
    Result r = iFilterSp->clearTimeStamp();
    return (int) r;
}

static jobject android_media_tv_Tuner_time_filter_get_timestamp(JNIEnv *env, jobject filter) {
    sp<TimeFilter> filterSp = getTimeFilter(env, filter);
    if (filterSp == NULL) {
        ALOGD("Failed get timestamp: time filter not found");
        return NULL;
    }

    sp<ITimeFilter> iFilterSp = filterSp->getITimeFilter();
    Result res;
    uint64_t timestamp;
    iFilterSp->getTimeStamp(
            [&](Result r, uint64_t t) {
                res = r;
                timestamp = t;
            });
    if (res != Result::SUCCESS) {
        return NULL;
    }

    jclass longClazz = env->FindClass("java/lang/Long");
    jmethodID longInit = env->GetMethodID(longClazz, "<init>", "(J)V");

    jobject longObj = env->NewObject(longClazz, longInit, static_cast<jlong>(timestamp));
    return longObj;
}

static jobject android_media_tv_Tuner_time_filter_get_source_time(JNIEnv *env, jobject filter) {
    sp<TimeFilter> filterSp = getTimeFilter(env, filter);
    if (filterSp == NULL) {
        ALOGD("Failed get source time: time filter not found");
        return NULL;
    }

    sp<ITimeFilter> iFilterSp = filterSp->getITimeFilter();
    Result res;
    uint64_t timestamp;
    iFilterSp->getSourceTime(
            [&](Result r, uint64_t t) {
                res = r;
                timestamp = t;
            });
    if (res != Result::SUCCESS) {
        return NULL;
    }

    jclass longClazz = env->FindClass("java/lang/Long");
    jmethodID longInit = env->GetMethodID(longClazz, "<init>", "(J)V");

    jobject longObj = env->NewObject(longClazz, longInit, static_cast<jlong>(timestamp));
    return longObj;
}

static int android_media_tv_Tuner_time_filter_close(JNIEnv *env, jobject filter) {
    sp<TimeFilter> filterSp = getTimeFilter(env, filter);
    if (filterSp == NULL) {
        ALOGD("Failed close time filter: time filter not found");
        return (int) Result::INVALID_STATE;
    }

    Result r = filterSp->getITimeFilter()->close();
    if (r == Result::SUCCESS) {
        filterSp->decStrong(filter);
        env->SetLongField(filter, gFields.timeFilterContext, 0);
    }
    return (int) r;
}

static jobject android_media_tv_Tuner_open_descrambler(JNIEnv *env, jobject thiz, jint) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->openDescrambler();
}

static jint android_media_tv_Tuner_descrambler_add_pid(
        JNIEnv *env, jobject descrambler, jint pidType, jint pid, jobject filter) {
    sp<IDescrambler> descramblerSp = getDescrambler(env, descrambler);
    if (descramblerSp == NULL) {
        return (jint) Result::NOT_INITIALIZED;
    }
    sp<IFilter> iFilterSp = getFilter(env, filter)->getIFilter();
    Result result = descramblerSp->addPid(getDemuxPid((int)pidType, (int)pid), iFilterSp);
    return (jint) result;
}

static jint android_media_tv_Tuner_descrambler_remove_pid(
        JNIEnv *env, jobject descrambler, jint pidType, jint pid, jobject filter) {
    sp<IDescrambler> descramblerSp = getDescrambler(env, descrambler);
    if (descramblerSp == NULL) {
        return (jint) Result::NOT_INITIALIZED;
    }
    sp<IFilter> iFilterSp = getFilter(env, filter)->getIFilter();
    Result result = descramblerSp->removePid(getDemuxPid((int)pidType, (int)pid), iFilterSp);
    return (jint) result;
}

static jint android_media_tv_Tuner_descrambler_set_key_token(
        JNIEnv* env, jobject descrambler, jbyteArray keyToken) {
    sp<IDescrambler> descramblerSp = getDescrambler(env, descrambler);
    if (descramblerSp == NULL) {
        return (jint) Result::NOT_INITIALIZED;
    }
    int size = env->GetArrayLength(keyToken);
    std::vector<uint8_t> v(size);
    env->GetByteArrayRegion(keyToken, 0, size, reinterpret_cast<jbyte*>(&v[0]));
    Result result = descramblerSp->setKeyToken(v);
    return (jint) result;
}

static jint android_media_tv_Tuner_close_descrambler(JNIEnv* env, jobject descrambler) {
    sp<IDescrambler> descramblerSp = getDescrambler(env, descrambler);
    if (descramblerSp == NULL) {
        return (jint) Result::NOT_INITIALIZED;
    }
    Result r = descramblerSp->close();
    if (r == Result::SUCCESS) {
        descramblerSp->decStrong(descrambler);
    }
    return (jint) r;
}

static jobject android_media_tv_Tuner_open_dvr_recorder(
        JNIEnv* env, jobject thiz, jlong bufferSize) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->openDvr(DvrType::RECORD, bufferSize);
}

static jobject android_media_tv_Tuner_open_dvr_playback(
        JNIEnv* env, jobject thiz, jlong bufferSize) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->openDvr(DvrType::PLAYBACK, bufferSize);
}

static jobject android_media_tv_Tuner_get_demux_caps(JNIEnv* env, jobject thiz) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->getDemuxCaps();
}

static jint android_media_tv_Tuner_open_demux(JNIEnv* env, jobject thiz, jint /* handle */) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return (jint) tuner->openDemux();
}

static jint android_media_tv_Tuner_close_tuner(JNIEnv* env, jobject thiz) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return (jint) tuner->close();
}

static jint android_media_tv_Tuner_close_demux(JNIEnv* env, jobject thiz, jint /* handle */) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->closeDemux();
}

static jint android_media_tv_Tuner_close_frontend(JNIEnv* env, jobject thiz, jint /* handle */) {
    sp<JTuner> tuner = getTuner(env, thiz);
    return tuner->closeFrontend();
}

static jint android_media_tv_Tuner_attach_filter(JNIEnv *env, jobject dvr, jobject filter) {
    sp<Dvr> dvrSp = getDvr(env, dvr);
    if (dvrSp == NULL) {
        return (jint) Result::NOT_INITIALIZED;
    }
    sp<Filter> filterSp = getFilter(env, filter);
    if (filterSp == NULL) {
        return (jint) Result::INVALID_ARGUMENT;
    }
    sp<IDvr> iDvrSp = dvrSp->getIDvr();
    sp<IFilter> iFilterSp = filterSp->getIFilter();
    Result result = iDvrSp->attachFilter(iFilterSp);
    return (jint) result;
}

static jint android_media_tv_Tuner_detach_filter(JNIEnv *env, jobject dvr, jobject filter) {
    sp<Dvr> dvrSp = getDvr(env, dvr);
    if (dvrSp == NULL) {
        return (jint) Result::NOT_INITIALIZED;
    }
    sp<Filter> filterSp = getFilter(env, filter);
    if (filterSp == NULL) {
        return (jint) Result::INVALID_ARGUMENT;
    }
    sp<IDvr> iDvrSp = dvrSp->getIDvr();
    sp<IFilter> iFilterSp = filterSp->getIFilter();
    Result result = iDvrSp->detachFilter(iFilterSp);
    return (jint) result;
}

static jint android_media_tv_Tuner_configure_dvr(JNIEnv *env, jobject dvr, jobject settings) {
    sp<Dvr> dvrSp = getDvr(env, dvr);
    if (dvrSp == NULL) {
        ALOGD("Failed to configure dvr: dvr not found");
        return (int)Result::NOT_INITIALIZED;
    }
    sp<IDvr> iDvrSp = dvrSp->getIDvr();
    bool isRecorder =
            env->IsInstanceOf(dvr, env->FindClass("android/media/tv/tuner/dvr/DvrRecorder"));
    Result result = iDvrSp->configure(getDvrSettings(env, settings, isRecorder));
    if (result != Result::SUCCESS) {
        return (jint) result;
    }
    MQDescriptorSync<uint8_t> dvrMQDesc;
    Result getQueueDescResult = Result::UNKNOWN_ERROR;
    iDvrSp->getQueueDesc(
            [&](Result r, const MQDescriptorSync<uint8_t>& desc) {
                dvrMQDesc = desc;
                getQueueDescResult = r;
                ALOGD("getDvrQueueDesc");
            });
    if (getQueueDescResult == Result::SUCCESS) {
        dvrSp->mDvrMQ = std::make_unique<MQ>(dvrMQDesc, true);
        EventFlag::createEventFlag(
                dvrSp->mDvrMQ->getEventFlagWord(), &(dvrSp->mDvrMQEventFlag));
    }
    return (jint) getQueueDescResult;
}

static jint android_media_tv_Tuner_start_dvr(JNIEnv *env, jobject dvr) {
    sp<Dvr> dvrSp = getDvr(env, dvr);
    if (dvrSp == NULL) {
        ALOGD("Failed to start dvr: dvr not found");
        return (jint) Result::NOT_INITIALIZED;
    }
    sp<IDvr> iDvrSp = dvrSp->getIDvr();
    Result result = iDvrSp->start();
    return (jint) result;
}

static jint android_media_tv_Tuner_stop_dvr(JNIEnv *env, jobject dvr) {
    sp<Dvr> dvrSp = getDvr(env, dvr);
    if (dvrSp == NULL) {
        ALOGD("Failed to stop dvr: dvr not found");
        return (jint) Result::NOT_INITIALIZED;
    }
    sp<IDvr> iDvrSp = dvrSp->getIDvr();
    Result result = iDvrSp->stop();
    return (jint) result;
}

static jint android_media_tv_Tuner_flush_dvr(JNIEnv *env, jobject dvr) {
    sp<Dvr> dvrSp = getDvr(env, dvr);
    if (dvrSp == NULL) {
        ALOGD("Failed to flush dvr: dvr not found");
        return (jint) Result::NOT_INITIALIZED;
    }
    sp<IDvr> iDvrSp = dvrSp->getIDvr();
    Result result = iDvrSp->flush();
    return (jint) result;
}

static jint android_media_tv_Tuner_close_dvr(JNIEnv* env, jobject dvr) {
    sp<Dvr> dvrSp = getDvr(env, dvr);
    if (dvrSp == NULL) {
        ALOGD("Failed to close dvr: dvr not found");
        return (jint) Result::NOT_INITIALIZED;
    }
    return dvrSp->close();
}

static sp<Lnb> getLnb(JNIEnv *env, jobject lnb) {
    return (Lnb *)env->GetLongField(lnb, gFields.lnbContext);
}

static jint android_media_tv_Tuner_lnb_set_voltage(JNIEnv* env, jobject lnb, jint voltage) {
    sp<ILnb> iLnbSp = getLnb(env, lnb)->getILnb();
    Result r = iLnbSp->setVoltage(static_cast<LnbVoltage>(voltage));
    return (jint) r;
}

static int android_media_tv_Tuner_lnb_set_tone(JNIEnv* env, jobject lnb, jint tone) {
    sp<ILnb> iLnbSp = getLnb(env, lnb)->getILnb();
    Result r = iLnbSp->setTone(static_cast<LnbTone>(tone));
    return (jint) r;
}

static int android_media_tv_Tuner_lnb_set_position(JNIEnv* env, jobject lnb, jint position) {
    sp<ILnb> iLnbSp = getLnb(env, lnb)->getILnb();
    Result r = iLnbSp->setSatellitePosition(static_cast<LnbPosition>(position));
    return (jint) r;
}

static int android_media_tv_Tuner_lnb_send_diseqc_msg(JNIEnv* env, jobject lnb, jbyteArray msg) {
    sp<ILnb> iLnbSp = getLnb(env, lnb)->getILnb();
    int size = env->GetArrayLength(msg);
    std::vector<uint8_t> v(size);
    env->GetByteArrayRegion(msg, 0, size, reinterpret_cast<jbyte*>(&v[0]));
    Result r = iLnbSp->sendDiseqcMessage(v);
    return (jint) r;
}

static int android_media_tv_Tuner_close_lnb(JNIEnv* env, jobject lnb) {
    sp<Lnb> lnbSp = getLnb(env, lnb);
    Result r = lnbSp->getILnb()->close();
    if (r == Result::SUCCESS) {
        lnbSp->decStrong(lnb);
        env->SetLongField(lnb, gFields.lnbContext, 0);
    }
    return (jint) r;
}

static void android_media_tv_Tuner_dvr_set_fd(JNIEnv *env, jobject dvr, jint fd) {
    sp<Dvr> dvrSp = getDvr(env, dvr);
    if (dvrSp == NULL) {
        ALOGD("Failed to set FD for dvr: dvr not found");
    }
    dvrSp->mFd = (int) fd;
    ALOGD("set fd = %d", dvrSp->mFd);
}

static jlong android_media_tv_Tuner_read_dvr(JNIEnv *env, jobject dvr, jlong size) {
    sp<Dvr> dvrSp = getDvr(env, dvr);
    if (dvrSp == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException",
                "Failed to read dvr: dvr not found");
        return 0;
    }

    long available = dvrSp->mDvrMQ->availableToWrite();
    long write = std::min((long) size, available);

    MQ::MemTransaction tx;
    long ret = 0;
    if (dvrSp->mDvrMQ->beginWrite(write, &tx)) {
        auto first = tx.getFirstRegion();
        auto data = first.getAddress();
        long length = first.getLength();
        long firstToWrite = std::min(length, write);
        ret = read(dvrSp->mFd, data, firstToWrite);

        if (ret < 0) {
            ALOGE("[DVR] Failed to read from FD: %s", strerror(errno));
            jniThrowRuntimeException(env, strerror(errno));
            return 0;
        }
        if (ret < firstToWrite) {
            ALOGW("[DVR] file to MQ, first region: %ld bytes to write, but %ld bytes written",
                    firstToWrite, ret);
        } else if (firstToWrite < write) {
            ALOGD("[DVR] write second region: %ld bytes written, %ld bytes in total", ret, write);
            auto second = tx.getSecondRegion();
            data = second.getAddress();
            length = second.getLength();
            int secondToWrite = std::min(length, write - firstToWrite);
            ret += read(dvrSp->mFd, data, secondToWrite);
        }
        ALOGD("[DVR] file to MQ: %ld bytes need to be written, %ld bytes written", write, ret);
        if (!dvrSp->mDvrMQ->commitWrite(ret)) {
            ALOGE("[DVR] Error: failed to commit write!");
            return 0;
        }

    } else {
        ALOGE("dvrMq.beginWrite failed");
    }
    return (jlong) ret;
}

static jlong android_media_tv_Tuner_read_dvr_from_array(
        JNIEnv* env, jobject dvr, jbyteArray buffer, jlong offset, jlong size) {
    sp<Dvr> dvrSp = getDvr(env, dvr);
    if (dvrSp == NULL) {
        ALOGW("Failed to read dvr: dvr not found");
        return 0;
    }
    if (dvrSp->mDvrMQ == NULL) {
        ALOGW("Failed to read dvr: dvr not configured");
        return 0;
    }

    jlong available = dvrSp->mDvrMQ->availableToWrite();
    size = std::min(size, available);

    jboolean isCopy;
    jbyte *src = env->GetByteArrayElements(buffer, &isCopy);
    if (src == nullptr) {
        ALOGD("Failed to GetByteArrayElements");
        return 0;
    }

    if (dvrSp->mDvrMQ->write(reinterpret_cast<unsigned char*>(src) + offset, size)) {
        env->ReleaseByteArrayElements(buffer, src, 0);
        dvrSp->mDvrMQEventFlag->wake(static_cast<uint32_t>(DemuxQueueNotifyBits::DATA_CONSUMED));
    } else {
        ALOGD("Failed to write FMQ");
        env->ReleaseByteArrayElements(buffer, src, 0);
        return 0;
    }
    return size;
}

static jlong android_media_tv_Tuner_write_dvr(JNIEnv *env, jobject dvr, jlong size) {
    sp<Dvr> dvrSp = getDvr(env, dvr);
    if (dvrSp == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException",
                "Failed to write dvr: dvr not found");
        return 0;
    }

    if (dvrSp->mDvrMQ == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException",
                "Failed to write dvr: dvr not configured");
        return 0;
    }

    MQ& dvrMq = dvrSp->getDvrMQ();

    long available = dvrMq.availableToRead();
    long toRead = std::min((long) size, available);

    long ret = 0;
    MQ::MemTransaction tx;
    if (dvrMq.beginRead(toRead, &tx)) {
        auto first = tx.getFirstRegion();
        auto data = first.getAddress();
        long length = first.getLength();
        long firstToRead = std::min(length, toRead);
        ret = write(dvrSp->mFd, data, firstToRead);

        if (ret < 0) {
            ALOGE("[DVR] Failed to write to FD: %s", strerror(errno));
            jniThrowRuntimeException(env, strerror(errno));
            return 0;
        }
        if (ret < firstToRead) {
            ALOGW("[DVR] MQ to file: %ld bytes read, but %ld bytes written", firstToRead, ret);
        } else if (firstToRead < toRead) {
            ALOGD("[DVR] read second region: %ld bytes read, %ld bytes in total", ret, toRead);
            auto second = tx.getSecondRegion();
            data = second.getAddress();
            length = second.getLength();
            int secondToRead = toRead - firstToRead;
            ret += write(dvrSp->mFd, data, secondToRead);
        }
        ALOGD("[DVR] MQ to file: %ld bytes to be read, %ld bytes written", toRead, ret);
        if (!dvrMq.commitRead(ret)) {
            ALOGE("[DVR] Error: failed to commit read!");
            return 0;
        }

    } else {
        ALOGE("dvrMq.beginRead failed");
    }

    return (jlong) ret;
}

static jlong android_media_tv_Tuner_write_dvr_to_array(
        JNIEnv *env, jobject dvr, jbyteArray buffer, jlong offset, jlong size) {
    sp<Dvr> dvrSp = getDvr(env, dvr);
    if (dvrSp == NULL) {
        ALOGW("Failed to write dvr: dvr not found");
        return 0;
    }
    if (dvrSp->mDvrMQ == NULL) {
        ALOGW("Failed to write dvr: dvr not configured");
        return 0;
    }
    return copyData(env, dvrSp->mDvrMQ, dvrSp->mDvrMQEventFlag, buffer, offset, size);
}

static sp<MediaEvent> getMediaEventSp(JNIEnv *env, jobject mediaEventObj) {
    return (MediaEvent *)env->GetLongField(mediaEventObj, gFields.mediaEventContext);
}

static jobject android_media_tv_Tuner_media_event_get_linear_block(
        JNIEnv* env, jobject mediaEventObj) {
    sp<MediaEvent> mediaEventSp = getMediaEventSp(env, mediaEventObj);
    if (mediaEventSp == NULL) {
        ALOGD("Failed get MediaEvent");
        return NULL;
    }

    return mediaEventSp->getLinearBlock();
}

static jobject android_media_tv_Tuner_media_event_get_audio_handle(
        JNIEnv* env, jobject mediaEventObj) {
    sp<MediaEvent> mediaEventSp = getMediaEventSp(env, mediaEventObj);
    if (mediaEventSp == NULL) {
        ALOGD("Failed get MediaEvent");
        return NULL;
    }

    android::Mutex::Autolock autoLock(mediaEventSp->mLock);
    uint64_t audioHandle = mediaEventSp->getAudioHandle();
    jclass longClazz = env->FindClass("java/lang/Long");
    jmethodID longInit = env->GetMethodID(longClazz, "<init>", "(J)V");

    jobject longObj = env->NewObject(longClazz, longInit, static_cast<jlong>(audioHandle));
    return longObj;
}

static void android_media_tv_Tuner_media_event_finalize(JNIEnv* env, jobject mediaEventObj) {
    sp<MediaEvent> mediaEventSp = getMediaEventSp(env, mediaEventObj);
    if (mediaEventSp == NULL) {
        ALOGD("Failed get MediaEvent");
        return;
    }

    android::Mutex::Autolock autoLock(mediaEventSp->mLock);
    mediaEventSp->mAvHandleRefCnt--;
    mediaEventSp->finalize();

    mediaEventSp->decStrong(mediaEventObj);
}

static const JNINativeMethod gTunerMethods[] = {
    { "nativeInit", "()V", (void *)android_media_tv_Tuner_native_init },
    { "nativeSetup", "()V", (void *)android_media_tv_Tuner_native_setup },
    { "nativeGetFrontendIds", "()Ljava/util/List;",
            (void *)android_media_tv_Tuner_get_frontend_ids },
    { "nativeOpenFrontendByHandle", "(I)Landroid/media/tv/tuner/Tuner$Frontend;",
            (void *)android_media_tv_Tuner_open_frontend_by_handle },
    { "nativeCloseFrontendByHandle", "(I)I",
            (void *)android_media_tv_Tuner_close_frontend_by_handle },
    { "nativeTune", "(ILandroid/media/tv/tuner/frontend/FrontendSettings;)I",
            (void *)android_media_tv_Tuner_tune },
    { "nativeStopTune", "()I", (void *)android_media_tv_Tuner_stop_tune },
    { "nativeScan", "(ILandroid/media/tv/tuner/frontend/FrontendSettings;I)I",
            (void *)android_media_tv_Tuner_scan },
    { "nativeStopScan", "()I", (void *)android_media_tv_Tuner_stop_scan },
    { "nativeSetLnb", "(I)I", (void *)android_media_tv_Tuner_set_lnb },
    { "nativeSetLna", "(Z)I", (void *)android_media_tv_Tuner_set_lna },
    { "nativeGetFrontendStatus", "([I)Landroid/media/tv/tuner/frontend/FrontendStatus;",
            (void *)android_media_tv_Tuner_get_frontend_status },
    { "nativeGetAvSyncHwId", "(Landroid/media/tv/tuner/filter/Filter;)Ljava/lang/Integer;",
            (void *)android_media_tv_Tuner_get_av_sync_hw_id },
    { "nativeGetAvSyncTime", "(I)Ljava/lang/Long;",
            (void *)android_media_tv_Tuner_get_av_sync_time },
    { "nativeConnectCiCam", "(I)I", (void *)android_media_tv_Tuner_connect_cicam },
    { "nativeDisconnectCiCam", "()I", (void *)android_media_tv_Tuner_disconnect_cicam },
    { "nativeGetFrontendInfo", "(I)Landroid/media/tv/tuner/frontend/FrontendInfo;",
            (void *)android_media_tv_Tuner_get_frontend_info },
    { "nativeOpenFilter", "(IIJ)Landroid/media/tv/tuner/filter/Filter;",
            (void *)android_media_tv_Tuner_open_filter },
    { "nativeOpenTimeFilter", "()Landroid/media/tv/tuner/filter/TimeFilter;",
            (void *)android_media_tv_Tuner_open_time_filter },
    { "nativeGetLnbIds", "()[I", (void *)android_media_tv_Tuner_get_lnb_ids },
    { "nativeOpenLnbByHandle", "(I)Landroid/media/tv/tuner/Lnb;",
            (void *)android_media_tv_Tuner_open_lnb_by_handle },
    { "nativeOpenLnbByName", "(Ljava/lang/String;)Landroid/media/tv/tuner/Lnb;",
            (void *)android_media_tv_Tuner_open_lnb_by_name },
    { "nativeOpenDescramblerByHandle", "(I)Landroid/media/tv/tuner/Descrambler;",
            (void *)android_media_tv_Tuner_open_descrambler },
    { "nativeOpenDvrRecorder", "(J)Landroid/media/tv/tuner/dvr/DvrRecorder;",
            (void *)android_media_tv_Tuner_open_dvr_recorder },
    { "nativeOpenDvrPlayback", "(J)Landroid/media/tv/tuner/dvr/DvrPlayback;",
            (void *)android_media_tv_Tuner_open_dvr_playback },
    { "nativeGetDemuxCapabilities", "()Landroid/media/tv/tuner/DemuxCapabilities;",
            (void *)android_media_tv_Tuner_get_demux_caps },
    { "nativeOpenDemuxByhandle", "(I)I", (void *)android_media_tv_Tuner_open_demux },
    { "nativeClose", "()I", (void *)android_media_tv_Tuner_close_tuner },
    { "nativeCloseFrontend", "(I)I", (void *)android_media_tv_Tuner_close_frontend },
    { "nativeCloseDemux", "(I)I", (void *)android_media_tv_Tuner_close_demux },
};

static const JNINativeMethod gFilterMethods[] = {
    { "nativeConfigureFilter", "(IILandroid/media/tv/tuner/filter/FilterConfiguration;)I",
            (void *)android_media_tv_Tuner_configure_filter },
    { "nativeGetId", "()I", (void *)android_media_tv_Tuner_get_filter_id },
    { "nativeSetDataSource", "(Landroid/media/tv/tuner/filter/Filter;)I",
            (void *)android_media_tv_Tuner_set_filter_data_source },
    { "nativeStartFilter", "()I", (void *)android_media_tv_Tuner_start_filter },
    { "nativeStopFilter", "()I", (void *)android_media_tv_Tuner_stop_filter },
    { "nativeFlushFilter", "()I", (void *)android_media_tv_Tuner_flush_filter },
    { "nativeRead", "([BJJ)I", (void *)android_media_tv_Tuner_read_filter_fmq },
    { "nativeClose", "()I", (void *)android_media_tv_Tuner_close_filter },
};

static const JNINativeMethod gTimeFilterMethods[] = {
    { "nativeSetTimestamp", "(J)I", (void *)android_media_tv_Tuner_time_filter_set_timestamp },
    { "nativeClearTimestamp", "()I", (void *)android_media_tv_Tuner_time_filter_clear_timestamp },
    { "nativeGetTimestamp", "()Ljava/lang/Long;",
            (void *)android_media_tv_Tuner_time_filter_get_timestamp },
    { "nativeGetSourceTime", "()Ljava/lang/Long;",
            (void *)android_media_tv_Tuner_time_filter_get_source_time },
    { "nativeClose", "()I", (void *)android_media_tv_Tuner_time_filter_close },
};

static const JNINativeMethod gDescramblerMethods[] = {
    { "nativeAddPid", "(IILandroid/media/tv/tuner/filter/Filter;)I",
            (void *)android_media_tv_Tuner_descrambler_add_pid },
    { "nativeRemovePid", "(IILandroid/media/tv/tuner/filter/Filter;)I",
            (void *)android_media_tv_Tuner_descrambler_remove_pid },
    { "nativeSetKeyToken", "([B)I", (void *)android_media_tv_Tuner_descrambler_set_key_token },
    { "nativeClose", "()I", (void *)android_media_tv_Tuner_close_descrambler },
};

static const JNINativeMethod gDvrRecorderMethods[] = {
    { "nativeAttachFilter", "(Landroid/media/tv/tuner/filter/Filter;)I",
            (void *)android_media_tv_Tuner_attach_filter },
    { "nativeDetachFilter", "(Landroid/media/tv/tuner/filter/Filter;)I",
            (void *)android_media_tv_Tuner_detach_filter },
    { "nativeConfigureDvr", "(Landroid/media/tv/tuner/dvr/DvrSettings;)I",
            (void *)android_media_tv_Tuner_configure_dvr },
    { "nativeStartDvr", "()I", (void *)android_media_tv_Tuner_start_dvr },
    { "nativeStopDvr", "()I", (void *)android_media_tv_Tuner_stop_dvr },
    { "nativeFlushDvr", "()I", (void *)android_media_tv_Tuner_flush_dvr },
    { "nativeClose", "()I", (void *)android_media_tv_Tuner_close_dvr },
    { "nativeSetFileDescriptor", "(I)V", (void *)android_media_tv_Tuner_dvr_set_fd },
    { "nativeWrite", "(J)J", (void *)android_media_tv_Tuner_write_dvr },
    { "nativeWrite", "([BJJ)J", (void *)android_media_tv_Tuner_write_dvr_to_array },
};

static const JNINativeMethod gDvrPlaybackMethods[] = {
    { "nativeAttachFilter", "(Landroid/media/tv/tuner/filter/Filter;)I",
            (void *)android_media_tv_Tuner_attach_filter },
    { "nativeDetachFilter", "(Landroid/media/tv/tuner/filter/Filter;)I",
            (void *)android_media_tv_Tuner_detach_filter },
    { "nativeConfigureDvr", "(Landroid/media/tv/tuner/dvr/DvrSettings;)I",
            (void *)android_media_tv_Tuner_configure_dvr },
    { "nativeStartDvr", "()I", (void *)android_media_tv_Tuner_start_dvr },
    { "nativeStopDvr", "()I", (void *)android_media_tv_Tuner_stop_dvr },
    { "nativeFlushDvr", "()I", (void *)android_media_tv_Tuner_flush_dvr },
    { "nativeClose", "()I", (void *)android_media_tv_Tuner_close_dvr },
    { "nativeSetFileDescriptor", "(I)V", (void *)android_media_tv_Tuner_dvr_set_fd },
    { "nativeRead", "(J)J", (void *)android_media_tv_Tuner_read_dvr },
    { "nativeRead", "([BJJ)J", (void *)android_media_tv_Tuner_read_dvr_from_array },
};

static const JNINativeMethod gLnbMethods[] = {
    { "nativeSetVoltage", "(I)I", (void *)android_media_tv_Tuner_lnb_set_voltage },
    { "nativeSetTone", "(I)I", (void *)android_media_tv_Tuner_lnb_set_tone },
    { "nativeSetSatellitePosition", "(I)I", (void *)android_media_tv_Tuner_lnb_set_position },
    { "nativeSendDiseqcMessage", "([B)I", (void *)android_media_tv_Tuner_lnb_send_diseqc_msg },
    { "nativeClose", "()I", (void *)android_media_tv_Tuner_close_lnb },
};

static const JNINativeMethod gMediaEventMethods[] = {
    { "nativeGetLinearBlock", "()Landroid/media/MediaCodec$LinearBlock;",
            (void *)android_media_tv_Tuner_media_event_get_linear_block },
    { "nativeGetAudioHandle", "()Ljava/lang/Long;",
            (void *)android_media_tv_Tuner_media_event_get_audio_handle },
    { "nativeFinalize", "()V",
            (void *)android_media_tv_Tuner_media_event_finalize },
};

static bool register_android_media_tv_Tuner(JNIEnv *env) {
    if (AndroidRuntime::registerNativeMethods(
            env, "android/media/tv/tuner/Tuner", gTunerMethods, NELEM(gTunerMethods)) != JNI_OK) {
        ALOGE("Failed to register tuner native methods");
        return false;
    }
    if (AndroidRuntime::registerNativeMethods(
            env, "android/media/tv/tuner/filter/Filter",
            gFilterMethods,
            NELEM(gFilterMethods)) != JNI_OK) {
        ALOGE("Failed to register filter native methods");
        return false;
    }
    if (AndroidRuntime::registerNativeMethods(
            env, "android/media/tv/tuner/filter/TimeFilter",
            gTimeFilterMethods,
            NELEM(gTimeFilterMethods)) != JNI_OK) {
        ALOGE("Failed to register time filter native methods");
        return false;
    }
    if (AndroidRuntime::registerNativeMethods(
            env, "android/media/tv/tuner/Descrambler",
            gDescramblerMethods,
            NELEM(gDescramblerMethods)) != JNI_OK) {
        ALOGE("Failed to register descrambler native methods");
        return false;
    }
    if (AndroidRuntime::registerNativeMethods(
            env, "android/media/tv/tuner/dvr/DvrRecorder",
            gDvrRecorderMethods,
            NELEM(gDvrRecorderMethods)) != JNI_OK) {
        ALOGE("Failed to register dvr recorder native methods");
        return false;
    }
    if (AndroidRuntime::registerNativeMethods(
            env, "android/media/tv/tuner/dvr/DvrPlayback",
            gDvrPlaybackMethods,
            NELEM(gDvrPlaybackMethods)) != JNI_OK) {
        ALOGE("Failed to register dvr playback native methods");
        return false;
    }
    if (AndroidRuntime::registerNativeMethods(
            env, "android/media/tv/tuner/Lnb",
            gLnbMethods,
            NELEM(gLnbMethods)) != JNI_OK) {
        ALOGE("Failed to register lnb native methods");
        return false;
    }
    if (AndroidRuntime::registerNativeMethods(
            env, "android/media/tv/tuner/filter/MediaEvent",
            gMediaEventMethods,
            NELEM(gMediaEventMethods)) != JNI_OK) {
        ALOGE("Failed to register MediaEvent native methods");
        return false;
    }
    return true;
}

jint JNI_OnLoad(JavaVM* vm, void* /* reserved */)
{
    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("ERROR: GetEnv failed\n");
        return result;
    }
    assert(env != NULL);

    if (!register_android_media_tv_Tuner(env)) {
        ALOGE("ERROR: Tuner native registration failed\n");
        return result;
    }
    return JNI_VERSION_1_4;
}
