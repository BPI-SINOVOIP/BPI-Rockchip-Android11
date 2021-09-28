## 6.1\. Developer Tools

Device implementations:

*   [C-0-1] MUST support the Android Developer Tools provided in the Android
SDK.
*   [**Android Debug Bridge (adb)**](http://developer.android.com/tools/help/adb.html)
    *   [C-0-2] MUST support adb as documented in the Android SDK and the shell
        commands provided in the AOSP, which can be used by app developers,
        including [`dumpsys`](https://source.android.com/devices/input/diagnostics.html)
        `cmd stats`
    *   [C-0-11] MUST support the shell command `cmd testharness`. Upgrading
        device implementations from an earlier Android version without a
        persistent data block MAY be exempted from C-0-11.
    *   [C-0-3] MUST NOT alter the format or the contents of device system
        events (batterystats , diskstats, fingerprint, graphicsstats, netstats,
        notification, procstats) logged via the dumpsys command.
    *   [C-0-10] MUST record, without omission, and make the following events
        accessible and available to the `cmd stats` shell command and the
        `StatsManager` System API class.
        *   ActivityForegroundStateChanged
        *   AnomalyDetected
        *   AppBreadcrumbReported
        *   AppCrashOccurred
        *   AppStartOccurred
        *   BatteryLevelChanged
        *   BatterySaverModeStateChanged
        *   BleScanResultReceived
        *   BleScanStateChanged
        *   ChargingStateChanged
        *   DeviceIdleModeStateChanged
        *   ForegroundServiceStateChanged
        *   GpsScanStateChanged
        *   JobStateChanged
        *   PluggedStateChanged
        *   ScheduledJobStateChanged
        *   ScreenStateChanged
        *   SyncStateChanged
        *   SystemElapsedRealtime
        *   UidProcessStateChanged
        *   WakelockStateChanged
        *   WakeupAlarmOccurred
        *   WifiLockStateChanged
        *   WifiMulticastLockStateChanged
        *   WifiScanStateChanged
    *   [C-0-4] MUST have the device-side adb daemon be inactive by default and
    there MUST be a user-accessible mechanism to turn on the Android Debug
    Bridge.
    *   [C-0-5] MUST support secure adb. Android includes support for secure
    adb. Secure adb enables adb on known authenticated hosts.
    *   [C-0-6] MUST provide a mechanism allowing adb to be connected from a
    host machine. Specifically:

    If device implementations without a USB port support peripheral mode, they:

    *   [C-3-1] MUST implement adb via local-area network (such as Ethernet
    or Wi-Fi).
    *   [C-3-2] MUST provide drivers for Windows 7, 8 and 10, allowing
    developers to connect to the device using the adb protocol.

    If device implementations support adb connections to a host machine via
    Wi-Fi, they:

    *   [C-4-1] MUST have the `AdbManager#isAdbWifiSupported()` method
    return `true`.

    If device implementations support adb connections to a host machine via
    Wi-Fi and includes at least one camera, they:

    *   [C-5-1] MUST have the `AdbManager#isAdbWifiQrSupported()` method
     return `true`.

*    [**Dalvik Debug Monitor Service (ddms)**](http://developer.android.com/tools/debugging/ddms.html)
    *   [C-0-7] MUST support all ddms features as documented in the Android SDK.
    As ddms uses adb, support for ddms SHOULD be inactive by default, but
    MUST be supported whenever the user has activated the Android Debug Bridge,
    as above.
*    [**Monkey**](http://developer.android.com/tools/help/monkey.html)
    *   [C-0-8] MUST include the Monkey framework and make it available for
    applications to use.
*    [**SysTrace**](http://developer.android.com/tools/help/systrace.html)
    *   [C-0-9] MUST support the systrace tool as documented in the Android SDK.
    Systrace must be inactive by default and there MUST be a user-accessible
    mechanism to turn on Systrace.
*    [**Perfetto**](https://developer.android.com/studio/command-line/perfetto)
    *   [C-SR] Are STRONGLY RECOMMENDED to expose a `/system/bin/perfetto`
        binary to the shell user which cmdline complies with
        [the perfetto documentation](
        https://developer.android.com/studio/command-line/perfetto).
    *   [C-SR] The perfetto binary is STRONGLY RECOMMENDED to accept as input a
        protobuf config that complies with the schema defined in
        [the perfetto documentation](
        https://developer.android.com/studio/command-line/perfetto).
    *   [C-SR] The perfetto binary is STRONGLY RECOMMENDED to write as output a
        protobuf trace that complies with the schema defined in
        [the perfetto documentation](
        https://developer.android.com/studio/command-line/perfetto).
    *   [C-SR] Are STRONGLY RECOMMENDED to provide, through the perfetto binary,
        at least the data sources described  in [the perfetto documentation](
        https://developer.android.com/studio/command-line/perfetto).
*    [**Low Memory Killer**](https://source.android.com/devices/tech/perf/lmkd)
    *   [C-0-10] MUST write a `LMK_KILL_OCCURRED_FIELD_NUMBER` Atom to the
        statsd log when an app is terminated by the [Low Memory Killer](
        https://source.android.com/devices/tech/perf/lmkd).
*    [**Test Harness Mode**](https://source.android.com/compatibility/cts/harness)
    If device implementations support the shell command `cmd testharness` and
    run `cmd testharness enable`, they:
    *   [C-2-1] MUST return `true` for
        `ActivityManager.isRunningInUserTestHarness()`
    *   [C-2-2] MUST implement Test Harness Mode as described in
        [Test Harness Mode documentation](
        https://source.android.com/compatibility/cts/harness).

If device implementations report the support of Vulkan 1.0 or higher via the
`android.hardware.vulkan.version` feature flags, they:

*   [C-1-1] MUST provide an affordance for the app developer to enable/disable
    GPU debug layers.
*   [C-1-2] MUST, when the GPU debug layers are enabled, enumerate layers in
    libraries provided by external tools (i.e. not part of the platform or
    application package) found in debuggable applications' base directory to
    support [vkEnumerateInstanceLayerProperties()](
    https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/vkEnumerateInstanceLayerProperties.html)
    and [vkCreateInstance()](
    https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/vkCreateInstance.html)
    API methods.
