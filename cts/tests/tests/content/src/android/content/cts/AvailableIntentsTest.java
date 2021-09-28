/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.content.cts;

import static com.android.compatibility.common.util.RequiredServiceRule.hasService;

import android.app.DownloadManager;
import android.app.SearchManager;
import android.content.ComponentName;
import android.content.ContentUris;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.media.RingtoneManager;
import android.net.Uri;
import android.net.wifi.WifiManager;
import android.os.storage.StorageManager;
import android.platform.test.annotations.AppModeFull;
import android.provider.AlarmClock;
import android.provider.MediaStore;
import android.provider.Settings;
import android.provider.Telephony;
import android.telecom.TelecomManager;
import android.test.AndroidTestCase;

import com.android.compatibility.common.util.CddTest;
import com.android.compatibility.common.util.FeatureUtil;

import java.util.List;

@AppModeFull // TODO(Instant) Figure out which intents should be visible
public class AvailableIntentsTest extends AndroidTestCase {
    private static final String NORMAL_URL = "http://www.google.com/";
    private static final String SECURE_URL = "https://www.google.com/";
    private static final String QRCODE= "DPP:I:SN=4774LH2b4044;M:010203040506;K:MDkwEwYHKoZIzj" +
            "0CAQYIKoZIzj0DAQcDIgADURzxmttZoIRIPWGoQMV00XHWCAQIhXruVWOz0NjlkIA=;;";

    /**
     * Assert target intent can be handled by at least one Activity.
     * @param intent - the Intent will be handled.
     */
    private void assertCanBeHandled(final Intent intent) {
        PackageManager packageManager = mContext.getPackageManager();
        List<ResolveInfo> resolveInfoList = packageManager.queryIntentActivities(intent, 0);
        assertNotNull(resolveInfoList);
        // one or more activity can handle this intent.
        assertTrue(resolveInfoList.size() > 0);
    }

    /**
     * Assert target intent is not resolved by a filter with priority greater than 0.
     * @param intent - the Intent will be handled.
     */
    private void assertDefaultHandlerValidPriority(final Intent intent) {
        PackageManager packageManager = mContext.getPackageManager();
        List<ResolveInfo> resolveInfoList = packageManager.queryIntentActivities(intent, 0);
        assertNotNull(resolveInfoList);
        // one or more activity can handle this intent.
        assertTrue(resolveInfoList.size() > 0);
        // no activities override defaults with a high priority. Only system activities can override
        // the priority.
        for (ResolveInfo resolveInfo : resolveInfoList) {
            assertTrue(resolveInfo.priority <= 0);
        }
    }

    private void assertHandledBySystemOnly(final Intent intent) {
        PackageManager packageManager = mContext.getPackageManager();
        List<ResolveInfo> resolveInfoList = packageManager.queryIntentActivities(intent, 0);
        assertNotNull(resolveInfoList);

        // At least one system activity can handle this intent.
        assertTrue(resolveInfoList.size() > 0);
        for (ResolveInfo resolveInfo : resolveInfoList) {
            final int flags = resolveInfo.activityInfo.applicationInfo.flags;
            assertTrue("Package: " + resolveInfo.getComponentInfo().packageName,
                    (flags & ApplicationInfo.FLAG_SYSTEM) != 0);
        }
    }

    private void assertHandledBySelfOnly(final Intent intent) {
        PackageManager packageManager = mContext.getPackageManager();
        List<ResolveInfo> resolveInfoList = packageManager.queryIntentActivities(intent, 0);
        assertNotNull(resolveInfoList);

        assertTrue(resolveInfoList.size() > 0);
        for (ResolveInfo resolveInfo : resolveInfoList) {
            final ActivityInfo ai = resolveInfo.activityInfo;
            assertEquals("android.content.cts", ai.packageName);
        }
    }

    /**
     * Test ACTION_VIEW when url is http://web_address,
     * it will open a browser window to the URL specified.
     */
    public void testViewNormalUrl() {
        Uri uri = Uri.parse(NORMAL_URL);
        Intent intent = new Intent(Intent.ACTION_VIEW, uri);
        assertCanBeHandled(intent);
    }

    /**
     * Test ACTION_VIEW when url is https://web_address,
     * it will open a browser window to the URL specified.
     */
    public void testViewSecureUrl() {
        Uri uri = Uri.parse(SECURE_URL);
        Intent intent = new Intent(Intent.ACTION_VIEW, uri);
        assertCanBeHandled(intent);
    }

    /**
     * Test ACTION_WEB_SEARCH when url is http://web_address,
     * it will open a browser window to the URL specified.
     */
    public void testWebSearchNormalUrl() {
        Uri uri = Uri.parse(NORMAL_URL);
        Intent intent = new Intent(Intent.ACTION_WEB_SEARCH);
        intent.putExtra(SearchManager.QUERY, uri);
        assertCanBeHandled(intent);
    }

    /**
     * Test ACTION_WEB_SEARCH when url is https://web_address,
     * it will open a browser window to the URL specified.
     */
    public void testWebSearchSecureUrl() {
        Uri uri = Uri.parse(SECURE_URL);
        Intent intent = new Intent(Intent.ACTION_WEB_SEARCH);
        intent.putExtra(SearchManager.QUERY, uri);
        assertCanBeHandled(intent);
    }

    /**
     * Test ACTION_WEB_SEARCH when url is empty string,
     * google search will be applied for the plain text.
     */
    public void testWebSearchPlainText() {
        String searchString = "where am I?";
        Intent intent = new Intent(Intent.ACTION_WEB_SEARCH);
        intent.putExtra(SearchManager.QUERY, searchString);
        assertCanBeHandled(intent);
    }

    /**
     * Test ACTION_CALL when uri is a phone number, it will call the entered phone number.
     */
    public void testCallPhoneNumber() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Uri uri = Uri.parse("tel:2125551212");
            Intent intent = new Intent(Intent.ACTION_CALL, uri);
            assertCanBeHandled(intent);
        }
    }

    /**
     * Test ACTION_DIAL when uri is a phone number, it will dial the entered phone number.
     */
    public void testDialPhoneNumber() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Uri uri = Uri.parse("tel:(212)5551212");
            Intent intent = new Intent(Intent.ACTION_DIAL, uri);
            assertCanBeHandled(intent);
        }
    }

    /**
     * Test ACTION_DIAL when uri is a phone number, it will dial the entered phone number.
     */
    public void testDialVoicemail() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Uri uri = Uri.parse("voicemail:");
            Intent intent = new Intent(Intent.ACTION_DIAL, uri);
            assertCanBeHandled(intent);
        }
    }

    /**
     * Test ACTION_CHANGE_PHONE_ACCOUNTS, it will display the phone account preferences.
     */
    public void testChangePhoneAccounts() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Intent intent = new Intent(TelecomManager.ACTION_CHANGE_PHONE_ACCOUNTS);
            assertCanBeHandled(intent);
        }
    }

    /**
     * Test ACTION_SHOW_CALL_ACCESSIBILITY_SETTINGS, it will display the call accessibility preferences.
     */
    public void testShowCallAccessibilitySettings() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Intent intent = new Intent(TelecomManager.ACTION_SHOW_CALL_ACCESSIBILITY_SETTINGS);
            assertCanBeHandled(intent);
        }
    }

    /**
     * Test ACTION_SHOW_CALL_SETTINGS, it will display the call preferences.
     */
    public void testShowCallSettings() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Intent intent = new Intent(TelecomManager.ACTION_SHOW_CALL_SETTINGS);
            assertCanBeHandled(intent);
        }
    }

    /**
     * Test ACTION_SHOW_RESPOND_VIA_SMS_SETTINGS, it will display the respond by SMS preferences.
     */
    public void testShowRespondViaSmsSettings() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Intent intent = new Intent(TelecomManager.ACTION_SHOW_RESPOND_VIA_SMS_SETTINGS);
            assertCanBeHandled(intent);
        }
    }

    /**
     * Test start camera by intent
     */
    public void testCamera() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA)
                || packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_FRONT)) {
            Intent intent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
            assertCanBeHandled(intent);

            intent.setAction(MediaStore.ACTION_VIDEO_CAPTURE);
            assertCanBeHandled(intent);

            intent.setAction(MediaStore.INTENT_ACTION_STILL_IMAGE_CAMERA);
            assertCanBeHandled(intent);

            intent.setAction(MediaStore.INTENT_ACTION_VIDEO_CAMERA);
            assertCanBeHandled(intent);
        }
    }

    /**
     * TODO: This is a separate test so it can more easily be suppressed while we
     * fix targets that are out of compliance.
     */
    public void testImageCaptureIntentsHandledBySystem() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA)
                || packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_FRONT)) {

            Intent intent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
            assertHandledBySystemOnly(intent);

            intent.setAction(MediaStore.ACTION_VIDEO_CAPTURE);
            assertHandledBySystemOnly(intent);

            intent.setAction(MediaStore.INTENT_ACTION_VIDEO_CAMERA);
            assertHandledBySystemOnly(intent);
        }
    }

    public void testImageCaptureIntentWithExplicitTargeting() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA)
                || packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_FRONT)) {
            Intent intent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
            intent.setPackage("android.content.cts");
            assertHandledBySelfOnly(intent);

            intent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
            intent.setComponent(ComponentName.createRelative(
                    "android.content.cts",
                    "com.android.cts.content.StubCameraIntentHandlerActivity"));
            assertHandledBySelfOnly(intent);
        }
    }

    public void testSettings() {
        assertCanBeHandled(new Intent(Settings.ACTION_SETTINGS));
    }

    /**
     * Test add event in calendar
     */
    public void testCalendarAddAppointment() {
        Intent addAppointmentIntent = new Intent(Intent.ACTION_EDIT);
        addAppointmentIntent.setType("vnd.android.cursor.item/event");
        assertCanBeHandled(addAppointmentIntent);
    }

    /**
     * Test view call logs
     */
    public void testContactsCallLogs() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setType("vnd.android.cursor.dir/calls");
            assertCanBeHandled(intent);
        }
    }

    /**
     * Test view music playback
     */
    public void testMusicPlayback() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setDataAndType(ContentUris.withAppendedId(
                MediaStore.Audio.Media.INTERNAL_CONTENT_URI, 1), "audio/*");
        assertCanBeHandled(intent);
    }

    public void testAlarmClockSetAlarm() {
        Intent intent = new Intent(AlarmClock.ACTION_SET_ALARM);
        intent.putExtra(AlarmClock.EXTRA_MESSAGE, "Custom message");
        intent.putExtra(AlarmClock.EXTRA_HOUR, 12);
        intent.putExtra(AlarmClock.EXTRA_MINUTES, 0);
        assertCanBeHandled(intent);
    }

    public void testAlarmClockShowAlarms() {
        Intent intent = new Intent(AlarmClock.ACTION_SHOW_ALARMS);
        assertCanBeHandled(intent);
    }

    public void testAlarmClockDismissAlarm() {
        if (mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_LEANBACK_ONLY)) {
            return;
        }
        Intent intent = new Intent(AlarmClock.ACTION_DISMISS_ALARM);
        assertCanBeHandled(intent);
    }

    public void testAlarmClockSnoozeAlarm() {
        if (mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_LEANBACK_ONLY)) {
            return;
        }
        Intent intent = new Intent(AlarmClock.ACTION_SNOOZE_ALARM);
        intent.putExtra(AlarmClock.EXTRA_ALARM_SNOOZE_DURATION, 10);
        assertCanBeHandled(intent);
    }

    public void testAlarmClockSetTimer() {
        Intent intent = new Intent(AlarmClock.ACTION_SET_TIMER);
        intent.putExtra(AlarmClock.EXTRA_LENGTH, 60000);
        assertCanBeHandled(intent);
    }

    public void testAlarmClockShowTimers() {
        if (mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_LEANBACK_ONLY)) {
            return;
        }
        Intent intent = new Intent(AlarmClock.ACTION_SHOW_TIMERS);
        assertCanBeHandled(intent);
    }

    public void testOpenDocumentAny() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        assertCanBeHandled(intent);
    }

    public void testOpenDocumentImage() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("image/*");
        assertCanBeHandled(intent);
    }

    public void testCreateDocument() {
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("text/plain");
        assertCanBeHandled(intent);
    }

    public void testGetContentAny() {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        assertCanBeHandled(intent);
    }

    public void testGetContentImage() {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("image/*");
        assertCanBeHandled(intent);
    }

    public void testRingtonePicker() {
        assertCanBeHandled(new Intent(RingtoneManager.ACTION_RINGTONE_PICKER));
    }

    public void testViewDownloads() {
        assertCanBeHandled(new Intent(DownloadManager.ACTION_VIEW_DOWNLOADS));
    }

    public void testManageStorage() {
        assertCanBeHandled(new Intent(StorageManager.ACTION_MANAGE_STORAGE));
    }

    public void testFingerprintEnrollStart() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_FINGERPRINT)) {
            assertCanBeHandled(new Intent(Settings.ACTION_FINGERPRINT_ENROLL));
        }
    }

    public void testUsageAccessSettings() {
        PackageManager packageManager = mContext.getPackageManager();
        if (!packageManager.hasSystemFeature(PackageManager.FEATURE_WATCH)) {
            assertCanBeHandled(new Intent(Settings.ACTION_USAGE_ACCESS_SETTINGS));
        }
    }

    public void testPictureInPictureSettings() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)) {
            assertCanBeHandled(new Intent(Settings.ACTION_PICTURE_IN_PICTURE_SETTINGS));
        }
    }

    public void testInteractAcrossProfilesSettings() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_MANAGED_PROFILES)) {
            assertCanBeHandled(new Intent(Settings.ACTION_MANAGE_CROSS_PROFILE_ACCESS));
        }
    }

    public void testChangeDefaultSmsApplication() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            assertCanBeHandled(new Intent(Telephony.Sms.Intents.ACTION_CHANGE_DEFAULT));
        }
    }

    public void testLocationScanningSettings() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_WATCH)) {
            // Skip the test for wearable device.
            return;
        }
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_WIFI)
                || packageManager.hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) {
            assertCanBeHandled(new Intent("android.settings.LOCATION_SCANNING_SETTINGS"));
        }
    }

    public void testSettingsSearchIntent() {
        if (FeatureUtil.isTV() || FeatureUtil.isAutomotive() || FeatureUtil.isWatch()) {
            return;
        }
        assertCanBeHandled(new Intent(Settings.ACTION_APP_SEARCH_SETTINGS));
    }

    public void testChangeDefaultDialer() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            assertCanBeHandled(new Intent(TelecomManager.ACTION_CHANGE_DEFAULT_DIALER));
        }
    }

    public void testTapAnPaySettings() {
        PackageManager packageManager = mContext.getPackageManager();
        if (packageManager.hasSystemFeature(PackageManager.FEATURE_NFC_HOST_CARD_EMULATION)) {
            assertCanBeHandled(new Intent(Settings.ACTION_NFC_PAYMENT_SETTINGS));
        }
    }

    public void testPowerUsageSummarySettings() {
        if (isHandheld()) {
            assertCanBeHandled(new Intent(Intent.ACTION_POWER_USAGE_SUMMARY));
        }
    }

    @CddTest(requirement = "7.4.2.6/C-1-1")
    public void testEasyConnectIntent() {
        // Android only supports Initiator-... modes right now, which require the device to
        // have a QR-code capture mechanism. Therefore this feature does not make sense on
        // non-handheld devices.
        if (!isHandheld()) {
            return;
        }
        WifiManager manager = mContext.getSystemService(WifiManager.class);

        if (manager.isEasyConnectSupported()) {
            Intent intent = new Intent(Settings.ACTION_PROCESS_WIFI_EASY_CONNECT_URI);
            intent.setData(Uri.parse(QRCODE));
            assertCanBeHandled(intent);
        }
    }

    public void testRequestSetAutofillServiceIntent() {
        Intent intent = new Intent(Settings.ACTION_REQUEST_SET_AUTOFILL_SERVICE)
                .setData(Uri.parse("package:android.content.cts"));
        assertCanBeHandled(intent);
    }

    public void testNotificationPolicyDetailIntent() {
        if (!isHandheld()) {
            return;
        }

        Intent intent = new Intent(Settings.ACTION_NOTIFICATION_POLICY_ACCESS_DETAIL_SETTINGS)
                .setData(Uri.parse("package:android.content.cts"));
        assertCanBeHandled(intent);
    }

    public void testRequestEnableContentCaptureIntent() {
        if (!hasService(Context.CONTENT_CAPTURE_MANAGER_SERVICE)) return;

        Intent intent = new Intent(Settings.ACTION_REQUEST_ENABLE_CONTENT_CAPTURE);
        assertCanBeHandled(intent);
    }

    public void testVoiceInputSettingsIntent() {
        // Non-handheld devices do not allow more than one VoiceInteractionService, and therefore do
        // not have to support this Intent.
        if (!isHandheld()) {
            return;
        }
        Intent intent = new Intent(Settings.ACTION_VOICE_INPUT_SETTINGS);
        assertCanBeHandled(intent);
    }

    public void testAddNetworksIntent() {
        assertCanBeHandled(new Intent(Settings.ACTION_WIFI_ADD_NETWORKS));
    }

    private boolean isHandheld() {
        // handheld nature is not exposed to package manager, for now
        // we check for touchscreen and NOT watch, NOT tv and NOT car
        PackageManager pm = getContext().getPackageManager();
        return pm.hasSystemFeature(pm.FEATURE_TOUCHSCREEN)
                && !pm.hasSystemFeature(pm.FEATURE_WATCH)
                && !pm.hasSystemFeature(pm.FEATURE_TELEVISION)
                && !pm.hasSystemFeature(pm.FEATURE_AUTOMOTIVE);
    }
}
