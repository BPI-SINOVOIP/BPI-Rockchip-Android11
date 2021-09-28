## 9.8\. Privacy

### 9.8.1\. Usage History

Android stores the history of the user's choices and manages such history by
[UsageStatsManager](https://developer.android.com/reference/android/app/usage/UsageStatsManager.html).

Device implementations:

*   [C-0-1] MUST keep a reasonable retention period of such user history.
*   [SR] Are STRONGLY RECOMMENDED to keep the 14 days retention period as
    configured by default in the AOSP implementation.

Android stores the system events using the [`StatsLog`](https://developer.android.com/reference/android/util/StatsLog.html)
identifiers, and manages such history via the `StatsManager` and the
`IncidentManager` System API.

Device implementations:

*   [C-0-2] MUST only include the fields marked with `DEST_AUTOMATIC` in the
    incident report created by the System API class `IncidentManager`.
*   [C-0-3] MUST not use the system event identifiers to log any other event
    than what is described in the [`StatsLog`](https://developer.android.com/reference/android/util/StatsLog.html)
    SDK documents. If additional system events are logged, they MAY use a
    different atom identifier in the range between 100,000 and 200,000.

### 9.8.2\. Recording

Device implementations:

*   [C-0-1] MUST NOT preload or distribute software components out-of-box that
    send the user's private information (e.g. keystrokes, text displayed on the
    screen, bugreport) off the device without the user's consent or clear
    ongoing notifications.
*   [C-0-2] MUST display and obtain explicit user consent that includes exactly
    the same message as AOSP whenever screen casting or screen recording is
    enabled via [`MediaProjection`](https://developer.android.com/reference/android/media/projection/MediaProjection)
    or proprietary APIs. MUST NOT provide users an affordance to
    disable future display of the user consent.
*   [C-0-3] MUST have an ongoing notification to the user while screen casting
    or screen recording is enabled. AOSP meets this requirement by showing an
    ongoing notification icon in the status bar.

If device implementations include functionality in the system that either
captures the contents displayed on the screen and/or records the audio stream
played on the device other than via the System API `ContentCaptureService`, or
other proprietary means described in
[Section 9.8.6 Content Capture](#9_8_6_content_capture), they:

*   [C-1-1] MUST have an ongoing notification to the user whenever this
    functionality is enabled and actively capturing/recording.

If device implementations include a component enabled out-of-box, capable of
recording ambient audio and/or record the audio played on the device
to infer useful information about user’s context, they:

*   [C-2-1] MUST NOT store in persistent on-device storage or transmit off the
    device the recorded raw audio or any format that can be converted back into
    the original audio or a near facsimile, except with explicit user consent.

### 9.8.3\. Connectivity

If device implementations have a USB port with USB peripheral mode support,
they:

*   [C-1-1] MUST present a user interface asking for the user's consent before
allowing access to the contents of the shared storage over the USB port.


### 9.8.4\. Network Traffic

Device implementations:

*   [C-0-1] MUST preinstall the same root certificates for the system-trusted
    Certificate Authority (CA) store as [provided](https://source.android.com/security/overview/app-security.html#certificate-authorities)
    in the upstream Android Open Source Project.
*   [C-0-2] MUST ship with an empty user root CA store.
*   [C-0-3] MUST display a warning to the user indicating the network traffic
    may be monitored, when a user root CA is added.

If device traffic is routed through a VPN, device implementations:

*   [C-1-1] MUST display a warning to the user indicating either:
    *   That network traffic may be monitored.
    *   That network traffic is being routed through the specific VPN
        application providing the VPN.

If device implementations have a mechanism, enabled out-of-box by default, that
routes network data traffic through a proxy server or VPN gateway (for example,
preloading a VPN service with `android.permission.CONTROL_VPN` granted), they:

*    [C-2-1] MUST ask for the user's consent before enabling that mechanism,
     unless that VPN is enabled by the Device Policy Controller via the
     [`DevicePolicyManager.setAlwaysOnVpnPackage()`](https://developer.android.com/reference/android/app/admin/DevicePolicyManager.html#setAlwaysOnVpnPackage%28android.content.ComponentName, java.lang.String, boolean%29)
     , in which case the user does not need to provide a separate consent, but
     MUST only be notified.

If device implementations implement a user affordance to toggle on the
"always-on VPN" function of a 3rd-party VPN app, they:

*    [C-3-1] MUST disable this user affordance for apps that do not support
     always-on VPN service in the `AndroidManifest.xml` file via setting the
     [`SERVICE_META_DATA_SUPPORTS_ALWAYS_ON`](https://developer.android.com/reference/android/net/VpnService.html#SERVICE_META_DATA_SUPPORTS_ALWAYS_ON)
     attribute to `false`.

### 9.8.5\. Device Identifiers

Device implementations:

*   [C-0-1] MUST prevent access to the device serial number and, where
    applicable, IMEI/MEID, SIM serial number, and International Mobile
    Subscriber Identity (IMSI) from an app, unless it meets one of the following
    requirements:
    * is a signed carrier app that is verified by device manufacturers.
    * has been granted the `READ_PRIVILEGED_PHONE_STATE` permission.
    * has carrier privileges as defined in [`UICC Carrier Privileges`](https://source.android.com/devices/tech/config/uicc).
    * is a device owner or profile owner that has been granted the
      `READ_PHONE_STATE` permission.

### 9.8.6\. Content Capture

Android, through the System API `ContentCaptureService`, or by other proprietary
means, supports a mechanism for device implementations to capture the
following interactions between the applications and the user.

*    Text and graphics rendered on-screen, including but not limited to,
     notifications and assist data via [`AssistStructure`](
     https://developer.android.com/reference/android/app/assist/AssistStructure)
     API.
*    Media data, such as audio or video, recorded or played by the device.
*    Input events (e.g. key, mouse, gesture, voice, video, and accessibility).
*    Any other events that an application provides to the system via the
     [`Content Capture`](
     https://developer.android.com/reference/android/view/contentcapture/package-summary)
     API or a similarly capable, proprietary API.
*    Any text or other data sent via the [`TextClassifier API`](https://developer.android.com/reference/android/view/textclassifier/TextClassifier)
     to the System TextClassifier i.e to the system service to understand
     the meaning of text, as well as generating predicted next actions based on
     the text.

If device implementations capture the data above, they:

*    [C-0-1] MUST encrypt all such data when stored in the device. This
     encryption MAY be carried out using Android File Based Encryption, or any
     of the ciphers listed as API version 26+ described in [Cipher SDK](
     https://developer.android.com/reference/javax/crypto/Cipher).
*    [C-0-2] MUST NOT back up either raw or encrypted data using
     [Android backup methods](
     https://developer.android.com/guide/topics/data/backup) or any other back
     up methods.
*    [C-0-3] MUST only send all such data and the log of the device using a
     privacy-preserving mechanism. The privacy-preserving mechanism
     is defined as “those which allow only analysis in aggregate and prevent
     matching of logged events or derived outcomes to individual users”, to
     prevent any per-user data being introspectable (e.g., implemented using
     a differential privacy technology such as [`RAPPOR`](
     https://github.com/google/rappor)).
*    [C-0-4] MUST NOT associate such data with any user identity (such
     as [`Account`](https://developer.android.com/reference/android/accounts/Account))
     on the device, except with explicit user consent each time the data is
     associated.
*    [C-0-5] MUST NOT share such data with other apps, except with
     explicit user consent every time it is shared.
*    [C-0-6] MUST provide user affordance to erase such data that
     the `ContentCaptureService` or the proprietary means collects if the
     data is stored in any form on the device.

If device implementations include a service that implements the System API
`ContentCaptureService`, or any proprietary service that captures the data
as described as above, they:

*    [C-1-1] MUST NOT allow users to replace the content capture service with a
     user-installable application or service and MUST only allow the
     preinstalled service to capture such data.
*    [C-1-2] MUST NOT allow any apps other than the preinstalled content capture
     service mechanism to be able to capture such data.
*    [C-1-3] MUST provide user affordance to disable the content capture
     service.
*    [C-1-4] MUST NOT omit user affordance to manage Android permissions that
     are held by the content capture service and follow Android permissions
     model as described in [Section 9.1. Permission](#9_1_permissions.md).
*    [C-SR] Are STRONGLY RECOMMENDED to keep the content capturing service
     components separate, for example, not binding the service or sharing process
     IDs, from other system components except for the following:

     *    Telephony, Contacts, System UI, and Media

### 9.8.7\. Clipboard Access

Device implementations:

  * [C-0-1] MUST NOT return a clipped data on the clipboard (e.g. via the
    [`ClipboardManager`](
    https://developer.android.com/reference/android/content/ClipboardManager)
    API) unless the app is the default IME or is the app that currently has
    focus.

### 9.8.8\. Location

Device implementations:

*   [C-0-1] MUST NOT turn on/off device location setting and Wi-Fi/Bluetooth
scanning settings without explicit user consent or user initiation.
*   [C-0-2] MUST provide the user affordance to access location related
information including recent location requests, app level permissions and usage
of Wi-Fi/Bluetooth scanning for determining location.
*   [C-0-3] MUST ensure that the application using Emergency Location Bypass API
[LocationRequest.setLocationSettingsIgnored()] is a user initiated emergency
session (e.g. dial 911 or text to 911). For Automotive however, a vehicle MAY
initiate an emergency session without active user interaction in the case
a crash/accident is detected (e.g. to satisfy eCall requirements).
*   [C-0-4] MUST preserve the Emergency Location Bypass API's ability to
bypass device location settings without changing the settings.
*   [C-0-5] MUST schedule a notification that reminds the user after an app in
the background has accessed their location using the
[`ACCESS_BACKGROUND_LOCATION`] permission.

### 9.8.9\. Installed apps

Android apps targeting API level 30 or above cannot see details about other
installed apps by default (see [Package visibility](
https://developer.android.com/preview/privacy/package-visibility) in the Android
SDK documentation).

Device implementations:

*   [C-0-1] MUST NOT expose to any app targeting API level 30 or above details
    about any other installed app, unless the app is already able to see details
    about the other installed app through the managed APIs. This includes but is
    not limited to details exposed by any custom APIs added by the device
    implementer, or accessible via the filesystem.

### 9.8.10\. Connectivity Bug Report

If device implementations generate bug reports using System API
`BUGREPORT_MODE_TELEPHONY` with BugreportManager, they:

*   [C-1-1] MUST obtain user consent every time the System API
    `BUGREPORT_MODE_TELEPHONY` is called to generate a report and MUST NOT
    prompt the user to consent to all future requests from the application.
*   [C-1-2] MUST display and obtain explicit user consent when the reports are
    starting to be generated and MUST NOT return the generated report
    to the requesting app without explicit user consent.
*   [C-1-3] MUST generate requested reports containing at least the following
    information:
    *   TelephonyDebugService dump
    *   TelephonyRegistry dump
    *   WifiService dump
    *   ConnectivityService dump
    *   A dump of the calling package's CarrierService instance (if bound)
    *   Radio log buffer
*   [C-1-4] MUST NOT include the following in the generated reports:
    *   Any kind of information unrelated to connectivity debugging.
    *   Any kind of user-installed application traffic logs or detailed profiles
        of user-installed applications/packages (UIDs are okay, package names
        are not).
*   MAY include additional information that is not associated with any user
    identity. (e.g. vendor logs).

If device implementations include additional information (e.g vendor logs) in
the bug report and that information has privacy/security/battery/storage/memory
impact, they:

*   [C-SR] Are STRONGLY RECOMMENDED to have a developer setting defaulted to
    disabled. The AOSP meets this by providing the
    `Enable verbose vendor logging` option in developer settings to include
    additional device-specific vendor logs in the bug reports.

### 9.8.11\. Data blobs sharing

Android, through [BlobStoreManager](
https://developer.android.com/reference/android/app/blob/BlobStoreManager)
allows apps to contribute data blobs to the System to be shared with a selected
set of apps.

If device implementations support shared data blobs as described in the
[SDK documentation](https://developer.android.com/reference/android/app/blob/BlobStoreManager),
they:

  * [C-1-1] MUST NOT share data blobs belonging to apps beyond what they
    intended to allow (i.e. the scope of default access and the other access
    modes that can be specified using
    [BlobStoreManager.session#allowPackageAccess()](
    https://developer.android.com/reference/android/app/blob/BlobStoreManager.Session#allowPackageAccess%28java.lang.String%2C%2520byte%5B%5D%29),
    [BlobStoreManager.session#allowSameSignatureAccess()](
    https://developer.android.com/reference/android/app/blob/BlobStoreManager.Session#allowSameSignatureAccess%28%29),
    or [BlobStoreManager.session#allowPublicAccess()](
    https://developer.android.com/reference/android/app/blob/BlobStoreManager.Session#allowPublicAccess%28%29)
    MUST NOT be modified). The AOSP reference implementation meets these
    requirements.
  * [C-1-2] MUST NOT send off device or share with other apps the secure hashes
    of data blobs (which are used to control access).