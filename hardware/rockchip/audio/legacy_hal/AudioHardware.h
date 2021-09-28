/*
** Copyright 2008, The Android Open-Source Project
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

/*
**
**
**
**             Audio Hardware Commit log
**
**V1.0.0
**	1)Merge from 4.4 and fix some compile error
**
*/

//AudioHardware Version
#define AUDIO_HAL_VERSION_NAME "sys.audio.version"
#define AUDIO_HAL_VERSION      "1.0.0"

#ifndef ANDROID_AUDIO_HARDWARE_H
#define ANDROID_AUDIO_HARDWARE_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/threads.h>
#include <utils/SortedVector.h>

#include <hardware_legacy/AudioHardwareBase.h>

#include "secril-client.h"

#include <speex/speex.h>
#include <speex/speex_preprocess.h>
#include <speex/speex_resampler.h>
extern "C" {
    struct pcm;
    struct mixer;
    struct mixer_ctl;
};

namespace android_audio_legacy {

// TODO: determine actual audio DSP and hardware latency
// Additionnal latency introduced by audio DSP and hardware in ms
#define AUDIO_HW_OUT_LATENCY_MS 0
// Default audio output sample rate
#define AUDIO_HW_OUT_SAMPLERATE 44100
// Default audio output channel mask
#define AUDIO_HW_OUT_CHANNELS (AudioSystem::CHANNEL_OUT_STEREO)
// Default audio output sample format
#define AUDIO_HW_OUT_FORMAT (AudioSystem::PCM_16_BIT)
// Kernel pcm out buffer size in frames at 44.1kHz
#define AUDIO_HW_OUT_PERIOD_MULT 16 // (16 * 64 = 1024 frames)
#define AUDIO_HW_OUT_PERIOD_SZ (PCM_PERIOD_SZ_MIN * AUDIO_HW_OUT_PERIOD_MULT)
#define AUDIO_HW_OUT_PERIOD_CNT 4
// Default audio output buffer size in bytes
#define AUDIO_HW_OUT_PERIOD_BYTES (AUDIO_HW_OUT_PERIOD_SZ * 2 * sizeof(int16_t))

// Default audio input sample rate
#define AUDIO_HW_IN_SAMPLERATE 44100
// Default audio input channel mask
#define AUDIO_HW_IN_CHANNELS (AudioSystem::CHANNEL_IN_STEREO)
// Default audio input sample format
#define AUDIO_HW_IN_FORMAT (AudioSystem::PCM_16_BIT)
// Number of buffers in audio driver for input
#define AUDIO_HW_NUM_IN_BUF 4
// Kernel pcm in buffer size in frames at 44.1kHz (before resampling)
#define AUDIO_HW_IN_PERIOD_MULT 16  // (8* 64 = 512 frames)
#define AUDIO_HW_IN_PERIOD_SZ (PCM_PERIOD_SZ_MIN * AUDIO_HW_IN_PERIOD_MULT)
#define AUDIO_HW_IN_PERIOD_CNT 6
// Default audio input buffer size in bytes (8kHz mono)
#define AUDIO_HW_IN_PERIOD_BYTES ((AUDIO_HW_IN_PERIOD_SZ*sizeof(int16_t))/8)

#define INPUT_SOURCE_KEY "Input Source"


//1:Enable the AGC funtion ;0: disable the AGC function
#define SPEEX_AGC_ENABLE 0

//1:Enable the denoise funtion ;0: disable the denoise function

#define SPEEX_DENOISE_ENABLE 1

#define RESAMPLER_QUALITY SPEEX_RESAMPLER_QUALITY_DEFAULT


class AudioHardware : public AudioHardwareBase
{
    class AudioStreamOutALSA;
    class AudioStreamInALSA;
public:

    AudioHardware();
    virtual ~AudioHardware();
    virtual status_t initCheck();

    virtual status_t setVoiceVolume(float volume);
    virtual status_t setMasterVolume(float volume);

    virtual status_t setMode(int mode);

    virtual status_t setMicMute(bool state);
    virtual status_t getMicMute(bool* state);

    virtual status_t setParameters(const String8& keyValuePairs);
    virtual String8 getParameters(const String8& keys);

    virtual android_audio_legacy::AudioStreamOut* openOutputStream(
        uint32_t devices, int *format=0, uint32_t *channels=0,
        uint32_t *sampleRate=0, status_t *status=0);

    virtual android_audio_legacy::AudioStreamIn* openInputStream(
        uint32_t devices, int *format, uint32_t *channels,
        uint32_t *sampleRate, status_t *status,
        AudioSystem::audio_in_acoustics acoustics);

    virtual status_t setMasterMute(bool muted) ;


    virtual int createAudioPatch(unsigned int num_sources,
                               const struct audio_port_config *sources,
                               unsigned int num_sinks,
                               const struct audio_port_config *sinks,
                               audio_patch_handle_t *handle) ;

    virtual int releaseAudioPatch(audio_patch_handle_t handle) ;

    virtual int getAudioPort(struct audio_port *port) ;

    virtual int setAudioPortConfig(const struct audio_port_config *config) ;


    virtual android_audio_legacy::AudioStreamOut* openOutputStreamWithFlags(uint32_t devices,
                                          audio_output_flags_t flags,
                                          int *format,
                                          uint32_t *channels,
                                          uint32_t *sampleRate,
                                          status_t *status);
    virtual void closeOutputStream(android_audio_legacy::AudioStreamOut* out);
    virtual void closeInputStream(android_audio_legacy::AudioStreamIn* in);

    virtual size_t getInputBufferSize(
        uint32_t sampleRate, int format, int channelCount);

            int  mode() { return mMode; }
            unsigned getOutputRouteFromDevice(uint32_t device);
            unsigned getInputRouteFromDevice(uint32_t device);
            unsigned getVoiceRouteFromDevice(uint32_t device);
            unsigned getRouteFromDevice(uint32_t device);

            status_t setIncallPath_l(uint32_t device);

            status_t setInputSource_l(String8 source);

    static uint32_t    getInputSampleRate(uint32_t sampleRate);
           android::sp <AudioStreamInALSA> getActiveInput_l();

            android::Mutex& lock() { return mLock; }

           struct pcm *openPcmOut_l();
           void closePcmOut_l();

           struct pcm *getPcm() { return mPcm; };

            android::sp <AudioStreamOutALSA>  output() { return mOutput; }

protected:
    virtual status_t dump(int fd, const Vector<String16>& args);

private:

    bool            mInit;
    bool            mMicMute;
     android::sp <AudioStreamOutALSA>                 mOutput;
     android::SortedVector <  android::sp<AudioStreamInALSA> >   mInputs;
     android::Mutex           mLock;
    struct pcm*     mPcm;
    uint32_t        mPcmOpenCnt;
    uint32_t        mMixerOpenCnt;
    bool            mInCallAudioMode;
    bool            mVoipAudioMode;

    String8         mInputSource;
    bool            mBluetoothNrec;
    void*           mSecRilLibHandle;
    HRilClient      mRilClient;
    bool            mActivatedCP;
    HRilClient      (*openClientRILD)  (void);
    int             (*disconnectRILD)  (HRilClient);
    int             (*closeClientRILD) (HRilClient);
    int             (*isConnectedRILD) (HRilClient);
    int             (*connectRILD)     (HRilClient);
    int             (*setCallVolume)   (HRilClient, SoundType, int);
    int             (*setCallAudioPath)(HRilClient, AudioPath);
    int             (*setCallClockSync)(HRilClient, SoundClockCondition);
    void            loadRILD(void);
    status_t        connectRILDIfRequired(void);

    //  trace driver operations for dump
    int             mDriverOp;

    static uint32_t         checkInputSampleRate(uint32_t sampleRate);
    static const uint32_t   inputSamplingRates[];

    class AudioStreamOutALSA : public AudioStreamOut, public  android::RefBase
    {
    public:
        AudioStreamOutALSA();
        virtual ~AudioStreamOutALSA();
        status_t set(AudioHardware* mHardware,
                     uint32_t devices,
                     int *pFormat,
                     uint32_t *pChannels,
                     uint32_t *pRate);
        virtual uint32_t sampleRate()
            const { return mSampleRate; }
        virtual size_t bufferSize()
            const { return mBufferSize; }
        virtual uint32_t channels()
            const { return mChannels; }
        virtual int format()
            const { return AUDIO_HW_OUT_FORMAT; }
        virtual uint32_t latency()
            const { return (1000 * AUDIO_HW_OUT_PERIOD_CNT *
                            (bufferSize()/frameSize()))/sampleRate() +
                AUDIO_HW_OUT_LATENCY_MS; }
        virtual status_t setVolume(float left, float right)
        { return INVALID_OPERATION; }
        virtual ssize_t write(const void* buffer, size_t bytes);
        virtual status_t standby();
                bool checkStandby();

        virtual status_t dump(int fd, const Vector<String16>& args);
        virtual status_t setParameters(const String8& keyValuePairs);
        virtual String8 getParameters(const String8& keys);
        uint32_t device() { return mDevices; }
        virtual status_t getRenderPosition(uint32_t *dspFrames);

                void doStandby_l();
                void close_l();
                status_t open_l();
                int standbyCnt() { return mStandbyCnt; }

                void lock() { mLock.lock(); }
                void unlock() { mLock.unlock(); }

    private:

        android::Mutex mLock;
        AudioHardware* mHardware;
        struct mixer_ctl *mRouteCtl;
        const char *next_route;
        bool mStandby;
        uint32_t mDevices;
        uint32_t mChannels;
        uint32_t mSampleRate;
        size_t mBufferSize;
        //  trace driver operations for dump
        int mDriverOp;
        int mStandbyCnt;
    };

    class DownSampler;

    class BufferProvider
    {
    public:

        struct Buffer {
            union {
                void*       raw;
                short*      i16;
                int8_t*     i8;
            };
            size_t frameCount;
        };

        virtual ~BufferProvider() {}

        virtual status_t getNextBuffer(Buffer* buffer) = 0;
        virtual void releaseBuffer(Buffer* buffer) = 0;
    };

    class DownSampler {
    public:
        DownSampler(uint32_t outSampleRate,
                  uint32_t inSampleRate,
                  uint32_t channelCount,
                  uint32_t frameCount,
                  BufferProvider* provider);

        virtual ~DownSampler();

                void reset();
                status_t initCheck() { return mStatus; }
                int resample(int16_t* out, size_t *outFrameCount);

    private:
        status_t    mStatus;
        BufferProvider* mProvider;
        uint32_t mSampleRate;
        uint32_t mChannelCount;
        uint32_t mFrameCount;
        int16_t *mTmpOutBuf;
        int mOutBufPos;
        int mInOutBuf;
        int mInInBuf;
        SpeexResamplerState *mInResampler;   // handle on input speex resampler
    };


    class AudioStreamInALSA : public AudioStreamIn, public BufferProvider, public  android::RefBase
    {

     public:
                    AudioStreamInALSA();
        virtual     ~AudioStreamInALSA();
        status_t    set(AudioHardware* hw,
                    uint32_t devices,
                    int *pFormat,
                    uint32_t *pChannels,
                    uint32_t *pRate,
                    AudioSystem::audio_in_acoustics acoustics);
        virtual size_t bufferSize() const { return mBufferSize; }
        virtual uint32_t channels() const { return mChannels; }
        virtual int format() const { return AUDIO_HW_IN_FORMAT; }
        virtual uint32_t sampleRate() const { return mSampleRate; }
        virtual status_t setGain(float gain);// { return INVALID_OPERATION; }
        virtual ssize_t read(void* buffer, ssize_t bytes);
        virtual status_t dump(int fd, const Vector<String16>& args);
        virtual status_t standby();
                bool checkStandby();
        virtual status_t setParameters(const String8& keyValuePairs);
        virtual String8 getParameters(const String8& keys);
        virtual unsigned int getInputFramesLost() const { return 0; }
                uint32_t device() { return mDevices; }
                void doStandby_l();
                void close_l();
                status_t open_l();
                int standbyCnt() { return mStandbyCnt; }

        static size_t getBufferSize(uint32_t sampleRate, int channelCount);

        // BufferProvider
        virtual status_t getNextBuffer(BufferProvider::Buffer* buffer);
        virtual void releaseBuffer(BufferProvider::Buffer* buffer);

        void lock() { mLock.lock(); }
        void unlock() { mLock.unlock(); }
		
    virtual status_t addAudioEffect(effect_handle_t effect){return 0;};
    virtual status_t removeAudioEffect(effect_handle_t effect){return 0;};

    private:
         android::Mutex mLock;
        AudioHardware* mHardware;
        struct pcm *mPcm;
        struct mixer_ctl *mRouteCtl;
        const char *next_route;
        bool mStandby;
        uint32_t mDevices;
        uint32_t mChannels;
        uint32_t mChannelCount;
        uint32_t mSampleRate;
        uint32_t mReqSampleRate;
        uint32_t mInSampleRate;
        size_t mBufferSize;
        DownSampler *mDownSampler;
        status_t mReadStatus;
        size_t mInPcmInBuf;
        int16_t *mPcmIn;
		bool mMicMute;
        //  trace driver operations for dump
        int mDriverOp;
        int mStandbyCnt;
		uint32_t mDropCnt;
#if (SPEEX_AGC_ENABLE||SPEEX_DENOISE_ENABLE)
        SpeexPreprocessState* mSpeexState;
        int mSpeexFrameSize;
		int16_t *mSpeexPcmIn;
#endif//SPEEX_AGC_ENABLE||SPEEX_DENOISE_ENABLE
    };

};

}; // namespace android

#endif
