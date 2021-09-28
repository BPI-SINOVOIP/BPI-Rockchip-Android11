
## 3.2\. Soft API Compatibility

In addition to the managed APIs from [section 3.1](#3_1_managed_api_compatibility),
Android also includes a significant runtime-only “soft” API, in the form of such
things as intents, permissions, and similar aspects of Android applications that
cannot be enforced at application compile time.

### 3.2.1\. Permissions

*   [C-0-1] Device implementers MUST support and enforce all permission
constants as documented by the [Permission reference page](http://developer.android.com/reference/android/Manifest.permission.html).
Note that [section 9](#9_security_model_compatibility) lists additional
requirements related to the Android security model.

### 3.2.2\. Build Parameters

The Android APIs include a number of constants on the
[android.os.Build class](http://developer.android.com/reference/android/os/Build.html)
that are intended to describe the current device.

*   [C-0-1] To provide consistent, meaningful values across device
implementations, the table below includes additional restrictions on the formats
of these values to which device implementations MUST conform.

<table>
 <tr>
    <th>Parameter</th>
    <th>Details</th>
 </tr>
 <tr>
    <td>VERSION.RELEASE</td>
    <td>The version of the currently-executing Android system, in human-readable
    format. This field MUST have one of the string values defined in
    <a href="http://source.android.com/compatibility/ANDROID_VERSION/versions.html">ANDROID_VERSION</a>.</td>
 </tr>
 <tr>
    <td>VERSION.SDK</td>
    <td>The version of the currently-executing Android system, in a format
    accessible to third-party application code. For Android ANDROID_VERSION,
    this field MUST have the integer value ANDROID_VERSION_INT.</td>
 </tr>
 <tr>
    <td>VERSION.SDK_INT</td>
    <td>The version of the currently-executing Android system, in a format
    accessible to third-party application code. For Android ANDROID_VERSION,
    this field MUST have the integer value ANDROID_VERSION_INT.</td>
 </tr>
 <tr>
    <td>VERSION.INCREMENTAL</td>
    <td>A value chosen by the device implementer designating the specific build
    of the currently-executing Android system, in human-readable format. This
    value MUST NOT be reused for different builds made available to end users. A
    typical use of this field is to indicate which build number or
    source-control change identifier was used to generate the build. The value
    of this field MUST be encodable as printable 7-bit ASCII and match the
    regular expression &ldquo;^[^ :\/~]+$&rdquo;.</td>
 </tr>
 <tr>
    <td>BOARD</td>
    <td>A value chosen by the device implementer identifying the specific
    internal hardware used by the device, in human-readable format. A possible
    use of this field is to indicate the specific revision of the board powering
    the device. The value of this field MUST be encodable as 7-bit ASCII and
    match the regular expression &ldquo;^[a-zA-Z0-9_-]+$&rdquo;.</td>
 </tr>
 <tr>
    <td>BRAND</td>
    <td>A value reflecting the brand name associated with the device as known to
    the end users. MUST be in human-readable format and SHOULD represent the
    manufacturer of the device or the company brand under which the device is
    marketed. The value of this field MUST be encodable as 7-bit ASCII and match
    the regular expression &ldquo;^[a-zA-Z0-9_-]+$&rdquo;.</td>
 </tr>
 <tr>
    <td>SUPPORTED_ABIS</td>
    <td>The name of the instruction set (CPU type + ABI convention) of native
    code. See <a href="#3_3_native_api_compatibility">section 3.3. Native API
    Compatibility</a>.</td>
 </tr>
 <tr>
    <td>SUPPORTED_32_BIT_ABIS</td>
    <td>The name of the instruction set (CPU type + ABI convention) of native
    code. See <a href="#3_3_native_api_compatibility">section 3.3. Native API
    Compatibility</a>.</td>
 </tr>
 <tr>
    <td>SUPPORTED_64_BIT_ABIS</td>
    <td>The name of the second instruction set (CPU type + ABI convention) of
    native code. See <a href="#3_3_native_api_compatibility">section 3.3. Native
    API Compatibility</a>.</td>
 </tr>
 <tr>
    <td>CPU_ABI</td>
    <td>The name of the instruction set (CPU type + ABI convention) of native
    code. See <a href="#3_3_native_api_compatibility">section 3.3. Native API
    Compatibility</a>.</td>
 </tr>
 <tr>
    <td>CPU_ABI2</td>
    <td>The name of the second instruction set (CPU type + ABI convention) of
    native code. See <a href="#3_3_native_api_compatibility">section 3.3. Native
    API Compatibility</a>.</td>
 </tr>
 <tr>
    <td>DEVICE</td>
    <td>A value chosen by the device implementer containing the development name
    or code name identifying the configuration of the hardware features and
    industrial design of the device. The value of this field MUST be encodable
    as 7-bit ASCII and match the regular expression
    &ldquo;^[a-zA-Z0-9_-]+$&rdquo;. This device name MUST NOT change during the
    lifetime of the product.</td>
 </tr>
 <tr>
    <td>FINGERPRINT</td>
    <td>A string that uniquely identifies this build. It SHOULD be reasonably
    human-readable. It MUST follow this template:
    <p class="small">$(BRAND)/$(PRODUCT)/<br>
        &nbsp;&nbsp;&nbsp;&nbsp;$(DEVICE):$(VERSION.RELEASE)/$(ID)/$(VERSION.INCREMENTAL):$(TYPE)/$(TAGS)</p>
    <p>For example:</p>
<p class="small">acme/myproduct/<br>
      &nbsp;&nbsp;&nbsp;&nbsp;mydevice:ANDROID_VERSION/LMYXX/3359:userdebug/test-keys</p>
      <p>The fingerprint MUST NOT include whitespace characters. The value of
      this field MUST be encodable as 7-bit ASCII.</p></td>
 </tr>
 <tr>
    <td>HARDWARE</td>
    <td>The name of the hardware (from the kernel command line or /proc). It
    SHOULD be reasonably human-readable. The value of this field MUST be
    encodable as 7-bit ASCII and match the regular expression
    &ldquo;^[a-zA-Z0-9_-]+$&rdquo;.</td>
 </tr>
 <tr>
    <td>HOST</td>
    <td>A string that uniquely identifies the host the build was built on, in
    human-readable format. There are no requirements on the specific format of
    this field, except that it MUST NOT be null or the empty string ("").</td>
 </tr>
 <tr>
    <td>ID</td>
    <td>An identifier chosen by the device implementer to refer to a specific
    release, in human-readable format. This field can be the same as
    android.os.Build.VERSION.INCREMENTAL, but SHOULD be a value sufficiently
    meaningful for end users to distinguish between software builds. The value
    of this field MUST be encodable as 7-bit ASCII and match the regular
    expression &ldquo;^[a-zA-Z0-9._-]+$&rdquo;.</td>
 </tr>
 <tr>
    <td>MANUFACTURER</td>
    <td>The trade name of the Original Equipment Manufacturer (OEM) of the
    product. There are no requirements on the specific format of this field,
    except that it MUST NOT be null or the empty string (""). This field
    MUST NOT change during the lifetime of the product.</td>
 </tr>
 <tr>
    <td>MODEL</td>
    <td>A value chosen by the device implementer containing the name of the
    device as known to the end user. This SHOULD be the same name under which
    the device is marketed and sold to end users. There are no requirements on
    the specific format of this field, except that it MUST NOT be null or the
    empty string (""). This field MUST NOT change during the
    lifetime of the product.</td>
 </tr>
 <tr>
    <td>PRODUCT</td>
    <td>A value chosen by the device implementer containing the development name
    or code name of the specific product (SKU) that MUST be unique within the
    same brand. MUST be human-readable, but is not necessarily intended for view
    by end users. The value of this field MUST be encodable as 7-bit ASCII and
    match the regular expression &ldquo;^[a-zA-Z0-9_-]+$&rdquo;. This product
    name MUST NOT change during the lifetime of the product.</td>
 </tr>
 <tr>
    <td>SERIAL</td>
    <td>MUST return "UNKNOWN".</td>
 </tr>
 <tr>
    <td>TAGS</td>
    <td>A comma-separated list of tags chosen by the device implementer that
    further distinguishes the build. The tags MUST be encodable as 7-bit ASCII
    and match the regular expression &ldquo;^[a-zA-Z0-9._-]+&rdquo; and MUST
    have one of the values corresponding to the three typical Android platform
    signing configurations: release-keys, dev-keys, and test-keys.</td>
 </tr>
 <tr>
    <td>TIME</td>
    <td>A value representing the timestamp of when the build occurred.</td>
 </tr>
 <tr>
    <td>TYPE</td>
    <td>A value chosen by the device implementer specifying the runtime
    configuration of the build. This field MUST have one of the values
    corresponding to the three typical Android runtime configurations: user,
    userdebug, or eng.</td>
 </tr>
 <tr>
    <td>USER</td>
    <td>A name or user ID of the user (or automated user) that generated the
    build. There are no requirements on the specific format of this field,
    except that it MUST NOT be null or the empty string ("").</td>
 </tr>
 <tr>
    <td>SECURITY_PATCH</td>
    <td>A value indicating the security patch level of a build. It MUST signify
    that the build is not in any way vulnerable to any of the issues described
    up through the designated Android Public Security Bulletin. It MUST be in
    the format [YYYY-MM-DD], matching a defined string documented in the
    <a href="http://source.android.com/security/bulletin"> Android Public Security
    Bulletin</a> or in the <a href="http://source.android.com/security/advisory">
    Android Security Advisory</a>, for example "2015-11-01".</td>
 </tr>
 <tr>
    <td>BASE_OS</td>
    <td>A value representing the FINGERPRINT parameter of the build that is
    otherwise identical to this build except for the patches provided in the
    Android Public Security Bulletin. It MUST report the correct value and if
    such a build does not exist, report an empty string ("").</td>
 </tr>
 <tr>
    <td><a href="https://developer.android.com/reference/android/os/Build.html#BOOTLOADER">BOOTLOADER</a></td>
    <td> A value chosen by the device implementer identifying the specific
    internal bootloader version used in the device, in human-readable format.
    The value of this field MUST be encodable as 7-bit ASCII and match the
    regular expression &ldquo;^[a-zA-Z0-9._-]+$&rdquo;.</td>
 </tr>
 <tr>
    <td><a href="https://developer.android.com/reference/android/os/Build.html#getRadioVersion()">getRadioVersion()</a></td>
    <td> MUST (be or return) a value chosen by the device implementer
    identifying the specific internal radio/modem version used in the device,
    in human-readable format. If a device does not have any internal
    radio/modem it MUST return NULL. The value of this field MUST be
    encodable as 7-bit ASCII and match the regular expression
    &ldquo;^[a-zA-Z0-9._-,]+$&rdquo;.</td>
 </tr>
 <tr>
    <td><a href="https://developer.android.com/reference/android/os/Build.html#getSerial()">getSerial()</a></td>
    <td> MUST (be or return) a hardware serial number, which MUST be available
    and unique across devices with the same MODEL and MANUFACTURER. The value of
    this field MUST be encodable as 7-bit ASCII and match the regular expression
    &ldquo;^[a-zA-Z0-9._-,]+$&rdquo;.</td>
 </tr>
</table>

### 3.2.3\. Intent Compatibility

#### 3.2.3.1\. Common Application Intents

Android intents allow application components to request functionality from
other Android components. The Android upstream project includes a list of
applications which implement several intent patterns to perform common actions.

Device implementations:

*   [C-SR] Are STRONGLY RECOMMENDED to preload one or more applications or
service components with an intent handler, for all the public intent filter
patterns defined by the following application intents listed [here](https://developer.android.com/about/versions/11/reference/common-intents-30)
and provide fulfillment i.e meet with the developer expectation for these common
application intents as described in the SDK.

Please refer to [Section 2](#2_device_types) for mandatory application intents
for each device type.

#### 3.2.3.2\. Intent Resolution

*   [C-0-1] As Android is an extensible platform, device implementations MUST
allow each intent pattern referenced in [section 3.2.3.1](#3_2_3_1_common_application_intents)
, except for Settings, to be overridden by third-party applications. The
upstream Android open source implementation allows this by default.

*   [C-0-2] Device implementers MUST NOT attach special privileges to system
applications' use of these intent patterns, or prevent third-party applications
from binding to and assuming control of these patterns. This prohibition
specifically includes but is not limited to disabling the “Chooser” user
interface that allows the user to select between multiple applications that all
handle the same intent pattern.

*   [C-0-3] Device implementations MUST provide a user interface for users to
modify the default activity for intents.

*   However, device implementations MAY provide default activities for specific
URI patterns (e.g. http://play.google.com) when the default activity provides a
more specific attribute for the data URI. For example, an intent filter pattern
specifying the data URI “http://www.android.com” is more specific than the
browser's core intent pattern for “http://”.

Android also includes a mechanism for third-party apps to declare an
authoritative default [app linking behavior](https://developer.android.com/training/app-links)
for certain types of web URI intents. When such authoritative declarations are
defined in an app's intent filter patterns, device implementations:

*   [C-0-4] MUST attempt to validate any intent filters by performing the
validation steps defined in the [Digital Asset Links specification](https://developers.google.com/digital-asset-links)
as implemented by the Package Manager in the upstream Android Open Source
Project.
*   [C-0-5] MUST attempt validation of the intent filters during the installation of
the application and set all successfully validated URI intent filters as
default app handlers for their URIs.
*   MAY set specific URI intent filters as default app handlers for their URIs,
if they are successfully verified but other candidate URI filters fail
verification. If a device implementation does this, it MUST provide the
user appropriate per-URI pattern overrides in the settings menu.
*   MUST provide the user with per-app App Links controls in Settings as
follows:
      *   [C-0-6] The user MUST be able to override holistically the default app
      links behavior for an app to be: always open, always ask, or never open,
      which must apply to all candidate URI intent filters equally.
      *   [C-0-7] The user MUST be able to see a list of the candidate URI intent
      filters.
      *   The device implementation MAY provide the user with the ability to
      override specific candidate URI intent filters that were successfully
      verified, on a per-intent filter basis.
      *   [C-0-8] The device implementation MUST provide users with the ability to
      view and override specific candidate URI intent filters if the device
      implementation lets some candidate URI intent filters succeed
      verification while some others can fail.

#### 3.2.3.3\. Intent Namespaces

*   [C-0-1] Device implementations MUST NOT include any Android component that
honors any new intent or broadcast intent patterns using an ACTION, CATEGORY, or
other key string in the android.* or com.android.* namespace.
*   [C-0-2] Device implementers MUST NOT include any Android components that
honor any new intent or broadcast intent patterns using an ACTION, CATEGORY, or
other key string in a package space belonging to another organization.
*   [C-0-3] Device implementers MUST NOT alter or extend any of the intent
patterns listed in [section 3.2.3.1](#3_2_3_1_common_application_intents).
*   Device implementations MAY include intent patterns using namespaces clearly
and obviously associated with their own organization. This prohibition is
analogous to that specified for Java language classes in [section 3.6](#3_6_api_namespaces).

#### 3.2.3.4\. Broadcast Intents

Third-party applications rely on the platform to broadcast certain intents to
notify them of changes in the hardware or software environment.

Device implementations:

*   [C-0-1] MUST broadcast the public broadcast intents listed [here](https://developer.android.com/about/versions/11/reference/broadcast-intents-30)
in response to appropriate system events as described in the SDK documentation.
Note that this requirement is not conflicting with section 3.5 as the
limitation for background applications are also described in the SDK
documentation. Also certain broadcast intents are conditional upon hardware
support, if the device supports the necessary hardware they MUST broadcast the
intents and provide the behavior inline with SDK documentation.

#### 3.2.3.5\. Conditional Application Intents

Android includes settings that provide users an easy way to select their
default applications, for example for Home screen or SMS.

Where it makes sense, device implementations MUST provide a similar settings
menu and be compatible with the intent filter pattern and API methods described
in the SDK documentation as below.

If device implementations report `android.software.home_screen`, they:

*   [C-1-1] MUST honor the [`android.settings.HOME_SETTINGS`](
http://developer.android.com/reference/android/provider/Settings.html#ACTION_HOME_SETTINGS)
intent to show a default app settings menu for Home Screen.

If device implementations report `android.hardware.telephony`, they:

*   [C-2-1] MUST provide a settings menu that will call the
[`android.provider.Telephony.ACTION_CHANGE_DEFAULT`](
http://developer.android.com/reference/android/provider/Telephony.Sms.Intents.html#ACTION_CHANGE_DEFAULT)
intent to show a dialog to change the default SMS application.

*   [C-2-2] MUST honor the [`android.telecom.action.CHANGE_DEFAULT_DIALER`](
https://developer.android.com/reference/android/telecom/TelecomManager.html#ACTION_CHANGE_DEFAULT_DIALER)
intent to show a dialog to allow the user to change the default Phone
application.
     *    MUST use the user-selected default Phone app's UI for incoming and
     outgoing calls except for emergency calls, which would use the
     preinstalled Phone app.

*   [C-2-3] MUST honor the [android.telecom.action.CHANGE_PHONE_ACCOUNTS](
https://developer.android.com/reference/android/telecom/TelecomManager.html#ACTION_CHANGE_PHONE_ACCOUNTS)
intent to provide user affordance to configure the [`ConnectionServices`](
https://developer.android.com/reference/android/telecom/ConnectionService.html)
associated with the [`PhoneAccounts`](
https://developer.android.com/reference/android/telecom/PhoneAccount.html), as
well as a default PhoneAccount that the telecommunications service provider will
use to place outgoing calls. The AOSP implementation meets this requirement by
including a "Calling Accounts option" menu within the "Calls" settings menu.

*   [C-2-4] MUST allow [`android.telecom.CallRedirectionService`](https://developer.android.com/reference/android/telecom/CallRedirectionService)
for an app that holds the [`android.app.role.CALL_REDIRECTION`](https://developer.android.com/reference/android/app/role/RoleManager#ROLE_CALL_REDIRECTION)
role.
*   [C-2-5] MUST provide the user affordance to choose an app that holds the
[`android.app.role.CALL_REDIRECTION`](https://developer.android.com/reference/android/app/role/RoleManager#ROLE_CALL_REDIRECTION)
role.
*   [C-2-6] MUST honor the [android.intent.action.SENDTO](https://developer.android.com/reference/android/content/Intent#ACTION_SENDTO)
and [android.intent.action.VIEW](https://developer.android.com/reference/android/content/Intent#ACTION_VIEW)
intents and provide an activity to send/display SMS messages.
*   [C-SR] Are Strongly Recommended to honor [android.intent.action.ANSWER](https://developer.android.com/reference/android/content/Intent#ACTION_ANSWER),
[android.intent.action.CALL](https://developer.android.com/reference/android/content/Intent#ACTION_CALL),
[android.intent.action.CALL_BUTTON](https://developer.android.com/reference/android/content/Intent#ACTION_CALL_BUTTON),
[android.intent.action.VIEW](https://developer.android.com/reference/android/content/Intent#ACTION_VIEW)
& [android.intent.action.DIAL](https://developer.android.com/reference/android/content/Intent#ACTION_DIAL)
intents with a preloaded dialer application which can handle these intents and
provide fulfillment as described in the SDK.

If device implementations report `android.hardware.nfc.hce`, they:

*   [C-3-1] MUST honor the [android.settings.NFC_PAYMENT_SETTINGS](
http://developer.android.com/reference/android/provider/Settings.html#ACTION_NFC_PAYMENT_SETTINGS)
intent to show a default app settings menu for Tap and Pay.
*   [C-3-2] MUST honor [android.nfc.cardemulation.action.ACTION_CHANGE_DEFAULT](https://developer.android.com/reference/android/nfc/cardemulation/CardEmulation#ACTION_CHANGE_DEFAULT)
intent to show an activity which opens a dialog to ask the user to change the
default card emulation service for a certain category as described in the SDK.

If device implementations report `android.hardware.nfc`, they:

*   [C-4-1] MUST honor these intents [android.nfc.action.NDEF_DISCOVERED](https://developer.android.com/reference/android/nfc/NfcAdapter#ACTION_NDEF_DISCOVERED),
[android.nfc.action.TAG_DISCOVERED](https://developer.android.com/reference/android/nfc/NfcAdapter#ACTION_TAG_DISCOVERED)
& [android.nfc.action.TECH_DISCOVERED](https://developer.android.com/reference/android/nfc/NfcAdapter#ACTION_TECH_DISCOVERED),
to show an activity which fulfils developer expectations for these intents as
described in the SDK.

If device implementations support the `VoiceInteractionService` and have more
than one application using this API installed at a time, they:

*   [C-4-1] MUST honor the [`android.settings.ACTION_VOICE_INPUT_SETTINGS`](https://developer.android.com/reference/android/provider/Settings.html#ACTION_VOICE_INPUT_SETTINGS)
intent to show a default app settings menu for voice input and assist.

If device implementations report `android.hardware.bluetooth`, they:

*   [C-5-1] MUST honor the [‘android.bluetooth.adapter.action.REQUEST_ENABLE’](https://developer.android.com/reference/kotlin/android/bluetooth/BluetoothAdapter#action_request_enable)
intent and show a system activity to allow the user to turn on Bluetooth. 
*   [C-5-2] MUST honor the
[‘android.bluetooth.adapter.action.REQUEST_DISCOVERABLE’](https://developer.android.com/reference/android/bluetooth/BluetoothAdapter#ACTION_REQUEST_DISCOVERABLE)
intent and show a system activity that requests discoverable mode.

If device implementations support the DND feature, they:

*   [C-6-1] MUST implement an activity that would respond to the intent
[`ACTION_NOTIFICATION_POLICY_ACCESS_SETTINGS`](https://developer.android.com/reference/android/provider/Settings#ACTION_NOTIFICATION_POLICY_ACCESS_SETTINGS),
which for implementations with UI_MODE_TYPE_NORMAL it MUST be an activity where
the user can grant or deny the app access to DND policy configurations.

If device implementations allow users to use third-party input methods on the
device, they:

*   [C-7-1] MUST provide a user-accessible mechanism to add and configure
third-party input methods in response to the
[`android.settings.INPUT_METHOD_SETTINGS`](https://developer.android.com/reference/android/provider/Settings#ACTION_INPUT_METHOD_SETTINGS)
intent.

If device implementations support third-party accessibility services, they:

*   [C-8-1] MUST honor the [`android.settings.ACCESSIBILITY_SETTINGS`](https://developer.android.com/reference/android/provider/Settings#ACTION_ACCESSIBILITY_SETTINGS)
intent to provide a user-accessible mechanism to enable and disable the
third-party accessibility services alongside the preloaded accessibility
services.

If device implementations include support for Wi-Fi Easy Connect and expose the
functionality to third-party apps, they:

*   [C-9-1] MUST implement the [Settings#ACTION_PROCESS_WIFI_EASY_CONNECT_URI](https://developer.android.com/reference/android/provider/Settings.html#ACTION_PROCESS_WIFI_EASY_CONNECT_URI)
Intent APIs as described in the SDK documentation.

If device implementations provide the data saver mode, they:
*   [C-10-1] MUST provide a user interface in the settings, that handles the
[`Settings.ACTION_IGNORE_BACKGROUND_DATA_RESTRICTIONS_SETTINGS`](https://developer.android.com/reference/android/provider/Settings.html#ACTION_IGNORE_BACKGROUND_DATA_RESTRICTIONS_SETTINGS)
intent, allowing users to add applications to or remove applications from
the allow list.

If device implementations do not provide the data saver mode, they:

*   [C-11-1] MUST have an activity that handles the
[`Settings.ACTION_IGNORE_BACKGROUND_DATA_RESTRICTIONS_SETTINGS`](https://developer.android.com/reference/android/provider/Settings#ACTION_IGNORE_BACKGROUND_DATA_RESTRICTIONS_SETTINGS)
intent but MAY implement it as a no-op.

If device implementations declare the support for camera via
`android.hardware.camera.any` they:

*   [C-12-1] MUST honor the [`android.media.action.STILL_IMAGE_CAMERA`](https://developer.android.com/reference/android/provider/MediaStore#INTENT_ACTION_STILL_IMAGE_CAMERA)
and [`android.media.action.STILL_IMAGE_CAMERA_SECURE`](https://developer.android.com/reference/android/provider/MediaStore#INTENT_ACTION_STILL_IMAGE_CAMERA_SECURE)
intent and launch the camera in still image mode as described in the SDK.
*   [C-12-2] MUST honor the [`android.media.action.VIDEO_CAMERA`](https://developer.android.com/reference/android/provider/MediaStore#INTENT_ACTION_VIDEO_CAMERA)
intent to launch the camera in video mode as described in the SDK.
*   [C-12-3] MUST honor only allow preinstalled Android applications to handle
the following intents [`MediaStore.ACTION_IMAGE_CAPTURE`](https://developer.android.com/reference/android/provider/MediaStore.html#ACTION_IMAGE_CAPTURE),
[`MediaStore.ACTION_IMAGE_CAPTURE_SECURE`](https://developer.android.com/reference/android/provider/MediaStore.html#ACTION_IMAGE_CAPTURE_SECURE),
and [`MediaStore.ACTION_VIDEO_CAPTURE`](https://developer.android.com/reference/android/provider/MediaStore.html#ACTION_VIDEO_CAPTURE)
as described in the [SDK document](https://developer.android.com/preview/behavior-changes-11?hl=zh-tw#media-capture).

If device implementations report `android.software.device_admin`, they:

*   [C-13-1] MUST honor the intent [`android.app.action.ADD_DEVICE_ADMIN`](https://developer.android.com/reference/android/app/admin/DevicePolicyManager#ACTION_ADD_DEVICE_ADMIN)
to invoke a UI to bring the user through adding the device administrator to
the system (or allowing them to reject it).

*   [C-13-2] MUST honor the  intents
[android.app.action.ADMIN_POLICY_COMPLIANCE](https://developer.android.com/reference/android/app/admin/DevicePolicyManager#ACTION_ADMIN_POLICY_COMPLIANCE),
[android.app.action.GET_PROVISIONING_MODE](https://developer.android.com/reference/android/app/admin/DevicePolicyManager#ACTION_GET_PROVISIONING_MODE),
[android.app.action.PROVISIONING_SUCCESSFUL](https://developer.android.com/reference/android/app/admin/DevicePolicyManager#ACTION_PROVISIONING_SUCCESSFUL),
[android.app.action.PROVISION_MANAGED_DEVICE](https://developer.android.com/reference/android/app/admin/DevicePolicyManager#ACTION_PROVISION_MANAGED_DEVICE),
[android.app.action.PROVISION_MANAGED_PROFILE](https://developer.android.com/reference/android/app/admin/DevicePolicyManager#ACTION_PROVISION_MANAGED_PROFILE),
[android.app.action.SET_NEW_PARENT_PROFILE_PASSWORD](https://developer.android.com/reference/android/app/admin/DevicePolicyManager#ACTION_SET_NEW_PARENT_PROFILE_PASSWORD),
[android.app.action.SET_NEW_PASSWORD](https://developer.android.com/reference/android/app/admin/DevicePolicyManager#ACTION_SET_NEW_PASSWORD)
& [android.app.action.START_ENCRYPTION](https://developer.android.com/reference/android/app/admin/DevicePolicyManager#ACTION_START_ENCRYPTION)
and have an activity to provide fulfillment for these intents as described
in SDK [here](https://developer.android.com/reference/android/app/admin/DevicePolicyManager).

If device implementations declare the [`android.software.autofill`](https://developer.android.com/reference/android/content/pm/PackageManager.html#FEATURE_AUTOFILL)
feature flag, they:

*   [C-14-1] MUST fully implement the [`AutofillService`](https://developer.android.com/reference/android/service/autofill/AutofillService.html)
and [`AutofillManager`](https://developer.android.com/reference/android/view/autofill/AutofillManager.html)
APIs and honor the [android.settings.REQUEST_SET_AUTOFILL_SERVICE](https://developer.android.com/reference/android/provider/Settings.html#ACTION_REQUEST_SET_AUTOFILL_SERVICE)
intent to show a default app settings menu to enable and disable autofill and
change the default autofill service for the user.

If device implementations include a pre-installed app or wish to allow
third-party apps to access the usage statistics, they:

*   [C-SR] are STRONGLY RECOMMENDED provide user-accessible mechanism to grant
or revoke access to the usage stats in response to the
[android.settings.ACTION_USAGE_ACCESS_SETTINGS](https://developer.android.com/reference/android/provider/Settings.html#ACTION&lowbar;USAGE&lowbar;ACCESS&lowbar;SETTINGS)
intent for apps that declare the `android.permission.PACKAGE_USAGE_STATS`
permission.

If device implementations intend to disallow any apps, including pre-installed
apps, from accessing the usage statistics, they:

*   [C-15-1] MUST still have an activity that handles the
[android.settings.ACTION_USAGE_ACCESS_SETTINGS](https://developer.android.com/reference/android/provider/Settings.html#ACTION&lowbar;USAGE&lowbar;ACCESS&lowbar;SETTINGS)
intent pattern but MUST implement it as a no-op, that is to have an equivalent
behavior as when the user is declined for access.

If device implementations report the feature `android.hardware.audio.output`,
they:

*   [C-SR] Are Strongly Recommended to honor android.intent.action.TTS_SERVICE,
android.speech.tts.engine.INSTALL_TTS_DATA &
android.speech.tts.engine.GET_SAMPLE_TEXT intents have an activity to provide
fulfillment for these intents as described in SDK [here](https://developer.android.com/reference/android/speech/tts/TextToSpeech.Engine).

Android includes support for interactive screensavers, previously referred to
as Dreams. Screen Savers allow users to interact with applications when a device
connected to a power source is idle or docked in a desk dock.
Device Implementations:

*   SHOULD include support for screen savers and provide a settings option for
users to configure screen savers in response to the
`android.settings.DREAM_SETTINGS` intent. 

### 3.2.4\. Activities on secondary/multiple displays

If device implementations allow launching normal [Android Activities](
https://developer.android.com/reference/android/app/Activity.html) on more than
one display, they:

*   [C-1-1] MUST set the `android.software.activities_on_secondary_displays`
    feature flag.
*   [C-1-2] MUST guarantee API compatibility similar to an activity running on
    the primary display.
*   [C-1-3] MUST land the new activity on the same display as the activity that
    launched it, when the new activity is launched without specifying a target
    display via the [`ActivityOptions.setLaunchDisplayId()`](
    https://developer.android.com/reference/android/app/ActivityOptions.html#setLaunchDisplayId%28int%29)
    API.
*   [C-1-4] MUST destroy all activities, when a display with the
    [`Display.FLAG_PRIVATE`](http://developer.android.com/reference/android/view/Display.html#FLAG_PRIVATE)
    flag is removed.
*   [C-1-5] MUST securely hide content on all screens when the device is locked
    with a secure lock screen, unless the app opts in to show on top of lock
    screen using [`Activity#setShowWhenLocked()`](
    https://developer.android.com/reference/android/app/Activity#setShowWhenLocked%28boolean%29)
    API.
*   SHOULD have [`android.content.res.Configuration`](
    https://developer.android.com/reference/android/content/res/Configuration.html)
    which corresponds to that display in order to be displayed, operate
    correctly, and maintain compatibility if an activity is launched on
    secondary display.

If device implementations allow launching normal [Android Activities](
https://developer.android.com/reference/android/app/Activity.html) on secondary
displays and a secondary display has the [android.view.Display.FLAG_PRIVATE](
https://developer.android.com/reference/android/view/Display.html#FLAG_PRIVATE)
flag:

*   [C-3-1] Only the owner of that display, system, and activities that are
    already on that display MUST be able to launch to it. Everyone can launch to
    a display that has [android.view.Display.FLAG_PUBLIC](https://developer.android.com/reference/android/view/Display.html#FLAG_PUBLIC)
    flag.
