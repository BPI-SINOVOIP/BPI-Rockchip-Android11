## 9.1\. Permissions

Device implementations:

*   [C-0-1] MUST support the [Android permissions model](
http://developer.android.com/guide/topics/security/permissions.html)
as defined in the Android developer documentation. Specifically, they
MUST enforce each permission defined as described in the SDK documentation; no
permissions may be omitted, altered, or ignored.

*   MAY add additional permissions, provided the new permission ID strings
are not in the `android.\*` namespace.

*   [C-0-2] Permissions with a `protectionLevel` of
[`PROTECTION_FLAG_PRIVILEGED`](
https://developer.android.com/reference/android/content/pm/PermissionInfo.html#PROTECTION&lowbar;FLAG&lowbar;PRIVILEGED)
MUST only be granted to apps preinstalled in the privileged path(s) of the system
image and within the subset of the explicitly whitelisted permissions for each
app. The AOSP implementation meets this requirement by reading and honoring
the whitelisted permissions for each app from the files in the
`etc/permissions/` path and using the `system/priv-app` path as the
privileged path.

Permissions with a protection level of dangerous are runtime permissions.
Applications with `targetSdkVersion` > 22 request them at runtime.

Device implementations:

*   [C-0-3] MUST show a dedicated interface for the user to decide
     whether to grant the requested runtime permissions and also provide
     an interface for the user to manage runtime permissions.
*   [C-0-4] MUST have one and only one implementation of both user
     interfaces.
*   [C-0-5] MUST NOT grant any runtime permissions to preinstalled
     apps unless:
     *   The user's consent can be obtained before the application
         uses it.
     *   The runtime permissions are associated with an intent pattern
         for which the preinstalled application is set as the default handler.
*   [C-0-6] MUST grant the `android.permission.RECOVER_KEYSTORE` permission
     only to system apps that register a properly secured Recovery Agent. A
     properly secured Recovery Agent is defined as an on-device software agent
     that synchronizes with an off-device remote storage, that is equipped with
     secure hardware with protection equivalent or stronger than what is
     described in
     [Google Cloud Key Vault Service](
     https://developer.android.com/preview/features/security/ckv-whitepaper.html)
     to prevent brute-force attacks on the lockscreen knowledge factor.

Device implementations:

*   [C-0-7] MUST adhere to [Android location permission](
    https://developer.android.com/privacy/device-location) properties when an app
    requests the location or physical activity data through standard Android API
    or proprietary mechanism. Such data includes but not limited to:

    *  Device's location (e.g. latitude and longitude).
    *  Information that can be used to determine or estimate the device's
       location (e.g. SSID, BSSID, Cell ID, or location of the network that the
       device is connected to).
    *  User's physical activity or classification of the physical activity.

More specifically, device implementations:

        *   [C-0-8] MUST obtain user consent to allow an app to access the
            location or physical activity data.
        *   [C-0-9] MUST grant a runtime permission ONLY to the app that holds
            sufficient permission as described on SDK.
            For example,
[TelephonyManager#getServiceState](https://developer.android.com/reference/android/telephony/TelephonyManager.html#getAllCellInfo())
            requires `android.permission.ACCESS_FINE_LOCATION`).

Permissions can be marked as restricted altering their behavior.

*   [C-0-10] Permissions marked with the flag `hardRestricted` MUST NOT be
     granted to an app unless:
     *   An app APK file is in the system partition.
     *   The user assigns a role that is associated with the `hardRestricted`
         permissions to an app.
     *   The installer grants the `hardRestricted` to an app.
     *   An app is granted the `hardRestricted` on an earlier Android version.

*   [C-0-11] Apps holding a `softRestricted` permission MUST get only limited
    access and MUST NOT gain full access until whitelisted as described in the
    SDK, where full and limited access is defined for each `softRestricted`
    permission (for example, [`READ_EXTERNAL_STORAGE`](
    https://developer.android.com/reference/android/Manifest.permission#READ_EXTERNAL_STORAGE)).

If device implementations provide a user affordance to choose which apps can
draw on top of other apps with an activity that handles the
[`ACTION_MANAGE_OVERLAY_PERMISSION`](https://developer.android.com/reference/android/provider/Settings.html#ACTION_MANAGE_OVERLAY_PERMISSION)
intent, they:

*   [C-2-1] MUST ensure that all activities with intent filters for the
    [`ACTION_MANAGE_OVERLAY_PERMISSION`](
    https://developer.android.com/reference/android/provider/Settings.html#ACTION_MANAGE_OVERLAY_PERMISSION)
    intent have the same UI screen, regardless of the initiating app or any
    information it provides.