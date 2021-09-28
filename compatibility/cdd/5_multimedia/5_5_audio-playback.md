## 5.5\. Audio Playback

Android includes the support to allow apps to playback audio through the audio
output peripheral as defined in section 7.8.2.

### 5.5.1\. Raw Audio Playback

If device implementations declare `android.hardware.audio.output`, they:

*   [C-1-1] MUST allow playback of raw audio content with the following
characteristics:

     *   **Source formats**: Linear PCM, 16-bit, 8-bit, float
     *   **Channels**: Mono, Stereo, valid multichannel configurations
            with up to 8 channels
     *   **Sampling rates (in Hz)**:
          * 8000, 11025, 16000, 22050, 32000, 44100, 48000 at the channel
              configurations listed above
          * 96000 in mono and stereo

*   SHOULD allow playback of raw audio content with the following
characteristics:

     *   **Sampling rates**: 24000, 48000

### 5.5.2\. Audio Effects

Android provides an [API for audio effects](
http://developer.android.com/reference/android/media/audiofx/AudioEffect.html)
for device implementations.

If device implementations declare the feature `android.hardware.audio.output`,
they:

*   [C-1-1] MUST support the `EFFECT_TYPE_EQUALIZER` and
`EFFECT_TYPE_LOUDNESS_ENHANCER` implementations controllable through the
AudioEffect subclasses `Equalizer` and `LoudnessEnhancer`.
*   [C-1-2] MUST support the visualizer API implementation, controllable through
the `Visualizer` class.
*   [C-1-3] MUST support the `EFFECT_TYPE_DYNAMICS_PROCESSING` implementation
controllable through the AudioEffect subclass [`DynamicsProcessing`](https://developer.android.com/reference/android/media/audiofx/DynamicsProcessing).
*   SHOULD support the `EFFECT_TYPE_BASS_BOOST`, `EFFECT_TYPE_ENV_REVERB`,
`EFFECT_TYPE_PRESET_REVERB`, and `EFFECT_TYPE_VIRTUALIZER` implementations
controllable through the `AudioEffect` sub-classes `BassBoost`,
`EnvironmentalReverb`, `PresetReverb`, and `Virtualizer`.
*  [C-SR] Are STRONGLY RECOMMENDED to support effects in floating-point and
multichannel.

### 5.5.3\. Audio Output Volume

Automotive device implementations:

*   SHOULD allow adjusting audio volume
separately per each audio stream using the content type or usage as defined
by [AudioAttributes]("http://developer.android.com/reference/android/media/AudioAttributes.html")
and car audio usage as publicly defined in `android.car.CarAudioManager`.
