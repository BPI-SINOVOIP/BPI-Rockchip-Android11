## 7.4\. Data Connectivity

### 7.4.1\. Telephony

“Telephony” as used by the Android APIs and this document refers specifically
to hardware related to placing voice calls and sending SMS messages via a GSM
or CDMA network. While these voice calls may or may not be packet-switched,
they are for the purposes of Android considered independent of any data
connectivity that may be implemented using the same network. In other words,
the Android “telephony” functionality and APIs refer specifically to voice
calls and SMS. For instance, device implementations that cannot place calls or
send/receive SMS messages are not considered a telephony device, regardless of
whether they use a cellular network for data connectivity.

*    Android MAY be used on devices that do not include telephony hardware. That
is, Android is compatible with devices that are not phones.

If device implementations include GSM or CDMA telephony, they:

*   [C-1-1] MUST declare the `android.hardware.telephony` feature flag and
other sub-feature flags according to the technology.
*   [C-1-2] MUST implement full support for the API for that technology.

If device implementations do not include telephony hardware, they:

*    [C-2-1] MUST implement the full APIs as no-ops.

If device implementations support eUICCs or eSIMs/embedded SIMs and include
a proprietary mechanism to make eSIM functionality available for third-party
developers, they:

*    [C-3-1] MUST provide a complete implementation of the [`EuiccManager API`](
https://developer.android.com/reference/android/telephony/euicc/EuiccManager).

#### 7.4.1.1\. Number Blocking Compatibility

If device implementations report the `android.hardware.telephony feature`, they:

*    [C-1-1] MUST include number blocking support
*    [C-1-2] MUST fully implement [`BlockedNumberContract`](
http://developer.android.com/reference/android/provider/BlockedNumberContract.html)
and the corresponding API as described in the SDK documentation.
*    [C-1-3] MUST block all calls and messages from a phone number in
'BlockedNumberProvider' without any interaction with apps. The only exception
is when number blocking is temporarily lifted as described in the SDK
documentation.
*    [C-1-4] MUST NOT write to the [platform call log provider](
http://developer.android.com/reference/android/provider/CallLog.html)
for a blocked call.
*    [C-1-5] MUST NOT write to the [Telephony provider](
http://developer.android.com/reference/android/provider/Telephony.html)
for a blocked message.
*    [C-1-6] MUST implement a blocked numbers management UI, which is opened
with the intent returned by `TelecomManager.createManageBlockedNumbersIntent()`
method.
*    [C-1-7] MUST NOT allow secondary users to view or edit the blocked numbers
on the device as the Android platform assumes the primary user to have full
control of the telephony services, a single instance, on the device. All
blocking related UI MUST be hidden for secondary users and the blocked list MUST
still be respected.
*   SHOULD migrate the blocked numbers into the provider when a device updates
to Android 7.0.

#### 7.4.1.2\. Telecom API

If device implementations report `android.hardware.telephony`, they:

*   [C-1-1] MUST support the `ConnectionService` APIs described in the [SDK](
    https://developer.android.com/guide/topics/connectivity/telecom/selfManaged.html).
*   [C-1-2] MUST display a new incoming call and provide user affordance to
    accept or reject the incoming call when the user is on an ongoing call
    that is made by a third-party app that does not support the hold feature
    specified via
    [`CAPABILITY_SUPPORT_HOLD`](
    https://developer.android.com/reference/android/telecom/Connection.html#CAPABILITY_SUPPORT_HOLD).
*   [C-1-3] MUST have an application that implements
    [InCallService](https://developer.android.com/reference/android/telecom/InCallService).
*   [C-SR] Are STRONGLY RECOMMENDED to notify the user that answering an
    incoming call will drop an ongoing call.

    The AOSP implementation meets these requirements by a heads-up notification
    which indicates to the user that answering an incoming call will cause the
    other call to be dropped.

*   [C-SR] Are STRONGLY RECOMMENDED to preload the default dialer app that
    shows a call log entry and the name of a third-party app in its call log
    when the third-party app sets the
    [`EXTRA_LOG_SELF_MANAGED_CALLS`](
    https://developer.android.com/reference/android/telecom/PhoneAccount.html#EXTRA_LOG_SELF_MANAGED_CALLS)
    extras key on its `PhoneAccount` to `true`.
*   [C-SR] Are STRONGLY RECOMMENDED to handle the audio headset's
    `KEYCODE_MEDIA_PLAY_PAUSE` and `KEYCODE_HEADSETHOOK` events for the
    [`android.telecom`](https://developer.android.com/reference/android/telecom/package-summary.html)
    APIs as below:
    *   Call [`Connection.onDisconnect()`](https://developer.android.com/reference/android/telecom/Connection.html#onDisconnect%28%29)
        when a short press of the key event is detected during an ongoing call.
    *   Call [`Connection.onAnswer()`](https://developer.android.com/reference/android/telecom/Connection.html#onAnswer%28%29)
        when a short press of the key event is detected during an incoming call.
    *   Call [`Connection.onReject()`](https://developer.android.com/reference/android/telecom/Connection.html#onReject%28%29)
        when a long press of the key event is detected during an incoming call.
    *   Toggle the mute status of the [`CallAudioState`](https://developer.android.com/reference/android/telecom/CallAudioState.html).


### 7.4.2\. IEEE 802.11 (Wi-Fi)

Device implementations:

*   SHOULD include support for one or more forms of 802.11\.

If device implementations include support for 802.11 and expose the
functionality to a third-party application, they:

*   [C-1-1] MUST implement the corresponding Android API.
*   [C-1-2] MUST report the hardware feature flag `android.hardware.wifi`.
*   [C-1-3] MUST implement the [multicast API](http://developer.android.com/reference/android/net/wifi/WifiManager.MulticastLock.html)
    as described in the SDK documentation.
*   [C-1-4] MUST support multicast DNS (mDNS) and MUST NOT filter mDNS packets
    (224.0.0.251) at any time of operation including:
    *   Even when the screen is not in an active state.
    *   For Android Television device implementations, even when in standby
power states.
*   [C-1-5] MUST NOT treat the [`WifiManager.enableNetwork()`](
    https://developer.android.com/reference/android/net/wifi/WifiManager.html#enableNetwork%28int%2C%20boolean%29)
    API method call as a sufficient indication to switch the currently active
    `Network` that is used by default for application traffic and is returned
    by [`ConnectivityManager`](https://developer.android.com/reference/android/net/ConnectivityManager)
    API methods such as [`getActiveNetwork`](https://developer.android.com/reference/android/net/ConnectivityManager#getActiveNetwork%28%29)
    and [`registerDefaultNetworkCallback`](https://developer.android.com/reference/android/net/ConnectivityManager#registerDefaultNetworkCallback%28android.net.ConnectivityManager.NetworkCallback,%20android.os.Handler%29).
    In other words, they MAY only disable the Internet access provided by any
    other network provider (e.g. mobile data) if they successfully validate
    that the Wi-Fi network is providing Internet access.
*   [C-1-6] Are STRONGLY RECOMMENDED to, when the
    [`ConnectivityManager.reportNetworkConnectivity()`](
    https://developer.android.com/reference/android/net/ConnectivityManager.html#reportNetworkConnectivity%28android.net.Network%2C%20boolean%29)
    API method is called, re-evaluate the Internet access on the `Network` and,
    once the evaluation determines that the current `Network` no longer provides
    Internet access, switch to any other available network (e.g. mobile
    data) that provides Internet access.
*   [C-SR] Are STRONGLY RECOMMENDED to randomize the source MAC address and
sequence number of probe request frames, once at the beginning of each scan,
while STA is disconnected.
    * Each group of probe request frames comprising one scan should use one
    consistent MAC address (SHOULD NOT randomize MAC address halfway through a
    scan).
    * Probe request sequence number should iterate as normal (sequentially)
    between the probe requests in a scan.
    * Probe request sequence number should randomize between the last probe
    request of a scan and the first probe request of the next scan.
*   [C-SR] Are STRONGLY RECOMMENDED, while STA is disconnected, to allow only
the following elements in probe request frames:
    * SSID Parameter Set (0)
    * DS Parameter Set (3)

If device implementations include support for Wi-Fi power save mode as defined
in IEEE 802.11 standard, they:

*   [C-3-1] MUST turn off Wi-Fi power save mode whenever an app acquires
    `WIFI_MODE_FULL_HIGH_PERF` lock or `WIFI_MODE_FULL_LOW_LATENCY` lock
    via [`WifiManager.createWifiLock()`](
    https://developer.android.com/reference/android/net/wifi/WifiManager.html#createWifiLock\(int,%2520java.lang.String\))
    and  [`WifiManager.WifiLock.acquire()`](
    https://developer.android.com/reference/android/net/wifi/WifiManager.WifiLock.html#acquire\(\))
    APIs and the lock is active.
*   [C-3-2] The average round trip latency between the device
    and an access point while the device is in a Wi-Fi Low Latency Lock
    (`WIFI_MODE_FULL_LOW_LATENCY`) mode MUST be smaller than the
    latency during a Wi-Fi High Perf Lock (`WIFI_MODE_FULL_HIGH_PERF`) mode.
*   [C-SR] Are STRONGLY RECOMMENDED to minimize Wi-Fi round trip latency
    whenever a Low Latency Lock (`WIFI_MODE_FULL_LOW_LATENCY`) is acquired
    and takes effect.

If device implementations support Wi-Fi and use Wi-Fi for location scanning,
they:

*    [C-2-1] MUST provide a user affordance to enable/disable the value read
     through the [`WifiManager.isScanAlwaysAvailable`](https://developer.android.com/reference/android/net/wifi/WifiManager.html#isScanAlwaysAvailable%28%29)
     API method.

#### 7.4.2.1\. Wi-Fi Direct

Device implementations:

*   SHOULD include support for Wi-Fi Direct (Wi-Fi peer-to-peer).

If device implementations include support for Wi-Fi Direct, they:

*   [C-1-1] MUST implement the [corresponding Android API](
    http://developer.android.com/reference/android/net/wifi/p2p/WifiP2pManager.html)
    as described in the SDK documentation.
*   [C-1-2] MUST report the hardware feature `android.hardware.wifi.direct`.
*   [C-1-3] MUST support regular Wi-Fi operation.
*   [C-1-4] MUST support Wi-Fi and Wi-Fi Direct operations concurrently.

#### 7.4.2.2\. Wi-Fi Tunneled Direct Link Setup

Device implementations:

*    SHOULD include support for
     [Wi-Fi Tunneled Direct Link Setup (TDLS)](
     http://developer.android.com/reference/android/net/wifi/WifiManager.html)
     as described in the Android SDK Documentation.

If device implementations include support for TDLS and TDLS is enabled by the
WiFiManager API, they:

*   [C-1-1] MUST declare support for TDLS through [`WifiManager.isTdlsSupported`]
(https://developer.android.com/reference/android/net/wifi/WifiManager.html#isTdlsSupported%28%29).
*   SHOULD use TDLS only when it is possible AND beneficial.
*   SHOULD have some heuristic and NOT use TDLS when its performance might be
    worse than going through the Wi-Fi access point.

#### 7.4.2.3\. Wi-Fi Aware

Device implementations:

*    SHOULD include support for [Wi-Fi Aware](
     http://www.wi-fi.org/discover-wi-fi/wi-fi-aware).

If device implementations include support for Wi-Fi Aware and expose the
functionality to third-party apps, then they:

*   [C-1-1] MUST implement the `WifiAwareManager` APIs as described in the
[SDK documentation](
http://developer.android.com/reference/android/net/wifi/aware/WifiAwareManager.html).
*   [C-1-2] MUST declare the `android.hardware.wifi.aware` feature flag.
*   [C-1-3] MUST support Wi-Fi and Wi-Fi Aware operations concurrently.
*   [C-1-4] MUST randomize the Wi-Fi Aware management interface address at intervals
    no longer than 30 minutes and whenever Wi-Fi Aware is enabled.

If device implementations include support for Wi-Fi Aware and
Wi-Fi Location as described in [Section 7.4.2.5](#7_4_2_5_Wi-Fi_Location) and
exposes these functionalities to third-party apps, then they:

*   [C-2-1] MUST implement the location-aware discovery APIs: [setRangingEnabled](
https://developer.android.com/reference/android/net/wifi/aware/PublishConfig.Builder.html#setRangingEnabled%28boolean%29),
 [setMinDistanceMm](
https://developer.android.com/reference/android/net/wifi/aware/SubscribeConfig.Builder#setMinDistanceMm%28int%29),
 [setMaxDistanceMm](
https://developer.android.com/reference/android/net/wifi/aware/SubscribeConfig.Builder#setMaxDistanceMm%28int%29)
, and
 [onServiceDiscoveredWithinRange](
https://developer.android.com/reference/android/net/wifi/aware/DiscoverySessionCallback#onServiceDiscoveredWithinRange%28android.net.wifi.aware.PeerHandle,%20byte[],%20java.util.List%3Cbyte[]%3E,%20int%29).

#### 7.4.2.4\. Wi-Fi Passpoint

Device implementations:

*    SHOULD include support for [Wi-Fi Passpoint](
     http://www.wi-fi.org/discover-wi-fi/wi-fi-certified-passpoint).

If device implementations include support for Wi-Fi Passpoint, they:

*  [C-1-1] MUST implement the Passpoint related `WifiManager` APIs as
described in the [SDK documentation](
http://developer.android.com/reference/android/net/wifi/WifiManager.html).
*  [C-1-2] MUST support IEEE 802.11u standard, specifically related
   to Network Discovery and Selection, such as Generic Advertisement
   Service (GAS) and Access Network Query Protocol (ANQP).

Conversely if device implementations do not include support for Wi-Fi
Passpoint:

*    [C-2-1] The implementation of the Passpoint related `WifiManager`
APIs MUST throw an `UnsupportedOperationException`.

#### 7.4.2.5\. Wi-Fi Location (Wi-Fi Round Trip Time - RTT)

Device implementations:

*    SHOULD include support for [Wi-Fi Location](
     https://www.wi-fi.org/discover-wi-fi/wi-fi-location).

If device implementations include support for Wi-Fi Location and expose the
functionality to third-party apps, then they:

*   [C-1-1] MUST implement the `WifiRttManager` APIs as described in the
[SDK documentation](
http://developer.android.com/reference/android/net/wifi/rtt/WifiRttManager.html).
*   [C-1-2] MUST declare the `android.hardware.wifi.rtt` feature flag.
*   [C-1-3] MUST randomize the source MAC address for each RTT burst
    which is executed while the Wi-Fi interface on which the RTT is
    being executed is not associated to an Access Point.

#### 7.4.2.6\. Wi-Fi Keepalive Offload

Device implementations:

*   SHOULD include support for Wi-Fi keepalive offload.

If device implementations include support for Wi-Fi keepalive offload and
expose the functionality to third-party apps, they:

*   [C-1-1] MUST support the
[SocketKeepAlive](https://developer.android.google.com/reference/android/net/SocketKeepalive.html) API.

*   [C-1-2] MUST support at least three concurrent keepalive slots over Wi-Fi and
at least one keepalive slot over cellular.

If device implementations do not include support for Wi-Fi keepalive offload,
they:

*   [C-2-1] MUST return [`ERROR_UNSUPPORTED`](
https://developer.android.google.com/reference/android/net/SocketKeepalive.html#ERROR_UNSUPPORTED).

#### 7.4.2.7\. Wi-Fi Easy Connect (Device Provisioning Protocol)

Device implementations:

*    SHOULD include support for [Wi-Fi Easy Connect (DPP)](
     https://www.wi-fi.org/file/wi-fi-certified-easy-connect-technology-overview).

If device implementations include support for Wi-Fi Easy Connect and expose the
functionality to third-party apps, they:

*   [C-1-1] MUST have the [WifiManager#isEasyConnectSupported\(\)](
    https://developer.android.com/reference/android/net/wifi/WifiManager.html#isEasyConnectSupported\(\))
    method return `true`.


### 7.4.3\. Bluetooth

If device implementations support Bluetooth Audio profile, they:

*    SHOULD support Advanced Audio Codecs and Bluetooth Audio Codecs
(e.g. LDAC).

If device implementations support HFP, A2DP and AVRCP, they:

*    SHOULD support at least 5 total connected devices.

If device implementations declare `android.hardware.vr.high_performance`
feature, they:

*    [C-1-1] MUST support Bluetooth 4.2 and Bluetooth LE Data Length Extension.

Android includes support for [Bluetooth and Bluetooth Low Energy](
http://developer.android.com/reference/android/bluetooth/package-summary.html).

If device implementations include support for Bluetooth and Bluetooth
Low Energy, they:

*    [C-2-1] MUST declare the relevant platform features
(`android.hardware.bluetooth` and `android.hardware.bluetooth_le`
respectively) and implement the platform APIs.
*    SHOULD implement relevant Bluetooth profiles such as
     A2DP, AVRCP, OBEX, HFP, etc. as appropriate for the device.

If device implementations include support for Bluetooth Low Energy, they:

*   [C-3-1] MUST declare the hardware feature `android.hardware.bluetooth_le`.
*   [C-3-2] MUST enable the GATT (generic attribute profile) based Bluetooth
APIs as described in the SDK documentation and
[android.bluetooth](
http://developer.android.com/reference/android/bluetooth/package-summary.html).
*   [C-3-3] MUST report the correct value for
`BluetoothAdapter.isOffloadedFilteringSupported()` to indicate whether the
filtering logic for the [ScanFilter](
https://developer.android.com/reference/android/bluetooth/le/ScanFilter.html)
API classes is implemented.
*   [C-3-4] MUST report the correct value for
`BluetoothAdapter.isMultipleAdvertisementSupported()` to indicate
whether Low Energy Advertising is supported.
*   [C-3-5] MUST implement a Resolvable Private Address (RPA) timeout no longer
    than 15 minutes and rotate the address at timeout to protect user privacy.
    To prevent timing attacks, timeout intervals MUST also be randomized
    between 5 and 15 minutes.
*   SHOULD support offloading of the filtering logic to the bluetooth chipset
when implementing the [ScanFilter API](
https://developer.android.com/reference/android/bluetooth/le/ScanFilter.html).
*   SHOULD support offloading of the batched scanning to the bluetooth chipset.
*   SHOULD support multi advertisement with at least 4 slots.

If device implementations support Bluetooth LE and use Bluetooth LE for
location scanning, they:

*    [C-4-1] MUST provide a user affordance to enable/disable the value read
     through the System API `BluetoothAdapter.isBleScanAlwaysAvailable()`.

If device implementations include support for Bluetooth LE and Hearing Aids
Profile, as described in
[Hearing Aid Audio Support Using Bluetooth LE](
https://source.android.com/devices/bluetooth/asha), they:

*   [C-5-1] MUST return `true` for
[BluetoothAdapter.getProfileProxy(context, listener, BluetoothProfile.HEARING_AID)](
https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getProfileProxy(android.content.Context,%20android.bluetooth.BluetoothProfile.ServiceListener,%20int)).

### 7.4.4\. Near-Field Communications

Device implementations:

*    SHOULD include a transceiver and related hardware for Near-Field
Communications (NFC).
*    [C-0-1] MUST implement `android.nfc.NdefMessage` and
`android.nfc.NdefRecord` APIs even if they do not include support for NFC or
declare the `android.hardware.nfc` feature as the classes represent a
protocol-independent data representation format.


If device implementations include NFC hardware and plan to make it available to
third-party apps, they:

*   [C-1-1] MUST report the `android.hardware.nfc` feature from the
[`android.content.pm.PackageManager.hasSystemFeature()` method](
http://developer.android.com/reference/android/content/pm/PackageManager.html).
*   MUST be capable of reading and writing NDEF messages via the following NFC
standards as below:
*   [C-1-2] MUST be capable of acting as an NFC Forum reader/writer
(as defined by the NFC Forum technical specification
NFCForum-TS-DigitalProtocol-1.0) via the following NFC standards:
     *   NfcA (ISO14443-3A)
     *   NfcB (ISO14443-3B)
     *   NfcF (JIS X 6319-4)
     *   IsoDep (ISO 14443-4)
     *   NFC Forum Tag Types 1, 2, 3, 4, 5 (defined by the NFC Forum)
*   [SR] STRONGLY RECOMMENDED to be capable of reading and writing NDEF
    messages as well as raw data via the following NFC standards. Note that
    while the NFC standards are stated as STRONGLY RECOMMENDED, the
    Compatibility Definition for a future version is planned to change these
    to MUST. These standards are optional in this version but will be required
    in future versions. Existing and new devices that run this version of
    Android are very strongly encouraged to meet these requirements now so
    they will be able to upgrade to the future platform releases.

*   [C-1-13] MUST poll for all supported technologies while in NFC discovery
    mode.
*   SHOULD be in NFC discovery mode while the device is awake with the
screen active and the lock-screen unlocked.
*   SHOULD be capable of reading the barcode and URL (if encoded) of
    [Thinfilm NFC Barcode](
    http://developer.android.com/reference/android/nfc/tech/NfcBarcode.html)
    products.

Note that publicly available links are not available for the JIS, ISO, and NFC
Forum specifications cited above.

Android includes support for NFC Host Card Emulation (HCE) mode.

If device implementations include an NFC controller chipset capable of HCE (for
NfcA and/or NfcB) and support Application ID (AID) routing, they:

*   [C-2-1] MUST report the `android.hardware.nfc.hce` feature constant.
*   [C-2-2] MUST support [NFC HCE APIs](
http://developer.android.com/guide/topics/connectivity/nfc/hce.html) as
defined in the Android SDK.

If device implementations include an NFC controller chipset capable of HCE
for NfcF, and implement the feature for third-party applications, they:

*   [C-3-1] MUST report the `android.hardware.nfc.hcef` feature constant.
*   [C-3-2] MUST implement the [NfcF Card Emulation APIs](
https://developer.android.com/reference/android/nfc/cardemulation/NfcFCardEmulation.html)
as defined in the Android SDK.

If device implementations include general NFC support as described in this
section and support MIFARE technologies (MIFARE Classic,
MIFARE Ultralight, NDEF on MIFARE Classic) in the reader/writer role, they:

*   [C-4-1] MUST implement the corresponding Android APIs as documented by
the Android SDK.
*   [C-4-2] MUST report the feature `com.nxp.mifare` from the
[`android.content.pm.PackageManager.hasSystemFeature`()](
http://developer.android.com/reference/android/content/pm/PackageManager.html)
method. Note that this is not a standard Android feature and as such does not
appear as a constant in the `android.content.pm.PackageManager` class.

### 7.4.5\. Networking protocols and APIs

#### 7.4.5.1\. Minimum Network Capability
Device implementations:

*   [C-0-1] MUST include support for one or more forms of
data networking. Specifically, device implementations MUST include support for
at least one data standard capable of 200 Kbit/sec or greater. Examples of
    technologies that satisfy this requirement include EDGE, HSPA, EV-DO,
    802.11g, Ethernet and Bluetooth PAN.
*   SHOULD also include support for at least one common wireless data
standard, such as 802.11 (Wi-Fi), when a physical networking standard (such as
Ethernet) is the primary data connection.
*   MAY implement more than one form of data connectivity.

#### 7.4.5.2\. IPv6

Device implementations:

*   [C-0-2] MUST include an IPv6 networking stack and support IPv6
communication using the managed APIs, such as `java.net.Socket` and
`java.net.URLConnection`, as well as the native APIs, such as `AF_INET6`
sockets.
*   [C-0-3] MUST enable IPv6 by default.
   *   MUST ensure that IPv6 communication is as reliable as IPv4, for example:
      *   [C-0-4] MUST maintain IPv6 connectivity in doze mode.
      *   [C-0-5] Rate-limiting MUST NOT cause the device to lose IPv6
      connectivity on any IPv6-compliant network that uses RA lifetimes of
      at least 180 seconds.
*   [C-0-6] MUST provide third-party applications with direct IPv6 connectivity
to the network when connected to an IPv6 network, without any form of address or
port translation happening locally on the device. Both managed APIs such as
[`Socket#getLocalAddress`](
https://developer.android.com/reference/java/net/Socket.html#getLocalAddress%28%29)
or [`Socket#getLocalPort`](
https://developer.android.com/reference/java/net/Socket.html#getLocalPort%28%29))
and NDK APIs such as `getsockname()` or `IPV6_PKTINFO` MUST return the IP
address and port that is actually used to send and receive packets on the
network and is visible as the source ip and port to internet (web) servers.


The required level of IPv6 support depends on the network type, as shown in
the following requirements.

If device implementations support Wi-Fi, they:

*   [C-1-1] MUST support dual-stack and IPv6-only operation on Wi-Fi.

If device implementations support Ethernet, they:

*   [C-2-1] MUST support dual-stack and IPv6-only operation on
    Ethernet.

If device implementations support Cellular data, they:

*   [C-3-1] MUST support IPv6 operation (IPv6-only and possibly dual-stack) on
    cellular.

If device implementations support more than one network type (e.g., Wi-Fi
and cellular data), they:

*   [C-4-1] MUST simultaneously meet the above requirements on each network
    when the device is simultaneously connected to more than one network type.

#### 7.4.5.3\. Captive Portals

A captive portal refers to a network that requires sign-in in order to
obtain internet access.


If device implementations provide a complete implementation of the
[`android.webkit.Webview API`](https://developer.android.com/reference/android/webkit/WebView.html),
they:

*   [C-1-1] MUST provide a captive portal application to handle the intent
    [`ACTION_CAPTIVE_PORTAL_SIGN_IN`](https://developer.android.com/reference/android/net/ConnectivityManager#ACTION_CAPTIVE_PORTAL_SIGN_IN)
    and display the captive portal login page, by sending that intent, on
    call to the System API
    `ConnectivityManager#startCaptivePortalApp(Network, Bundle)`.
*   [C-1-2] MUST perform detection of captive portals and support login
    through the captive portal application when the device is connected
    to any network type, including cellular/mobile network, WiFi, Ethernet
    or Bluetooth.
*   [C-1-3] MUST support logging in to captive portals using cleartext DNS
    when the device is configured to use private DNS strict mode.
*   [C-1-4] MUST use encrypted DNS as per the SDK documentation for
    [`android.net.LinkProperties.getPrivateDnsServerName`](https://developer.android.com/reference/android/net/LinkProperties.html#getPrivateDnsServerName%28%29)
    and [`android.net.LinkProperties.isPrivateDnsActive`](https://developer.android.com/reference/android/net/LinkProperties#isPrivateDnsActive%28%29)
    for all network traffic that is not explicitly communicating with the
    captive portal.
*   [C-1-5] MUST ensure that, while the user is logging in to a captive
    portal, the default network used by applications (as returned by
    [`ConnectivityManager.getActiveNetwork`](https://developer.android.com/reference/android/net/ConnectivityManager#getActiveNetwork%28%29),
    [`ConnectivityManager.registerDefaultNetworkCallback`](https://developer.android.com/reference/android/net/ConnectivityManager#registerDefaultNetworkCallback%28android.net.ConnectivityManager.NetworkCallback%29),
    and used by default by Java networking APIs such as java.net.Socket,
    and native APIs such as connect()) is any other available network
    that provides internet access, if available.

### 7.4.6\. Sync Settings

Device implementations:

*   [C-0-1] MUST have the master auto-sync setting on by default so that
the method [`getMasterSyncAutomatically()`](
    http://developer.android.com/reference/android/content/ContentResolver.html)
    returns “true”.

### 7.4.7\. Data Saver

If device implementations include a metered connection, they are:

*   [SR] STRONGLY RECOMMENDED to provide the data saver mode.

If device implementations provide the data saver mode, they:

*   [C-1-1] MUST support all the APIs in the `ConnectivityManager`
class as described in the [SDK documentation](
https://developer.android.com/training/basics/network-ops/data-saver.html)

If device implementations do not provide the data saver mode, they:

*   [C-2-1] MUST return the value `RESTRICT_BACKGROUND_STATUS_DISABLED` for
    [`ConnectivityManager.getRestrictBackgroundStatus()`](
    https://developer.android.com/reference/android/net/ConnectivityManager.html#getRestrictBackgroundStatus%28%29)
*   [C-2-2] MUST NOT broadcast
`ConnectivityManager.ACTION_RESTRICT_BACKGROUND_CHANGED`.

### 7.4.8\. Secure Elements

If device implementations support [Open Mobile API](https://developer.android.com/reference/android/se/omapi/package-summary)-capable
secure elements and make them available to third-party apps, they:

*   [C-1-1] MUST enumerate the available secure elements readers via
    [`android.se.omapi.SEService.getReaders()`](https://developer.android.com/reference/android/se/omapi/SEService#getReaders%28%29) API.

*   [C-1-2] MUST declare the correct feature flags via
    [`android.hardware.se.omapi.uicc`](
    https://developer.android.com/reference/android/content/pm/PackageManager#FEATURE_SE_OMAPI_UICC)
    for the device with UICC-based secure elements,
    [`android.hardware.se.omapi.ese`](
    https://developer.android.com/reference/android/content/pm/PackageManager#FEATURE_SE_OMAPI_ESE)
    for the device with eSE-based secure elements and
    [`android.hardware.se.omapi.sd`](
    https://developer.android.com/reference/android/content/pm/PackageManager#FEATURE_SE_OMAPI_SD)
    for the device with SD-based secure elements.