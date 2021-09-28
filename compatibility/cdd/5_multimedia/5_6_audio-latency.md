## 5.6\. Audio Latency

Audio latency is the time delay as an audio signal passes through a system.
Many classes of applications rely on short latencies, to achieve real-time
sound effects.

For the purposes of this section, use the following definitions:

*   **output latency**. The interval between when an application writes a frame
of PCM-coded data and when the corresponding sound is presented to environment
at an on-device transducer or signal leaves the device via a port and can be
observed externally.
*   **cold output latency**. The output latency for the first frame, when the
audio output system has been idle and powered down prior to the request.
*   **continuous output latency**. The output latency for subsequent frames,
after the device is playing audio.
*   **input latency**. The interval between when a sound is presented by
environment to device at an on-device transducer or signal enters the device via
a port and when an application reads the corresponding frame of PCM-coded data.
*   **lost input**. The initial portion of an input signal that is unusable or
unavailable.
*   **cold input latency**. The sum of lost input time and the input latency
for the first frame, when the audio input system has been idle and powered down
prior to the request.
*   **continuous input latency**. The input latency for subsequent frames,
while the device is capturing audio.
*   **cold output jitter**. The variability among separate measurements of cold
output latency values.
*   **cold input jitter**. The variability among separate measurements of cold
input latency values.
*   **continuous round-trip latency**. The sum of continuous input latency plus
continuous output latency plus one buffer period. The buffer period allows
time for the app to process the signal and time for the app to mitigate phase
difference between input and output streams.
*   **OpenSL ES PCM buffer queue API**. The set of PCM-related
[OpenSL ES](https://developer.android.com/ndk/guides/audio/opensl/index.html)
APIs within [Android NDK](https://developer.android.com/ndk/index.html).
*   **AAudio native audio API**. The set of
[AAudio](https://developer.android.com/ndk/guides/audio/aaudio/aaudio.html) APIs
within [Android NDK](https://developer.android.com/ndk/index.html).
*   **Timestamp**. A pair consisting of a relative frame position within a
stream and the estimated time when that frame enters or leaves the
audio processing pipeline on the associated endpoint.  See also
[AudioTimestamp](https://developer.android.com/reference/android/media/AudioTimestamp).
*   **glitch**. A temporary interruption or incorrect sample value in the audio signal,
typically caused by a
[buffer underrun](https://en.wikipedia.org/wiki/Buffer_underrun) for output,
buffer overrun for input, or any other source of digital or analog noise.

If device implementations declare `android.hardware.audio.output`, they
MUST meet or exceed the following requirements:

*   [C-1-1] The output timestamp returned by
[AudioTrack.getTimestamp](https://developer.android.com/reference/android/media/AudioTrack.html#getTimestamp(android.media.AudioTimestamp))
and `AAudioStream_getTimestamp` is accurate to +/- 2 ms.
*   [C-1-2] Cold output latency of 500 milliseconds or less.

If device implementations declare `android.hardware.audio.output` they are
STRONGLY RECOMMENDED to meet or exceed the following requirements:

*   [C-SR] Cold output latency of 100 milliseconds or less. Existing and new
    devices that run this version of Android are VERY STRONGLY RECOMMENDED
    to meet these requirements now. In a future platform release in 2021, we
    will require Cold output latency of 200 ms or less as a MUST.
*   [C-SR] Continuous output latency of 45 milliseconds or less.
*   [C-SR] Minimize the cold output jitter.
*   [C-SR] The output timestamp returned by
[AudioTrack.getTimestamp](https://developer.android.com/reference/android/media/AudioTrack.html#getTimestamp(android.media.AudioTimestamp))
and `AAudioStream_getTimestamp` is accurate to +/- 1 ms.

If device implementations meet the above requirements, after any initial
calibration, when using both the OpenSL ES PCM buffer queue and AAudio native audio APIs,
for continuous output latency and cold output latency over at least one supported audio
output device, they are:

*   [C-SR] STRONGLY RECOMMENDED to report low-latency audio by declaring
    `android.hardware.audio.low_latency` feature flag.
*   [C-SR] STRONGLY RECOMMENDED to meet the requirements for low-latency
    audio via the AAudio API.
*   [C-SR] STRONGLY RECOMMENDED to ensure that for streams that return
    [`AAUDIO_PERFORMANCE_MODE_LOW_LATENCY`](https://developer.android.com/ndk/guides/audio/aaudio/aaudio#performance-mode)
    from [`AAudioStream_getPerformanceMode()`](https://developer.android.com/ndk/reference/group/audio#aaudiostream_getperformancemode),
    the value returned by [`AAudioStream_getFramesPerBurst()`](https://developer.android.com/ndk/reference/group/audio#aaudiostream_getframesperburst)
    is less than or equal to the value returned by [`android.media.AudioManager.getProperty(String)`](https://developer.android.com/reference/android/media/AudioManager.html#getProperty%28java.lang.String%29)
    for property key [`AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER`](https://developer.android.com/reference/android/media/AudioManager.html#PROPERTY_OUTPUT_FRAMES_PER_BUFFER).

If device implementations do not meet the requirements for low-latency audio
via both the OpenSL ES PCM buffer queue and AAudio native audio APIs, they:

*   [C-2-1] MUST NOT report support for low-latency audio.

If device implementations include `android.hardware.microphone`, they
MUST meet these input audio requirements:

*   [C-3-1] Limit the error in input timestamps, as returned by
[AudioRecord.getTimestamp](https://developer.android.com/reference/android/media/AudioRecord.html#getTimestamp(android.media.AudioTimestamp,%20int))
or `AAudioStream_getTimestamp`, to +/- 2 ms.
"Error" here means the deviation from the correct value.
*   [C-3-2] Cold input latency of 500 milliseconds or less.

If device implementations include `android.hardware.microphone`, they are
STRONGLY RECOMMENDED to meet these input audio requirements:

   *   [C-SR] Cold input latency of 100 milliseconds or less. Existing and new
       devices that run this version of Android are VERY STRONGLY RECOMMENDED
       to meet these requirements now. In a future platform release in 2021 we
       will require Cold input latency of 200 ms or less as a MUST.
   *   [C-SR] Continuous input latency of 30 milliseconds or less.
   *   [C-SR] Continuous round-trip latency of 50 milliseconds or less.
   *   [C-SR] Minimize the cold input jitter.
   *   [C-SR] Limit the error in input timestamps, as returned by
[AudioRecord.getTimestamp](https://developer.android.com/reference/android/media/AudioRecord.html#getTimestamp(android.media.AudioTimestamp,%20int))
or `AAudioStream_getTimestamp`, to +/- 1 ms.
