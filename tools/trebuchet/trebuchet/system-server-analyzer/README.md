# Android System Server Startup Analyzer

This is a tool for processing systraces and summarizing how long it takes
to start `system_service` services.

To run the tool, enter the root directory for the
Trebuchet project and use the following command template:
`./gradlew :trebuchet:system-server-analyzer:run --args="<trace_file> [-t threshold_ms] [-o output_file]"`.

If you do not already have a trace file to analyze you can follow the steps
below:

1.Stop `system_server`:
  `adb shell stop`
2.Capture 10 seconds of tracing information using `atrace`:
  `adb shell atrace -o /sdcard/atrace-ss.txt -t 10 ss`
3.In another console, restart `system_server`:
  `adb shell start`
4.Pull the generate file:
  `adb pull /sdcard/atrace-ss.txt`

Below is an example of its output:

```
Opening `/tmp/atrace-ss.txt`
Progress: 100.00%
Parsing atrace-ss.txt took 171ms

StartServices: 5982.37ms
StartActivityManager: 87.21ms
StartPowerManager: 5.04ms
StartLightsService: 1012.62ms
StartDisplayManager: 7.08ms
WaitForDisplay: 23.80ms
StartPackageManagerService: 3267.64ms
StartOverlayManagerService: 128.18ms
StartBatteryService: 24.33ms
StartUsageService: 29.94ms
StartRollbackManagerService: 5.24ms
InstallSystemProviders: 55.23ms
StartWindowManagerService: 148.52ms
SetWindowManagerService: 10.88ms
WindowManagerServiceOnInitReady: 18.26ms
NetworkWatchlistService: 5.87ms
StartAccessibilityManagerService: 31.93ms
MakeDisplayReady: 19.62ms
StartStorageManagerService: 7.46ms
StartLockSettingsService: 28.12ms
StartWifi: 65.78ms
StartConnectivityService: 11.89ms
StartNotificationManager: 16.70ms
StartAudioService: 37.19ms
StartUsbService: 17.25ms
StartJobScheduler: 13.70ms
StartMediaSessionService: 8.07ms
MakeLockSettingsServiceReady: 89.10ms
StartBootPhaseLockSettingsReady: 16.21ms
StartBootPhaseSystemServicesReady: 83.95ms
MakeWindowManagerServiceReady: 5.40ms
MakePowerManagerServiceReady: 14.79ms
MakePackageManagerServiceReady: 48.18ms
PhaseActivityManagerReady: 456.15ms
Other: 157.12ms
```
