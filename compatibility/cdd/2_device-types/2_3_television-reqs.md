## 2.3\. Television Requirements

An **Android Television device** refers to an Android device implementation that
is an entertainment interface for consuming digital media, movies, games, apps,
and/or live TV for users sitting about ten feet away (a “lean back” or “10-foot
user interface”).

Android device implementations are classified as a Television if they meet all
the following criteria:

*   Have provided a mechanism to remotely control the rendered user interface on
    the display that might sit ten feet away from the user.
*   Have an embedded screen display with the diagonal length larger than 24
    inches OR include a video output port, such as VGA, HDMI, DisplayPort, or a
    wireless port for display.

The additional requirements in the rest of this section are specific to Android
Television device implementations.

### 2.3.1\. Hardware

Television device implementations:

*   [[7.2](#7_2_input_devices).2/T-0-1] MUST support [D-pad](
https://developer.android.com/reference/android/content/res/Configuration.html#NAVIGATION_DPAD).
*   [[7.2](#7_2_input_devices).3/T-0-1] MUST provide the Home and Back
functions.
*   [[7.2](#7_2_input_devices).3/T-0-2] MUST send both the normal and long press
event of the Back function ([`KEYCODE_BACK`](
http://developer.android.com/reference/android/view/KeyEvent.html#KEYCODE_BACK))
to the foreground application.
*   [[7.2](#7_2_input_devices).6.1/T-0-1] MUST include support for game
controllers and declare the `android.hardware.gamepad` feature flag.
*   [[7.2](#7_2_input_devices).7/T] SHOULD provide a remote control from which
users can access [non-touch navigation](#7_2_2_non-touch_navigation) and
[core navigation keys](#7_2_3_navigation_keys) inputs.


If Television device implementations include a 3-axis gyroscope, they:

*   [[7.3](#7_3_sensors).4/T-1-1] MUST be able to report events up to a
frequency of at least 100 Hz.
*   [[7.3](#7_3_sensors).4/T-1-2] MUST be capable of measuring orientation changes
up to 1000 degrees per second.


Television device implementations:

*   [[7.4](#7_4_data_connectivity).3/T-0-1] MUST support Bluetooth and
Bluetooth LE.
*   [[7.6](#7_6_memory_and_storage).1/T-0-1] MUST have at least 4 GB of
non-volatile storage available for application private data
(a.k.a. "/data" partition).

If Television device implementations include a USB port that supports host mode,
they:

*   [[7.5](#7_5_camera).3/T-1-1] MUST include support for an external camera
that connects through this USB port but is not necessarily always connected.

If TV device implementations are 32-bit:

*   [[7.6](#7_6_memory_and_storage).1/T-1-1] The memory available to the kernel
and userspace MUST be at least 896MB if any of the following densities are used:

    *   400dpi or higher on small/normal screens
    *   xhdpi or higher on large screens
    *   tvdpi or higher on extra large screens

If TV device implementations are 64-bit:

*   [[7.6](#7_6_memory_and_storage).1/T-2-1] The memory available to the kernel
and userspace MUST be at least 1280MB if any of the following densities are
used:

    *   400dpi or higher on small/normal screens
    *   xhdpi or higher on large screens
    *   tvdpi or higher on extra large screens

Note that the "memory available to the kernel and userspace" above refers to the
memory space provided in addition to any memory already
dedicated to hardware components such as radio, video, and so on that are not
under the kernel’s control on device implementations.

Television device implementations:

*   [[7.8](#7_8_audio).1/T] SHOULD include a microphone.
*   [[7.8](#7_8_audio).2/T-0-1] MUST have an audio output and declare
    `android.hardware.audio.output`.

### 2.3.2\. Multimedia

Television device implementations MUST support the following audio encoding
and decoding formats and make them available to third-party applications:

*    [[5.1](#5_1_media_codecs)/T-0-1] MPEG-4 AAC Profile (AAC LC)
*    [[5.1](#5_1_media_codecs)/T-0-2] MPEG-4 HE AAC Profile (AAC+)
*    [[5.1](#5_1_media_codecs)/T-0-3] AAC ELD (enhanced low delay AAC)


Television device implementations MUST support the following video encoding
formats and make them available to third-party applications:

*    [[5.2](#5_2_video_encoding)/T-0-1] H.264
*    [[5.2](#5_2_video_encoding)/T-0-2] VP8

Television device implementations:

*   [[5.2](#5_2_video_encoding).2/T-SR] Are STRONGLY RECOMMENDED to support
H.264 encoding of 720p and 1080p resolution videos at 30 frames per second.

Television device implementations MUST support the following video decoding
formats and make them available to third-party applications:

*    [[5.3.3](#5_3_video_decoding)/T-0-1] MPEG-4 SP
*    [[5.3.4](#5_3_video_decoding)/T-0-2] H.264 AVC
*    [[5.3.5](#5_3_video_decoding)/T-0-3] H.265 HEVC
*    [[5.3.6](#5_3_video_decoding)/T-0-4] VP8
*    [[5.3.7](#5_3_video_decoding)/T-0-5] VP9
*    [[5.3.1](#5_3_video_decoding)/T-0-6] MPEG-2

Television device implementations MUST support MPEG-2 decoding, as detailed in
Section 5.3.1, at standard video frame rates and resolutions up to and
including:

*   [[5.3.1](#5_3_video_decoding)/T-1-1] HD 1080p at 59.94 frames per second
with Main Profile High Level.
*   [[5.3.1](#5_3_video_decoding)/T-1-2] HD 1080i at 59.94 frames per second
with Main Profile High Level. They MUST deinterlace interlaced MPEG-2 video to
its progressive equivalent (e.g. from 1080i at 59.94 frames per second to 1080p
at 29.97 frames per second) and make it available to third-party applications.

Television device implementations MUST support H.264 decoding, as detailed in
Section 5.3.4, at standard video frame rates and resolutions up to and
including:

*   [[5.3.4](#5_3_video_decoding)/T-1-1] HD 1080p at 60 frames per second with
    Baseline Profile
*   [[5.3.4](#5_3_video_decoding)/T-1-2] HD 1080p at 60 frames per second with
    Main Profile
*   [[5.3.4](#5_3_video_decoding)/T-1-3] HD 1080p at 60 frames per second with
    High Profile Level 4.2

Television device implementations  with H.265 hardware decoders MUST support
H.265 decoding, as detailed in Section 5.3.5, at standard video frame rates
and resolutions up to and including:

*   [[5.3.5](#5_3_video_decoding)/T-1-1] HD 1080p at 60 frames per second with
    Main Profile Level 4.1

If Television device implementations with H.265 hardware decoders support
H.265 decoding and the UHD decoding profile, they:

*   [[5.3.5](#5_3_video_decoding)/T-2-1] MUST support UHD 3480p at 60 frames per
    second with Main10 Level 5 Main Tier profile

Television device implementations MUST support VP8 decoding, as detailed in
Section 5.3.6, at standard video frame rates and resolutions up to and
including:

*   [[5.3.6](#5_3_video_decoding)/T-1-1] HD 1080p at 60 frames per second decoding profile

Television device implementations  with VP9 hardware decoders MUST support VP9
decoding, as detailed in Section 5.3.7, at standard video frame rates and
resolutions up to and including:

*   [[5.3.7](#5_3_video_decoding)/T-1-1] HD 1080p at 60 frames per second with
    profile 0 (8 bit color depth)

If Television device implementations with VP9 hardware decoders support VP9
decoding and the UHD decoding profile, they:

*   [[5.3.7](#5_3_video_decoding)/T-2-1] MUST support UHD 3480p at 60 frames per
    second with profile 0 (8 bit color depth).
*   [[5.3.7](#5_3_video_decoding)/T-2-1] Are STRONGLY RECOMMENDED to support UHD
    3480p at 60 frames per second with profile 2 (10 bit color depth).

Television device implementations:

*   [[5.5](#5_5_audio_playback)/T-0-1] MUST include support for system Master
Volume and digital audio output volume attenuation on supported outputs,
except for compressed audio passthrough output (where no audio decoding is done
on the device).

If Television device implementations do not have a built in display,
but instead support an external display connected via HDMI, they:

*    [[5.8](#5_8_secure_media)/T-0-1] MUST set the HDMI output mode to
select the maximum resolution that can be supported with either a 50Hz or 60Hz
refresh rate.
*    [[5.8](#5_8_secure_media)/T-SR] Are STRONGLY RECOMMENDED to provide a user
configurable HDMI refresh rate selector.
*    [[5.8](#5_8_secure_media)] SHOULD set the HDMI output mode refresh rate
to either 50Hz or 60Hz, depending on the video refresh rate for the region the
device is sold in.

If Television device implementations do not have a built in display,
but instead support an external display connected via HDMI, they:

*    [[5.8](#5_8_secure_media)/T-1-1] MUST support HDCP 2.2.

If Television device implementations do not support UHD decoding, but
instead support an external display connected via HDMI, they:

*    [[5.8](#5_8_secure_media)/T-2-1] MUST support HDCP 1.4


### 2.3.3\. Software

Television device implementations:

*    [[3](#3_0_intro)/T-0-1] MUST declare the features
[`android.software.leanback`](
http://developer.android.com/reference/android/content/pm/PackageManager.html#FEATURE_LEANBACK)
and `android.hardware.type.television`.
*   [[3.2.3.1](#3_2_3_1_common_application_intents)/T-0-1]  MUST preload one or
more applications or service components with an intent handler, for all the
public intent filter patterns defined by the following application intents
listed [here](https://developer.android.com/about/versions/11/reference/common-intents-30).
*   [[3.4](#3_4_web_compatibility).1/T-0-1] MUST provide a complete
implementation of the `android.webkit.Webview` API.

If Android Television device implementations support a lock screen,they:

*   [[3.8](#3_8_user_interface_compatibility).10/T-1-1] MUST display the Lock
screen Notifications including the Media Notification Template.

Television device implementations:

*   [[3.8](#3_8_user_interface_compatibility).14/T-SR] Are STRONGLY RECOMMENDED
to support picture-in-picture (PIP) mode multi-window.
*   [[3.10](#3_10_accessibility)/T-0-1] MUST support third-party accessibility
services.
*   [[3.10](#3_10_accessibility)/T-SR] Are STRONGLY RECOMMENDED to
    preload accessibility services on the device comparable with or exceeding
    functionality of the Switch Access and TalkBack (for languages supported by
    the preinstalled Text-to-speech engine) accessibility services as provided in
    the [talkback open source project](https://github.com/google/talkback).


If Television device implementations report the feature
`android.hardware.audio.output`, they:

*   [[3.11](#3_11_text_to_speech)/T-SR] Are STRONGLY RECOMMENDED to include a
TTS engine supporting the languages available on the device.
*   [[3.11](#3_11_text_to_speech)/T-1-1] MUST support installation of
third-party TTS engines.


Television device implementations:

*    [[3.12](#3_12_tv_input_framework)/T-0-1] MUST support TV Input Framework.


### 2.3.4\. Performance and Power

*   [[8.1](#8_1_user_experience_consistency)/T-0-1] **Consistent frame latency**.
   Inconsistent frame latency or a delay to render frames MUST NOT happen more
   often than 5 frames in a second, and SHOULD be below 1 frames in a second.
*   [[8.2](#8_2_file_io_access_performance)/T-0-1] MUST ensure a sequential
   write performance of at least 5MB/s.
*   [[8.2](#8_2_file_io_access_performance)/T-0-2] MUST ensure a random write
   performance of at least 0.5MB/s.
*   [[8.2](#8_2_file_io_access_performance)/T-0-3] MUST ensure a sequential
   read performance of at least 15MB/s.
*   [[8.2](#8_2_file_io_access_performance)/T-0-4] MUST ensure a random read
   performance of at least 3.5MB/s.

If Television device implementations include features to improve device power
management that are included in AOSP or extend the features that are included
in AOSP, they:

* [[8.3](#8_3_power_saving_modes)/T-1-1] MUST provide user affordance to enable
  and disable the battery saver feature.

If Television device implementations do not have a battery they:

* [[8.3](#8_3_power_saving_modes)/T-1-2] MUST register the device as
a batteryless device as described in [Supporting Batteryless Devices](
https://source.android.com/devices/tech/power/batteryless).

If Television device implementations have a battery they:

* [[8.3](#8_3_power_saving_modes)/T-1-3] MUST provide user affordance to display
  all apps that are exempted from App Standby and Doze power-saving modes.

Television device implementations:

*    [[8.4](#8_4_power_consumption_accounting)/T-0-1] MUST provide a
per-component power profile that defines the [current consumption value](
http://source.android.com/devices/tech/power/values.html)
for each hardware component and the approximate battery drain caused by the
components over time as documented in the Android Open Source Project site.
*    [[8.4](#8_4_power_consumption_accounting)/T-0-2] MUST report all power
consumption values in milliampere hours (mAh).
*    [[8.4](#8_4_power_consumption_accounting)/T-0-3] MUST report CPU power
consumption per each process's UID. The Android Open Source Project meets the
requirement through the `uid_cputime` kernel module implementation.
*    [[8.4](#8_4_power_consumption_accounting)/T] SHOULD be attributed to the
hardware component itself if unable to attribute hardware component power usage
to an application.
*   [[8.4](#8_4_power_consumption_accounting)/T-0-4] MUST make this power usage
available via the [`adb shell dumpsys batterystats`](
http://source.android.com/devices/tech/power/batterystats.html)
shell command to the app developer.

### 2.3.5\. Security Model

Television device implementations:

*    [[9.11](#9_11_permissions)/T-0-1] MUST back up the keystore implementation
     with an isolated execution environment.
*    [[9.11](#9_11_permissions)/T-0-2] MUST have implementations of RSA, AES,
     ECDSA and HMAC cryptographic algorithms and MD5, SHA1, and SHA-2 family
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
*    [[9.11](#9_11_permissions)/T-0-3] MUST perform the lock screen
     authentication in the isolated execution environment and only when
     successful, allow the authentication-bound keys to be used. Lock screen
     credentials MUST be stored in a way that allows only the isolated execution
     environment to perform lock screen authentication. The upstream Android
     Open Source Project provides the
     [Gatekeeper Hardware Abstraction Layer (HAL)](
     http://source.android.com/devices/tech/security/authentication/gatekeeper.html)
     and Trusty, which can be used to satisfy this requirement.
*    [[9.11](#9_11_permissions)/T-0-4] MUST support key attestation where the
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

If Television device implementations support a secure lock screen, they:

*    [[9.11](#9_11_permissions)/T-1-1] MUST allow the user to choose the Sleep
     timeout for transition from the unlocked to the locked state, with a
     minimum allowable timeout up to 15 seconds or less.

### 2.3.6\. Developer Tools and Options Compatibility

Television device implementations:

*    [**Perfetto**](https://developer.android.com/studio/command-line/perfetto)
    *   [[6.1](#6_1_developer_tools)/T-0-1] MUST expose a `/system/bin/perfetto`
        binary to the shell user which cmdline complies with
        [the perfetto documentation](
        https://developer.android.com/studio/command-line/perfetto).
    *   [[6.1](#6_1_developer_tools)/T-0-2] The perfetto binary MUST accept as
        input a protobuf config that complies with the schema defined in
        [the perfetto documentation](
        https://developer.android.com/studio/command-line/perfetto).
    *   [[6.1](#6_1_developer_tools)/T-0-3] The perfetto binary MUST write as
        output a protobuf trace that complies with the schema defined in
        [the perfetto documentation](
        https://developer.android.com/studio/command-line/perfetto).
    *   [[6.1](#6_1_developer_tools)/T-0-4] MUST provide, through the perfetto
        binary, at least the data sources described  in [the perfetto documentation](
        https://developer.android.com/studio/command-line/perfetto).
