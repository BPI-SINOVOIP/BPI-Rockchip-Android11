# Interacting with Nanoapps from Client Code

[TOC]

Code that interacts with a nanoapp, for example within an Android app, is known
as the *client* of the nanoapp. There are two ways to interact with nanoapps
from the host (application processor): (1) Java (above the Context Hub HAL, from
the Android framework or an APK), and (2) native vendor code (beneath the
Context Hub HAL). Most clients, especially those with a UI component, should use
the Java method, unless a vendor-partition native implementation is required
(e.g. if interacting with a nanoapp is used to implement a different HAL, or if
communication with other beneath-HAL modules is required).

Interaction between nanoapps and clients occur through a flexible message
passing interface. Refer to the Nanoapp Developer Guide for recommendations on
how to design a protocol for use with a nanoapp.

## Java APIs

CHRE is exposed to Android apps holding the appropriate permissions through the
[ContextHubManager](https://android.googlesource.com/platform/frameworks/base/+/master/core/java/android/hardware/location/ContextHubManager.java)
and the associated
[ContextHubClient](https://android.googlesource.com/platform/frameworks/base/+/master/core/java/android/hardware/location/ContextHubClient.java)
system APIs.

To use the above APIs, your application must have access to the
`ACCESS_CONTEXT_HUB` permission, which is restricted to `signature|privileged`.
This permission must be declared in the app’s [Android
Manifest](https://developer.android.com/guide/topics/manifest/uses-permission-element),
and is only granted to APKs that are signed with the same key as the platform
(“signature” scope) or are preinstalled in the privileged apps folder *and* are
present on the [privileged permission
whitelist](https://source.android.com/devices/tech/config/perms-whitelist).

The recommended flow for Java nanoapp client code is as follows:

1. Retrieve the ContextHubManager object via
   `Context.getSystemService(ContextHubManager.class)` - this will produce a
   valid handle if the device supports `FEATURE_CONTEXT_HUB` as indicated by
   `PackageManager.hasSystemFeature()`
2. Retrieve a reference to a Context Hub via
   `ContextHubManager.getContextHubs()`
3. Confirm that the nanoapp is loaded and retrieve its version number by calling
   `ContextHubManager.queryNanoApps()`
4. If the nanoapp was found, create a `ContextHubClient` object through
   `ContextHubManager.createClient()`. This can be used to communicate with your
   nanoapp, and receive notifications of system events like reset. Note that the
   `createClient()` API supports two modes of operation, which define how events
   and data are passed to the client: direct callback with
   `ContextHubClientCallback` (requires a persistent process), or
   `PendingIntent` with `ContextHubIntentEvent` (can start an app’s process when
   an event occurs). To send messages to the nanoapp, use
   `ContextHubClient.sendMessageToNanoApp()`.

## Vendor Native

Depending on the details of the platform implementation, you may also be able to
interact with CHRE directly, beneath the Context Hub HAL, by using socket IPC as
exposed by the CHRE daemon reference implementation. This approach has some
advantages, like being able to interact with system nanoapps that are not
exposed at the Java level, and it can be used with other low-level beneath-HAL
code. However, it is not suitable for use from native code within an Android
app.

See `host/common/test/chre_test_client.cc` for an example of how to use this
interface. Note that SELinux configuration is generally required to whitelist
access to the CHRE socket.

