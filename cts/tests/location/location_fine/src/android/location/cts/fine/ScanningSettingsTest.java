/*
 * Copyright (C) 2019 The Android Open Source Project
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

package android.location.cts.fine;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.database.ContentObserver;
import android.net.wifi.WifiManager;
import android.os.Handler;
import android.os.HandlerThread;
import android.platform.test.annotations.AppModeFull;
import android.provider.Settings;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.Until;
import android.test.AndroidTestCase;

import com.android.compatibility.common.util.CddTest;
import com.android.compatibility.common.util.FeatureUtil;
import com.android.compatibility.common.util.PollingCheck;

import androidx.test.platform.app.InstrumentationRegistry;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests if system settings app provides scanning settings.
 */
@AppModeFull(reason = "Test cases don't apply for Instant apps")
public class ScanningSettingsTest extends AndroidTestCase {
    private static final String TAG = "ScanningSettingsTest";

    private static final int TIMEOUT = 8_000;  // 8 seconds
    private static final String SETTINGS_PACKAGE = "com.android.settings";

    private static final String WIFI_SCANNING_TITLE_RES =
            "location_scanning_wifi_always_scanning_title";
    private static final String BLUETOOTH_SCANNING_TITLE_RES =
            "location_scanning_bluetooth_always_scanning_title";

    private UiDevice mDevice;
    private Context mContext;
    private String mLauncherPackage;
    private PackageManager mPackageManager;

    @Override
    protected void setUp() {
        // Can't use assumeTrue / assumeFalse because this is not a junit test, and so doesn't
        // support using these keywords to trigger assumption failure and skip test.
        if (FeatureUtil.isTV() || FeatureUtil.isAutomotive() || FeatureUtil.isWatch()) {
            // TV, auto, and watch do not support the setting options of WIFI scanning and Bluetooth
            // scanning
            return;
        }
        mContext = InstrumentationRegistry.getInstrumentation().getContext();
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());

        mPackageManager = mContext.getPackageManager();
        final Intent launcherIntent = new Intent(Intent.ACTION_MAIN);
        launcherIntent.addCategory(Intent.CATEGORY_HOME);
        mLauncherPackage = mPackageManager.resolveActivity(launcherIntent,
                PackageManager.MATCH_DEFAULT_ONLY).activityInfo.packageName;
    }

    @CddTest(requirement = "7.4.2/C-2-1")
    public void testWifiScanningSettings() throws Exception {
        if (FeatureUtil.isTV() || FeatureUtil.isAutomotive() || FeatureUtil.isWatch()) {
            return;
        }
        launchScanningSettings();

        final Resources res = mPackageManager.getResourcesForApplication(SETTINGS_PACKAGE);
        final int resId = res.getIdentifier(WIFI_SCANNING_TITLE_RES, "string", SETTINGS_PACKAGE);
        final UiObject2 pref = mDevice.findObject(By.text(res.getString(resId)));

        final WifiManager wifiManager = mContext.getSystemService(WifiManager.class);

        final boolean checked = wifiManager.isScanAlwaysAvailable();

        // Click the preference to toggle the setting.
        pref.click();
        PollingCheck.check(
                "Scan Always Available wasn't toggled from " + checked + " to " + !checked,
                TIMEOUT,
                () -> !checked == wifiManager.isScanAlwaysAvailable()
        );

        // Click the preference again to toggle the setting back.
        pref.click();
        PollingCheck.check(
                "Scan Always Available wasn't toggled from " + !checked + " to " + checked,
                TIMEOUT,
                () -> checked == wifiManager.isScanAlwaysAvailable()
        );
    }

    @CddTest(requirement = "7.4.3/C-4-1")
    public void testBleScanningSettings() throws PackageManager.NameNotFoundException {
        if (FeatureUtil.isTV() || FeatureUtil.isAutomotive() || FeatureUtil.isWatch()) {
            return;
        }
        launchScanningSettings();
        toggleSettingAndVerify(BLUETOOTH_SCANNING_TITLE_RES,
                Settings.Global.BLE_SCAN_ALWAYS_AVAILABLE);
    }

    private void launchScanningSettings() {
        // Start from the home screen
        mDevice.pressHome();
        mDevice.wait(Until.hasObject(By.pkg(mLauncherPackage).depth(0)), TIMEOUT);

        final Intent intent = new Intent(Settings.ACTION_LOCATION_SCANNING_SETTINGS);
        // Clear out any previous instances
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        mContext.startActivity(intent);

        // Wait for the app to appear
        mDevice.wait(Until.hasObject(By.pkg(SETTINGS_PACKAGE).depth(0)), TIMEOUT);
    }

    private void clickAndWaitForSettingChange(UiObject2 pref, ContentResolver resolver,
            String settingKey) {
        final CountDownLatch latch = new CountDownLatch(1);
        final HandlerThread handlerThread = new HandlerThread(TAG);
        handlerThread.start();
        final ContentObserver observer = new ContentObserver(
                new Handler(handlerThread.getLooper())) {
            @Override
            public void onChange(boolean selfChange) {
                super.onChange(selfChange);
                latch.countDown();
            }
        };
        resolver.registerContentObserver(Settings.Global.getUriFor(settingKey), false, observer);
        pref.click();
        try {
            latch.await(TIMEOUT, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        handlerThread.quit();
        resolver.unregisterContentObserver(observer);
        assertEquals(0, latch.getCount());
    }

    private void toggleSettingAndVerify(String prefTitleRes, String settingKey)
            throws PackageManager.NameNotFoundException {
        final Resources res = mPackageManager.getResourcesForApplication(SETTINGS_PACKAGE);
        final int resId = res.getIdentifier(prefTitleRes, "string", SETTINGS_PACKAGE);
        final UiObject2 pref = mDevice.findObject(By.text(res.getString(resId)));
        final ContentResolver resolver = mContext.getContentResolver();
        final boolean checked = Settings.Global.getInt(resolver, settingKey, 0) == 1;

        // Click the preference to toggle the setting.
        clickAndWaitForSettingChange(pref, resolver, settingKey);
        assertEquals(!checked, Settings.Global.getInt(resolver, settingKey, 0) == 1);

        // Click the preference again to toggle the setting back.
        clickAndWaitForSettingChange(pref, resolver, settingKey);
        assertEquals(checked, Settings.Global.getInt(resolver, settingKey, 0) == 1);
    }
}
