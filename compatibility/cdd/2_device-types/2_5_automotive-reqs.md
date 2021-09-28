## 2.5\. Automotive Requirements

**Android Automotive implementation** refers to a vehicle head unit running
Android as an operating system for part or all of the system and/or
infotainment functionality.

Android device implementations are classified as an Automotive if they declare
the feature `android.hardware.type.automotive` or meet all the following
criteria.

*   Are embedded as part of, or pluggable to, an automotive vehicle.
*   Are using a screen in the driver's seat row as the primary display.

The additional requirements in the rest of this section are specific to Android
Automotive device implementations.

### 2.5.1\. Hardware

Automotive device implementations:

*   [[7.1](#7_1_display_and-graphics).1.1/A-0-1] MUST have a screen at least 6
inches in physical diagonal size.
*   [[7.1](#7_1_display_and_graphics).1.1/A-0-2] MUST have a screen size layout
of at least 750 dp x 480 dp.

*   [[7.2](#7_2_input_devices).3/A-0-1] MUST provide the Home function and MAY
provide Back and Recent functions.
*   [[7.2](#7_2_input_devices).3/A-0-2] MUST send both the normal and long press
event of the Back function ([`KEYCODE_BACK`](
http://developer.android.com/reference/android/view/KeyEvent.html#KEYCODE_BACK))
to the foreground application.
*   [[7.3](#7_3_sensors)/A-0-1] MUST implement and report
[`GEAR_SELECTION`](https://developer.android.com/reference/android/car/VehiclePropertyIds.html#GEAR_SELECTION),
[`NIGHT_MODE`](https://developer.android.com/reference/android/car/VehiclePropertyIds.html#NIGHT_MODE),
[`PERF_VEHICLE_SPEED`](https://developer.android.com/reference/android/car/VehiclePropertyIds.html#PERF_VEHICLE_SPEED)
and [`PARKING_BRAKE_ON`](https://developer.android.com/reference/android/car/VehiclePropertyIds.html#PARKING_BRAKE_ON).
*    [[7.3](#7_3_sensors)/A-0-2] The value of the
[`NIGHT_MODE`](https://developer.android.com/reference/android/car/VehiclePropertyIds.html#NIGHT_MODE)
flag MUST be consistent with dashboard day/night mode and SHOULD be based on
ambient light sensor input. The underlying ambient light sensor MAY be the same
as [Photometer](#7_3_7_photometer).
*   [[7.3](#7_3_sensors)/A-0-3] MUST provide sensor additional info field
[`TYPE_SENSOR_PLACEMENT`](https://developer.android.com/reference/android/hardware/SensorAdditionalInfo.html#TYPE_SENSOR_PLACEMENT)
as part of SensorAdditionalInfo for every sensor provided.
*   [[7.3](#7_3_sensors)/A-0-1] MAY dead reckon [Location](https://developer.android.com/reference/android/location/Location)
by fusing GPS/GNSS with additional sensors. If [Location](https://developer.android.com/reference/android/location/Location)
is dead reckoned, it is STRONGLY RECOMMENDED to implement and report the
corresponding [Sensor](https://developer.android.com/reference/android/hardware/Sensor)
types and/or [Vehicle Property IDs](https://developer.android.com/reference/android/car/VehiclePropertyIds)
used.
*   [[7.3](#7_3_sensors)/A-0-2] The [Location](https://developer.android.com/reference/android/location/Location)
requested via [LocationManager#requestLocationUpdates()](https://developer.android.com/reference/android/location/LocationManager)
MUST NOT be map matched.

If Automotive device implementations include a 3-axis accelerometer, they:

*   [[7.3](#7_3_sensors).1/A-1-1] MUST be able to report events up to a
frequency of at least 100 Hz.
*   [[7.3](#7_3_sensors).1/A-1-2] MUST comply with the Android
[car sensor coordinate system](
http://source.android.com/devices/sensors/sensor-types.html#auto_axes).

If Automotive device implementations include a 3-axis gyroscope, they:

*   [[7.3](#7_3_sensors).4/A-2-1] MUST be able to report events up to a
frequency of at least 100 Hz.
*   [[7.3](#7_3_sensors).4/A-2-2] MUST also implement the
[`TYPE_GYROSCOPE_UNCALIBRATED`](https://developer.android.com/reference/android/hardware/Sensor.html#TYPE_GYROSCOPE_UNCALIBRATED)
sensor.
*   [[7.3](#7_3_sensors).4/A-2-3] MUST be capable of measuring orientation changes
up to 250 degrees per second.
*   [[7.3](#7_3_sensors).4/A-SR] Are STRONGLY RECOMMENDED to configure the
gyroscope’s measurement range to +/-250dps in order to maximize the resolution
possible

If Automotive device implementations include a GPS/GNSS receiver, but do not
include cellular network-based data connectivity, they:

*    [[7.3](#7_3_sensors).3/A-3-1] MUST determine location the very first time
     the GPS/GNSS receiver is turned on or after 4+ days within 60 seconds.
*    [[7.3](#7_3_sensors).3/A-3-2] MUST meet the time-to-first-fix criteria as
     described in [7.3.3/C-1-2](#7_3_3_gps) and [7.3.3/C-1-6](#7_3_3_gps)
     for all other location requests (i.e requests which are not the first time
     ever or after 4+ days). The requirement [7.3.3/C-1-2](#7_3_3_gps) is
     typically met in vehicles without cellular network-based data connectivity,
     by using GNSS orbit predictions calculated on the receiver, or using the
     last known vehicle location along with the ability to dead reckon for at
     least 60 seconds with a position accuracy satisfying
     [7.3.3/C-1-3](#7_3_3_gps), or a combination of both.

Automotive device implementations:

*    [[7.4](#7_4_data_connectivity).3/A-0-1] MUST support Bluetooth and SHOULD
support Bluetooth LE.
*    [[7.4](#7_4_data_connectivity).3/A-0-2] Android Automotive implementations
MUST support the following Bluetooth profiles:
     * Phone calling over Hands-Free Profile (HFP).
     * Media playback over Audio Distribution Profile (A2DP).
     * Media playback control over Remote Control Profile (AVRCP).
     * Contact sharing using the Phone Book Access Profile (PBAP).
*    [[7.4](#7_4_data_connectivity).3/A-SR] Are STRONGLY RECOMMENDED to support
Message Access Profile (MAP).

*   [[7.4](#7_4_data_connectivity).5/A] SHOULD include support for cellular
network-based data connectivity.
*   [[7.4](#7_4_data_connectivity).5/A] MAY use the System API
`NetworkCapabilities#NET_CAPABILITY_OEM_PAID` constant for
networks that should be available to system apps.

An exterior view camera is a camera that images scenes outside of the device
implementation, like a dashcam.

Automotive device implementations:

*   SHOULD include one or more exterior view cameras.

If Automotive device implementations include an exterior view camera, for such
a camera, they:

*   [[7.5](#7_5_cameras)/A-1-1] MUST NOT have exterior view cameras accessible
via the [Android Camera APIs](
https://developer.android.com/guide/topics/media/camera), unless they comply
with camera [core requirements](#7_5_cameras).
*   [[7.5](#7_5_cameras)/A-SR] Are STRONGLY RECOMMENDED not to rotate or
horizontally mirror the camera preview.
*   [[7.5](#7_5_cameras).5/A-SR] Are STRONGLY RECOMMENDED to be oriented so that
the long dimension of the camera aligns with the horizon.
*   [[7.5](#7_5_cameras)/A-SR] Are STRONGLY RECOMMENDED to have a resolution
of at least 1.3 megapixels.
*   SHOULD have either fixed-focus or EDOF (extended depth of field) hardware.
*   SHOULD support [Android Synchronization Framework](
https://source.android.com/devices/graphics/sync).
*   MAY have either hardware auto-focus or software auto-focus implemented in
the camera driver.

Automotive device implementations:

*   [[7.6](#7_6_memory_and_storage).1/A-0-1] MUST have at least 4 GB of
non-volatile storage available for application private data
(a.k.a. "/data" partition).

*   [[7.6](#7_6_memory_and_storage).1/A] SHOULD format the data partition
to offer improved performance and longevity on flash storage, for example
using `f2fs` file-system.

If Automotive device implementations provide shared external storage via a
portion of the internal non-removable storage, they:

*   [[7.6](#7_6_memory_and_storage).1/A-SR] Are STRONGLY RECOMMENDED to reduce
I/O overhead on operations performed on the external storage, for example by
using `SDCardFS`.

If Automotive device implementations are 32-bit:

*   [[7.6](#7_6_memory_and_storage).1/A-1-1] The memory available to the kernel
and userspace MUST be at least 512MB if any of the following densities are used:
     *    280dpi or lower on small/normal screens
     *    ldpi or lower on extra large screens
     *    mdpi or lower on large screens

*   [[7.6](#7_6_memory_and_storage).1/A-1-2] The memory available to the kernel
and userspace MUST be at least 608MB if any of the following densities are used:
     *   xhdpi or higher on small/normal screens
     *   hdpi or higher on large screens
     *   mdpi or higher on extra large screens

*   [[7.6](#7_6_memory_and_storage).1/A-1-3] The memory available to the kernel
and userspace MUST be at least 896MB if any of the following densities are used:
     *   400dpi or higher on small/normal screens
     *   xhdpi or higher on large screens
     *   tvdpi or higher on extra large screens

*   [[7.6](#7_6_memory_and_storage).1/A-1-4] The memory available to the kernel
and userspace MUST be at least 1344MB if any of the following densities are
used:
     *   560dpi or higher on small/normal screens
     *   400dpi or higher on large screens
     *   xhdpi or higher on extra large screens

If Automotive device implementations are 64-bit:

*   [[7.6](#7_6_memory_and_storage).1/A-2-1] The memory available to the kernel
and userspace MUST be at least 816MB if any of the following densities are used:
     *   280dpi or lower on small/normal screens
     *   ldpi or lower on extra large screens
     *   mdpi or lower on large screens

*   [[7.6](#7_6_memory_and_storage).1/A-2-2] The memory available to the kernel
and userspace MUST be at least 944MB if any of the following densities are used:
     *   xhdpi or higher on small/normal screens
     *   hdpi or higher on large screens
     *   mdpi or higher on extra large screens

*   [[7.6](#7_6_memory_and_storage).1/A-2-3] The memory available to the kernel
and userspace MUST be at least 1280MB if any of the following densities are used:
     *  400dpi or higher on small/normal screens
     *  xhdpi or higher on large screens
     *  tvdpi or higher on extra large screens

*   [[7.6](#7_6_memory_and_storage).1/A-2-4] The memory available to the kernel
and userspace MUST be at least 1824MB if any of the following densities are used:
     *   560dpi or higher on small/normal screens
     *   400dpi or higher on large screens
     *   xhdpi or higher on extra large screens

Note that the "memory available to the kernel and userspace" above refers to the
memory space provided in addition to any memory already dedicated to hardware
components such as radio, video, and so on that are not under the kernel’s
control on device implementations.

Automotive device implementations:

*   [[7.7](#7_7_usb).1/A] SHOULD include a USB port supporting peripheral mode.

Automotive device implementations:

*   [[7.8](#7_8_audio).1/A-0-1] MUST include a microphone.

Automotive device implementations:

*   [[7.8](#7_8_audio).2/A-0-1] MUST have an audio output and declare
    `android.hardware.audio.output`.

### 2.5.2\. Multimedia

Automotive device implementations MUST support the following audio encoding
and decoding formats and make them available to third-party applications:

*    [[5.1](#5_1_media_codecs)/A-0-1] MPEG-4 AAC Profile (AAC LC)
*    [[5.1](#5_1_media_codecs)/A-0-2] MPEG-4 HE AAC Profile (AAC+)
*    [[5.1](#5_1_media_codecs)/A-0-3] AAC ELD (enhanced low delay AAC)

Automotive device implementations MUST support the following video encoding
formats and make them available to third-party applications:

*    [[5.2](#5_2_video_encoding)/A-0-1] H.264 AVC
*    [[5.2](#5_2_video_encoding)/A-0-2] VP8

Automotive device implementations MUST support the following video decoding
formats and make them available to third-party applications:

*    [[5.3](#5_3_video_decoding)/A-0-1] H.264 AVC
*    [[5.3](#5_3_video_decoding)/A-0-2] MPEG-4 SP
*    [[5.3](#5_3_video_decoding)/A-0-3] VP8
*    [[5.3](#5_3_video_decoding)/A-0-4] VP9

Automotive device implementations are STRONGLY RECOMMENDED to support the
following video decoding:

*    [[5.3](#5_3_video_decoding)/A-SR] H.265 HEVC


### 2.5.3\. Software

Automotive device implementations:

*   [[3](#3_0_intro)/A-0-1] MUST declare the feature
`android.hardware.type.automotive`.

*   [[3](#3_0_intro)/A-0-2] MUST support uiMode = [`UI_MODE_TYPE_CAR`](
http://developer.android.com/reference/android/content/res/Configuration.html#UI_MODE_TYPE_CAR).

*   [[3](#3_0_intro)/A-0-3] MUST support all public APIs in the
[`android.car.*`](https://developer.android.com/reference/android/car/package-summary)
namespace.

If Automotive device implementations provide a proprietary API using
[`android.car.CarPropertyManager`](https://developer.android.com/reference/android/car/hardware/property/CarPropertyManager) with
[`android.car.VehiclePropertyIds`](https://developer.android.com/reference/android/car/VehiclePropertyIds),
they:

*   [[3](#3_0_intro)/A-1-1] MUST NOT attach special privileges to system
    application's use of these properties, or prevent third-party applications
    from using these properties.
*   [[3](#3_0_intro)/A-1-2] MUST NOT replicate a vehicle property that already
    exists in the [SDK](https://developer.android.com/reference/android/car/VehiclePropertyIds).

Automotive device implementations:

*   [[3.2](#3_2_soft_api_compatibility).1/A-0-1] MUST support and enforce all
permissions constants as documented by the [Automotive Permission reference page](
https://developer.android.com/reference/android/car/Car).

*   [[3.2.3.1](#3_2_3_1_common_application_intents)/A-0-1]  MUST preload one or
more applications or service components with an intent handler, for all the
public intent filter patterns defined by the following application intents
listed [here](https://developer.android.com/about/versions/11/reference/common-intents-30).

*   [[3.4](#3_4_web_compatibility).1/A-0-1] MUST provide a complete
implementation of the `android.webkit.Webview` API.

*   [[3.8](#3_8_user_interface_compatibility).3/A-0-1] MUST display
notifications that use the [`Notification.CarExtender`](
https://developer.android.com/reference/android/app/Notification.CarExtender.html)
API when requested by third-party applications.

*   [[3.8](#3_8_user-interface-compatibility).4/A-SR] Are Strongly Recommended
to implement an assistant on the device to handle the [Assist action](
http://developer.android.com/reference/android/content/Intent.html#ACTION_ASSIST).

If Automotive device implementations include a push-to-talk button, they:

*   [[3.8](#3_8_user_interface_compatibility).4/A-1-1] MUST use a short press of
the push-to-talk button as the designated interaction to launch the
user-selected assist app, in other words the app that implements
[`VoiceInteractionService`](
https://developer.android.com/reference/android/service/voice/VoiceInteractionService).

Automotive device implementations:

*   [[3.8.3.1](#3_8_3_1_presentation_of_notifications)/A-0-1] MUST correctly
render resources as described in the [`Notifications on Automotive OS`](
https://developer.android.com/training/cars/notifications)
SDK documentation.
*   [[3.8.3.1](#3_8_3_1_presentation_of_notifications)/A-0-2] MUST display
PLAY and MUTE for notification actions in the place of those provided through
[`Notification.Builder.addAction()`](
https://developer.android.com/reference/android/app/Notification.Builder#addAction%28android.app.Notification.Action%29)
*   [[3.8.3.1](#3_8_3_1_presentation_of_notifications)/A] SHOULD restrict the
use of rich management tasks such as per-notification-channel controls.
MAY use UI affordance per application to reduce controls.

Automotive device implementations:

*   [[3.14](#3_14_media_ui)/A-0-1] MUST include a UI framework to support
third-party apps using the media APIs as described in section
[3.14](#3_14_media_ui).
*   [[3.14](#3_14_media_ui)/A-0-2] MUST allow the user to safely interact
with Media Applications while driving.
*   [[3.14](#3_14_media_ui)/A-0-3] MUST support the
[`CAR_INTENT_ACTION_MEDIA_TEMPLATE`](https://developer.android.com/reference/android/car/Car#CAR_INTENT_ACTION_MEDIA_TEMPLATE)
implicit Intent action with the
[`CAR_EXTRA_MEDIA_PACKAGE`](https://developer.android.com/reference/android/car/Car#CAR_EXTRA_MEDIA_PACKAGE)
extra.
*   [[3.14](#3_14_media_ui)/A-0-4] MUST provide an affordance to navigate into
a Media Application’s
[preference
activity](https://developer.android.com/reference/android/content/Intent.html#ACTION_APPLICATION_PREFERENCES),
but MUST only enable it when Car UX Restrictions are not in effect.
*   [[3.14](#3_14_media_ui)/A-0-5] MUST display
[error messages](https://developer.android.com/reference/android/support/v4/media/session/PlaybackStateCompat.html#getErrorMessage%28%29)
set by Media Applications, and MUST support the optional extras
[`ERROR_RESOLUTION_ACTION_LABEL`](https://developer.android.com/training/cars/media#require-sign-in)
and [`ERROR_RESOLUTION_ACTION_INTENT`](https://developer.android.com/training/cars/media#require-sign-in).
*   [[3.14](#3_14_media_ui)/A-0-6] MUST support an in-app search affordance for
apps that support searching.
*   [[3.14](#3_14_media_ui)/A-0-7] MUST respect
[`CONTENT_STYLE_BROWSABLE_HINT`](
https://developer.android.com/training/cars/media#default-content-style)
and [`CONTENT_STYLE_PLAYABLE_HINT`](
https://developer.android.com/training/cars/media#default-content-style)
definitions when displaying the [MediaBrowser](
https://developer.android.com/reference/android/media/browse/MediaBrowser.html)
hierarchy.

If Automotive device implementations include a default launcher app, they:

*   [[3.14](#3_14_media_ui)/A-1-1] MUST include media services and open them
with the [`CAR_INTENT_ACTION_MEDIA_TEMPLATE`](
https://developer.android.com/reference/android/car/Car#CAR_INTENT_ACTION_MEDIA_TEMPLATE)
intent.

Automotive device implementations:

*    [[3.8](#3_8_user-interface-compatibility)/A] MAY restrict the application
     requests to enter a full screen mode as described in [`immersive documentation`](
     https://developer.android.com/training/system-ui/immersive).
*   [[3.8](#3_8_user-interface-compatibility)/A] MAY keep the status bar and
    the navigation bar visible at all times.
*   [[3.8](#3_8_user-interface-compatibility)/A] MAY restrict the application
    requests to change the colors behind the system UI elements, to ensure
    those elements are clearly visible at all times.

### 2.5.4\. Performance and Power

Automotive device implementations:

*   [[8.2](#8_2_file_i/o_access_performance)/A-0-1] MUST report the number of
bytes read and written to non-volatile storage per each process's UID so the
stats are available to developers through System API
`android.car.storagemonitoring.CarStorageMonitoringManager`. The Android Open
Source Project meets the requirement through the `uid_sys_stats` kernel module.
*   [[8.3](#8_3_power_saving_modes)/A-1-3] MUST support [Garage Mode](
    https://source.android.com/devices/automotive/garage_mode).
*   [[8.3](#8_3_power_saving_modes)/A] SHOULD be in Garage Mode for at least
    15 minutes after every drive unless:
    *    The battery is drained.
    *    No idle jobs are scheduled.
    *    The driver exits Garage Mode.
*   [[8.4](#8_4_power_consumption_accounting)/A-0-1] MUST provide a
per-component power profile that defines the [current consumption value](http://source.android.com/devices/tech/power/values.html)
for each hardware component and the approximate battery drain caused by the
components over time as documented in the Android Open Source Project site.
*   [[8.4](#8_4_power_consumption_accounting)/A-0-2] MUST report all power
consumption values in milliampere hours (mAh).
*   [[8.4](#8_4_power_consumption_accounting)/A-0-3] MUST report CPU power
consumption per each process's UID. The Android Open Source Project meets the
requirement through the `uid_cputime` kernel module implementation.
*   [[8.4](#8_4_power_consumption_accounting)/A] SHOULD be attributed to the
hardware component itself if unable to attribute hardware component power usage
to an application.
*   [[8.4](#8_4_power_consumption_accounting)/A-0-4] MUST make this power usage
available via the [`adb shell dumpsys batterystats`](http://source.android.com/devices/tech/power/batterystats.html)
shell command to the app developer.

### 2.5.5\. Security Model

If Automotive device implementations support multiple users, they:

*   [[9.5](#9_5_multi-user_support)/A-1-1] MUST NOT allow users to interact with
nor switch into the [Headless System User](https://source.android.com/devices/tech/admin/multi-user#user_types),
except for [device provisioning](https://source.android.com/devices/tech/admin/provision).
*   [[9.5](#9_5_multi-user_support)/A-1-2] MUST switch into a [Secondary User](https://source.android.com/devices/tech/admin/multi-user#user_types)
before [`BOOT_COMPLETED`](https://developer.android.com/reference/android/content/Intent.html#ACTION_BOOT_COMPLETED).
*   [[9.5](#9_5_multi-user_support)/A-1-3] MUST support the ability to create
a [Guest User](https://source.android.com/devices/tech/admin/multi-user#user_types)
even when the maximum number of Users on a device has been reached.


Automotive device implementations:

*    [[9.11](#9_11_permissions)/A-0-1] MUST back up the keystore implementation
     with an isolated execution environment.
*    [[9.11](#9_11_permissions)/A-0-2] MUST have implementations of RSA, AES,
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
*    [[9.11](#9_11_permissions)/A-0-3] MUST perform the lock screen
     authentication in the isolated execution environment and only when
     successful, allow the authentication-bound keys to be used. Lock screen
     credentials MUST be stored in a way that allows only the isolated execution
     environment to perform lock screen authentication. The upstream Android
     Open Source Project provides the
     [Gatekeeper Hardware Abstraction Layer (HAL)](
     http://source.android.com/devices/tech/security/authentication/gatekeeper.html)
     and Trusty, which can be used to satisfy this requirement.
*    [[9.11](#9_11_permissions)/A-0-4] MUST support key attestation where the
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

Automotive device implementations:

*   [[9.14](#9_14_automotive_system_isolation)/A-0-1] MUST gatekeep messages
from Android framework vehicle subsystems, e.g., whitelisting permitted message
types and message sources.
*   [[9.14](#9_14_automotive_system_isolation)/A-0-2] MUST watchdog against
denial of service attacks from the Android framework or third-party apps. This
guards against malicious software flooding the vehicle network with traffic,
which may lead to malfunctioning vehicle subsystems.

### 2.5.6\. Developer Tools and Options Compatibility

Automotive device implementations:

*    [**Perfetto**](https://developer.android.com/studio/command-line/perfetto)
    *   [[6.1](#6_1_developer_tools)/A-0-1] MUST expose a `/system/bin/perfetto`
        binary to the shell user which cmdline complies with
        [the perfetto documentation](
        https://developer.android.com/studio/command-line/perfetto).
    *   [[6.1](#6_1_developer_tools)/A-0-2] The perfetto binary MUST accept as
        input a protobuf config that complies with the schema defined in
        [the perfetto documentation](
        https://developer.android.com/studio/command-line/perfetto).
    *   [[6.1](#6_1_developer_tools)/A-0-3] The perfetto binary MUST write as
        output a protobuf trace that complies with the schema defined in
        [the perfetto documentation](
        https://developer.android.com/studio/command-line/perfetto).
    *   [[6.1](#6_1_developer_tools)/A-0-4] MUST provide, through the perfetto
        binary, at least the data sources described  in [the perfetto documentation](
        https://developer.android.com/studio/command-line/perfetto).
