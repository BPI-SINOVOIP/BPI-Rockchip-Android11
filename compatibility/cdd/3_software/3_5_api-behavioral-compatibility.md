## 3.5\. API Behavioral Compatibility

Device implementations:

*    [C-0-9] MUST ensure that API behavioral compatibility is applied for all
installed apps unless they are restricted as described in
[Section 3.5.1](#3_5_1-background-restriction).
*    [C-0-10] MUST NOT implement the whitelisting approach that ensures API
behavioral compatibility only for apps that are selected by device
implementers.

The behaviors of each of the API types (managed, soft, native, and web) must be
consistent with the preferred implementation of the upstream
[Android Open Source Project](http://source.android.com/). Some specific areas
of compatibility are:

*    [C-0-1] Devices MUST NOT change the behavior or semantics of a
     standard intent.
*    [C-0-2] Devices MUST NOT alter the lifecycle or lifecycle semantics of
     a particular type of system component (such as Service, Activity, ContentProvider, etc.).
*    [C-0-3] Devices MUST NOT change the semantics of a standard permission.
*    Devices MUST NOT alter the limitations enforced on background applications.
     More specifically, for background apps:
     *    [C-0-4] they MUST stop executing callbacks that are registered by the
          app to receive outputs from the [`GnssMeasurement`](
          https://developer.android.com/reference/android/location/GnssMeasurement.html)
          and [`GnssNavigationMessage`](
          https://developer.android.com/reference/android/location/GnssNavigationMessage.html).
     *    [C-0-5] they MUST rate-limit the frequency of updates that are
          provided to the app through the [`LocationManager`](
          https://developer.android.com/reference/android/location/LocationManager.html)
          API class or the [`WifiManager.startScan()`](
          https://developer.android.com/reference/android/net/wifi/WifiManager.html#startScan%28%29)
          method.
     *    [C-0-6] if the app is targeting API level 25 or higher, they MUST NOT
          allow to register broadcast receivers for the implicit broadcasts of
          standard Android intents in the app's manifest, unless the broadcast
          intent requires a `"signature"` or `"signatureOrSystem"`
          [`protectionLevel`](
          https://developer.android.com/guide/topics/manifest/permission-element.html#plevel)
          permission or are on the [exemption list](
          https://developer.android.com/preview/features/background-broadcasts.html)
          .
     *    [C-0-7] if the app is targeting API level 25 or higher, they MUST stop
          the app's background services, just as if the app had called the
          services'[`stopSelf()`](
          https://developer.android.com/reference/android/app/Service.html#stopSelf%28%29)
          method, unless the app is placed on a temporary whitelist to handle a
          task that's visible to the user.
     *    [C-0-8] if the app is targeting API level 25 or higher, they MUST
          release the wakelocks the app holds.
*    [C-0-9] Devices MUST return the following security providers as the first
     seven array values from the [`Security.getProviders()`](
     https://developer.android.com/reference/java/security/Security.html#getProviders%28%29)
     method, in the given order and with the given names (as returned by
     [`Provider.getName()`](
     https://developer.android.com/reference/java/security/Provider.html#getName%28%29))
     and classes, unless the app has modified the list via
     [`insertProviderAt()`](
     https://developer.android.com/reference/java/security/Security.html#insertProviderAt%28java.security.Provider,%2520int%29)
     or [`removeProvider()`](
     https://developer.android.com/reference/java/security/Security.html#removeProvider%28java.lang.String%29). Devices
     MAY return additional providers after the specified list of providers
     below.
     1. **AndroidNSSP** - `android.security.net.config.NetworkSecurityConfigProvider`
     2. **AndroidOpenSSL** - `com.android.org.conscrypt.OpenSSLProvider`
     3. **CertPathProvider** - `sun.security.provider.CertPathProvider`
     4. **AndroidKeyStoreBCWorkaround** - `android.security.keystore.AndroidKeyStoreBCWorkaroundProvider`
     5. **BC** - `com.android.org.bouncycastle.jce.provider.BouncyCastleProvider`
     6. **HarmonyJSSE** - `com.android.org.conscrypt.JSSEProvider`
     7. **AndroidKeyStore** - `android.security.keystore.AndroidKeyStoreProvider`

The above list is not comprehensive. The Compatibility Test Suite (CTS) tests
significant portions of the platform for behavioral compatibility, but not all.
It is the responsibility of the implementer to ensure behavioral compatibility
with the Android Open Source Project. For this reason, device implementers
SHOULD use the source code available via the Android Open Source Project where
possible, rather than re-implement significant parts of the system.

## 3.5.1\. Application Restriction

If device implementations implement a proprietary mechanism to restrict apps and
that mechanism is more restrictive than [the Rare App Standby Bucket](
https://developer.android.com/topic/performance/power/power-details), they:

*    [C-1-1] MUST provide user affordance where the user can see the list of
restricted apps.
*    [C-1-2] MUST provide user affordance to turn on / off the restrictions
on each app.
*    [C-1-3] MUST not automatically apply restrictions without evidence of poor
system health behavior, but MAY apply the restrictions on apps upon detection
of poor system health behavior like stuck wakelocks, long running services, and
other criteria. The criteria MAY be determined by device implementers but MUST
be related to the app’s impact on the system health. Other criteria that are not
purely related to the system health, such as the app’s lack of popularity in
the market, MUST NOT be used as criteria.
*    [C-1-4] MUST not automatically apply app restrictions for apps when a user
has turned off app restrictions manually, and MAY suggest the user to apply
app restrictions.
*    [C-1-5] MUST inform users if app restrictions are applied to an app
automatically. Such information MUST be provided within 24 hours of when
the restrictions are applied.
*    [C-1-6] MUST return `true` for [`ActivityManager.isBackgroundRestricted()`](
https://developer.android.com/reference/android/app/ActivityManager.html#isBackgroundRestricted%28%29)
when the restricted app calls this API.
*    [C-1-7] MUST NOT restrict the top foreground app that is explicitly used
by the user.
*    [C-1-8] MUST suspend restrictions on an app that becomes the top foreground
application when the user explicitly starts to use the app that used to be
restricted.
*    [C-1-9] MUST report all app restriction events via [`UsageStats`](
https://developer.android.com/reference/android/app/usage/UsageStats). If device
implementations extend the app restrictions that are implemented in AOSP, MUST
follow the implementation described in [this document](
https://source.android.com/devices/tech/power/app_mgmt.html).
