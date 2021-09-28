# Android User Switch Analyzer

This is a tool for processing atrace and summarizing how long
user start, user switch and user unlock process takes during `user switching`

It also provides the time takes by top 5 slowest services in each category

To run the tool, enter the root directory for the
Trebuchet project and use the following command template:
`./gradlew :trebuchet:system-server-analyzer:run --args="<trace_file> [-u user_Id] [-o output_filename] [-c service_count]"`.

If you do not already have a trace file to analyze you can follow the steps
below: (Assuming switching from user 10 to user 11)

1.switch to user 10
  `adb shell am switch-user 10`
2.Capture 15 seconds of tracing information using `atrace`:
  `adb shell atrace -o /sdcard/atrace-ss.txt -t 15 ss`
3.In another console, switch to user 11:
  `adb shell am switch-user 11`
4.Pull the generate file:
  `adb pull /sdcard/atrace-ss.txt`

Alternatively, run-user-switch-perf.sh script in the scripts folder
  `./scripts/run-user-switch-perf.sh <from_user> <to_user>`
  `./scripts/run-user-switch-perf.sh 10 11`

Below is an example of its output:

```
Opening /tmp/atrace-ss.txt
Progress: 100.00%
Parsing atrace-ss.txt took 161ms
StartUser-11 duration: 175.161 ms
com.android.server.policy.PermissionPolicyService 136.691 ms
com.android.server.role.RoleManagerService 13.945 ms
com.android.server.voiceinteraction.VoiceInteractionManagerService 10.931 ms
com.android.server.locksettings.LockSettingsService$Lifecycle 2.052 ms
com.android.server.devicepolicy.DevicePolicyManagerService$Lifecycle 1.231 ms
SwitchUser-11 duration: 276.585 ms
com.android.server.om.OverlayManagerService 253.871 ms
com.android.server.inputmethod.InputMethodManagerService$Lifecycle 10.970 ms
com.android.server.usb.UsbService$Lifecycle 1.021 ms
com.android.server.media.MediaSessionService 0.522 ms
com.android.server.media.projection.MediaProjectionManagerService 0.493 ms
UnlockUser-11 duration: 72.586 ms
com.android.server.autofill.AutofillManagerService 28.399 ms
com.android.server.voiceinteraction.VoiceInteractionManagerService 16.270 ms
com.android.server.accounts.AccountManagerService$Lifecycle 6.775 ms
com.android.server.StorageManagerService$Lifecycle 4.069 ms
com.android.internal.car.CarServiceHelperService 1.290 ms
```
