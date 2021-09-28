## 2.4\. Watch Requirements

An **Android Watch device** refers to an Android device implementation intended to
be worn on the body, perhaps on the wrist.

Android device implementations are classified as a Watch if they meet all the
following criteria:

*   Have a screen with the physical diagonal length in the range from 1.1 to 2.5
    inches.
*   Have a mechanism provided to be worn on the body.

The additional requirements in the rest of this section are specific to Android
Watch device implementations.

### 2.4.1\. Hardware

Watch device implementations:

*   [[7.1](#7_1_display_and_graphics).1.1/W-0-1] MUST have a screen with the
physical diagonal size in the range from 1.1 to 2.5 inches.

*   [[7.2](#7_2_input_devices).3/W-0-1] MUST have the Home function available
to the user, and the Back function except for when it is in `UI_MODE_TYPE_WATCH`.

*   [[7.2](#7_2_input_devices).4/W-0-1] MUST support touchscreen input.

*   [[7.3](#7_3_sensors).1/W-SR] Are STRONGLY RECOMMENDED to include a 3-axis
accelerometer.

If Watch device implementations include a GPS/GNSS receiver and report the
capability to applications through the `android.hardware.location.gps` feature
flag, they:

*  [[7.3](#7_3_sensors).3/W-1-1] MUST report GNSS measurements, as soon as they
are found, even if a location calculated from GPS/GNSS is not yet reported.
*  [[7.3](#7_3_sensors).3/W-1-2] MUST report GNSS pseudoranges and pseudorange
rates, that, in open-sky conditions after determining the location, while
stationary or moving with less than 0.2 meter per second squared of
acceleration, are sufficient to calculate position within 20 meters, and speed
within 0.2 meters per second, at least 95% of the time.

If Watch device implementations include a 3-axis gyroscope, they:

*   [[7.3](#7_3_sensors).4/W-2-1] MUST be capable of measuring orientation changes
up to 1000 degrees per second.

Watch device implementations:

*   [[7.4](#7_4_data_connectivity).3/W-0-1] MUST support Bluetooth.

*   [[7.6](#7_6_memory_and_storage).1/W-0-1] MUST have at least 1 GB of
non-volatile storage available for application private data (a.k.a. "/data" partition).
*   [[7.6](#7_6_memory_and_storage).1/W-0-2] MUST have at least 416 MB memory
available to the kernel and userspace.

*   [[7.8](#7_8_audio).1/W-0-1] MUST include a microphone.

*   [[7.8](#7_8_audio).2/W] MAY have audio output.

### 2.4.2\. Multimedia

No additional requirements.

### 2.4.3\. Software

Watch device implementations:

*   [[3](#3_0_intro)/W-0-1] MUST declare the feature
`android.hardware.type.watch`.
*   [[3](#3_0_intro)/W-0-2] MUST support uiMode =
    [UI_MODE_TYPE_WATCH](
    http://developer.android.com/reference/android/content/res/Configuration.html#UI_MODE_TYPE_WATCH).
*   [[3.2.3.1](#3_2_3_1_common_application_intents)/W-0-1]  MUST preload one
or more applications or service components with an intent handler, for
all the public intent filter patterns defined by the following application
intents listed [here](https://developer.android.com/about/versions/11/reference/common-intents-30).

Watch device implementations:

*   [[3.8](#3_8_user_interface_compatibility).4/W-SR] Are STRONGLY RECOMMENDED
to implement an assistant on the device to handle the [Assist action](
http://developer.android.com/reference/android/content/Intent.html#ACTION_ASSIST).

Watch device implementations that declare the `android.hardware.audio.output`
feature flag:

*   [[3.10](#3_10_accessibility)/W-1-1] MUST support third-party accessibility
services.
*   [[3.10](#3_10_accessibility)/W-SR] Are STRONGLY RECOMMENDED to preload
accessibility services on the device comparable with or exceeding functionality
of the Switch Access and TalkBack (for languages supported by the preinstalled
Text-to-speech engine) accessibility services as provided in the
[talkback open source project]( https://github.com/google/talkback).

If Watch device implementations report the feature android.hardware.audio.output,
they:

*   [[3.11](#3_11_text_to_speech)/W-SR] Are STRONGLY RECOMMENDED to include a
TTS engine supporting the languages available on the device.

*   [[3.11](#3_11_text_to_speech)/W-0-1] MUST support installation of
third-party TTS engines.

### 2.4.4\. Performance and Power

If Watch device implementations include features to improve device power
management that are included in AOSP or extend the features that are included
in AOSP, they:

*   [[8.3](#8_3_power_saving_modes)/W-SR] Are STRONGLY RECOMMENDED to provide
    user affordance to display all apps that are exempted from App Standby and
    Doze power-saving modes.
*   [[8.3](#8_3_power_saving_modes)/W-SR] Are STRONGLY RECOMMENDED to provide
    user affordance to enable and disable the battery saver feature.

Watch device implementations:

*    [[8.4](#8_4_power_consumption_accounting)/W-0-1] MUST provide a
per-component power profile that defines the [current consumption value](
http://source.android.com/devices/tech/power/values.html)
for each hardware component and the approximate battery drain caused by the
components over time as documented in the Android Open Source Project site.
*    [[8.4](#8_4_power_consumption_accounting)/W-0-2] MUST report all power
consumption values in milliampere hours (mAh).
*    [[8.4](#8_4_power_consumption_accounting)/W-0-3] MUST report CPU power
consumption per each process's UID. The Android Open Source Project meets the
requirement through the `uid_cputime` kernel module implementation.
*   [[8.4](#8_4_power_consumption_accounting)/W-0-4] MUST make this power usage
available via the [`adb shell dumpsys batterystats`](
http://source.android.com/devices/tech/power/batterystats.html)
shell command to the app developer.
*   [[8.4](#8_4_power_consumption_accounting)/W] SHOULD be attributed to the
hardware component itself if unable to attribute hardware component power usage
to an application.
