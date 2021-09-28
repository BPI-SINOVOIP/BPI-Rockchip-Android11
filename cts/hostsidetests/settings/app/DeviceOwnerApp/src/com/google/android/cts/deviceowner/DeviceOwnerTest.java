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
package com.google.android.cts.deviceowner;

import android.app.admin.DeviceAdminReceiver;
import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.RemoteException;
import android.provider.Settings;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.Until;
import android.test.InstrumentationTestCase;
import androidx.test.InstrumentationRegistry;

/**
 * Class for device-owner based tests.
 *
 * <p>This class handles making sure that the test is the device owner and that it has an active
 * admin registered if necessary. The admin component can be accessed through {@link #getWho()}.
 */
public class DeviceOwnerTest extends InstrumentationTestCase {

    public static final int TIMEOUT = 2000;

    protected Context mContext;
    protected UiDevice mDevice;

    /** Device Admin receiver for DO. */
    public static class BasicAdminReceiver extends DeviceAdminReceiver {
        /* empty */
    }

    static final String PACKAGE_NAME = DeviceOwnerTest.class.getPackage().getName();
    static final ComponentName RECEIVER_COMPONENT =
            new ComponentName(PACKAGE_NAME, BasicAdminReceiver.class.getName());

    protected DevicePolicyManager mDevicePolicyManager;
    protected PackageManager mPackageManager;
    protected boolean mIsDeviceOwner;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getContext();
        mDevice = UiDevice.getInstance(getInstrumentation());
        mPackageManager = mContext.getPackageManager();
        mDevicePolicyManager =
                (DevicePolicyManager) mContext.getSystemService(Context.DEVICE_POLICY_SERVICE);

        mIsDeviceOwner = mDevicePolicyManager.isDeviceOwnerApp(PACKAGE_NAME);
        if (mIsDeviceOwner) {
            assertTrue(mDevicePolicyManager.isAdminActive(RECEIVER_COMPONENT));

            // Note DPM.getDeviceOwner() now always returns null on non-DO users as of NYC.
            assertEquals(PACKAGE_NAME, mDevicePolicyManager.getDeviceOwner());
        }

        try {
            mDevice.setOrientationNatural();
        } catch (RemoteException e) {
            throw new RuntimeException("failed to freeze device orientation", e);
        }
        wakeupDeviceAndPressHome();
    }

    private void wakeupDeviceAndPressHome() throws Exception {
        mDevice.wakeUp();
        mDevice.pressMenu();
        mDevice.pressHome();
    }

    @Override
    protected void tearDown() throws Exception {
        mDevice.pressBack();
        mDevice.pressHome();
        mDevice.waitForIdle(TIMEOUT); // give UI time to finish animating
    }

    private boolean launchPrivacyAndCheckWorkPolicyInfo() throws Exception {
        // Launch Settings
        launchSettingsPage(InstrumentationRegistry.getContext(), Settings.ACTION_PRIVACY_SETTINGS);

        // Wait for loading permission usage data.
        mDevice.waitForIdle(TIMEOUT);

        return (null != mDevice.wait(Until.findObject(By.text("Your work policy info")), TIMEOUT));
    }

    private void launchSettingsPage(Context ctx, String pageName) throws Exception {
        Intent intent = new Intent(pageName);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        ctx.startActivity(intent);
        Thread.sleep(TIMEOUT * 2);
    }

    private void disableWorkPolicyInfoActivity() {
        mContext.getPackageManager()
                .setComponentEnabledSetting(
                        new ComponentName(mContext, WorkPolicyInfoActivity.class),
                        PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
                        PackageManager.DONT_KILL_APP);
    }

    /**
     * If the app is the active device owner and has work policy info, then we should have a Privacy
     * entry for it.
     */
    public void testDeviceOwnerWithInfo() throws Exception {
        assertTrue(mIsDeviceOwner);
        assertTrue(
                "Couldn't find work policy info settings entry",
                launchPrivacyAndCheckWorkPolicyInfo());
    }

    /**
     * If the app is the active device owner, but doesn't have work policy info, then we shouldn't
     * have a Privacy entry for it.
     */
    public void testDeviceOwnerWithoutInfo() throws Exception {
        assertTrue(mIsDeviceOwner);
        disableWorkPolicyInfoActivity();
        assertFalse(
                "Work policy info settings entry shouldn't be present",
                launchPrivacyAndCheckWorkPolicyInfo());
    }

    /**
     * If the app is NOT the active device owner, then we should not have a Privacy entry for work
     * policy info.
     */
    public void testNonDeviceOwnerWithInfo() throws Exception {
        assertFalse(mIsDeviceOwner);
        assertFalse(
                "Work policy info settings entry shouldn't be present",
                launchPrivacyAndCheckWorkPolicyInfo());
    }

    /**
     * If the app is NOT the active device owner, and doesn't have work policy info, then we should
     * not have a Privacy entry for work policy info.
     */
    public void testNonDeviceOwnerWithoutInfo() throws Exception {
        assertFalse(mIsDeviceOwner);
        disableWorkPolicyInfoActivity();
        assertFalse(
                "Work policy info settings entry shouldn't be present",
                launchPrivacyAndCheckWorkPolicyInfo());
    }
}
