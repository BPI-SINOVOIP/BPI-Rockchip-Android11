## 5.10\. Professional Audio

If device implementations report support for feature
`android.hardware.audio.pro` via the
[android.content.pm.PackageManager](
http://developer.android.com/reference/android/content/pm/PackageManager.html)
class, they:

*    [C-1-1] MUST report support for feature
`android.hardware.audio.low_latency`.
*    [C-1-2] MUST have the continuous round-trip audio latency, as defined in
[section 5.6 Audio Latency](#5_6_audio_latency), MUST be 20 milliseconds or less and SHOULD be
10 milliseconds or less over at least one supported path.
*    [C-1-3] MUST include a USB port(s) supporting USB host mode and USB
peripheral mode.
*    [C-1-4] MUST report support for feature `android.software.midi`.
*    [C-1-5] MUST meet latencies and USB audio requirements using both the
[OpenSL ES](https://developer.android.com/ndk/guides/audio/opensl-for-android.html)
PCM buffer queue API and at least one path of the [AAudio native audio](https://developer.android.com/ndk/guides/audio/aaudio/aaudio.html)
API.
*    [SR] Are STRONGLY RECOMMENDED to meet latencies and USB audio requirements
using the [AAudio native audio](https://developer.android.com/ndk/guides/audio/aaudio/aaudio.html)
API over the [MMAP path](https://source.android.com/devices/audio/aaudio).
*    [C-1-6] MUST have Cold output latency of 200 milliseconds or less.
*    [C-1-7] MUST have Cold input latency of 200 milliseconds or less.
*    [SR] Are STRONGLY RECOMMENDED to provide a consistent level of CPU
performance while audio is active and CPU load is varying. This should be tested
using the Android app version of [SynthMark](https://github.com/google/synthmark)
commit id [09b13c6f49ea089f8c31e5d035f912cc405b7ab8](https://github.com/google/synthmark/commit/09b13c6f49ea089f8c31e5d035f912cc405b7ab8).
SynthMark uses a software synthesizer running on a simulated audio framework
that measures system performance. The SynthMark app needs to be run using the
“Automated Test” option and achieve the following results:
    * voicemark.90 &gt;= 32 voices
    * latencymark.fixed.little &lt;= 15 msec
    * latencymark.dynamic.little &lt;= 50 msec

See the SynthMark [documentation](https://github.com/google/synthmark/blob/master/docs/README.md)
for an explanation of the benchmarks.

*    SHOULD minimize audio clock inaccuracy and drift relative to standard time.
*    SHOULD minimize audio clock drift relative to the CPU `CLOCK_MONOTONIC`
when both are active.
*    SHOULD minimize audio latency over on-device transducers.
*    SHOULD minimize audio latency over USB digital audio.
*    SHOULD document audio latency measurements over all paths.
*    SHOULD minimize jitter in audio buffer completion callback entry times, as this
affects usable percentage of full CPU bandwidth by the callback.
*    SHOULD provide zero audio glitches under normal use at reported latency.
*    SHOULD provide zero inter-channel latency difference.
*    SHOULD minimize MIDI mean latency over all transports.
*    SHOULD minimize MIDI latency variability under load (jitter) over all transports.
*    SHOULD provide accurate MIDI timestamps over all transports.
*    SHOULD minimize audio signal noise over on-device transducers, including the
period immediately after cold start.
*    SHOULD provide zero audio clock difference between the input and output sides of
corresponding end-points, when both are active. Examples of corresponding
end-points include the on-device microphone and speaker, or the audio jack input
and output.
*    SHOULD handle audio buffer completion callbacks for the input and output sides
of corresponding end-points on the same thread when both are active, and enter
the output callback immediately after the return from the input callback.  Or
if it is not feasible to handle the callbacks on the same thread, then enter the
output callback shortly after entering the input callback to permit the
application to have a consistent timing of the input and output sides.
*    SHOULD minimize the phase difference between HAL audio buffering for the input
and output sides of corresponding end-points.
*    SHOULD minimize touch latency.
*    SHOULD minimize touch latency variability under load (jitter).
*    SHOULD have a latency from touch input to audio output of less than or
equal to 40 ms.

If device implementations meet all of the above requirements, they:

*   [SR] STRONGLY RECOMMENDED to report support for feature
`android.hardware.audio.pro` via the [`android.content.pm.PackageManager`](
http://developer.android.com/reference/android/content/pm/PackageManager.html)
class.

If device implementations include a 4 conductor 3.5mm audio jack, they:

*   [C-2-1] MUST have the continuous round-trip audio latency to be 20
milliseconds or less over the audio jack path.
*   [SR] STRONGLY RECOMMENDED to comply with
section [Mobile device (jack) specifications](
https://source.android.com/devices/accessories/headset/jack-headset-spec)
of the [Wired Audio Headset Specification (v1.1)](
https://source.android.com/devices/accessories/headset/plug-headset-spec).
*   The continuous round-trip audio latency SHOULD be 10 milliseconds
or less over the audio jack path.

If device implementations omit a 4 conductor 3.5mm audio jack and
include a USB port(s) supporting USB host mode, they:

*   [C-3-1] MUST implement the USB audio class.
*   [C-3-2] MUST have a continuous round-trip audio latency of 20
milliseconds or less over the USB host mode port using USB audio class.
*   The continuous round-trip audio latency SHOULD be 10 milliseconds
or less over the USB host mode port using USB audio class.
*   [C-SR] Are STRONGLY RECOMMENDED to support simultaneous I/O up to 8 channels
    each direction, 96 kHz sample rate, and 24-bit or 32-bit depth, when used
    with USB audio peripherals that also support these requirements.

If device implementations include an HDMI port, they:

*   SHOULD support output in stereo and eight channels at 20-bit or
24-bit depth and 192 kHz without bit-depth loss or resampling,
in at least one configuration.
