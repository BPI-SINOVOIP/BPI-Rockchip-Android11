/*
** Copyright 2010, The Android Open-Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

//#define LOG_NDEBUG 0

#define LOG_TAG "AudioHardware"

#include <utils/Log.h>
#include <utils/String8.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <dlfcn.h>
#include <fcntl.h>

#include "AudioHardware.h"
#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>

#include <cutils/properties.h>

#include "AudioUsbAudioHardware.h"

extern "C" {
#include "alsa_audio.h"
}

//when you want write the output data ,you can open this maroc.
//#define DEBUG_ALSA_OUT
//#define DEBUG_ALSA_IN
#ifdef TARGET_RK2928
#define AMP_ENABLE_TIME 230//TARGET_RK2928 codec use amplifier enable time (Unit:ms)
#endif
namespace android_audio_legacy {

const uint32_t AudioHardware::inputSamplingRates[] = {
        8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

//  trace driver operations for dump
//
#define DRIVER_TRACE

enum {
    DRV_NONE,
    DRV_PCM_OPEN,
    DRV_PCM_CLOSE,
    DRV_PCM_WRITE,
    DRV_PCM_READ,
    DRV_MIXER_OPEN,
    DRV_MIXER_CLOSE,
    DRV_MIXER_GET,
    DRV_MIXER_SEL
};

#ifdef DRIVER_TRACE
#define TRACE_DRIVER_IN(op) mDriverOp = op;
#define TRACE_DRIVER_OUT mDriverOp = DRV_NONE;
#else
#define TRACE_DRIVER_IN(op)
#define TRACE_DRIVER_OUT
#endif

// ----------------------------------------------------------------------------

AudioHardware::AudioHardware() :
    mInit(false),
    mMicMute(false),
    mPcm(NULL),
    mPcmOpenCnt(0),
    mMixerOpenCnt(0),
    mInCallAudioMode(false),
    mVoipAudioMode(false),
    mInputSource("Default"),
    mBluetoothNrec(true),
    mSecRilLibHandle(NULL),
    mRilClient(0),
    mActivatedCP(false),
    mDriverOp(DRV_NONE)
{
    char pname[40];
    snprintf(pname, sizeof(pname), AUDIO_HAL_VERSION_NAME);
    property_set(pname, AUDIO_HAL_VERSION);

    loadRILD();
    mInit = true;

    TRACE_DRIVER_IN(DRV_MIXER_OPEN)
    route_init();
    TRACE_DRIVER_OUT
}

AudioHardware::~AudioHardware()
{
    for (size_t index = 0; index < mInputs.size(); index++) {
        closeInputStream(mInputs[index].get());
    }
    mInputs.clear();
    closeOutputStream((android_audio_legacy::AudioStreamOut*)mOutput.get());

    if (mPcm) {
        TRACE_DRIVER_IN(DRV_PCM_CLOSE)
        route_pcm_close(PLAYBACK_OFF_ROUTE);
        TRACE_DRIVER_OUT
    }

    TRACE_DRIVER_IN(DRV_MIXER_CLOSE)
    route_uninit();
    TRACE_DRIVER_OUT

    mInit = false;
}

status_t AudioHardware::initCheck()
{
    return mInit ? NO_ERROR : NO_INIT;
}

void AudioHardware::loadRILD(void)
{
    /*mSecRilLibHandle = dlopen("libsecril-client.so", RTLD_NOW);

    if (mSecRilLibHandle) {
        ALOGV("libsecril-client.so is loaded");

        openClientRILD   = (HRilClient (*)(void))
                              dlsym(mSecRilLibHandle, "OpenClient_RILD");
        disconnectRILD   = (int (*)(HRilClient))
                              dlsym(mSecRilLibHandle, "Disconnect_RILD");
        closeClientRILD  = (int (*)(HRilClient))
                              dlsym(mSecRilLibHandle, "CloseClient_RILD");
        isConnectedRILD  = (int (*)(HRilClient))
                              dlsym(mSecRilLibHandle, "isConnected_RILD");
        connectRILD      = (int (*)(HRilClient))
                              dlsym(mSecRilLibHandle, "Connect_RILD");
        setCallVolume    = (int (*)(HRilClient, SoundType, int))
                              dlsym(mSecRilLibHandle, "SetCallVolume");
        setCallAudioPath = (int (*)(HRilClient, AudioPath))
                              dlsym(mSecRilLibHandle, "SetCallAudioPath");
        setCallClockSync = (int (*)(HRilClient, SoundClockCondition))
                              dlsym(mSecRilLibHandle, "SetCallClockSync");

        if (!openClientRILD  || !disconnectRILD   || !closeClientRILD ||
            !isConnectedRILD || !connectRILD      ||
            !setCallVolume   || !setCallAudioPath || !setCallClockSync) {
            ALOGE("Can't load all functions from libsecril-client.so");

            dlclose(mSecRilLibHandle);
            mSecRilLibHandle = NULL;
        } else {
            mRilClient = openClientRILD();
            if (!mRilClient) {
                ALOGE("OpenClient_RILD() error");

                dlclose(mSecRilLibHandle);
                mSecRilLibHandle = NULL;
            }
        }
    } else {
        ALOGE("Can't load libsecril-client.so");
    }*/
}

status_t AudioHardware::connectRILDIfRequired(void)
{
    if (!mSecRilLibHandle) {
        ALOGE("connectIfRequired() lib is not loaded");
        return INVALID_OPERATION;
    }

    if (isConnectedRILD(mRilClient)) {
        return OK;
    }

    if (connectRILD(mRilClient) != RIL_CLIENT_ERR_SUCCESS) {
        ALOGE("Connect_RILD() error");
        return INVALID_OPERATION;
    }

    return OK;
}

android_audio_legacy::AudioStreamOut* AudioHardware::openOutputStream(
    uint32_t devices, int *format, uint32_t *channels,
    uint32_t *sampleRate, status_t *status)
{
     android::sp <AudioStreamOutALSA> out;
    status_t rc;

    { // scope for the lock
         android::Mutex::Autolock lock(mLock);

        // only one output stream allowed
        if (mOutput != 0) {
            if (status) {
                *status = INVALID_OPERATION;
            }
            return NULL;
        }

        out = new AudioStreamOutALSA();

        rc = out->set(this, devices, format, channels, sampleRate);
        if (rc == NO_ERROR) {
            mOutput = out;
        }
    }

    if (rc != NO_ERROR) {
        if (out != 0) {
            out.clear();
        }
    }
    if (status) {
        *status = rc;
    }

    return out.get();
}


// default implementation calls its "without flags" counterpart
android_audio_legacy::AudioStreamOut* AudioHardware::openOutputStreamWithFlags(uint32_t devices,
                                          audio_output_flags_t flags,
                                          int *format,
                                          uint32_t *channels,
                                          uint32_t *sampleRate,
                                          status_t *status)
{
    return openOutputStream(devices, format, channels, sampleRate, status);
}

status_t AudioHardware::setMasterMute(bool muted){
    return INVALID_OPERATION;
}


int AudioHardware::createAudioPatch(unsigned int num_sources,
                               const struct audio_port_config *sources,
                               unsigned int num_sinks,
                               const struct audio_port_config *sinks,
                               audio_patch_handle_t *handle){
        return 0;
}

int AudioHardware::releaseAudioPatch(audio_patch_handle_t handle){
        return 0;
}

int AudioHardware::getAudioPort(struct audio_port *port) {
        return 0;
}

int AudioHardware::setAudioPortConfig(const struct audio_port_config *config) {
        return 0;
}

void AudioHardware::closeOutputStream(android_audio_legacy::AudioStreamOut* out) {
     android::sp <AudioStreamOutALSA> spOut;
    {
         android::Mutex::Autolock lock(mLock);
        if (mOutput == 0 || mOutput.get() != out) {
            ALOGW("Attempt to close invalid output stream");
            return;
        }
        spOut = mOutput;
        mOutput.clear();
    }
    spOut.clear();
}

android_audio_legacy::AudioStreamIn* AudioHardware::openInputStream(
    uint32_t devices, int *format, uint32_t *channels,
    uint32_t *sampleRate, status_t *status,
    android_audio_legacy::AudioSystem::audio_in_acoustics acoustic_flags)
{
    // check for valid input source
    if (!android_audio_legacy::AudioSystem::isInputDevice((android_audio_legacy::AudioSystem::audio_devices)devices)) {
        if (status) {
            *status = BAD_VALUE;
        }
        return NULL;
    }

    status_t rc = NO_ERROR;
     android::sp <AudioStreamInALSA> in;

    { // scope for the lock
         android::Mutex::Autolock lock(mLock);

        in = new AudioStreamInALSA();
        rc = in->set(this, devices, format, channels, sampleRate, acoustic_flags);
        if (rc == NO_ERROR) {
            mInputs.add(in);
        }
    }

    if (rc != NO_ERROR) {
        if (in != 0) {
            in.clear();
        }
    }
    if (status) {
        *status = rc;
    }

    ALOGV("AudioHardware::openInputStream()%p", in.get());
    return in.get();
}

void AudioHardware::closeInputStream(AudioStreamIn* in) {

     android::sp<AudioStreamInALSA> spIn;
    {
         android::Mutex::Autolock lock(mLock);

        ssize_t index = mInputs.indexOf((AudioStreamInALSA *)in);
        if (index < 0) {
            ALOGW("Attempt to close invalid input stream");
            return;
        }
        spIn = mInputs[index];
        mInputs.removeAt(index);
    }
    ALOGV("AudioHardware::closeInputStream()%p", in);
    spIn.clear();
}


status_t AudioHardware::setMode(int mode)
{
    android::sp<AudioStreamOutALSA> spOut;
    android::sp<AudioStreamInALSA> spIn;
    status_t status;

    // bump thread priority to speed up mutex acquisition
    int  priority = getpriority(PRIO_PROCESS, 0);
    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_URGENT_AUDIO);

    // Mutex acquisition order is always out -> in -> hw
    android::AutoMutex lock(mLock);

    spOut = mOutput;
    while (spOut != 0) {
        if (!spOut->checkStandby()) {
            int cnt = spOut->standbyCnt();
            mLock.unlock();
            spOut->lock();
            mLock.lock();
            // make sure that another thread did not change output state while the
            // mutex is released
            if ((spOut == mOutput) && (cnt == spOut->standbyCnt())) {
                break;
            }
            spOut->unlock();
            spOut = mOutput;
        } else {
            spOut.clear();
        }
    }

    spIn = getActiveInput_l();
    while (spIn != 0) {
        int cnt = spIn->standbyCnt();
        mLock.unlock();
        spIn->lock();
        mLock.lock();
        // make sure that another thread did not change input state while the
        // mutex is released
        if ((spIn == getActiveInput_l()) && (cnt == spIn->standbyCnt())) {
            break;
        }
        spIn->unlock();
        spIn = getActiveInput_l();
    }

    setpriority(PRIO_PROCESS, 0, priority);

    int prevMode = mMode;
    status = AudioHardwareBase::setMode(mode);
    ALOGV("setMode() : new %d, old %d", mMode, prevMode);
    if (status == NO_ERROR) {
        // activate call clock in radio when entering in call or ringtone mode
        if (prevMode == AudioSystem::MODE_NORMAL)
        {
            if ((!mActivatedCP) && (mSecRilLibHandle) && (connectRILDIfRequired() == OK)) {
                setCallClockSync(mRilClient, SOUND_CLOCK_START);
                mActivatedCP = true;
            }
        }

        //close voip before incall opening
        if ((mMode != AudioSystem::MODE_IN_COMMUNICATION)
            && mVoipAudioMode) {
            setInputSource_l(mInputSource);
            TRACE_DRIVER_IN(DRV_MIXER_SEL)
            route_set_controls(VOIP_OFF_ROUTE);//close voip
            TRACE_DRIVER_OUT

            mVoipAudioMode = false;
        }

        if (mMode == AudioSystem::MODE_IN_CALL && !mInCallAudioMode) {
            //sleep latency time to finish music
            if (mOutput != 0) {
                mLock.unlock();
                usleep((mOutput->latency() + 70) * 1000);
                mLock.lock();
            }

            ALOGV("setMode() openPcmOut_l()");
            openPcmOut_l();
            setInputSource_l(String8("Default"));
            if (mOutput != 0 && AudioSystem::popCount(mOutput->device()) == 1)
                setIncallPath_l(mOutput->device());
            mInCallAudioMode = true;
        }

        if (mMode != AudioSystem::MODE_IN_CALL && mInCallAudioMode) {
            setInputSource_l(mInputSource);

            TRACE_DRIVER_IN(DRV_MIXER_SEL)
            route_pcm_close(INCALL_OFF_ROUTE);//close incall
            TRACE_DRIVER_OUT

            ALOGV("setMode() closePcmOut_l()");
            closePcmOut_l();

            mInCallAudioMode = false;
        }

        if (mMode == AudioSystem::MODE_IN_COMMUNICATION && !mVoipAudioMode) {
            setInputSource_l(String8("Default"));
            if (mOutput != 0) {
                mOutput->doStandby_l();
            }
            mVoipAudioMode = true;
        }

        if (mMode == AudioSystem::MODE_NORMAL) {
            if(mActivatedCP)
                mActivatedCP = false;
        }
    }

    if (spIn != 0) {
        spIn->unlock();
    }
    if (spOut != 0) {
        spOut->unlock();
    }

    return status;
}

status_t AudioHardware::setMicMute(bool state)
{
    ALOGV("setMicMute(%d) mMicMute %d", state, mMicMute);
     android::sp<AudioStreamInALSA> spIn;
    {
         android::AutoMutex lock(mLock);
        if (mMicMute != state) {
            mMicMute = state;
            // in call mute is handled by RIL
            if (mMode != AudioSystem::MODE_IN_CALL) {
                spIn = getActiveInput_l();
            }
        }
    }

    if (spIn != 0) {
        spIn->setGain(mMicMute?0.0:1.0);
    }

    return NO_ERROR;
}

status_t AudioHardware::getMicMute(bool* state)
{
    *state = mMicMute;
    return NO_ERROR;
}

status_t AudioHardware::setParameters(const String8& keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);
    String8 value;
    String8 key;
    const char BT_NREC_KEY[] = "bt_headset_nrec";
    const char BT_NREC_VALUE_ON[] = "on";

    key = String8(BT_NREC_KEY);
    if (param.get(key, value) == NO_ERROR) {
        if (value == BT_NREC_VALUE_ON) {
            mBluetoothNrec = true;
        } else {
            mBluetoothNrec = false;
            ALOGD("Turning noise reduction and echo cancellation off for BT "
                 "headset");
        }
    }

    return NO_ERROR;
}

String8 AudioHardware::getParameters(const String8& keys)
{
    AudioParameter request = AudioParameter(keys);
    AudioParameter reply = AudioParameter();

    ALOGV("getParameters() %s", keys.string());

    return reply.toString();
}

size_t AudioHardware::getInputBufferSize(uint32_t sampleRate, int format, int channelCount)
{
    if (format != AudioSystem::PCM_16_BIT) {
        ALOGW("getInputBufferSize bad format: %d", format);
        return 0;
    }
    if (channelCount < 1 || channelCount > 2) {
        ALOGW("getInputBufferSize bad channel count: %d", channelCount);
        return 0;
    }
    if (sampleRate != 8000 && sampleRate != 11025 && sampleRate != 16000 &&
            sampleRate != 22050 && sampleRate != 44100  && sampleRate != 48000) {
        ALOGW("getInputBufferSize bad sample rate: %d", sampleRate);
        return 0;
    }

    return AudioStreamInALSA::getBufferSize(sampleRate, channelCount);
}


status_t AudioHardware::setVoiceVolume(float volume)
{
    ALOGV("setVoiceVolume() volume %f", volume);

    android::AutoMutex lock(mLock);
    if (AudioSystem::MODE_IN_CALL == mMode) {

        uint32_t device = AudioSystem::DEVICE_OUT_EARPIECE;
        char ctlName[44] = "";
        if (mOutput != 0) {
            device = mOutput->device();
        }

        ALOGV("setVoiceVolume() route(%d)", device);
        switch (device) {
            case AudioSystem::DEVICE_OUT_EARPIECE:
                ALOGV("earpiece call volume");
                strcpy(ctlName, "Earpiece Playback Volume");
                break;

            case AudioSystem::DEVICE_OUT_SPEAKER:
                ALOGV("speaker call volume");
                strcpy(ctlName, "Speaker Playback Volume");
                break;

            case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO:
            case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
            case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
                ALOGV("bluetooth call volume");
                break;

            case AudioSystem::DEVICE_OUT_WIRED_HEADSET:
            case AudioSystem::DEVICE_OUT_WIRED_HEADPHONE: // Use receive path with 3 pole headset.
                ALOGV("headset call volume");
                strcpy(ctlName, "Headphone Playback Volume");
                break;

            default:
                ALOGW("Call volume setting error!!!0x%08x \n", device);
                strcpy(ctlName, "Earpiece Playback Volume");
                break;
        }
        TRACE_DRIVER_IN(DRV_MIXER_SEL)
        route_set_voice_volume(ctlName, volume);
        TRACE_DRIVER_OUT
    }

    return NO_ERROR;
}

status_t AudioHardware::setMasterVolume(float volume)
{
    ALOGV("Set master volume to %f.\n", volume);
    // We return an error code here to let the audioflinger do in-software
    // volume on top of the maximum volume that we set through the SND API.
    // return error - software mixer will handle it
    return -1;
}

static const int kDumpLockRetries = 50;
static const int kDumpLockSleep = 20000;

static bool tryLock( android::Mutex& mutex)
{
    bool locked = false;
    for (int i = 0; i < kDumpLockRetries; ++i) {
        if (mutex.tryLock() == NO_ERROR) {
            locked = true;
            break;
        }
        usleep(kDumpLockSleep);
    }
    return locked;
}

status_t AudioHardware::dump(int fd, const Vector<String16>& args)
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    bool locked = tryLock(mLock);
    if (!locked) {
        snprintf(buffer, SIZE, "\n\tAudioHardware maybe deadlocked\n");
    } else {
        mLock.unlock();
    }

    snprintf(buffer, SIZE, "\tInit %s\n", (mInit) ? "OK" : "Failed");
    result.append(buffer);
    snprintf(buffer, SIZE, "\tMic Mute %s\n", (mMicMute) ? "ON" : "OFF");
    result.append(buffer);
    snprintf(buffer, SIZE, "\tmPcm: %p\n", mPcm);
    result.append(buffer);
    snprintf(buffer, SIZE, "\tmPcmOpenCnt: %d\n", mPcmOpenCnt);
    result.append(buffer);
    snprintf(buffer, SIZE, "\tmMixerOpenCnt: %d\n", mMixerOpenCnt);
    result.append(buffer);
    snprintf(buffer, SIZE, "\tIn Call Audio Mode %s\n",
             (mInCallAudioMode) ? "ON" : "OFF");
    result.append(buffer);
    snprintf(buffer, SIZE, "\tInput source %s\n", mInputSource.string());
    result.append(buffer);
    snprintf(buffer, SIZE, "\tmSecRilLibHandle: %p\n", mSecRilLibHandle);
    result.append(buffer);
    snprintf(buffer, SIZE, "\tmRilClient: %p\n", mRilClient);
    result.append(buffer);
    snprintf(buffer, SIZE, "\tCP %s\n",
             (mActivatedCP) ? "Activated" : "Deactivated");
    result.append(buffer);
    snprintf(buffer, SIZE, "\tmDriverOp: %d\n", mDriverOp);
    result.append(buffer);

    snprintf(buffer, SIZE, "\n\tmOutput %p dump:\n", mOutput.get());
    result.append(buffer);
    write(fd, result.string(), result.size());
    if (mOutput != 0) {
        mOutput->dump(fd, args);
    }

    snprintf(buffer, SIZE, "\n\t%d inputs opened:\n", mInputs.size());
    write(fd, buffer, strlen(buffer));
    for (size_t i = 0; i < mInputs.size(); i++) {
        snprintf(buffer, SIZE, "\t- input %d dump:\n", i);
        write(fd, buffer, strlen(buffer));
        mInputs[i]->dump(fd, args);
    }

    return NO_ERROR;
}

status_t AudioHardware::setIncallPath_l(uint32_t device)
{
    ALOGV("setIncallPath_l: device %x", device);

    if (mMode == AudioSystem::MODE_IN_CALL) {
        TRACE_DRIVER_IN(DRV_PCM_OPEN)
        if (!mPcm)
            openPcmOut_l();
        else
            mPcm = route_pcm_open(getRouteFromDevice(device), mPcm->flags);
        TRACE_DRIVER_OUT
    }

    return NO_ERROR;
}

struct pcm *AudioHardware::openPcmOut_l()
{
    ALOGD("openPcmOut_l() mPcmOpenCnt: %d", mPcmOpenCnt);
    if (mPcmOpenCnt++ == 0) {
        if (mPcm != NULL) {
            ALOGE("openPcmOut_l() mPcmOpenCnt == 0 and mPcm == %p\n", mPcm);
            mPcmOpenCnt--;
            return NULL;
        }
        unsigned flags = PCM_OUT;

        flags |= (AUDIO_HW_OUT_PERIOD_MULT - 1) << PCM_PERIOD_SZ_SHIFT;
        flags |= (AUDIO_HW_OUT_PERIOD_CNT - PCM_PERIOD_CNT_MIN) << PCM_PERIOD_CNT_SHIFT;

        if (mOutput->sampleRate() == 48000) {
            flags |= PCM_48000HZ;
        }

        TRACE_DRIVER_IN(DRV_PCM_OPEN)
        mPcm = route_pcm_open(getRouteFromDevice(mOutput->device()), flags);
        TRACE_DRIVER_OUT
        if (!pcm_ready(mPcm)) {
            ALOGE("openPcmOut_l() cannot open pcm_out driver: %s\n", pcm_error(mPcm));
            TRACE_DRIVER_IN(DRV_PCM_CLOSE)
            route_pcm_close(PLAYBACK_OFF_ROUTE);
            TRACE_DRIVER_OUT
            mPcmOpenCnt--;
            mPcm = NULL;
        }
    }
    return mPcm;
}

void AudioHardware::closePcmOut_l()
{
    ALOGD("closePcmOut_l() mPcmOpenCnt: %d", mPcmOpenCnt);
    if (mPcmOpenCnt == 0) {
        ALOGE("closePcmOut_l() mPcmOpenCnt == 0");
        return;
    }

    if (--mPcmOpenCnt == 0) {
        ALOGV("close_l() reset Playback Path to OFF");
        TRACE_DRIVER_IN(DRV_PCM_CLOSE)
        route_pcm_close(PLAYBACK_OFF_ROUTE);
        TRACE_DRIVER_OUT
        mPcm = NULL;
    }
}

unsigned AudioHardware::getOutputRouteFromDevice(uint32_t device)
{
    if (mMode != AudioSystem::MODE_RINGTONE && mMode != AudioSystem::MODE_NORMAL)
        return PLAYBACK_OFF_ROUTE;

    switch (device) {
    case AudioSystem::DEVICE_OUT_EARPIECE:
        return EARPIECE_NORMAL_ROUTE;
    case AudioSystem::DEVICE_OUT_SPEAKER:
        if (mMode == AudioSystem::MODE_RINGTONE) return SPEAKER_RINGTONE_ROUTE;
        else return SPEAKER_NORMAL_ROUTE;
    case AudioSystem::DEVICE_OUT_WIRED_HEADPHONE:
        if (mMode == AudioSystem::MODE_RINGTONE) return HEADPHONE_RINGTONE_ROUTE;
        else return HEADPHONE_NORMAL_ROUTE;
    case AudioSystem::DEVICE_OUT_WIRED_HEADSET:
        if (mMode == AudioSystem::MODE_RINGTONE) return HEADSET_RINGTONE_ROUTE;
        else return HEADSET_NORMAL_ROUTE;
    case (AudioSystem::DEVICE_OUT_SPEAKER|AudioSystem::DEVICE_OUT_WIRED_HEADPHONE):
    case (AudioSystem::DEVICE_OUT_SPEAKER|AudioSystem::DEVICE_OUT_WIRED_HEADSET):
        if (mMode == AudioSystem::MODE_RINGTONE) return SPEAKER_HEADPHONE_RINGTONE_ROUTE;
        else return SPEAKER_HEADPHONE_NORMAL_ROUTE;
    case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO:
    case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
    case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
        return BLUETOOTH_NORMAL_ROUTE;
    case AudioSystem::DEVICE_OUT_AUX_DIGITAL:
        return HDMI_NORMAL_ROUTE;
    case AudioSystem::DEVICE_OUT_ANLG_DOCK_HEADSET:
    case AudioSystem::DEVICE_OUT_DGTL_DOCK_HEADSET:
        return USB_NORMAL_ROUTE;
    default:
        return PLAYBACK_OFF_ROUTE;
    }
}

unsigned AudioHardware::getVoiceRouteFromDevice(uint32_t device)
{
    if (mMode != AudioSystem::MODE_IN_CALL && mMode != AudioSystem::MODE_IN_COMMUNICATION)
        return INCALL_OFF_ROUTE;

    if (device & AudioSystem::DEVICE_OUT_BLUETOOTH_SCO ||
        device & AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET ||
        device & AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT) {
        if (mMode == AudioSystem::MODE_IN_CALL) return BLUETOOTH_INCALL_ROUTE;
        else return BLUETOOTH_VOIP_ROUTE;
    } else if (device & AudioSystem::DEVICE_OUT_WIRED_HEADPHONE) {
        if (mMode == AudioSystem::MODE_IN_CALL) return HEADPHONE_INCALL_ROUTE;
        else return HEADPHONE_VOIP_ROUTE;
    } else if (device & AudioSystem::DEVICE_OUT_WIRED_HEADSET) {
        if (mMode == AudioSystem::MODE_IN_CALL) return HEADSET_INCALL_ROUTE;
        else return HEADSET_VOIP_ROUTE;
    } else if (device & AudioSystem::DEVICE_OUT_ANLG_DOCK_HEADSET ||
        device & AudioSystem::DEVICE_OUT_DGTL_DOCK_HEADSET) {
        if (mMode == AudioSystem::MODE_IN_CALL) return EARPIECE_INCALL_ROUTE;
        else return USB_NORMAL_ROUTE;
    } else if (device & AudioSystem::DEVICE_OUT_AUX_DIGITAL) {
        if (mMode == AudioSystem::MODE_IN_CALL) return EARPIECE_INCALL_ROUTE;
        else return HDMI_NORMAL_ROUTE;
    } else if (device & AudioSystem::DEVICE_OUT_EARPIECE) {
        if (mMode == AudioSystem::MODE_IN_CALL) return EARPIECE_INCALL_ROUTE;
        else return EARPIECE_VOIP_ROUTE;
    } else if (device & AudioSystem::DEVICE_OUT_SPEAKER) {
        if (mMode == AudioSystem::MODE_IN_CALL) return SPEAKER_INCALL_ROUTE;
        else return SPEAKER_VOIP_ROUTE;
    } else {
        if (mMode == AudioSystem::MODE_IN_CALL) return INCALL_OFF_ROUTE;
        else return VOIP_OFF_ROUTE;
    }
}

unsigned AudioHardware::getInputRouteFromDevice(uint32_t device)
{
    if (mMicMute) {
        return CAPTURE_OFF_ROUTE;
    }

    switch (device) {
    case AudioSystem::DEVICE_IN_BUILTIN_MIC:
        return MAIN_MIC_CAPTURE_ROUTE;
    case AudioSystem::DEVICE_IN_WIRED_HEADSET:
        return HANDS_FREE_MIC_CAPTURE_ROUTE;
    case AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET:
        return BLUETOOTH_SOC_MIC_CAPTURE_ROUTE;
    case AudioSystem::DEVICE_IN_ANLG_DOCK_HEADSET:
        return USB_CAPTURE_ROUTE;
    default:
        return CAPTURE_OFF_ROUTE;
    }
}

unsigned AudioHardware::getRouteFromDevice(uint32_t device)
{
    if (device & AudioSystem::DEVICE_IN_ALL)
        return getInputRouteFromDevice(device);

    switch (mMode) {
    case AudioSystem::MODE_IN_CALL:
    case AudioSystem::MODE_IN_COMMUNICATION:
        return getVoiceRouteFromDevice(device);
    default:
        return getOutputRouteFromDevice(device);
    }
}

uint32_t AudioHardware::getInputSampleRate(uint32_t sampleRate)
{
    uint32_t i;
    uint32_t prevDelta;
    uint32_t delta;

    for (i = 0, prevDelta = 0xFFFFFFFF; i < sizeof(inputSamplingRates)/sizeof(uint32_t); i++, prevDelta = delta) {
        delta = abs(sampleRate - inputSamplingRates[i]);
        if (delta > prevDelta) break;
    }
    // i is always > 0 here
    return inputSamplingRates[i-1];
}

// getActiveInput_l() must be called with mLock held
 android::sp <AudioHardware::AudioStreamInALSA> AudioHardware::getActiveInput_l()
{
     android::sp< AudioHardware::AudioStreamInALSA> spIn;

    for (size_t i = 0; i < mInputs.size(); i++) {
        // return first input found not being in standby mode
        // as only one input can be in this state
        if (!mInputs[i]->checkStandby()) {
            spIn = mInputs[i];
            break;
        }
    }

    return spIn;
}

status_t AudioHardware::setInputSource_l(String8 source)
{
     ALOGV("setInputSource_l(%s)", source.string());
     if (source != mInputSource) {
         if ((source == "Default") || (mMode != AudioSystem::MODE_IN_CALL)) {
             ALOGV("mixer_ctl_select, Input Source, (%s)", source.string());
             TRACE_DRIVER_IN(DRV_MIXER_SEL)
             route_set_input_source(source.string());
             TRACE_DRIVER_OUT
         }
         mInputSource = source;
     }

     return NO_ERROR;
}


//------------------------------------------------------------------------------
//  AudioStreamOutALSA
//------------------------------------------------------------------------------
#ifdef DEBUG_ALSA_OUT
static FILE * alsa_out_fp = NULL;
#endif

AudioHardware::AudioStreamOutALSA::AudioStreamOutALSA() :
    mHardware(0), mRouteCtl(0),
    mStandby(true), mDevices(0), mChannels(AUDIO_HW_OUT_CHANNELS),
    mSampleRate(AUDIO_HW_OUT_SAMPLERATE), mBufferSize(AUDIO_HW_OUT_PERIOD_BYTES),
    mDriverOp(DRV_NONE), mStandbyCnt(0)
{
#ifdef DEBUG_ALSA_OUT
	if(alsa_out_fp== NULL)
		alsa_out_fp = fopen("/data/data/out.pcm","a+");
	if(alsa_out_fp)
		ALOGI("------------>openfile success");         
#endif                                                         
}

status_t AudioHardware::AudioStreamOutALSA::set(
    AudioHardware* hw, uint32_t devices, int *pFormat,
    uint32_t *pChannels, uint32_t *pRate)
{
    int lFormat = pFormat ? *pFormat : 0;
    uint32_t lChannels = pChannels ? *pChannels : 0;
    uint32_t lRate = pRate ? *pRate : 0;

    mHardware = hw;
    mDevices = devices;

    // fix up defaults
    if (lFormat == 0) lFormat = format();
    if (lChannels == 0) lChannels = channels();
    if (lRate == 0) lRate = sampleRate();

    if (devices & AudioSystem::DEVICE_OUT_ANLG_DOCK_HEADSET ||
        devices & AudioSystem::DEVICE_OUT_DGTL_DOCK_HEADSET) {
        uint32_t usbChannels = (lChannels == AudioSystem::CHANNEL_OUT_MONO) ? 1 : 2;
        mSampleRate = get_USBAudio_sampleRate(UA_Playback_type, lRate);
        mChannels = (get_USBAudio_Channels(UA_Playback_type, usbChannels) == 1) ?
            AudioSystem::CHANNEL_OUT_MONO : AudioSystem::CHANNEL_OUT_STEREO;
    }

    if (pFormat) *pFormat = format();
    if (pChannels) *pChannels = channels();
    if (pRate) *pRate = sampleRate();

    mBufferSize = AUDIO_HW_OUT_PERIOD_BYTES;

    return NO_ERROR;
}

AudioHardware::AudioStreamOutALSA::~AudioStreamOutALSA()
{
    standby();
#ifdef DEBUG_ALSA_OUT
	if(alsa_out_fp)
		fclose(alsa_out_fp);
#endif                                  
}


ssize_t AudioHardware::AudioStreamOutALSA::write(const void* buffer, size_t bytes)
{
    //    ALOGV("AudioStreamOutALSA::write(%p, %u)", buffer, bytes);
    status_t status = NO_INIT;
    const uint8_t* p = static_cast<const uint8_t*>(buffer);
    int ret;
	
#ifdef DEBUG_ALSA_OUT
	if(alsa_out_fp)
		fwrite(buffer,1,bytes,alsa_out_fp);
#endif     

    if (mHardware == NULL) return NO_INIT;

    { // scope for the lock

        android::AutoMutex lock(mLock);

        if (mStandby) {
            android::AutoMutex hwLock(mHardware->lock());

            ALOGD("AudioHardware pcm playback is exiting standby.");
            acquire_wake_lock (PARTIAL_WAKE_LOCK, "AudioOutLock");

           /* android::sp<AudioStreamInALSA> spIn = mHardware->getActiveInput_l();
            while (spIn != 0) {
                int cnt = spIn->standbyCnt();
                mHardware->lock().unlock();
                // Mutex acquisition order is always out -> in -> hw
                spIn->lock();
                mHardware->lock().lock();
                // make sure that another thread did not change input state
                // while the mutex is released
                if ((spIn == mHardware->getActiveInput_l()) &&
                        (cnt == spIn->standbyCnt())) {
                    ALOGV("AudioStreamOutALSA::write() force input standby");
                    spIn->close_l();
                    break;
                }
                spIn->unlock();
                spIn = mHardware->getActiveInput_l();
            }*/
            // spIn is not 0 here only if the input was active and has been
            // closed above

            // open output before input
            open_l();

           /* if (spIn != 0) {
                if (spIn->open_l() != NO_ERROR) {
                    spIn->doStandby_l();
                }
                spIn->unlock();
            }*/
            if (mHardware->getPcm() == NULL) {
                release_wake_lock("AudioOutLock");
                goto Error;
            }
            mStandby = false;
#ifdef TARGET_RK2928
            usleep(AMP_ENABLE_TIME*1000);
#endif
        }

        TRACE_DRIVER_IN(DRV_PCM_WRITE)
        ret = pcm_write(mHardware->getPcm(),(void*) p, bytes);
        TRACE_DRIVER_OUT

        if (ret == 0) {
            return bytes;
        }
        ALOGW("write error: %d", errno);
        status = -errno;
    }
Error:

    standby();

    // Simulate audio output timing in case of error
    usleep((((bytes * 1000) / frameSize()) * 1000) / sampleRate());

    return status;
}

status_t AudioHardware::AudioStreamOutALSA::standby()
{
    if (mHardware == NULL) return NO_INIT;

     android::AutoMutex lock(mLock);

    { // scope for the AudioHardware lock
        android::AutoMutex hwLock(mHardware->lock());
        if (mHardware->mode() != AudioSystem::MODE_IN_CALL)
            doStandby_l();
    }

    return NO_ERROR;
}

void AudioHardware::AudioStreamOutALSA::doStandby_l()
{
    mStandbyCnt++;

    if (!mStandby) {
        ALOGD("AudioHardware pcm playback is going to standby.");
        release_wake_lock("AudioOutLock");
        mStandby = true;
    }

    close_l();
}

void AudioHardware::AudioStreamOutALSA::close_l()
{
    if (mHardware->getPcm()) {
        mHardware->closePcmOut_l();
    }
}

status_t AudioHardware::AudioStreamOutALSA::open_l()
{
    ALOGV("open pcm_out driver");
    mHardware->openPcmOut_l();
    if (mHardware->getPcm() == NULL) {
        return NO_INIT;
    }
    return NO_ERROR;
}

status_t AudioHardware::AudioStreamOutALSA::dump(int fd, const Vector<String16>& args)
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    bool locked = tryLock(mLock);
    if (!locked) {
        snprintf(buffer, SIZE, "\n\t\tAudioStreamOutALSA maybe deadlocked\n");
    } else {
        mLock.unlock();
    }

    snprintf(buffer, SIZE, "\t\tmHardware: %p\n", mHardware);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmRouteCtl: %p\n", mRouteCtl);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tStandby %s\n", (mStandby) ? "ON" : "OFF");
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmDevices: 0x%08x\n", mDevices);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmChannels: 0x%08x\n", mChannels);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmSampleRate: %d\n", mSampleRate);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmBufferSize: %d\n", mBufferSize);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmDriverOp: %d\n", mDriverOp);
    result.append(buffer);

    ::write(fd, result.string(), result.size());

    return NO_ERROR;
}

bool AudioHardware::AudioStreamOutALSA::checkStandby()
{
    return mStandby;
}

status_t AudioHardware::AudioStreamOutALSA::setParameters(const String8& keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);
    status_t status = NO_ERROR;
    int value;
    ALOGD("AudioStreamOutALSA::setParameters() %s", keyValuePairs.string());

    if (mHardware == NULL) return NO_INIT;

    {
         bool needStandby = false;
         android::AutoMutex lock(mLock);

        if (param.getInt(String8(AudioParameter::keyRouting), value) == NO_ERROR)
        {
            //for MID alsa ,not have a routing change.
            android::AutoMutex hwLock(mHardware->lock());

            if (mDevices != (uint32_t)value && value != AUDIO_DEVICE_NONE) {
                mDevices = (uint32_t)value;
                if (mHardware->mode() == AudioSystem::MODE_IN_CALL) {
                    mHardware->setIncallPath_l(mDevices);
                } else
                    needStandby = true;
            }

            param.remove(String8(AudioParameter::keyRouting));
        }

        if (param.getInt(String8(AudioParameter::keySamplingRate), value) == NO_ERROR)
        {
            if (mSampleRate != (uint32_t)value && value != 0 &&
                (value == 48000 || value == 44100)) {
                mSampleRate = (uint32_t)value;

                android::AutoMutex hwLock(mHardware->lock());
                if (mHardware->mode() != AudioSystem::MODE_IN_CALL) {
                    needStandby = true;
                }
            }
            param.remove(String8(AudioParameter::keySamplingRate));
        }

        if (needStandby){
            android::AutoMutex hwLock(mHardware->lock());
            doStandby_l();
        }
    }

    if (param.size()) {
        status = BAD_VALUE;
    }


    return status;

}

String8 AudioHardware::AudioStreamOutALSA::getParameters(const String8& keys)
{
    AudioParameter param = AudioParameter(keys);
    String8 value;
    String8 key = String8(AudioParameter::keyRouting);

    if (param.get(key, value) == NO_ERROR) {
        param.addInt(key, (int)mDevices);
    }

    if (param.get(String8(AudioParameter::keySamplingRate), value) == NO_ERROR) {
        param.addInt(String8(AudioParameter::keySamplingRate), (int)mSampleRate);
    }

    ALOGV("AudioStreamOutALSA::getParameters() %s", param.toString().string());
    return param.toString();
}

status_t AudioHardware::AudioStreamOutALSA::getRenderPosition(uint32_t *dspFrames)
{
    //TODO
    return INVALID_OPERATION;
}



//------------------------------------------------------------------------------
//  AudioStreamInALSA
//------------------------------------------------------------------------------

#ifdef DEBUG_ALSA_IN
static FILE * alsa_in_fp = NULL;
#endif

AudioHardware::AudioStreamInALSA::AudioStreamInALSA() :
    mHardware(0), mPcm(0), mRouteCtl(0),
    mStandby(true), mDevices(0), mChannels(AUDIO_HW_IN_CHANNELS), mChannelCount(1),
    mSampleRate(AUDIO_HW_IN_SAMPLERATE),  mReqSampleRate(AUDIO_HW_IN_SAMPLERATE),
    mInSampleRate(AUDIO_HW_IN_SAMPLERATE), mBufferSize(AUDIO_HW_IN_PERIOD_BYTES),
    mDownSampler(NULL), mReadStatus(NO_ERROR),mMicMute(false), mDriverOp(DRV_NONE),
    mStandbyCnt(0), mDropCnt(0)
{
#ifdef DEBUG_ALSA_IN
       alsa_in_fp = fopen("/data/data/in.pcm","wb");
       if(alsa_in_fp)
               ALOGI("alsa_streamin open /sdcard/in.pcm file success");
#endif
#if (SPEEX_AGC_ENABLE||SPEEX_DENOISE_ENABLE)
        mSpeexState = NULL;
        mSpeexFrameSize = 0;
		mSpeexPcmIn = NULL;
#endif//SPEEX_AGC_ENABLE||SPEEX_DENOISE_ENABLE

}

status_t AudioHardware::AudioStreamInALSA::set(
    AudioHardware* hw, uint32_t devices, int *pFormat,
    uint32_t *pChannels, uint32_t *pRate, AudioSystem::audio_in_acoustics acoustics)
{
    if (pFormat == 0 || *pFormat != AUDIO_HW_IN_FORMAT) {
        *pFormat = AUDIO_HW_IN_FORMAT;
        return BAD_VALUE;
    }
    if (pRate == 0) {
        return BAD_VALUE;
    }

    if (*pRate == 0) *pRate = sampleRate();

    uint32_t rate = AudioHardware::getInputSampleRate(*pRate);

    if (rate != *pRate) {
        *pRate = rate;
        return BAD_VALUE;
    }

    if (devices & AudioSystem::DEVICE_IN_ANLG_DOCK_HEADSET) {
        mInSampleRate = get_USBAudio_sampleRate(UA_Record_type, *pRate);
    }

    if (pChannels == 0 || (*pChannels != AudioSystem::CHANNEL_IN_MONO &&
        *pChannels != AudioSystem::CHANNEL_IN_STEREO)) {
        *pChannels = AUDIO_HW_IN_CHANNELS;
        return BAD_VALUE;
    }

    if (devices & AudioSystem::DEVICE_IN_ANLG_DOCK_HEADSET) {
        uint32_t usbChannels = (*pChannels == AudioSystem::CHANNEL_IN_MONO) ? 1 : 2;
        *pChannels = (get_USBAudio_Channels(UA_Record_type, usbChannels) == 1) ?
            AudioSystem::CHANNEL_IN_MONO : AudioSystem::CHANNEL_IN_STEREO;
    } else
        *pChannels = AudioSystem::CHANNEL_IN_STEREO;

    mHardware = hw;

    ALOGV("AudioStreamInALSA::set(%d, %d, %u)", *pFormat, *pChannels, *pRate);

    mDevices = devices;
    mChannels = *pChannels;
    mChannelCount = AudioSystem::popCount(mChannels);
    mReqSampleRate = rate;
    if (rate >= mInSampleRate) {
        mSampleRate = mInSampleRate;
    } else {
        mSampleRate = rate;
    }
    mBufferSize = getBufferSize(mSampleRate, AudioSystem::popCount(*pChannels));

    ALOGV("mInSampleRate %d, mSampleRate %d", mInSampleRate, mSampleRate);
    if (mSampleRate < mInSampleRate) {
        mDownSampler = new AudioHardware::DownSampler(mSampleRate,
                                                  mInSampleRate,
                                                  mChannelCount,
                                                  AUDIO_HW_IN_PERIOD_SZ,
                                                  this);
        status_t status = mDownSampler->initCheck();
        if (status != NO_ERROR) {
            delete mDownSampler;
            mDownSampler = NULL;
            ALOGW("AudioStreamInALSA::set() downsampler init failed: %d", status);
            return status;
        }

        mPcmIn = new int16_t[AUDIO_HW_IN_PERIOD_SZ * mChannelCount];
    }
#if (SPEEX_AGC_ENABLE||SPEEX_DENOISE_ENABLE)
    mSpeexFrameSize = mBufferSize/((mChannelCount*sizeof(int16_t))*2);//
    mSpeexPcmIn = new int16_t[mSpeexFrameSize];
    mSpeexState = speex_preprocess_state_init(mSpeexFrameSize, mSampleRate);
	if(mSpeexState == NULL)
		return BAD_VALUE;
#if SPEEX_AGC_ENABLE
	int agc = 1;   
    float  q= 27000; //取值范围可以自己改不要超过30000；  
    //actually default is 8000(0,32768),here make it louder for voice is not loudy enough by default. 8000   
    speex_preprocess_ctl(mSpeexState, SPEEX_PREPROCESS_SET_AGC, &agc);//增益   
    speex_preprocess_ctl(mSpeexState, SPEEX_PREPROCESS_SET_AGC_LEVEL,&q);
#endif//SPEEX_AGC_ENABLE

#if SPEEX_DENOISE_ENABLE
    int denoise = 1;
#if SPEEX_AGC_ENABLE
    int noiseSuppress = -32;//DB值可以自己改， 根据具体产品修改出适合的值，-25~-45
#else
	  int noiseSuppress = -24;
#endif    
    speex_preprocess_ctl(mSpeexState, SPEEX_PREPROCESS_SET_DENOISE, &denoise);
    speex_preprocess_ctl(mSpeexState, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noiseSuppress);
#endif//SPEEX_DENOISE_ENABLE

#endif//SPEEX_AGC_ENABLE||SPEEX_DENOISE_ENABLE
    return NO_ERROR;
}

AudioHardware::AudioStreamInALSA::~AudioStreamInALSA()
{
       standby();
    if (mDownSampler != NULL) {
        delete mDownSampler;
        mDownSampler = NULL;
        if (mPcmIn != NULL) {
            delete[] mPcmIn;
            mPcmIn = NULL;
        }
    }

#if (SPEEX_AGC_ENABLE||SPEEX_DENOISE_ENABLE)
    if (mSpeexState) {
        speex_preprocess_state_destroy(mSpeexState);
        mSpeexState = NULL;
    }
    if(mSpeexPcmIn) {
        delete[] mSpeexPcmIn;
        mSpeexPcmIn = NULL;
    }
#endif //SPEEX_AGC_ENABLE||SPEEX_DENOISE_ENABLE

#ifdef DEBUG_ALSA_IN
       if(alsa_in_fp)
               fclose(alsa_in_fp);
#endif
}
status_t AudioHardware::AudioStreamInALSA::setGain(float gain)
{ 
	if(gain == 0.0)
		mMicMute= true;
	else
		mMicMute = false;
	
	return NO_ERROR; 
}

ssize_t AudioHardware::AudioStreamInALSA::read(void* buffer, ssize_t bytes)
{
    //    ALOGV("AudioStreamInALSA::read(%p, %u)", buffer, bytes);
    status_t status = NO_INIT;
    int ret;

    if (mHardware == NULL) return NO_INIT;

    { // scope for the lock
         android::AutoMutex lock(mLock);

        if (mStandby) {
             android::AutoMutex hwLock(mHardware->lock());

            ALOGD("AudioHardware pcm capture is exiting standby.");
            acquire_wake_lock (PARTIAL_WAKE_LOCK, "AudioInLock");

           /* android::sp<AudioStreamOutALSA> spOut = mHardware->output();
            while (spOut != 0) {
                if (!spOut->checkStandby()) {
                    int cnt = spOut->standbyCnt();
                    mHardware->lock().unlock();
                    mLock.unlock();
                    // Mutex acquisition order is always out -> in -> hw
                    spOut->lock();
                    mLock.lock();
                    mHardware->lock().lock();
                    // make sure that another thread did not change output state
                    // while the mutex is released
                    if ((spOut == mHardware->output()) && (cnt == spOut->standbyCnt())) {
                        ALOGV("AudioStreamInALSA::read() force output standby");
                        spOut->close_l();
                        break;
                    }
                    spOut->unlock();
                    spOut = mHardware->output();
                } else {
                    spOut.clear();
                }
            }
            // spOut is not 0 here only if the output was active and has been
            // closed above

            // open output before input
            if (spOut != 0) {
                if (spOut->open_l() != NO_ERROR) {
                    spOut->doStandby_l();
                }
                spOut->unlock();
            }*/

            open_l();

            if (mPcm == NULL) {
                release_wake_lock("AudioInLock");
                goto Error;
            }
            mStandby = false;
        }


        if (mDownSampler != NULL) {
            size_t frames = bytes / frameSize();
            size_t framesIn = 0;
            mReadStatus = 0;
            do {
                size_t outframes = frames - framesIn;
                mDownSampler->resample(
                        (int16_t *)buffer + (framesIn * mChannelCount),
                        &outframes);
                framesIn += outframes;
            } while ((framesIn < frames) && mReadStatus == 0);
            ret = mReadStatus;
            bytes = framesIn * frameSize();
        } else {
            TRACE_DRIVER_IN(DRV_PCM_READ)
            ret = pcm_read(mPcm, buffer, bytes);
            TRACE_DRIVER_OUT
        }

        if (ret == 0) {
			//drop 0.5S input data 
			if(mDropCnt < mSampleRate/2)
			{
				memset(buffer,0,bytes);
				mDropCnt += bytes/frameSize();
			}
			else if (mMicMute)
			{
				memset(buffer,0,bytes);
			}
			
#ifdef DEBUG_ALSA_IN
			if(alsa_in_fp)
				fwrite(buffer,1,bytes,alsa_in_fp);
#endif      

#if (SPEEX_AGC_ENABLE||SPEEX_DENOISE_ENABLE)			
			if(!mMicMute)
			{
                int index = 0;
				int startPos = 0;
                spx_int16_t* data = (spx_int16_t*) buffer;

				int curFrameSize = bytes/(mChannelCount*sizeof(int16_t));
				
				if(curFrameSize != 2*mSpeexFrameSize)
					ALOGV("the current request have some error mSpeexFrameSize %d bytes %d ",mSpeexFrameSize,bytes);
				
				while(curFrameSize >= startPos+mSpeexFrameSize)
				{
										
					for(index = startPos; index< startPos + mSpeexFrameSize ;index++ )
						mSpeexPcmIn[index-startPos] = data[index*mChannelCount]/2 + data[index*mChannelCount+1]/2;
					
	         		speex_preprocess_run(mSpeexState,mSpeexPcmIn);
#ifndef TARGET_RK2928				
					for(unsigned long ch = 0 ; ch < mChannelCount;ch++)
						for(index = startPos; index< startPos + mSpeexFrameSize ;index++ )
						{
							data[index*mChannelCount+ch] = mSpeexPcmIn[index-startPos];
						}
#else
					for(index = startPos; index< startPos + mSpeexFrameSize ;index++ )
					{
						int tmp = (int)mSpeexPcmIn[index-startPos]+ mSpeexPcmIn[index-startPos]/2;
						data[index*mChannelCount+0] = tmp > 32767 ? 32767 : (tmp < -32768 ? -32768 : tmp);
					}
					for(int ch = 1 ; ch < mChannelCount;ch++)						
						for(index = startPos; index< startPos + mSpeexFrameSize ;index++ )
						{
							data[index*mChannelCount+ch] = data[index*mChannelCount+0];
						}
#endif
					startPos += mSpeexFrameSize;
				}
            }
#endif//(SPEEX_AGC_ENABLE||SPEEX_DENOISE_ENABLE)		
            return bytes;
        }

        ALOGW("read error: %d", ret);
        status = ret;
    }

Error:

    standby();

    // Simulate audio output timing in case of error
    usleep((((bytes * 1000) / frameSize()) * 1000) / sampleRate());

    return status;
}

status_t AudioHardware::AudioStreamInALSA::standby()
{
    if (mHardware == NULL) return NO_INIT;

    android::AutoMutex lock(mLock);

    { // scope for AudioHardware lock
        android::AutoMutex hwLock(mHardware->lock());
        doStandby_l();
    }
    return NO_ERROR;
}

void AudioHardware::AudioStreamInALSA::doStandby_l()
{
    mStandbyCnt++;

    if (!mStandby) {
        ALOGD("AudioHardware pcm capture is going to standby.");
        release_wake_lock("AudioInLock");
        mStandby = true;
    }
    close_l();
}

void AudioHardware::AudioStreamInALSA::close_l()
{
    if (mPcm) {
        ALOGV("close_l() reset Capture MIC Path to OFF");

        TRACE_DRIVER_IN(DRV_PCM_CLOSE)
        route_pcm_close(CAPTURE_OFF_ROUTE);
        TRACE_DRIVER_OUT
        mPcm = NULL;
    }
}

status_t AudioHardware::AudioStreamInALSA::open_l()
{
    unsigned flags = PCM_IN;

    flags |= (AUDIO_HW_IN_PERIOD_MULT * mInSampleRate / AUDIO_HW_IN_SAMPLERATE - 1) << PCM_PERIOD_SZ_SHIFT;
    flags |= (AUDIO_HW_IN_PERIOD_CNT - PCM_PERIOD_CNT_MIN)
            << PCM_PERIOD_CNT_SHIFT;

    if (mChannels == AudioSystem::CHANNEL_IN_MONO)
        flags |= PCM_MONO;

    if (mInSampleRate == 8000)
        flags |= PCM_8000HZ;
    else if (mInSampleRate == 48000)
        flags |= PCM_48000HZ;

    ALOGV("open pcm_in driver");
    TRACE_DRIVER_IN(DRV_PCM_OPEN)
    mPcm = route_pcm_open(mHardware->getRouteFromDevice(mDevices), flags);
    TRACE_DRIVER_OUT
    if (!pcm_ready(mPcm)) {
        ALOGE("cannot open pcm_in driver: %s\n", pcm_error(mPcm));
        TRACE_DRIVER_IN(DRV_PCM_CLOSE)
        route_pcm_close(CAPTURE_OFF_ROUTE);
        TRACE_DRIVER_OUT
        mPcm = NULL;
        return NO_INIT;
    }

    if (mDownSampler != NULL) {
        mInPcmInBuf = 0;
        mDownSampler->reset();
    }

    if (mHardware->mode() != AudioSystem::MODE_IN_CALL) {
        ALOGV("read() wakeup setting Capture route");
        TRACE_DRIVER_IN(DRV_MIXER_SEL)
        route_set_controls(mHardware->getRouteFromDevice(mDevices));
        TRACE_DRIVER_OUT
    }

    return NO_ERROR;
}

status_t AudioHardware::AudioStreamInALSA::dump(int fd, const Vector<String16>& args)
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    bool locked = tryLock(mLock);
    if (!locked) {
        snprintf(buffer, SIZE, "\n\t\tAudioStreamInALSA maybe deadlocked\n");
    } else {
        mLock.unlock();
    }

    snprintf(buffer, SIZE, "\t\tmHardware: %p\n", mHardware);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmPcm: %p\n", mPcm);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tStandby %s\n", (mStandby) ? "ON" : "OFF");
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmDevices: 0x%08x\n", mDevices);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmChannels: 0x%08x\n", mChannels);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmSampleRate: %d\n", mSampleRate);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmBufferSize: %d\n", mBufferSize);
    result.append(buffer);
    snprintf(buffer, SIZE, "\t\tmDriverOp: %d\n", mDriverOp);
    result.append(buffer);
    write(fd, result.string(), result.size());

    return NO_ERROR;
}

bool AudioHardware::AudioStreamInALSA::checkStandby()
{
    return mStandby;
}

status_t AudioHardware::AudioStreamInALSA::setParameters(const String8& keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);
    status_t status = NO_ERROR;
    int value;
    String8 source;
    bool reconfig = false;

    ALOGD("AudioStreamInALSA::setParameters() %s", keyValuePairs.string());

    if (mHardware == NULL) return NO_INIT;

    {
        bool needStandby = false;
        android::AutoMutex lock(mLock);

        if (param.get(String8(INPUT_SOURCE_KEY), source) == NO_ERROR) {
            android::AutoMutex hwLock(mHardware->lock());

            mHardware->setInputSource_l(source);

            param.remove(String8(INPUT_SOURCE_KEY));
        }

        if (param.getInt(String8(AudioParameter::keySamplingRate), value) == NO_ERROR)
        {
            if (mInSampleRate != (uint32_t)value && value != 0 &&
                (value == 8000 || value == 44100 || value == 48000)) {
                mInSampleRate = (uint32_t)value;
                reconfig = true;
                if (mHardware->mode() != AudioSystem::MODE_IN_CALL) {
                    needStandby = true;
                }
            }
            param.remove(String8(AudioParameter::keySamplingRate));
        }

        if (param.getInt(String8(AudioParameter::keyChannels), value) == NO_ERROR)
        {
            if (mChannels != (uint32_t)value && (uint32_t)value != 0 &&
                (value == AudioSystem::CHANNEL_IN_STEREO || value == AudioSystem::CHANNEL_IN_MONO)) {
                mChannels = (uint32_t)value;
                reconfig = true;
                if (mHardware->mode() != AudioSystem::MODE_IN_CALL) {
                    needStandby = true;
                }
            }
            param.remove(String8(AudioParameter::keyChannels));
        }

        if (param.getInt(String8(AudioParameter::keyRouting), value) == NO_ERROR)
        {
            if (mDevices != (uint32_t)value && (uint32_t)value != AUDIO_DEVICE_NONE) {
                mDevices = (uint32_t)value;
                if (mHardware->mode() != AudioSystem::MODE_IN_CALL) {
                    needStandby = true;
                }
            }
            param.remove(String8(AudioParameter::keyRouting));
        }

        if (needStandby){
            android::AutoMutex hwLock(mHardware->lock());
            doStandby_l();
        }

        //close downsampler and open again to update params
        if (reconfig) {
            android::AutoMutex hwLock(mHardware->lock());
            if (mDownSampler != NULL) {
                delete mDownSampler;
                mDownSampler = NULL;
                if (mPcmIn != NULL) {
                    delete[] mPcmIn;
                    mPcmIn = NULL;
                }
            }
#if (SPEEX_AGC_ENABLE||SPEEX_DENOISE_ENABLE)
            if (mSpeexState) {
                speex_preprocess_state_destroy(mSpeexState);
                mSpeexState = NULL;
            }
            if(mSpeexPcmIn) {
                delete[] mSpeexPcmIn;
                mSpeexPcmIn = NULL;
            }
#endif //SPEEX_AGC_ENABLE||SPEEX_DENOISE_ENABLE

            int pFormat = AUDIO_HW_IN_FORMAT;
            uint32_t pChannels = mChannels;
            uint32_t pRate = mReqSampleRate;

            if (set(mHardware, mDevices, &pFormat, &pChannels, &pRate, (AudioSystem::audio_in_acoustics)0) != NO_ERROR) {
                ALOGE("AudioStreamInALSA; call set error!");
                return BAD_VALUE;
            }
        }
    }

    if (param.size()) {
        status = BAD_VALUE;
    }

    return status;

}

String8 AudioHardware::AudioStreamInALSA::getParameters(const String8& keys)
{
    AudioParameter param = AudioParameter(keys);
    String8 value;
    String8 key = String8(AudioParameter::keyRouting);

    if (param.get(key, value) == NO_ERROR) {
        param.addInt(key, (int)mDevices);
    }

    if (param.get(String8(AudioParameter::keySamplingRate), value) == NO_ERROR) {
        param.addInt(String8(AudioParameter::keySamplingRate), (int)mInSampleRate);
    }

    if (param.get(String8(AudioParameter::keyChannels), value) == NO_ERROR) {
        param.addInt(String8(AudioParameter::keyChannels), (int)mChannels);
    }

    ALOGV("AudioStreamInALSA::getParameters() %s", param.toString().string());
    return param.toString();
}

status_t AudioHardware::AudioStreamInALSA::getNextBuffer(AudioHardware::BufferProvider::Buffer* buffer)
{
    if (mPcm == NULL) {
        buffer->raw = NULL;
        buffer->frameCount = 0;
        mReadStatus = NO_INIT;
        return NO_INIT;
    }

    if (mInPcmInBuf == 0) {
        TRACE_DRIVER_IN(DRV_PCM_READ)
        mReadStatus = pcm_read(mPcm,(void*) mPcmIn, AUDIO_HW_IN_PERIOD_SZ * frameSize() * mInSampleRate / AUDIO_HW_IN_SAMPLERATE);
        TRACE_DRIVER_OUT
        if (mReadStatus != 0) {
            buffer->raw = NULL;
            buffer->frameCount = 0;
            return mReadStatus;
        }
        mInPcmInBuf = AUDIO_HW_IN_PERIOD_SZ * mInSampleRate / AUDIO_HW_IN_SAMPLERATE;
    }

    buffer->frameCount = (buffer->frameCount > mInPcmInBuf) ? mInPcmInBuf : buffer->frameCount;
    buffer->i16 = mPcmIn + (AUDIO_HW_IN_PERIOD_SZ  * mInSampleRate / AUDIO_HW_IN_SAMPLERATE - mInPcmInBuf) * mChannelCount;

    return mReadStatus;
}

void AudioHardware::AudioStreamInALSA::releaseBuffer(Buffer* buffer)
{
    mInPcmInBuf -= buffer->frameCount;
}

size_t AudioHardware::AudioStreamInALSA::getBufferSize(uint32_t sampleRate, int channelCount)
{
    size_t ratio;

    switch (sampleRate) {
    case 8000:
    case 11025:
    case 12000:
        ratio = 4;
        break;
    case 16000:
    case 22050:
    case 24000:
        ratio = 2;
        break;
    case 32000:
    case 44100:
    case 48000:
    default:
        ratio = 1;
        break;
    }

    return (AUDIO_HW_IN_PERIOD_SZ * channelCount * sizeof(int16_t)) / ratio ;
}

//------------------------------------------------------------------------------
//  DownSampler
//------------------------------------------------------------------------------

/*
 * 2.30 fixed point FIR filter coefficients for conversion 44100 -> 22050.
 * (Works equivalently for 22010 -> 11025 or any other halving, of course.)
 *
 * Transition band from about 18 kHz, passband ripple < 0.1 dB,
 * stopband ripple at about -55 dB, linear phase.
 *
 * Design and display in MATLAB or Octave using:
 *
 * filter = fir1(19, 0.5); filter = round(filter * 2**30); freqz(filter * 2**-30);
 */
static const int32_t filter_22khz_coeff[] = {
    2089257, 2898328, -5820678, -10484531,
    19038724, 30542725, -50469415, -81505260,
    152544464, 478517512, 478517512, 152544464,
    -81505260, -50469415, 30542725, 19038724,
    -10484531, -5820678, 2898328, 2089257,
};
#define NUM_COEFF_22KHZ (sizeof(filter_22khz_coeff) / sizeof(filter_22khz_coeff[0]))
#define OVERLAP_22KHZ (NUM_COEFF_22KHZ - 2)

/*
 * Convolution of signals A and reverse(B). (In our case, the filter response
 * is symmetric, so the reversing doesn't matter.)
 * A is taken to be in 0.16 fixed-point, and B is taken to be in 2.30 fixed-point.
 * The answer will be in 16.16 fixed-point, unclipped.
 *
 * This function would probably be the prime candidate for SIMD conversion if
 * you want more speed.
 */
int32_t fir_convolve(const int16_t* a, const int32_t* b, int num_samples)
{
        int32_t sum = 1 << 13;
        for (int i = 0; i < num_samples; ++i) {
                sum += a[i] * (b[i] >> 16);
        }
        return sum >> 14;
}

/* Clip from 16.16 fixed-point to 0.16 fixed-point. */
int16_t clip(int32_t x)
{
    if (x < -32768) {
        return -32768;
    } else if (x > 32767) {
        return 32767;
    } else {
        return x;
    }
}

/*
 * Convert a chunk from 44 kHz to 22 kHz. Will update num_samples_in and num_samples_out
 * accordingly, since it may leave input samples in the buffer due to overlap.
 *
 * Input and output are taken to be in 0.16 fixed-point.
 */
void resample_2_1(int16_t* input, int16_t* output, int* num_samples_in, int* num_samples_out)
{
    if (*num_samples_in < (int)NUM_COEFF_22KHZ) {
        *num_samples_out = 0;
        return;
    }

    int odd_smp = *num_samples_in & 0x1;
    int num_samples = *num_samples_in - odd_smp - OVERLAP_22KHZ;

    for (int i = 0; i < num_samples; i += 2) {
            output[i / 2] = clip(fir_convolve(input + i, filter_22khz_coeff, NUM_COEFF_22KHZ));
    }

    memmove(input, input + num_samples, (OVERLAP_22KHZ + odd_smp) * sizeof(*input));
    *num_samples_out = num_samples / 2;
    *num_samples_in = OVERLAP_22KHZ + odd_smp;
}

/*
 * 2.30 fixed point FIR filter coefficients for conversion 22050 -> 16000,
 * or 11025 -> 8000.
 *
 * Transition band from about 14 kHz, passband ripple < 0.1 dB,
 * stopband ripple at about -50 dB, linear phase.
 *
 * Design and display in MATLAB or Octave using:
 *
 * filter = fir1(23, 16000 / 22050); filter = round(filter * 2**30); freqz(filter * 2**-30);
 */
static const int32_t filter_16khz_coeff[] = {
    2057290, -2973608, 1880478, 4362037,
    -14639744, 18523609, -1609189, -38502470,
    78073125, -68353935, -59103896, 617555440,
    617555440, -59103896, -68353935, 78073125,
    -38502470, -1609189, 18523609, -14639744,
    4362037, 1880478, -2973608, 2057290,
};
#define NUM_COEFF_16KHZ (sizeof(filter_16khz_coeff) / sizeof(filter_16khz_coeff[0]))
#define OVERLAP_16KHZ (NUM_COEFF_16KHZ - 1)

/*
 * Convert a chunk from 22 kHz to 16 kHz. Will update num_samples_in and
 * num_samples_out accordingly, since it may leave input samples in the buffer
 * due to overlap.
 *
 * This implementation is rather ad-hoc; it first low-pass filters the data
 * into a temporary buffer, and then converts chunks of 441 input samples at a
 * time into 320 output samples by simple linear interpolation. A better
 * implementation would use a polyphase filter bank to do these two operations
 * in one step.
 *
 * Input and output are taken to be in 0.16 fixed-point.
 */

#define RESAMPLE_16KHZ_SAMPLES_IN 441
#define RESAMPLE_16KHZ_SAMPLES_OUT 320

void resample_441_320(int16_t* input, int16_t* output, int* num_samples_in, int* num_samples_out)
{
    const int num_blocks = (*num_samples_in - OVERLAP_16KHZ) / RESAMPLE_16KHZ_SAMPLES_IN;
    if (num_blocks < 1) {
        *num_samples_out = 0;
        return;
    }

    for (int i = 0; i < num_blocks; ++i) {
        uint32_t tmp[RESAMPLE_16KHZ_SAMPLES_IN];
        for (int j = 0; j < RESAMPLE_16KHZ_SAMPLES_IN; ++j) {
            tmp[j] = fir_convolve(input + i * RESAMPLE_16KHZ_SAMPLES_IN + j,
                          filter_16khz_coeff,
                          NUM_COEFF_16KHZ);
        }

        const float step_float = (float)RESAMPLE_16KHZ_SAMPLES_IN / (float)RESAMPLE_16KHZ_SAMPLES_OUT;

        uint32_t in_sample_num = 0;   // 16.16 fixed point
        const uint32_t step = (uint32_t)(step_float * 65536.0f + 0.5f);  // 16.16 fixed point
        for (int j = 0; j < RESAMPLE_16KHZ_SAMPLES_OUT; ++j, in_sample_num += step) {
            const uint32_t whole = in_sample_num >> 16;
            const uint32_t frac = (in_sample_num & 0xffff);  // 0.16 fixed point
            const int32_t s1 = tmp[whole];
            const int32_t s2 = tmp[whole + 1];
            *output++ = clip(s1 + (((s2 - s1) * (int32_t)frac) >> 16));
        }
    }

    const int samples_consumed = num_blocks * RESAMPLE_16KHZ_SAMPLES_IN;
    memmove(input, input + samples_consumed, (*num_samples_in - samples_consumed) * sizeof(*input));
    *num_samples_in -= samples_consumed;
    *num_samples_out = RESAMPLE_16KHZ_SAMPLES_OUT * num_blocks;
}


AudioHardware::DownSampler::DownSampler(uint32_t outSampleRate,
                                    uint32_t inSampleRate,
                                    uint32_t channelCount,
                                    uint32_t frameCount,
                                    AudioHardware::BufferProvider* provider)
    :  mStatus(NO_INIT), mProvider(provider), mSampleRate(outSampleRate),
       mChannelCount(channelCount), mFrameCount(frameCount),
       mTmpOutBuf(NULL),mInResampler(NULL)

{
    ALOGV("AudioHardware::DownSampler() cstor %p SR %d channels %d frames %d",
         this, mSampleRate, mChannelCount, mFrameCount);

    if (mSampleRate != 8000 && mSampleRate != 11025 && mSampleRate != 12000&&mSampleRate != 16000 &&
            mSampleRate != 22050 && mSampleRate != 24000 &&mSampleRate != 32000 && mSampleRate != 44100&& mSampleRate != 48000) {
        ALOGW("AudioHardware::DownSampler cstor: bad sampling rate: %d", mSampleRate);
        return;
    }

    mTmpOutBuf= new int16_t[mFrameCount*mChannelCount];
	int error;
	ALOGI("--->speex_resampler_init ch=%d in =%d,out =%d",mChannelCount,inSampleRate,mSampleRate);
	mInResampler =  speex_resampler_init(mChannelCount,
                                               inSampleRate,
                                               mSampleRate,
                                               RESAMPLER_QUALITY,
                                               &error);
    if (mInResampler == NULL) {
        ALOGW("Session_SetConfig Cannot create speex resampler: %s",
             speex_resampler_strerror(error));
        return ;
    }

    mStatus = NO_ERROR;
}

AudioHardware::DownSampler::~DownSampler()
{
    if (mTmpOutBuf) delete[] mTmpOutBuf;
    if (mInResampler) speex_resampler_destroy(mInResampler);
}

void AudioHardware::DownSampler::reset()
{
    mOutBufPos = 0;
    mInOutBuf = 0;
}


int AudioHardware::DownSampler::resample(int16_t* out, size_t *outFrameCount)
{
    if (mStatus != NO_ERROR) {
        return mStatus;
    }

    if (out == NULL || outFrameCount == NULL) {
        return BAD_VALUE;
    }


    int outFrames = 0;
    int remaingFrames = *outFrameCount;

    if (mInOutBuf) {
        //ALOGV("mInOutBuf = %d remaingFrames  =%d",mInOutBuf,remaingFrames);
        int frames = (remaingFrames > mInOutBuf) ? mInOutBuf : remaingFrames;

        memcpy((void*)out, (void*)(mTmpOutBuf+mOutBufPos*mChannelCount),frames*mChannelCount*sizeof(int16_t));

        remaingFrames -= frames;
        mInOutBuf -= frames;
        mOutBufPos += frames;
        outFrames += frames;
    }

    while (remaingFrames) {
        ALOGW_IF((mInOutBuf != 0), "mInOutBuf should be 0 here");

        AudioHardware::BufferProvider::Buffer buf;
        buf.frameCount =  mFrameCount;
        int ret = mProvider->getNextBuffer(&buf);
        if (buf.raw == NULL) {
            *outFrameCount = outFrames;
            return ret;
        }

        uint inFrameCount = buf.frameCount;
        uint outFrameCount = mFrameCount*mChannelCount;
        //ALOGV("before resample inFrameCount = %d",inFrameCount);
        if (mChannelCount == 1) {
            speex_resampler_process_int(mInResampler,
                                        0,
                                        (const spx_int16_t *)buf.raw,
                                        &inFrameCount,
                                        mTmpOutBuf,
                                        &outFrameCount);
        } else {
            speex_resampler_process_interleaved_int(mInResampler,
                                                    (const spx_int16_t *)buf.raw,
                                                    &inFrameCount,
                                                    mTmpOutBuf,
                                                    &outFrameCount);
        }
        //ALOGV("after resample inFrameCount use = %d outFrameCount = %d",inFrameCount,outFrameCount);
        buf.frameCount = inFrameCount;
        mProvider->releaseBuffer(&buf);

        mInOutBuf = outFrameCount;

        int frames = (remaingFrames > mInOutBuf) ? mInOutBuf : remaingFrames;

        memcpy((void*)(out+outFrames*mChannelCount), (void*)mTmpOutBuf,frames*mChannelCount*sizeof(int16_t));

        remaingFrames -= frames;
        outFrames += frames;
        mOutBufPos = frames;
        mInOutBuf -= frames;
    }

    return 0;
}

//------------------------------------------------------------------------------
//  Factory
//------------------------------------------------------------------------------

extern "C" AudioHardwareInterface* createAudioHardware(void) {
    return new AudioHardware();
}

}; // namespace android
