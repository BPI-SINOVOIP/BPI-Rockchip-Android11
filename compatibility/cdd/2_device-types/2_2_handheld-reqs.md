## 2.2\. Handheld Requirements

An **Android Handheld device** refers to an Android device implementation that is
typically used by holding it in the hand, such as an mp3 player, phone, or
tablet.

Android device implementations are classified as a Handheld if they meet all the
following criteria:

*   Have a power source that provides mobility, such as a battery.
*   Have a physical diagonal screen size in the range of 3.3 inches (or
    2.5 inches for devices which launched on an API level earlier than
    Android 11) to 8 inches.

The additional requirements in the rest of this section are specific to Android
Handheld device implementations.


<div class="note">
<b>Note:</b> Requirements that do not apply to Android Tablet devices are marked with an *.
</div>

### 2.2.1\. Hardware

Handheld device implementations:

*   [[7.1](#7_1_display_and_graphics).1.1/H-0-1] MUST have at least one
Android-compatible display that meets all requirements described on this
document.
*   [[7.1](#7_1_display_and_graphics).1.3/H-SR] Are STRONGLY RECOMMENDED to
provide users an affordance to change the display size (screen density).

If Handheld device implementations support software screen rotation, they:

*   [[7.1](#7_1_display_and_graphics).1.1/H-1-1]* MUST make the logical screen
that is made available for third party applications be at least 2 inches on the
short edge(s) and 2.7 inches on the long edge(s).
Devices which launched on an API level earlier than that of this document are
exempted from this requirement.

If Handheld device implementations do not support software screen rotation,
they:

*   [[7.1](#7_1_display_and_graphics).1.1/H-2-1]* MUST make the logical screen
that is made available for third party applications be at least 2.7 inches on
the short edge(s).
Devices which launched on an API level earlier than that of this document are
exempted from this requirement.

If Handheld device implementations claim support for high dynamic range
displays through [`Configuration.isScreenHdr()`
](https://developer.android.com/reference/android/content/res/Configuration.html#isScreenHdr%28%29)
, they:

*   [[7.1](#7_1_display-and-graphics).4.5/H-1-1] MUST advertise support for the
    `EGL_EXT_gl_colorspace_bt2020_pq`, `EGL_EXT_surface_SMPTE2086_metadata`,
    `EGL_EXT_surface_CTA861_3_metadata`, `VK_EXT_swapchain_colorspace`, and
    `VK_EXT_hdr_metadata` extensions.

Handheld device implementations:

*   [[7.1](#7_1_display_and_graphics).4.6/H-0-1] MUST report whether the device
    supports the GPU profiling capability via a system property
    `graphics.gpu.profiler.support`.

If Handheld device implementations declare support via a system property
`graphics.gpu.profiler.support`, they:

*    [[7.1](#7_1_display_and_graphics).4.6/H-1-1] MUST report as output a
     protobuf trace that complies with the schema for GPU counters and GPU
     renderstages defined in the [Perfetto documentation](https://developer.android.com/studio/command-line/perfetto).
*    [[7.1](#7_1_display_and_graphics).4.6/H-1-2] MUST report conformant values
     for the device’s GPU counters following the
     [gpu counter trace packet proto](https://android.googlesource.com/platform/external/perfetto/+/refs/heads/master/protos/perfetto/trace/gpu/gpu_counter_event.proto).
*    [[7.1](#7_1_display_and_graphics).4.6/H-1-3] MUST report conformant values
     for the device’s GPU RenderStages following the
     [render stage trace packet proto](https://android.googlesource.com/platform/external/perfetto/+/refs/heads/master/protos/perfetto/trace/gpu/gpu_render_stage_event.proto).
*    [[7.1](#7_1_display_and_graphics).4.6/H-1-4] MUST report a GPU Frequency
     tracepoint as specified by the format: [power/gpu_frequency](https://android.googlesource.com/platform/external/perfetto/+/refs/heads/master/protos/perfetto/trace/ftrace/power.proto).

Handheld device implementations:

*   [[7.1](#7_1_display_and_graphics).5/H-0-1] MUST include support for legacy
application compatibility mode as implemented by the upstream Android open
source code. That is, device implementations MUST NOT alter the triggers or
thresholds at which compatibility mode is activated, and MUST NOT alter the
behavior of the compatibility mode itself.
*   [[7.2](#7_2_input_devices).1/H-0-1] MUST include support for third-party
Input Method Editor (IME) applications.
*   [[7.2](#7_2_input_devices).3/H-0-3] MUST provide the Home function on
    all the Android-compatible displays that provide the home screen.
*   [[7.2](#7_2_input_devices).3/H-0-4] MUST provide the Back function on all
    the Android-compatible displays and the Recents function on at least one of
    the Android-compatible displays.
*   [[7.2](#7_2_input_devices).3/H-0-2] MUST send both the normal and long press
event of the Back function ([`KEYCODE_BACK`](
http://developer.android.com/reference/android/view/KeyEvent.html#KEYCODE_BACK))
to the foreground application. These events MUST NOT be consumed by the system
and CAN be triggered by outside of the Android device (e.g. external hardware
keyboard connected to the Android device).
*   [[7.2](#7_2_input_devices).4/H-0-1] MUST support touchscreen input.
*   [[7.2](#7_2_input_devices).4/H-SR] Are STRONGLY RECOMMENDED to launch the
user-selected assist app, in other words the app that implements
VoiceInteractionService, or an activity handling the [`ACTION_ASSIST`](https://developer.android.com/reference/android/content/Intent#ACTION_ASSIST)
on long-press of [`KEYCODE_MEDIA_PLAY_PAUSE`](https://developer.android.com/reference/android/view/KeyEvent#KEYCODE_MEDIA_PLAY_PAUSE)
or [`KEYCODE_HEADSETHOOK`](https://developer.android.com/reference/android/view/KeyEvent#KEYCODE_HEADSETHOOK)
if the foreground activity does not handle those long-press events.
*  [[7.3](#7_3_sensors).1/H-SR] Are STRONGLY RECOMMENDED to include a 3-axis
accelerometer.

If Handheld device implementations include a 3-axis accelerometer, they:

*  [[7.3](#7_3_sensors).1/H-1-1] MUST be able to report events up to a frequency
of at least 100 Hz.

If Handheld device implementations include a GPS/GNSS receiver and report the
capability to applications through the `android.hardware.location.gps` feature
flag, they:

*  [[7.3](#7_3_sensors).3/H-2-1] MUST report GNSS measurements, as soon as they
are found, even if a location calculated from GPS/GNSS is not yet reported.
*  [[7.3](#7_3_sensors).3/H-2-2] MUST report GNSS pseudoranges and pseudorange
rates, that, in open-sky conditions after determining the location, while
stationary or moving with less than 0.2 meter per second squared of
acceleration, are sufficient to calculate position within 20 meters, and speed
within 0.2 meters per second, at least 95% of the time.

If Handheld device implementations include a 3-axis gyroscope, they:

*  [[7.3](#7_3_sensors).4/H-3-1] MUST be able to report events up to a frequency
of at least 100 Hz.
*  [[7.3](#7_3_sensors).4/H-3-2] MUST be capable of measuring orientation changes
up to 1000 degrees per second.

Handheld device implementations that can make a voice call and indicate
any value other than `PHONE_TYPE_NONE` in `getPhoneType`:

*  [[7.3](#7_3_sensors).8/H] SHOULD include a proximity sensor.

Handheld device implementations:

*  [[7.3](#7_3_sensors).11/H-SR] Are RECOMMENDED to support pose sensor with 6
degrees of freedom.
*  [[7.4](#7_4_data_connectivity).3/H] SHOULD include support for Bluetooth and
Bluetooth LE.

If Handheld device implementations include a metered connection, they:

*  [[7.4](#7_4_data_connectivity).7/H-1-1] MUST provide the data saver mode.

If Handheld device implementations include a logical camera device that lists
capabilities using
[`CameraMetadata.REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA`](
https://developer.android.com/reference/android/hardware/camera2/CameraMetadata#REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA),
they:

*   [[7.5](#7_5_camera).4/H-1-1] MUST have normal field of view (FOV) by default
    and it MUST be between 50 and 90 degrees.

Handheld device implementations:

*  [[7.6](#7_6_memory_and_storage).1/H-0-1] MUST have at least 4 GB of
non-volatile storage available for application private data
(a.k.a. "/data" partition).
*  [[7.6](#7_6_memory_and_storage).1/H-0-2] MUST return “true” for
`ActivityManager.isLowRamDevice()` when there is less than 1GB of memory
available to the kernel and userspace.

If Handheld device implementations declare support of only a 32-bit ABI:

*   [[7.6](#7_6_memory_and_storage).1/H-1-1] The memory available to the kernel
    and userspace MUST be at least 416MB if the default display uses framebuffer
    resolutions up to qHD (e.g. FWVGA).

*   [[7.6](#7_6_memory_and_storage).1/H-2-1] The memory available to the kernel
    and userspace MUST be at least 592MB if the default display uses framebuffer
    resolutions up to HD+ (e.g. HD, WSVGA).

*   [[7.6](#7_6_memory_and_storage).1/H-3-1] The memory available to the kernel
    and userspace MUST be at least 896MB if the default display uses framebuffer
    resolutions up to FHD (e.g. WSXGA+).

*   [[7.6](#7_6_memory_and_storage).1/H-4-1] The memory available to the kernel
    and userspace MUST be at least 1344MB if the default display uses
    framebuffer resolutions up to QHD (e.g. QWXGA).

If Handheld device implementations declare support of 32-bit and 64-bit ABIs:

*   [[7.6](#7_6_memory_and_storage).1/H-5-1] The memory available to the kernel
    and userspace MUST
    be at least 816MB if the default display uses framebuffer resolutions up
    to qHD (e.g. FWVGA).

*   [[7.6](#7_6_memory_and_storage).1/H-6-1] The memory available to the kernel
    and userspace MUST be at least
    944MB if the default display uses framebuffer resolutions up to HD+
    (e.g. HD, WSVGA).

*   [[7.6](#7_6_memory_and_storage).1/H-7-1] The memory available to the kernel
    and userspace MUST be at least
    1280MB if the default display uses framebuffer resolutions up to FHD
    (e.g. WSXGA+).

*   [[7.6](#7_6_memory_and_storage).1/H-8-1] The memory available to the kernel
    and userspace MUST be at least
    1824MB if the default display uses framebuffer resolutions up to QHD
    (e.g. QWXGA).

Note that the "memory available to the kernel and userspace" above refers to the
memory space provided in addition to any memory already dedicated to hardware
components such as radio, video, and so on that are not under the kernel’s
control on device implementations.

If Handheld device implementations include less than or equal to 1GB of memory
available to the kernel and userspace, they:

*   [[7.6](#7_6_memory-and-storage).1/H-9-1] MUST declare the feature flag
    [`android.hardware.ram.low`](
    https://developer.android.com/reference/android/content/pm/PackageManager.html#FEATURE_RAM_LOW).
*   [[7.6](#7_6_memory-and-storage).1/H-9-2] MUST have at least 1.1 GB of
    non-volatile storage for application
    private data (a.k.a. "/data" partition).

If Handheld device implementations include more than 1GB of memory available
to the kernel and userspace, they:

*   [[7.6](#7_6_memory-and-storage).1/H-10-1] MUST have at least 4GB of
    non-volatile storage available for
    application private data (a.k.a. "/data" partition).
*   SHOULD declare the feature flag [`android.hardware.ram.normal`](
    https://developer.android.com/reference/android/content/pm/PackageManager.html#FEATURE_RAM_NORMAL).

Handheld device implementations:

*   [[7.6](#7_6_memory_and_storage).2/H-0-1] MUST NOT provide an application
shared storage smaller than 1 GiB.
*   [[7.7](#7_7_usb).1/H] SHOULD include a USB port supporting peripheral mode.

If handheld device implementations include a USB port supporting peripheral
mode, they:

*   [[7.7](#7_7_usb).1/H-1-1] MUST implement the Android Open Accessory (AOA)
API.

If Handheld device implementations include a USB port supporting host mode,
they:

*   [[7.7](#7_7_usb).2/H-1-1] MUST implement the [USB audio class](
http://developer.android.com/reference/android/hardware/usb/UsbConstants.html#USB_CLASS_AUDIO)
as documented in the Android SDK documentation.

Handheld device implementations:

*   [[7.8](#7_8_audio).1/H-0-1] MUST include a microphone.
*   [[7.8](#7_8_audio).2/H-0-1] MUST have an audio output and declare
`android.hardware.audio.output`.

If Handheld device implementations are capable of meeting all the performance
requirements for supporting VR mode and include support for it, they:

*   [[7.9](#7_9_virtual_reality).1/H-1-1] MUST declare the
`android.hardware.vr.high_performance` feature flag.
*   [[7.9](#7_9_virtual_reality).1/H-1-2] MUST include an application
implementing `android.service.vr.VrListenerService` that can be enabled by VR
applications via `android.app.Activity#setVrModeEnabled`.

If Handheld device implementations include one or more USB-C port(s) in host
mode and implement (USB audio class), in addition to requirements in
[section 7.7.2](#7_7_2_USB_host_mode), they:

*    [[7.8](#7_8_audio).2.2/H-1-1] MUST provide the following software mapping
of HID codes:

<table>
  <tr>
   <th>Function</th>
   <th>Mappings</th>
   <th>Context</th>
   <th>Behavior</th>
  </tr>
  <tr>
   <td rowspan="6">A</td>
   <td rowspan="6"><strong>HID usage page</strong>: 0x0C<br>
       <strong>HID usage</strong>: 0x0CD<br>
       <strong>Kernel key</strong>: <code>KEY_PLAYPAUSE</code><br>
       <strong>Android key</strong>: <code>KEYCODE_MEDIA_PLAY_PAUSE</code></td>
   <td rowspan="2">Media playback</td>
   <td><strong>Input</strong>: Short press<br>
       <strong>Output</strong>: Play or pause</td>
  </tr>
  <tr>
   <td><strong>Input</strong>: Long press<br>
       <strong>Output</strong>: Launch voice command<br>
       <strong>Sends</strong>:
       <code>android.speech.action.VOICE_SEARCH_HANDS_FREE</code> if the device
       is locked or its screen is off. Sends
       <code>android.speech.RecognizerIntent.ACTION_WEB_SEARCH</code> otherwise</td>
  </tr>
  <tr>
   <td rowspan="2">Incoming call</td>
   <td><strong>Input</strong>: Short press<br>
       <strong>Output</strong>: Accept call</td>
  </tr>
  <tr>
   <td><strong>Input</strong>: Long press<br>
       <strong>Output</strong>: Reject call</td>
  </tr>
  <tr>
   <td rowspan="2">Ongoing call</td>
   <td><strong>Input</strong>: Short press<br>
       <strong>Output</strong>: End call</td>
  </tr>
  <tr>
   <td><strong>Input</strong>: Long press<br>
       <strong>Output</strong>: Mute or unmute microphone</td>
  </tr>
  <tr>
   <td>B</td>
   <td><strong>HID usage page</strong>: 0x0C<br>
       <strong>HID usage</strong>: 0x0E9<br>
       <strong>Kernel key</strong>: <code>KEY_VOLUMEUP</code><br>
       <strong>Android key</strong>: <code>VOLUME_UP</code></td>
   <td>Media playback, Ongoing call</td>
   <td><strong>Input</strong>: Short or long press<br>
       <strong>Output</strong>: Increases the system or headset volume</td>
  </tr>
  <tr>
   <td>C</td>
   <td><strong>HID usage page</strong>: 0x0C<br>
       <strong>HID usage</strong>: 0x0EA<br>
       <strong>Kernel key</strong>: <code>KEY_VOLUMEDOWN</code><br>
       <strong>Android key</strong>: <code>VOLUME_DOWN</code></td>
   <td>Media playback, Ongoing call</td>
   <td><strong>Input</strong>: Short or long press<br>
       <strong>Output</strong>: Decreases the system or headset volume</td>
  </tr>
  <tr>
   <td>D</td>
   <td><strong>HID usage page</strong>: 0x0C<br>
       <strong>HID usage</strong>: 0x0CF<br>
       <strong>Kernel key</strong>: <code>KEY_VOICECOMMAND</code><br>
       <strong>Android key</strong>: <code>KEYCODE_VOICE_ASSIST</code></td>
   <td>All. Can be triggered in any instance.</td>
   <td><strong>Input</strong>: Short or long press<br>
       <strong>Output</strong>: Launch voice command</td>
  </tr>
</table>

*    [[7.8](#7_8_audio).2.2/H-1-2] MUST trigger [ACTION_HEADSET_PLUG](https://developer.android.com/reference/android/content/Intent#ACTION_HEADSET_PLUG)
upon a plug insert, but only after the USB audio interfaces and endpoints have
been properly enumerated in order to identify the type of terminal connected.

When the USB audio terminal types 0x0302 is detected, they:

*    [[7.8](#7_8_audio).2.2/H-2-1] MUST broadcast Intent ACTION_HEADSET_PLUG with
"microphone" extra set to 0.

When the USB audio terminal types 0x0402 is detected, they:

*    [[7.8](#7_8_audio).2.2/H-3-1] MUST broadcast Intent ACTION_HEADSET_PLUG with
"microphone" extra set to 1.

When API AudioManager.getDevices() is called while the USB peripheral is
connected they:

*    [[7.8](#7_8_audio).2.2/H-4-1] MUST list a device of type [AudioDeviceInfo.TYPE_USB_HEADSET](https://developer.android.com/reference/android/media/AudioDeviceInfo#TYPE_USB_HEADSET)
and role isSink() if the USB audio terminal type field is 0x0302.

*    [[7.8](#7_8_audio).2.2/H-4-2] MUST list a device of type
AudioDeviceInfo.TYPE_USB_HEADSET and role isSink() if the USB audio terminal
type field is 0x0402.

*    [[7.8](#7_8_audio).2.2/H-4-3] MUST list a device of type
AudioDeviceInfo.TYPE_USB_HEADSET and role isSource() if the USB audio terminal
type field is 0x0402.

*    [[7.8](#7_8_audio).2.2/H-4-4] MUST list a device of type [AudioDeviceInfo.TYPE_USB_DEVICE](
https://developer.android.com/reference/android/media/AudioDeviceInfo#TYPE_USB_DEVICE)
and role isSink() if the USB audio terminal type field is 0x603.

*    [[7.8](#7_8_audio).2.2/H-4-5] MUST list a device of type
AudioDeviceInfo.TYPE_USB_DEVICE and role isSource() if the USB audio terminal
type field is 0x604.

*    [[7.8](#7_8_audio).2.2/H-4-6] MUST list a device of type
AudioDeviceInfo.TYPE_USB_DEVICE and role isSink() if the USB audio terminal type
field is 0x400.

*    [[7.8](#7_8_audio).2.2/H-4-7] MUST list a device of type
AudioDeviceInfo.TYPE_USB_DEVICE and role isSource() if the USB audio terminal
type field is 0x400.

*    [[7.8](#7_8_audio).2.2/H-SR] Are STRONGLY RECOMMENDED upon connection of a
USB-C audio peripheral, to perform enumeration of USB descriptors, identify
terminal types and broadcast Intent ACTION_HEADSET_PLUG in less than
1000 milliseconds.

If Handheld device implementations include at least one haptic actuator, they:

*   [[7.10](#7_10_haptics)/H-SR]* Are STRONGLY RECOMMENDED NOT to use an
    eccentric rotating mass (ERM) haptic actuator(vibrator).
*   [[7.10](#7_10_haptics)/H]* SHOULD position the placement of the actuator
    near the location where the device is typically held or touched by hands.
*   [[7.10](#7_10_haptics)/H-SR]* Are STRONGLY RECOMMENDED to implement all
    public constants for [clear haptics](https://source.android.com/devices/haptics)
    in [android.view.HapticFeedbackConstants](https://developer.android.com/reference/android/view/HapticFeedbackConstants#constants)
    namely (CLOCK_TICK, CONTEXT_CLICK, KEYBOARD_PRESS, KEYBOARD_RELEASE,
    KEYBOARD_TAP, LONG_PRESS, TEXT_HANDLE_MOVE, VIRTUAL_KEY,
    VIRTUAL_KEY_RELEASE, CONFIRM, REJECT, GESTURE_START and GESTURE_END).
*   [[7.10](#7_10_haptics)/H-SR]* Are STRONGLY RECOMMENDED to implement all
    public constants for [clear haptics](https://source.android.com/devices/haptics)
    in [android.os.VibrationEffect](https://developer.android.com/reference/android/os/VibrationEffect)
    namely (EFFECT_TICK, EFFECT_CLICK, EFFECT_HEAVY_CLICK and
    EFFECT_DOUBLE_CLICK) and all public constants for [rich haptics](https://source.android.com/devices/haptics)
    in [android.os.VibrationEffect.Composition](https://developer.android.com/reference/android/os/VibrationEffect.Composition)
    namely (PRIMITIVE_CLICK and PRIMITIVE_TICK).
*   [[7.10](#7_10_haptics)/H-SR]* Are STRONGLY RECOMMENDED to use these linked
    haptic constants [mappings](https://source.android.com/devices/haptics).
*   [[7.10](#7_10_haptics)/H-SR]* Are STRONGLY RECOMMENDED to follow
    [quality assessment](https://source.android.com/devices/haptics)
    for [createOneShot()](https://developer.android.com/reference/android/os/VibrationEffect#createOneShot%28long,%20int%29)
    and [createWaveform()](https://developer.android.com/reference/android/os/VibrationEffect#createOneShot%28long,%20int%29)
    API's.
*   [[7.10](#7_10_haptics)/H-SR]* Are STRONGLY RECOMMENDED to verify the
    capabilities for amplitude scalability by running
    [android.os.Vibrator.hasAmplitudeControl()](https://developer.android.com/reference/android/os/Vibrator#hasAmplitudeControl%28%29).

Linear resonant actuator (LRA) is a single mass spring system which has a
dominant resonant frequency where the mass translates in the direction of
desired motion.

If Handheld device implementations include at least one linear resonant
actuator, they:

*  [[7.10](#7_10_haptics)/H]* SHOULD move the haptic actuator in the X-axis of
   portrait orientation.

If Handheld device implementations have a haptic actuator which is X-axis
Linear resonant actuator (LRA), they:

*   [[7.10](#7_10_haptics)/H-SR]* Are STRONGLY RECOMMENDED to have the resonant
    frequency of the X-axis LRA be under 200 Hz.

If handheld device implementations follow haptic constants mapping, they:

*   [[7.10](#7_10_haptics)/H-SR]* Are STRONGLY RECOMMENDED to perform a
    [quality assessment](https://source.android.com/devices/haptics)
    for haptic constants.

### 2.2.2\. Multimedia

Handheld device implementations MUST support the following audio encoding and
decoding formats and make them available to third-party applications:

*    [[5.1](#5_1_media_codecs)/H-0-1] AMR-NB
*    [[5.1](#5_1_media_codecs)/H-0-2] AMR-WB
*    [[5.1](#5_1_media_codecs)/H-0-3] MPEG-4 AAC Profile (AAC LC)
*    [[5.1](#5_1_media_codecs)/H-0-4] MPEG-4 HE AAC Profile (AAC+)
*    [[5.1](#5_1_media-codecs)/H-0-5] AAC ELD (enhanced low delay AAC)

Handheld device implementations MUST support the following video encoding
formats and make them available to third-party applications:

*    [[5.2](#5_2_video_encoding)/H-0-1] H.264 AVC
*    [[5.2](#5_2_video_encoding)/H-0-2] VP8

Handheld device implementations MUST support the following video decoding
formats and make them available to third-party applications:

*    [[5.3](#5_3_video_decoding)/H-0-1] H.264 AVC
*    [[5.3](#5_3_video_decoding)/H-0-2] H.265 HEVC
*    [[5.3](#5_3_video_decoding)/H-0-3] MPEG-4 SP
*    [[5.3](#5_3_video_decoding)/H-0-4] VP8
*    [[5.3](#5_3_video_decoding)/H-0-5] VP9

### 2.2.3\. Software

Handheld device implementations:

*   [[3.2.3.1](#3_2_3_1_common_application_intents)/H-0-1] MUST have an
application that handles the [`ACTION_GET_CONTENT`](
https://developer.android.com/reference/android/content/Intent.html#ACTION_GET_CONTENT),
[`ACTION_OPEN_DOCUMENT`](
https://developer.android.com/reference/android/content/Intent#ACTION_OPEN_DOCUMENT),
[`ACTION_OPEN_DOCUMENT_TREE`](
https://developer.android.com/reference/android/content/Intent.html#ACTION_OPEN_DOCUMENT_TREE),
and [`ACTION_CREATE_DOCUMENT`](
https://developer.android.com/reference/android/content/Intent.html#ACTION_CREATE_DOCUMENT)
intents as described in the SDK documents, and provide the user affordance
to access the document provider data by using [`DocumentsProvider`](
https://developer.android.com/reference/android/provider/DocumentsProvider) API.
*   [[3.2.3.1](#3_2_3_1_common_application_intents)/H-0-2]*  MUST preload one
or more applications or service components with an intent handler, for
all the public intent filter patterns defined by the following application
intents listed [here](https://developer.android.com/about/versions/11/reference/common-intents-30).
*   [[3.2.3.1](#3_2_3_1_common_application_intents)/H-SR] Are STRONGLY
RECOMMENDED to preload an email application which can handle [ACTION_SENDTO](https://developer.android.com/reference/android/content/Intent#ACTION_SENDTO)
or [ACTION_SEND](https://developer.android.com/reference/android/content/Intent#ACTION_SEND)
or [ACTION_SEND_MULTIPLE](https://developer.android.com/reference/android/content/Intent#ACTION_SEND_MULTIPLE)
intents to send an email.
*   [[3.4](#3_4_web_compatibility).1/H-0-1] MUST provide a complete
implementation of the `android.webkit.Webview` API.
*   [[3.4](#3_4_web_compatibility).2/H-0-1] MUST include a standalone Browser
application for general user web browsing.
*   [[3.8](#3_8_user_interface_compatibility).1/H-SR] Are STRONGLY RECOMMENDED
to implement a default launcher that supports in-app pinning of shortcuts,
widgets and [widgetFeatures](
https://developer.android.com/reference/android/appwidget/AppWidgetProviderInfo.html#widgetFeatures).
*   [[3.8](#3_8_user_interface_compatibility).1/H-SR] Are STRONGLY RECOMMENDED
to implement a default launcher that provides quick access to the additional
shortcuts provided by third-party apps through the [ShortcutManager](
https://developer.android.com/reference/android/content/pm/ShortcutManager.html)
API.
*   [[3.8](#3_8_user_interface_compatibility).1/H-SR] Are STRONGLY RECOMMENDED
to include a default launcher app that shows badges for the app icons.
*   [[3.8](#3_8_user-interface_compatibility).2/H-SR] Are STRONGLY RECOMMENDED
to support third-party app widgets.
*   [[3.8](#3_8_user-interface_compatibility).3/H-0-1] MUST allow third-party
apps to notify users of notable events through the [`Notification`](
https://developer.android.com/reference/android/app/Notification.html) and
[`NotificationManager`](
https://developer.android.com/reference/android/app/NotificationManager.html)
API classes.
*   [[3.8](#3_8_user_interface_compatibility).3/H-0-2] MUST support rich
notifications.
*   [[3.8](#3_8_user_interface_compatibility).3/H-0-3] MUST support heads-up
notifications.
*   [[3.8](#3_8_user_interface_compatibility).3/H-0-4] MUST include a
notification shade, providing the user the ability to directly control (e.g.
reply, snooze, dismiss, block) the notifications through user affordance such as
action buttons or the control panel as implemented in the AOSP.
*   [[3.8](#3_8_user_interface_compatibility).3/H-0-5] MUST display the choices
provided through [`RemoteInput.Builder setChoices()`](
https://developer.android.com/reference/android/app/RemoteInput.Builder.html#setChoices%28java.lang.CharSequence[]%29)
in the notification shade.
*   [[3.8](#3_8_user_interface_compatibility).3/H-SR] Are STRONGLY RECOMMENDED
to display the first choice provided through [`RemoteInput.Builder setChoices()`](
https://developer.android.com/reference/android/app/RemoteInput.Builder.html#setChoices%28java.lang.CharSequence[]%29)
in the notification shade without additional user interaction.
*   [[3.8](#3_8_user_interface_compatibility).3/H-SR] Are STRONGLY RECOMMENDED
to display all the choices provided through [`RemoteInput.Builder setChoices()`](
https://developer.android.com/reference/android/app/RemoteInput.Builder.html#setChoices%28java.lang.CharSequence[]%29)
in the notification shade when the user expands all notifications in the
notification shade.
*   [[3.8](#3_8_user_interface_compatibility).3.1/H-SR] Are STRONGLY RECOMMENDED
    to display actions for which [`Notification.Action.Builder.setContextual`](https://developer.android.com/reference/android/app/Notification.Action.Builder.html#setContextual%28boolean%29)
    is set as `true` in-line with the replies displayed by
    [`Notification.Remoteinput.Builder.setChoices`](https://developer.android.com/reference/android/app/RemoteInput.Builder.html#setChoices%28java.lang.CharSequence[]%29).
*   [[3.8](#3_8_user_interface_compatibility).4/H-SR] Are STRONGLY RECOMMENDED
to implement an assistant on the device to handle the [Assist action](
http://developer.android.com/reference/android/content/Intent.html#ACTION_ASSIST).

If Handheld device implementations support Assist action, they:

*   [[3.8](#3_8_user_interface_compatibility).4/H-SR] Are STRONGLY RECOMMENDED
to use long press on `HOME` key as the designated interaction to launch the
assist app as described in [section 7.2.3](#7_2_3_navigation_keys). MUST launch
the user-selected assist app, in other words the app that implements
[`VoiceInteractionService`](
https://developer.android.com/reference/android/service/voice/VoiceInteractionService)
, or an activity handling the `ACTION_ASSIST` intent.

If Handheld device implementations support [`conversation notifications`](https://developer.android.com/preview/features/conversations#api-notifications)
and group them into a separate section from alerting and silent non-conversation
notifications, they:

*   [[3.8](#3_8_user_interface_compatibility).4/H-1-1]* MUST display
    conversation notifications ahead of non conversation notifications with
    the exception of ongoing foreground service notifications and
    [importance:high](https://developer.android.com/reference/android/app/NotificationManager#IMPORTANCE_HIGH)
    notifications.

If Android Handheld device implementations support a lock screen, they:

*   [[3.8](#3_8_user_interface_compatibility).10/H-1-1] MUST display the Lock
screen Notifications including the Media Notification Template.

If Handheld device implementations support a secure lock screen, they:

*   [[3.9](#3_9_device_administration)/H-1-1] MUST implement the full range of
[device administration](
http://developer.android.com/guide/topics/admin/device-admin.html)
policies defined in the Android SDK documentation.
*   [[3.9](#3_9_device_administration)/H-1-2]  MUST declare the support of
managed profiles via the `android.software.managed_users` feature flag, except when the device is configured so that it would [report](
http://developer.android.com/reference/android/app/ActivityManager.html#isLowRamDevice%28%29)
itself as a low RAM device or so that it allocates internal (non-removable)
storage as shared storage.

If Handheld device implementations include support for
[`ControlsProviderService`](https://developer.android.com/reference/android/service/controls/ControlsProviderService)
and [`Control`](https://developer.android.com/reference/android/service/controls/Control)
APIs and allow third-party applications to publish [`device controls`](
https://developer.android.com/preview/features/device-control), then they:

*   [[3.8](#3_8_user_interface_compatibility).16/H-1-1] MUST declare the feature
    flag [`android.software.controls`](https://developer.android.com/reference/android/content/pm/PackageManager#FEATURE_CONTROLS)
    and set it to `true`.
*   [[3.8](#3_8_user_interface_compatibility).16/H-1-2] MUST provide a user
    affordance with the ability to add, edit, select, and operate the user’s
    favorite device controls from the controls registered by the third-party
    applications through the [`ControlsProviderService`](https://developer.android.com/reference/android/service/controls/ControlsProviderService)
    and the [`Control`](https://developer.android.com/reference/android/service/controls/Control#getDeviceType%28%29)
    APIs.
*   [[3.8](#3_8_user_interface_compatibility).16/H-1-3] MUST provide access to
    this user affordance within three interactions from a default Launcher.
*   [[3.8](#3_8_user_interface_compatibility).16/H-1-4] MUST accurately render
    in this user affordance the name and icon of each third-party app that
    provides controls via the [`ControlsProviderService`](https://developer.android.com/reference/android/service/controls/ControlsProviderService)
    API as well as any specified fields provided by the [`Control`](https://developer.android.com/reference/android/service/controls/Control)
    APIs.

Conversely, If Handheld device implementations do not implement such controls,
they:

*   [[3.8](#3_8_user_interface_compatibility).16/H-2-1] MUST report `null` for
    the [`ControlsProviderService`](https://developer.android.com/reference/android/service/controls/ControlsProviderService)
    and the [`Control`](https://developer.android.com/reference/android/service/controls/Control)
    APIs.
*   [[3.8](#3_8_user_interface_compatibility).16/H-2-2] MUST declare the feature
    flag [`android.software.controls`](https://developer.android.com/reference/android/content/pm/PackageManager#FEATURE_CONTROLS)
    and set it to `false`.

Handheld device implementations:

*  [[3.10](#3_10_accessibility)/H-0-1] MUST support third-party accessibility
services.
*  [[3.10](#3_10_accessibility)/H-SR] Are STRONGLY RECOMMENDED to preload
accessibility services on the device comparable with or exceeding functionality
of the Switch Access and TalkBack (for languages supported by the preinstalled
Text-to-speech engine) accessibility services as provided in the [talkback open
source project](https://github.com/google/talkback).
*   [[3.11](#3_11_text_to_speech)/H-0-1] MUST support installation of
third-party TTS engines.
*   [[3.11](#3_11_text_to_speech)/H-SR] Are STRONGLY RECOMMENDED to include a
TTS engine supporting the languages available on the device.
*   [[3.13](#3_13_quick_settings)/H-SR] Are STRONGLY RECOMMENDED to include a
Quick Settings UI component.

If Android handheld device implementations declare `FEATURE_BLUETOOTH` or
`FEATURE_WIFI` support, they:

*   [[3.16](#3_16_companion_device_pairing)/H-1-1] MUST support the companion device pairing
feature.

If the navigation function is provided as an on-screen, gesture-based action:

*   [[7.2](#7_2_input_devices).3/H] The gesture recognition zone for the Home
    function SHOULD be no higher than 32 dp in height from the bottom of the
    screen.

If Handheld device implementations provide a navigation function as a gesture
from anywhere on the left and right edges of the screen:

*   [[7.2](#7_2_input_devices).3/H-0-1] The navigation function's gesture area
    MUST be less than 40 dp in width on each side. The gesture area SHOULD be
    24 dp in width by default.

### 2.2.4\. Performance and Power

*   [[8.1](#8_1_user_experience_consistency)/H-0-1] **Consistent frame latency**.
Inconsistent frame latency or a delay to render frames MUST NOT happen more
often than 5 frames in a second, and SHOULD be below 1 frames in a second.
*   [[8.1](#8_1_user_experience_consistency)/H-0-2] **User interface latency**.
Device implementations MUST ensure low latency user experience by scrolling a
list of 10K list entries as defined by the Android Compatibility Test Suite
(CTS) in less than 36 secs.
*   [[8.1](#8_1_user_experience_consistency)/H-0-3] **Task switching**. When
multiple applications have been launched, re-launching an already-running
application after it has been launched MUST take less than 1 second.

Handheld device implementations:

*   [[8.2](#8_2_file_io_access_performance)/H-0-1] MUST ensure a sequential
write performance of at least 5 MB/s.
*   [[8.2](#8_2_file_io_access_performance)/H-0-2] MUST ensure a random write
performance of at least 0.5 MB/s.
*   [[8.2](#8_2_file_io_access_performance)/H-0-3] MUST ensure a sequential read
performance of at least 15 MB/s.
*   [[8.2](#8_2_file_io_access_performance)/H-0-4] MUST ensure a random read
performance of at least 3.5 MB/s.

If Handheld device implementations include features to improve device power
management that are included in AOSP or extend the features that are included
in AOSP, they:

* [[8.3](#8_3_power_saving_modes)/H-1-1] MUST provide user affordance to enable
  and disable the battery saver feature.
* [[8.3](#8_3_power_saving_modes)/H-1-2] MUST provide user affordance to display
  all apps that are exempted from App Standby and Doze power-saving modes.

Handheld device implementations:

*    [[8.4](#8_4_power_consumption_accounting)/H-0-1] MUST provide a
per-component power profile that defines the [current consumption value](
http://source.android.com/devices/tech/power/values.html)
for each hardware component and the approximate battery drain caused by the
components over time as documented in the Android Open Source Project site.
*    [[8.4](#8_4_power_consumption_accounting)/H-0-2] MUST report all power
consumption values in milliampere hours (mAh).
*    [[8.4](#8_4_power_consumption_accounting)/H-0-3] MUST report CPU power
consumption per each process's UID. The Android Open Source Project meets the
requirement through the `uid_cputime` kernel module implementation.
*   [[8.4](#8_4_power_consumption_accounting)/H-0-4] MUST make this power usage
available via the [`adb shell dumpsys batterystats`](
http://source.android.com/devices/tech/power/batterystats.html)
shell command to the app developer.
*   [[8.4](#8_4_power_consumption_accounting)/H] SHOULD be attributed to the
hardware component itself if unable to attribute hardware component power usage
to an application.

If Handheld device implementations include a screen or video output, they:

*   [[8.4](#8_4_power_consumption_accounting)/H-1-1] MUST honor the
[`android.intent.action.POWER_USAGE_SUMMARY`](
http://developer.android.com/reference/android/content/Intent.html#ACTION_POWER_USAGE_SUMMARY)
intent and display a settings menu that shows this power usage.

### 2.2.5\. Security Model

Handheld device implementations:

*   [[9.1](#9_1_permissions)/H-0-1] MUST allow third-party apps to access the
usage statistics via the `android.permission.PACKAGE_USAGE_STATS` permission and
provide a user-accessible mechanism to grant or revoke access to such apps in
response to the [`android.settings.ACTION_USAGE_ACCESS_SETTINGS`](
https://developer.android.com/reference/android/provider/Settings.html#ACTION&lowbar;USAGE&lowbar;ACCESS&lowbar;SETTINGS)
intent.

Handheld device implementations (\* Not applicable for Tablet):

*    [[9.11](#9_11_permissions)/H-0-2]\* MUST back up the keystore implementation
     with an isolated execution environment.
*    [[9.11](#9_11_permissions)/H-0-3]\* MUST have implementations of RSA, AES,
     ECDSA, and HMAC cryptographic algorithms and MD5, SHA1, and SHA-2 family
     hash functions to properly support the Android Keystore system's supported
     algorithms in an area that is securely isolated from the code running on
     the kernel and above. Secure isolation MUST block all potential mechanisms
     by which kernel or userspace code might access the internal state of the
     isolated environment, including DMA. The upstream Android Open Source
     Project (AOSP) meets this requirement by using the [Trusty](
     https://source.android.com/security/trusty/) implementation, but another
     ARM TrustZone-based solution or a third-party reviewed secure
     implementation of a proper hypervisor-based isolation are alternative
     options.
*    [[9.11](#9_11_permissions)/H-0-4]\* MUST perform the lock screen
     authentication in the isolated execution environment and only when
     successful, allow the authentication-bound keys to be used. Lock screen
     credentials MUST be stored in a way that allows only the isolated execution
     environment to perform lock screen authentication. The upstream Android
     Open Source Project provides the
     [Gatekeeper Hardware Abstraction Layer (HAL)](
     http://source.android.com/devices/tech/security/authentication/gatekeeper.html)
     and Trusty, which can be used to satisfy this requirement.
*    [[9.11](#9_11_permissions)/H-0-5]\* MUST support key attestation where the
     attestation signing key is protected by secure hardware and signing is
     performed in secure hardware. The attestation signing keys MUST be shared
     across large enough number of devices to prevent the keys from being used
     as device identifiers. One way of meeting this requirement is to share the
     same attestation key unless at least 100,000 units of a given SKU are
     produced. If more than 100,000 units of an SKU are produced, a different
     key MAY be used for each 100,000 units.

Note that if a device implementation is already launched on an earlier Android
version, such a device is exempted from the requirement to have a keystore
backed by an isolated execution environment and support the key attestation,
unless it declares the `android.hardware.fingerprint` feature which requires a
keystore backed by an isolated execution environment.

When Handheld device implementations support a secure lock screen, they:

*   [[9.11](#9_11_permissions)/H-1-1] MUST allow the user to choose the shortest
    sleep timeout, that is a transition time from the unlocked to the locked
    state, as 15 seconds or less.
*   [[9.11](#9_11_permissions)/H-1-2] MUST provide user affordance to hide
    notifications and disable all forms of authentication except for the
    primary authentication described in
    [9.11.1 Secure Lock Screen](#9_11_1_secure-lock-screen). The AOSP meets the
    requirement as lockdown mode.

### 2.2.6\. Developer Tools and Options Compatibility

Handheld device implementations (\* Not applicable for Tablet):

*   [[6.1](#6_1_developer_tools)/H-0-1]\* MUST support the shell command
    `cmd testharness`.

Handheld device implementations  (\* Not applicable for Tablet):

*    [**Perfetto**](https://developer.android.com/studio/command-line/perfetto)
    *   [[6.1](#6_1_developer_tools)/H-0-2]\* MUST expose a `/system/bin/perfetto`
        binary to the shell user which cmdline complies with
        [the perfetto documentation](
        https://developer.android.com/studio/command-line/perfetto).
    *   [[6.1](#6_1_developer_tools)/H-0-3]\* The perfetto binary MUST accept as
        input a protobuf config that complies with the schema defined in
        [the perfetto documentation](
        https://developer.android.com/studio/command-line/perfetto).
    *   [[6.1](#6_1_developer_tools)/H-0-4]\* The perfetto binary MUST write as
        output a protobuf trace that complies with the schema defined in
        [the perfetto documentation](
        https://developer.android.com/studio/command-line/perfetto).
    *   [[6.1](#6_1_developer_tools)/H-0-5]\* MUST provide, through the perfetto
        binary, at least the data sources described  in [the perfetto documentation](
        https://developer.android.com/studio/command-line/perfetto).
    *   [[6.1](#6_1_developer_tools)/H-0-6]\* The perfetto traced daemon MUST be
        enabled by default (system property `persist.traced.enable`).
