## 5.4\. Audio Recording

While some of the requirements outlined in this section are listed as SHOULD
since Android 4.3, the Compatibility Definition for future versions are planned
to change these to MUST. Existing and new Android devices are **STRONGLY
RECOMMENDED** to meet these requirements that are listed as SHOULD, or they
will not be able to attain Android compatibility when upgraded to the future
version.

### 5.4.1\. Raw Audio Capture and Microphone Information

If device implementations declare `android.hardware.microphone`, they:

*   [C-1-1] MUST allow capture of raw audio content with the following
characteristics:

     *   **Format**: Linear PCM, 16-bit
     *   **Sampling rates**: 8000, 11025, 16000, 44100, 48000 Hz
     *   **Channels**: Mono

*   SHOULD allow capture of raw audio content with the following
characteristics:
     *   **Format**: Linear PCM, 16-bit and 24-bit
     *   **Sampling rates**: 8000, 11025, 16000, 22050, 24000, 32000, 44100,
     48000 Hz
     *   **Channels**: As many channels as the number of microphones on the
     device

*   [C-1-2] MUST capture at above sample rates without up-sampling.
*   [C-1-3] MUST include an appropriate anti-aliasing filter when the
sample rates given above are captured with down-sampling.
*   SHOULD allow AM radio and DVD quality capture of raw audio content, which
means the following characteristics:

     *   **Format**: Linear PCM, 16-bit
     *   **Sampling rates**: 22050, 48000 Hz
     *   **Channels**: Stereo
*   [C-1-4] MUST honor the [`MicrophoneInfo`](
    https://developer.android.com/reference/android/media/MicrophoneInfo) API
    and properly fill in information for the available microphones on device
    accessible to the third-party applications via the
    [`AudioManager.getMicrophones()`](
    https://developer.android.com/reference/android/media/AudioManager#getMicrophones%28%29)
    API, and the currently active microphones which are accessible to the third
    party applications via the [`AudioRecord.getActiveMicrophones()`](
    https://developer.android.com/reference/android/media/AudioRecord#getActiveMicrophones%28%29)
    and [`MediaRecorder.getActiveMicrophones()`](https://developer.android.com/reference/android/media/MediaRecorder#getActiveMicrophones%28%29)
    APIs.
If device implementations allow AM radio and DVD quality capture of raw audio
content, they:

*   [C-2-1] MUST capture without up-sampling at any ratio higher
than 16000:22050 or 44100:48000.
*   [C-2-2] MUST include an appropriate anti-aliasing filter for any
up-sampling or down-sampling.

### 5.4.2\. Capture for Voice Recognition

If device implementations declare `android.hardware.microphone`, they:

*   [C-1-1] MUST capture
    `android.media.MediaRecorder.AudioSource.VOICE_RECOGNITION` audio source at
    one of the sampling rates, 44100 and 48000.
*   [C-1-2] MUST, by default, disable any noise reduction audio processing when
    recording an audio stream from the `AudioSource.VOICE_RECOGNITION` audio
    source.
*   [C-1-3] MUST, by default, disable any automatic gain control when recording
    an audio stream from the `AudioSource.VOICE_RECOGNITION` audio source.
*   SHOULD record the voice recognition audio stream with approximately flat
    amplitude versus frequency characteristics: specifically, ±3 dB, from 100 Hz
    to 4000 Hz.
*   SHOULD record the voice recognition audio stream with input sensitivity set
    such that a 90 dB sound power level (SPL) source at 1000 Hz yields RMS of
    2500 for 16-bit samples.
*   SHOULD record the voice recognition audio stream so that the PCM amplitude
    levels linearly track input SPL changes over at least a 30 dB range from -18
    dB to +12 dB re 90 dB SPL at the microphone.
*   SHOULD record the voice recognition audio stream with total harmonic
    distortion (THD) less than 1% for 1 kHz at 90 dB SPL input level at the
    microphone.

If device implementations declare `android.hardware.microphone` and noise
suppression (reduction) technologies tuned for speech recognition, they:

*   [C-2-1] MUST allow this audio effect to be controllable with the
    `android.media.audiofx.NoiseSuppressor` API.
*   [C-2-2] MUST uniquely identify each noise suppression technology
    implementation via the `AudioEffect.Descriptor.uuid` field.

### 5.4.3\. Capture for Rerouting of Playback

The `android.media.MediaRecorder.AudioSource` class includes the `REMOTE_SUBMIX`
audio source.

If device implementations declare both `android.hardware.audio.output` and
`android.hardware.microphone`, they:

*   [C-1-1] MUST properly implement the `REMOTE_SUBMIX` audio source so that
when an application uses the `android.media.AudioRecord` API to record from this
audio source, it captures a mix of all audio streams except for the following:

    * `AudioManager.STREAM_RING`
    * `AudioManager.STREAM_ALARM`
    * `AudioManager.STREAM_NOTIFICATION`

### 5.4.4\. Acoustic Echo Canceler

If device implementations declare `android.hardware.microphone`, they:

*   SHOULD implement an [Acoustic Echo Canceler](https://en.wikipedia.org/wiki/Echo_suppression_and_cancellation)
(AEC) technology tuned for voice communication and applied to the capture path
when capturing using `AudioSource.VOICE_COMMUNICATION`

If device implementations provides an Acoustic Echo Canceler which is
inserted in the capture audio path when `AudioSource.VOICE_COMMUNICATION`
is selected, they:

*   [C-SR] are STRONGLY_RECOMMENDED to declare this via [AcousticEchoCanceler](https://developer.android.com/reference/android/media/audiofx/AcousticEchoCanceler)
API method [AcousticEchoCanceler.isAvailable()](https://developer.android.com/reference/android/media/audiofx/AcousticEchoCanceler.html#isAvailable())
*   [C-SR] are STRONGLY_RECOMMENDED to allow this audio effect to be
controllable with the [AcousticEchoCanceler](https://developer.android.com/reference/android/media/audiofx/AcousticEchoCanceler)
API.
*   [C-SR] are STRONGLY_RECOMMENDED to uniquely identify each AEC technology
implementation via the [AudioEffect.Descriptor.uuid](https://developer.android.com/reference/android/media/audiofx/AudioEffect.Descriptor.html#uuid)
field.

### 5.4.5\. Concurrent Capture

If device implementations declare `android.hardware.microphone`,they MUST
implement concurrent capture as described in [this document](
https://developer.android.com/guide/topics/media/sharing-audio-input). Specifically:

*   [C-1-1] MUST allow concurrent access to microphone by an accessibility
    service capturing with `AudioSource.VOICE_RECOGNITION` and at least one
    application capturing with any `AudioSource`.
*   [C-1-2] MUST allow concurrent access to microphone by a pre-installed
    application that holds an Assistant role and at least one application
    capturing with any `AudioSource` except for
    `AudioSource.VOICE_COMMUNICATION` or `AudioSource.CAMCORDER`.
*   [C-1-3] MUST silence the audio capture for any other application, except for
    an accessibility service, while an application is capturing with
    `AudioSource.VOICE_COMMUNICATION` or `AudioSource.CAMCORDER`. However, when
    an app is capturing via `AudioSource.VOICE_COMMUNICATION` then another app
    can capture the voice call if it is a privileged (pre-installed) app with
    permission `CAPTURE_AUDIO_OUTPUT`.
*   [C-1-4] If two or more applications are capturing concurrently and if
    neither app has an UI on top, the one that started capture the most recently
    receives audio.

### 5.4.6\. Microphone Gain Levels

If device implementations declare `android.hardware.microphone`, they:

*   SHOULD exhibit approximately flat amplitude-versus-frequency
    characteristics in the mid-frequency range: specifically ±3dB from 100
    Hz to 4000 Hz for each and every microphone used to record the voice
    recognition audio source.
*   SHOULD set audio input sensitivity such that a 1000 Hz sinusoidal
    tone source played at 90 dB Sound Pressure Level (SPL) yields a response
    with RMS of 2500 for 16 bit-samples (or -22.35 dB Full Scale for floating
    point/double precision samples) for each and every microphone used to
    record the voice recognition audio source.
*   [C-SR] are STRONGLY RECOMMENDED to exhibit amplitude levels in the low
    frequency range: specifically from ±20 dB from 5 Hz to 100 Hz compared
    to the mid-frequency range for each and every microphone used to record
    the voice recognition audio source.
*   [C-SR] are STRONGLY RECOMMENDED to exhibit amplitude levels in the
    high frequency range: specifically from ±30 dB from 4000 Hz to 22 KHz
    compared to the mid-frequency range for each and every microphone used
    to record the voice recognition audio source.
