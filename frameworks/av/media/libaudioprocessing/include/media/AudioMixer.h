/*
**
** Copyright 2007, The Android Open Source Project
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

#ifndef ANDROID_AUDIO_MIXER_H
#define ANDROID_AUDIO_MIXER_H

#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>

#include <android/os/IExternalVibratorService.h>
#include <media/AudioMixerBase.h>
#include <media/BufferProviders.h>
#include <utils/threads.h>

// FIXME This is actually unity gain, which might not be max in future, expressed in U.12
#define MAX_GAIN_INT AudioMixerBase::UNITY_GAIN_INT

namespace android {

// ----------------------------------------------------------------------------

// AudioMixer extends AudioMixerBase by adding support for down- and up-mixing
// and time stretch that are implemented via Effects HAL, and adding support
// for haptic channels which depends on Vibrator service. This is the version
// that is used by Audioflinger.

class AudioMixer : public AudioMixerBase
{
public:
    // maximum number of channels supported for the content
    static const uint32_t MAX_NUM_CHANNELS_TO_DOWNMIX = AUDIO_CHANNEL_COUNT_MAX;

    enum { // extension of AudioMixerBase parameters
        DOWNMIX_TYPE    = 0x4004,
        // for haptic
        HAPTIC_ENABLED  = 0x4007, // Set haptic data from this track should be played or not.
        HAPTIC_INTENSITY = 0x4008, // Set the intensity to play haptic data.
        // for target TIMESTRETCH
        PLAYBACK_RATE   = 0x4300, // Configure timestretch on this track name;
                                  // parameter 'value' is a pointer to the new playback rate.
    };

    typedef enum { // Haptic intensity, should keep consistent with VibratorService
        HAPTIC_SCALE_MUTE = os::IExternalVibratorService::SCALE_MUTE,
        HAPTIC_SCALE_VERY_LOW = os::IExternalVibratorService::SCALE_VERY_LOW,
        HAPTIC_SCALE_LOW = os::IExternalVibratorService::SCALE_LOW,
        HAPTIC_SCALE_NONE = os::IExternalVibratorService::SCALE_NONE,
        HAPTIC_SCALE_HIGH = os::IExternalVibratorService::SCALE_HIGH,
        HAPTIC_SCALE_VERY_HIGH = os::IExternalVibratorService::SCALE_VERY_HIGH,
    } haptic_intensity_t;
    static constexpr float HAPTIC_SCALE_VERY_LOW_RATIO = 2.0f / 3.0f;
    static constexpr float HAPTIC_SCALE_LOW_RATIO = 3.0f / 4.0f;
    static const constexpr float HAPTIC_MAX_AMPLITUDE_FLOAT = 1.0f;

    static inline bool isValidHapticIntensity(haptic_intensity_t hapticIntensity) {
        switch (hapticIntensity) {
        case HAPTIC_SCALE_MUTE:
        case HAPTIC_SCALE_VERY_LOW:
        case HAPTIC_SCALE_LOW:
        case HAPTIC_SCALE_NONE:
        case HAPTIC_SCALE_HIGH:
        case HAPTIC_SCALE_VERY_HIGH:
            return true;
        default:
            return false;
        }
    }

    AudioMixer(size_t frameCount, uint32_t sampleRate)
            : AudioMixerBase(frameCount, sampleRate) {
        pthread_once(&sOnceControl, &sInitRoutine);
    }

    bool isValidChannelMask(audio_channel_mask_t channelMask) const override;

    void setParameter(int name, int target, int param, void *value) override;
    void setBufferProvider(int name, AudioBufferProvider* bufferProvider);

private:

    struct Track : public TrackBase {
        Track() : TrackBase() {}

        ~Track()
        {
            // mInputBufferProvider need not be deleted.
            // Ensure the order of destruction of buffer providers as they
            // release the upstream provider in the destructor.
            mTimestretchBufferProvider.reset(nullptr);
            mPostDownmixReformatBufferProvider.reset(nullptr);
            mDownmixerBufferProvider.reset(nullptr);
            mReformatBufferProvider.reset(nullptr);
            mContractChannelsNonDestructiveBufferProvider.reset(nullptr);
            mAdjustChannelsBufferProvider.reset(nullptr);
        }

        uint32_t getOutputChannelCount() override {
            return mDownmixerBufferProvider.get() != nullptr ? mMixerChannelCount : channelCount;
        }
        uint32_t getMixerChannelCount() override {
            return mMixerChannelCount + mMixerHapticChannelCount;
        }

        status_t    prepareForDownmix();
        void        unprepareForDownmix();
        status_t    prepareForReformat();
        void        unprepareForReformat();
        status_t    prepareForAdjustChannels();
        void        unprepareForAdjustChannels();
        status_t    prepareForAdjustChannelsNonDestructive(size_t frames);
        void        unprepareForAdjustChannelsNonDestructive();
        void        clearContractedBuffer();
        bool        setPlaybackRate(const AudioPlaybackRate &playbackRate);
        void        reconfigureBufferProviders();

        /* Buffer providers are constructed to translate the track input data as needed.
         * See DownmixerBufferProvider below for how the Track buffer provider
         * is wrapped by another one when dowmixing is required.
         *
         * TODO: perhaps make a single PlaybackConverterProvider class to move
         * all pre-mixer track buffer conversions outside the AudioMixer class.
         *
         * 1) mInputBufferProvider: The AudioTrack buffer provider.
         * 2) mAdjustChannelsBufferProvider: Expands or contracts sample data from one interleaved
         *    channel format to another. Expanded channels are filled with zeros and put at the end
         *    of each audio frame. Contracted channels are copied to the end of the buffer.
         * 3) mContractChannelsNonDestructiveBufferProvider: Non-destructively contract sample data.
         *    This is currently using at audio-haptic coupled playback to separate audio and haptic
         *    data. Contracted channels could be written to given buffer.
         * 4) mReformatBufferProvider: If not NULL, performs the audio reformat to
         *    match either mMixerInFormat or mDownmixRequiresFormat, if the downmixer
         *    requires reformat. For example, it may convert floating point input to
         *    PCM_16_bit if that's required by the downmixer.
         * 5) mDownmixerBufferProvider: If not NULL, performs the channel remixing to match
         *    the number of channels required by the mixer sink.
         * 6) mPostDownmixReformatBufferProvider: If not NULL, performs reformatting from
         *    the downmixer requirements to the mixer engine input requirements.
         * 7) mTimestretchBufferProvider: Adds timestretching for playback rate
         */
        AudioBufferProvider* mInputBufferProvider;    // externally provided buffer provider.
        // TODO: combine mAdjustChannelsBufferProvider and
        // mContractChannelsNonDestructiveBufferProvider
        std::unique_ptr<PassthruBufferProvider> mAdjustChannelsBufferProvider;
        std::unique_ptr<PassthruBufferProvider> mContractChannelsNonDestructiveBufferProvider;
        std::unique_ptr<PassthruBufferProvider> mReformatBufferProvider;
        std::unique_ptr<PassthruBufferProvider> mDownmixerBufferProvider;
        std::unique_ptr<PassthruBufferProvider> mPostDownmixReformatBufferProvider;
        std::unique_ptr<PassthruBufferProvider> mTimestretchBufferProvider;

        audio_format_t mDownmixRequiresFormat;  // required downmixer format
                                                // AUDIO_FORMAT_PCM_16_BIT if 16 bit necessary
                                                // AUDIO_FORMAT_INVALID if no required format

        AudioPlaybackRate    mPlaybackRate;

        // Haptic
        bool                 mHapticPlaybackEnabled;
        haptic_intensity_t   mHapticIntensity;
        audio_channel_mask_t mHapticChannelMask;
        uint32_t             mHapticChannelCount;
        audio_channel_mask_t mMixerHapticChannelMask;
        uint32_t             mMixerHapticChannelCount;
        uint32_t             mAdjustInChannelCount;
        uint32_t             mAdjustOutChannelCount;
        uint32_t             mAdjustNonDestructiveInChannelCount;
        uint32_t             mAdjustNonDestructiveOutChannelCount;
        bool                 mKeepContractedChannels;

        float getHapticScaleGamma() const {
        // Need to keep consistent with the value in VibratorService.
        switch (mHapticIntensity) {
        case HAPTIC_SCALE_VERY_LOW:
            return 2.0f;
        case HAPTIC_SCALE_LOW:
            return 1.5f;
        case HAPTIC_SCALE_HIGH:
            return 0.5f;
        case HAPTIC_SCALE_VERY_HIGH:
            return 0.25f;
        default:
            return 1.0f;
        }
        }

        float getHapticMaxAmplitudeRatio() const {
        // Need to keep consistent with the value in VibratorService.
        switch (mHapticIntensity) {
        case HAPTIC_SCALE_VERY_LOW:
            return HAPTIC_SCALE_VERY_LOW_RATIO;
        case HAPTIC_SCALE_LOW:
            return HAPTIC_SCALE_LOW_RATIO;
        case HAPTIC_SCALE_NONE:
        case HAPTIC_SCALE_HIGH:
        case HAPTIC_SCALE_VERY_HIGH:
            return 1.0f;
        default:
            return 0.0f;
        }
        }
    };

    inline std::shared_ptr<Track> getTrack(int name) {
        return std::static_pointer_cast<Track>(mTracks[name]);
    }

    std::shared_ptr<TrackBase> preCreateTrack() override;
    status_t postCreateTrack(TrackBase *track) override;

    void preProcess() override;
    void postProcess() override;

    bool setChannelMasks(int name,
            audio_channel_mask_t trackChannelMask, audio_channel_mask_t mixerChannelMask) override;

    static void sInitRoutine();

    static pthread_once_t sOnceControl; // initialized in constructor by first new
};

// ----------------------------------------------------------------------------
} // namespace android

#endif // ANDROID_AUDIO_MIXER_H
