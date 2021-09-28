## 8.3\. Power-Saving Modes

If device implementations include features to improve device power management
that are included in AOSP (e.g. App Standby Bucket, Doze) or extend the features
that do not apply harder restrictions than [the Rare App Standby Bucket](
https://developer.android.com/topic/performance/power/power-details), they:

*   [C-1-1] MUST NOT deviate from the AOSP implementation for the triggering,
    maintenance, wakeup algorithms and the use of global system settings of App
    Standby and Doze power-saving modes.
*   [C-1-2] MUST NOT deviate from the AOSP implementation for the use of global
    settings to manage the throttling of jobs, alarm and network for apps in
    each bucket for App standby.
*   [C-1-3] MUST NOT deviate from the AOSP implementation for the number of the
    [App Standby Buckets](
    https://developer.android.com/topic/performance/appstandby) used for App
    Standby.
*   [C-1-4] MUST implement [App Standby Buckets](
    https://developer.android.com/topic/performance/appstandby) and Doze as
    described in [Power Management](
    https://source.android.com/devices/tech/power/mgmt).
*   [C-1-5] MUST return `true` for [`PowerManager.isPowerSaveMode()`](
    https://developer.android.com/reference/android/os/PowerManager#isPowerSaveMode%28%29)
    when the device is on power save mode.
*   [C-SR] Are STRONGLY RECOMMENDED to provide user affordance to enable and
    disable the battery saver feature.
*   [C-SR] Are STRONGLY RECOMMENDED to provide user affordance to display all
    Apps that are exempted from App Standby and Doze power-saving modes.

If device implementations extend power management features that are included
in AOSP and that extension applies more stringent restrictions than
[the Rare App Standby Bucket](
https://developer.android.com/topic/performance/power/power-details), refer to
[section 3.5.1](#3_5_api-behavioral-compatibility).

In addition to the power-saving modes, Android device implementations MAY
implement any or all of the 4 sleeping power states as defined by the Advanced
Configuration and Power Interface (ACPI).

If device implementations implement S4 power states as defined by the
ACPI, they:

*   [C-1-1] MUST enter this state only after the user has taken an explicit action
    to put the device in an inactive state (e.g. by closing a lid that is physically
    part of the device or turning off a vehicle or television) and before the
    user re-activates the device (e.g. by opening the lid or turning the vehicle
    or television back on).

If device implementations implement S3 power states as defined by the
ACPI, they:

*   [C-2-1] MUST meet C-1-1 above, or, MUST enter S3 state only when third-party
    applications do not need the system resources (e.g. the screen, CPU).

    Conversely, MUST exit from S3 state when third-party applications need the
    system resources, as described on this SDK.

    For example, while the third-party applications request to keep the screen
    on through `FLAG_KEEP_SCREEN_ON` or keep CPU running through
    `PARTIAL_WAKE_LOCK`, the device MUST NOT enter S3 state unless, as described
    in C-1-1, the user has taken explicit action to put the device in an
    inactive state. Conversely, at a time when a task that third-party apps
    implement through JobScheduler is triggered or Firebase Cloud Messaging is
    delivered to third-party apps, the device MUST exit the S3 state unless the
    user has put the device in an inactive state. These are not comprehensive
    examples and AOSP implements extensive wake-up signals that trigger a wakeup
    from this state.
