# AAE BugReport App

A test tool that is intended to show how to access Android Automotive OS bugreporting APIs.
It stores all bugreports in user 0 context and then allows uploading them to GCS.

## Flow

1. User long presses Notification icon
2. It opens BugReportActivity as dialog under current user (e.g. u10)
3. BugReportActivity connects to BugReportService and checks if a bugreporting is running.
4. If bugreporting is already running it shows in progress dialog
5. Otherwise it creates MetaBugReport record in a local db and starts recording audio message.
6. When the submit button is clicked, it saves the audio message in temp directory and starts
   BugReportService.
7. If the drivers cancels the dialog, the BugReportActivity deletes temp directory and closes the
   activity.
8. BugReportService running under current user (e.g. u10) starts collecting logs using dumpstate,
    and when finished it updates MetaBugReport using BugStorageProvider.
9. BugStorageProvider is running under u0, it schedules UploadJob.
10. UploadJob runs SimpleUploaderAsyncTask to upload the bugreport.

Bug reports are zipped and uploaded to GCS. GCS enables creating Pub/Sub
notifications that can be used to track when new  bug reports are uploaded.

## System configuration

BugReport app uses `CarBugreportServiceManager` to collect bug reports and
screenshots. `CarBugreportServiceManager` allows only one bug report app to
use it's APIs, by default it's none.

To allow AAE BugReport app to access the API, you need to overlay
`config_car_bugreport_application` in `packages/services/Car/service/res/values/config.xml`
with value `com.android.car.bugreport`.

## App Configuration

UI and upload configs are located in `res/` directory. Resources can be
[overlayed](https://source.android.com/setup/develop/new-device#use-resource-overlays)
for specific products.

### Config

Configs are defined in `Config.java`.

### System Properties

- `android.car.bugreport.force_enable` - set it `true` to enable bugreport app on **all builds**.

### Upload configuration

BugReport app uses `res/raw/gcs_credentials.json` for authentication and
`res/values/configs.xml` for obtaining GCS bucket name.

## Starting bugreporting

The app supports following intents:

1. `adb shell am start com.android.car.bugreport/.BugReportActivity`
    - generates `MetaBugReport.Type.INTERACTIVE` bug report, shows audio message dialog before
    collecting bugreport.
2. `adb shell am start-foreground-service -a com.android.car.bugreport.action.START_SILENT com.android.car.bugreport/.BugReportService`
    - generates `MetaBugReport.Type.SILENT` bug report, without audio message. It shows audio dialog
    after collecting bugreport.

## Testing

### Manually testing the app using the test script

Please follow these instructions to test the app:

1. Connect the device to your computer.
2. Make sure the device has Internet.
3. Start BugReport app; here is the list of possible ways to start:
   * Long press HVAC (A/C) icon
   * Long press Rear Defrost hardware button (hold up to 6 seconds)
   * Long press notification icon
   * Open BugReport app from launcher menu or external apps menu; and click Start Bug Report button.
   * Using adb, see above instructions under `Starting bugreporting`.
4. Bug report collection might take up to 7 minutes to finish.
   * It might fail to upload bugreport when time/time-zone is invalid.
   * In rare cases it might not upload the bugreport, depending Android's
     task scheduling rules.
   * You should see progress bar in notification shade.
5. Pull collected zip files from the device:
   * `adb pull /data/user/0/com.android.car.bugreport/bug_reports_pending/`
6. Please manually verify bug report contents.
   * Images - the should contain screenshots of all the physical displays.
   * Audio files - they should contain the audio message you recorded.
   * Dumpstate (bugreport) - it should contain logs and other information.
7. In any case if bugreport app is not properly functioning, please take adb bugreport and share
   the zip file with developers: `$ adb bugreport`.

NOTE: `utils/bugreport_app_tester.py` is deprecated.
