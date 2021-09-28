/*
 * Copyright (C) 2015 The Android Open Source Project
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
package com.android.cts.deviceandprofileowner;

import static android.Manifest.permission.READ_CONTACTS;
import static android.Manifest.permission.WRITE_CONTACTS;

import android.Manifest.permission;
import android.app.AppOpsManager;
import android.app.admin.DevicePolicyManager;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.BySelector;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.UiWatcher;
import android.support.test.uiautomator.Until;
import android.test.suitebuilder.annotation.Suppress;
import android.util.Log;

import com.google.android.collect.Sets;

import java.util.Set;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Test Runtime Permissions APIs in DevicePolicyManager.
 */
public class PermissionsTest extends BaseDeviceAdminTest {
    private static final String TAG = "PermissionsTest";

    private static final String PERMISSION_APP_PACKAGE_NAME
            = "com.android.cts.permissionapp";
    private static final String SIMPLE_PRE_M_APP_PACKAGE_NAME =
            "com.android.cts.launcherapps.simplepremapp";
    private static final String CUSTOM_PERM_A_NAME = "com.android.cts.permissionapp.permA";
    private static final String CUSTOM_PERM_B_NAME = "com.android.cts.permissionapp.permB";
    private static final String DEVELOPMENT_PERMISSION = "android.permission.INTERACT_ACROSS_USERS";

    private static final String PERMISSIONS_ACTIVITY_NAME
            = PERMISSION_APP_PACKAGE_NAME + ".PermissionActivity";
    private static final String ACTION_CHECK_HAS_PERMISSION
            = "com.android.cts.permission.action.CHECK_HAS_PERMISSION";
    private static final String ACTION_REQUEST_PERMISSION
            = "com.android.cts.permission.action.REQUEST_PERMISSION";
    private static final String ACTION_PERMISSION_RESULT
            = "com.android.cts.permission.action.PERMISSION_RESULT";
    private static final String EXTRA_PERMISSION
            = "com.android.cts.permission.extra.PERMISSION";
    private static final String EXTRA_GRANT_STATE
            = "com.android.cts.permission.extra.GRANT_STATE";
    private static final int PERMISSION_ERROR = -2;
    private static final BySelector CRASH_POPUP_BUTTON_SELECTOR = By
            .clazz(android.widget.Button.class.getName())
            .text("OK")
            .pkg("android");
    private static final BySelector CRASH_POPUP_TEXT_SELECTOR = By
            .clazz(android.widget.TextView.class.getName())
            .pkg("android");
    private static final String CRASH_WATCHER_ID = "CRASH";

    private static final Set<String> LOCATION_PERMISSIONS = Sets.newHashSet(
            permission.ACCESS_FINE_LOCATION,
            permission.ACCESS_BACKGROUND_LOCATION,
            permission.ACCESS_COARSE_LOCATION);

    public static final String AUTO_GRANTED_PERMISSIONS_CHANNEL_ID =
            "alerting auto granted permissions";

    private PermissionBroadcastReceiver mReceiver;
    private PackageManager mPackageManager;
    private UiDevice mDevice;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mReceiver = new PermissionBroadcastReceiver();
        mContext.registerReceiver(mReceiver, new IntentFilter(ACTION_PERMISSION_RESULT));
        mPackageManager = mContext.getPackageManager();
        mDevice = UiDevice.getInstance(getInstrumentation());
    }

    @Override
    protected void tearDown() throws Exception {
        mContext.unregisterReceiver(mReceiver);
        mDevice.removeWatcher(CRASH_WATCHER_ID);
        super.tearDown();
    }

    /** Return values of {@link #checkPermission} */
    int PERMISSION_DENIED = PackageManager.PERMISSION_DENIED;
    int PERMISSION_GRANTED = PackageManager.PERMISSION_GRANTED;
    int PERMISSION_DENIED_APP_OP = PackageManager.PERMISSION_DENIED - 1;

    /**
     * Correctly check a runtime permission. This also works for pre-m apps.
     */
    private int checkPermission(String permission, int uid, String packageName) {
        if (mContext.checkPermission(permission, -1, uid) == PackageManager.PERMISSION_DENIED) {
            return PERMISSION_DENIED;
        }

        if (mContext.getSystemService(AppOpsManager.class).noteProxyOpNoThrow(
                AppOpsManager.permissionToOp(permission), packageName, uid, null, null)
                != AppOpsManager.MODE_ALLOWED) {
            return PERMISSION_DENIED_APP_OP;
        }

        return PERMISSION_GRANTED;
    }

    public void testPermissionGrantState() throws Exception {
        assertSetPermissionGrantState(DevicePolicyManager.PERMISSION_GRANT_STATE_DENIED);
        assertPermissionGrantState(PackageManager.PERMISSION_DENIED);
        assertPermissionRequest(PackageManager.PERMISSION_DENIED);

        assertSetPermissionGrantState(DevicePolicyManager.PERMISSION_GRANT_STATE_DEFAULT);
        // Should stay denied
        assertPermissionGrantState(PackageManager.PERMISSION_DENIED);

        assertSetPermissionGrantState(DevicePolicyManager.PERMISSION_GRANT_STATE_GRANTED);
        assertPermissionGrantState(PackageManager.PERMISSION_GRANTED);
        assertPermissionRequest(PackageManager.PERMISSION_GRANTED);

        assertSetPermissionGrantState(DevicePolicyManager.PERMISSION_GRANT_STATE_DEFAULT);
        // Should stay granted
        assertPermissionGrantState(PackageManager.PERMISSION_GRANTED);
    }

    public void testPermissionPolicy() throws Exception {
        // reset permission to denied and unlocked
        assertSetPermissionGrantState(DevicePolicyManager.PERMISSION_GRANT_STATE_DENIED);
        assertSetPermissionGrantState(DevicePolicyManager.PERMISSION_GRANT_STATE_DEFAULT);

        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_AUTO_DENY);
        assertPermissionRequest(PackageManager.PERMISSION_DENIED);
        // permission should be locked, so changing the policy should not change the grant state
        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_PROMPT);
        assertPermissionRequest(PackageManager.PERMISSION_DENIED);
        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_AUTO_GRANT);
        assertPermissionRequest(PackageManager.PERMISSION_DENIED);

        // reset permission to denied and unlocked
        assertSetPermissionGrantState(DevicePolicyManager.PERMISSION_GRANT_STATE_DEFAULT);

        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_AUTO_GRANT);
        assertPermissionRequest(PackageManager.PERMISSION_GRANTED);
        // permission should be locked, so changing the policy should not change the grant state
        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_PROMPT);
        assertPermissionRequest(PackageManager.PERMISSION_GRANTED);
        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_AUTO_DENY);
        assertPermissionRequest(PackageManager.PERMISSION_GRANTED);

        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_PROMPT);
    }

    public void testPermissionMixedPolicies() throws Exception {
        assertSetPermissionGrantState(DevicePolicyManager.PERMISSION_GRANT_STATE_DENIED);
        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_AUTO_GRANT);
        assertPermissionRequest(PackageManager.PERMISSION_DENIED);

        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_AUTO_DENY);
        assertPermissionRequest(PackageManager.PERMISSION_DENIED);

        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_PROMPT);
        assertPermissionRequest(PackageManager.PERMISSION_DENIED);

        assertSetPermissionGrantState(DevicePolicyManager.PERMISSION_GRANT_STATE_GRANTED);
        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_AUTO_GRANT);
        assertPermissionRequest(PackageManager.PERMISSION_GRANTED);

        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_AUTO_DENY);
        assertPermissionRequest(PackageManager.PERMISSION_GRANTED);

        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_PROMPT);
        assertPermissionRequest(PackageManager.PERMISSION_GRANTED);
    }

    public void testAutoGrantMultiplePermissionsInGroup() throws Exception {
        // set policy to autogrant
        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_AUTO_GRANT);
        // both permissions should be granted
        assertPermissionRequest(READ_CONTACTS, PackageManager.PERMISSION_GRANTED);
        assertPermissionRequest(WRITE_CONTACTS, PackageManager.PERMISSION_GRANTED);
    }

    public void testPermissionGrantOfDisallowedPermissionWhileOtherPermIsGranted() throws Exception {
        assertSetPermissionGrantState(CUSTOM_PERM_A_NAME,
                DevicePolicyManager.PERMISSION_GRANT_STATE_GRANTED);
        assertSetPermissionGrantState(CUSTOM_PERM_B_NAME,
                DevicePolicyManager.PERMISSION_GRANT_STATE_DENIED);

        /*
         * CUSTOM_PERM_A_NAME and CUSTOM_PERM_B_NAME are in the same permission group and one is
         * granted the other one is not.
         *
         * It should not be possible to get the permission that was denied via policy granted by
         * requesting it.
         */
        assertPermissionRequest(CUSTOM_PERM_B_NAME, PackageManager.PERMISSION_DENIED);
    }

    @Suppress // Flakey.
    public void testPermissionPrompts() throws Exception {
        // register a crash watcher
        mDevice.registerWatcher(CRASH_WATCHER_ID, new UiWatcher() {
            @Override
            public boolean checkForCondition() {
                UiObject2 button = mDevice.findObject(CRASH_POPUP_BUTTON_SELECTOR);
                if (button != null) {
                    UiObject2 text = mDevice.findObject(CRASH_POPUP_TEXT_SELECTOR);
                    Log.d(TAG, "Removing an error dialog: " + text != null ? text.getText() : null);
                    button.click();
                    return true;
                }
                return false;
            }
        });
        mDevice.runWatchers();

        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_PROMPT);
        assertPermissionRequest(PackageManager.PERMISSION_DENIED, "permission_deny_button");
        assertPermissionRequest(PackageManager.PERMISSION_GRANTED, "permission_allow_button");
    }

    public void testPermissionUpdate_setDeniedState() throws Exception {
        assertEquals(mDevicePolicyManager.getPermissionGrantState(ADMIN_RECEIVER_COMPONENT,
                PERMISSION_APP_PACKAGE_NAME, READ_CONTACTS),
                DevicePolicyManager.PERMISSION_GRANT_STATE_DEFAULT);
        assertSetPermissionGrantState(DevicePolicyManager.PERMISSION_GRANT_STATE_DENIED);
    }

    public void testPermissionUpdate_setAutoDeniedPolicy() throws Exception {
        assertEquals(mDevicePolicyManager.getPermissionGrantState(ADMIN_RECEIVER_COMPONENT,
                PERMISSION_APP_PACKAGE_NAME, READ_CONTACTS),
                DevicePolicyManager.PERMISSION_GRANT_STATE_DEFAULT);
        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_AUTO_DENY);
        assertPermissionRequest(PackageManager.PERMISSION_DENIED);
    }

    public void testPermissionUpdate_checkDenied() throws Exception {
        assertPermissionRequest(PackageManager.PERMISSION_DENIED);
        assertPermissionGrantState(PackageManager.PERMISSION_DENIED);
    }

    public void testPermissionUpdate_setGrantedState() throws Exception {
        assertEquals(mDevicePolicyManager.getPermissionGrantState(ADMIN_RECEIVER_COMPONENT,
                PERMISSION_APP_PACKAGE_NAME, READ_CONTACTS),
                DevicePolicyManager.PERMISSION_GRANT_STATE_DEFAULT);
        assertSetPermissionGrantState(DevicePolicyManager.PERMISSION_GRANT_STATE_GRANTED);
    }

    public void testPermissionUpdate_setAutoGrantedPolicy() throws Exception {
        assertEquals(mDevicePolicyManager.getPermissionGrantState(ADMIN_RECEIVER_COMPONENT,
                PERMISSION_APP_PACKAGE_NAME, READ_CONTACTS),
                DevicePolicyManager.PERMISSION_GRANT_STATE_DEFAULT);
        assertSetPermissionPolicy(DevicePolicyManager.PERMISSION_POLICY_AUTO_GRANT);
        assertPermissionRequest(PackageManager.PERMISSION_GRANTED);
    }

    public void testPermissionUpdate_checkGranted() throws Exception {
        assertPermissionRequest(PackageManager.PERMISSION_GRANTED);
        assertPermissionGrantState(PackageManager.PERMISSION_GRANTED);
    }

    public void testPermissionGrantStateAppPreMDeviceAdminPreQ() throws Exception {
        // These tests are to make sure that pre-M apps are not granted/denied runtime permissions
        // by a profile owner that targets pre-Q
        assertCannotSetPermissionGrantStateAppPreM(
                DevicePolicyManager.PERMISSION_GRANT_STATE_DENIED);
        assertCannotSetPermissionGrantStateAppPreM(
                DevicePolicyManager.PERMISSION_GRANT_STATE_GRANTED);
    }

    public void testPermissionGrantStatePreMApp() throws Exception {
        // These tests are to make sure that pre-M apps can be granted/denied runtime permissions
        // by a profile owner targets Q or later
        assertCanSetPermissionGrantStateAppPreM(DevicePolicyManager.PERMISSION_GRANT_STATE_DENIED);
        assertCanSetPermissionGrantStateAppPreM(DevicePolicyManager.PERMISSION_GRANT_STATE_GRANTED);
    }

    public void testPermissionGrantState_developmentPermission() throws Exception {
        assertFailedToSetDevelopmentPermissionGrantState(
                DevicePolicyManager.PERMISSION_GRANT_STATE_DENIED);
        assertFailedToSetDevelopmentPermissionGrantState(
                DevicePolicyManager.PERMISSION_GRANT_STATE_DEFAULT);
        assertFailedToSetDevelopmentPermissionGrantState(
                DevicePolicyManager.PERMISSION_GRANT_STATE_GRANTED);
    }

    public void testUserNotifiedOfLocationPermissionGrant() throws Exception {
        for (String locationPermission : LOCATION_PERMISSIONS) {
            CountDownLatch notificationLatch = initLocationPermissionNotificationLatch();

            assertSetPermissionGrantState(locationPermission,
                    DevicePolicyManager.PERMISSION_GRANT_STATE_GRANTED);

            assertPermissionGrantState(locationPermission, PackageManager.PERMISSION_GRANTED);
            assertTrue(String.format("Did not receive notification for permission %s",
                    locationPermission), notificationLatch.await(60, TimeUnit.SECONDS));
            NotificationListener.getInstance().clearListeners();
        }
    }

    private void assertPermissionRequest(int expected) throws Exception {
        assertPermissionRequest(READ_CONTACTS, expected);
    }

    private void assertPermissionRequest(String permission, int expected) throws Exception {
        assertPermissionRequest(permission, expected, null);
    }

    private void assertPermissionRequest(int expected, String buttonResource)
            throws Exception {
        assertPermissionRequest(READ_CONTACTS, expected, buttonResource);
    }

    private void assertSetPermissionGrantState(int value) throws Exception {
        assertSetPermissionGrantState(READ_CONTACTS, value);
    }

    private void assertPermissionGrantState(int expected) throws Exception {
        assertPermissionGrantState(READ_CONTACTS, expected);
    }

    private void assertCannotSetPermissionGrantStateAppPreM(int value) throws Exception {
        assertCannotSetPermissionGrantStateAppPreM(READ_CONTACTS, value);
    }

    private void assertCanSetPermissionGrantStateAppPreM(int value) throws Exception {
        assertCanSetPermissionGrantStateAppPreM(READ_CONTACTS, value);
    }

    private void assertPermissionRequest(String permission, int expected, String buttonResource)
            throws Exception {
        Intent launchIntent = new Intent();
        launchIntent.setComponent(new ComponentName(PERMISSION_APP_PACKAGE_NAME,
                PERMISSIONS_ACTIVITY_NAME));
        launchIntent.putExtra(EXTRA_PERMISSION, permission);
        launchIntent.setAction(ACTION_REQUEST_PERMISSION);
        launchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        mContext.startActivity(launchIntent);
        pressPermissionPromptButton(buttonResource);
        assertEquals(expected, mReceiver.waitForBroadcast());
        assertEquals(expected, mPackageManager.checkPermission(permission,
                PERMISSION_APP_PACKAGE_NAME));
    }

    private void assertPermissionGrantState(String permission, int expected) throws Exception {
        assertEquals(expected, mPackageManager.checkPermission(permission,
                PERMISSION_APP_PACKAGE_NAME));
        Intent launchIntent = new Intent();
        launchIntent.setComponent(new ComponentName(PERMISSION_APP_PACKAGE_NAME,
                PERMISSIONS_ACTIVITY_NAME));
        launchIntent.putExtra(EXTRA_PERMISSION, permission);
        launchIntent.setAction(ACTION_CHECK_HAS_PERMISSION);
        launchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        mContext.startActivity(launchIntent);
        assertEquals(expected, mReceiver.waitForBroadcast());
    }

    private void assertSetPermissionPolicy(int value) throws Exception {
        mDevicePolicyManager.setPermissionPolicy(ADMIN_RECEIVER_COMPONENT,
                value);
        assertEquals(mDevicePolicyManager.getPermissionPolicy(ADMIN_RECEIVER_COMPONENT),
                value);
    }

    private void assertSetPermissionGrantState(String permission, int value) throws Exception {
        mDevicePolicyManager.setPermissionGrantState(ADMIN_RECEIVER_COMPONENT,
                PERMISSION_APP_PACKAGE_NAME, permission,
                value);
        assertEquals(mDevicePolicyManager.getPermissionGrantState(ADMIN_RECEIVER_COMPONENT,
                PERMISSION_APP_PACKAGE_NAME, permission),
                value);
    }

    private void assertFailedToSetDevelopmentPermissionGrantState(int value) throws Exception {
        assertFalse(mDevicePolicyManager.setPermissionGrantState(ADMIN_RECEIVER_COMPONENT,
                PERMISSION_APP_PACKAGE_NAME, DEVELOPMENT_PERMISSION, value));
        assertEquals(mDevicePolicyManager.getPermissionGrantState(ADMIN_RECEIVER_COMPONENT,
                PERMISSION_APP_PACKAGE_NAME, DEVELOPMENT_PERMISSION),
                DevicePolicyManager.PERMISSION_GRANT_STATE_DEFAULT);
        assertEquals(mPackageManager.checkPermission(DEVELOPMENT_PERMISSION,
                PERMISSION_APP_PACKAGE_NAME),
                PackageManager.PERMISSION_DENIED);
    }

    private void assertCannotSetPermissionGrantStateAppPreM(String permission, int value) throws Exception {
        assertFalse(mDevicePolicyManager.setPermissionGrantState(ADMIN_RECEIVER_COMPONENT,
                SIMPLE_PRE_M_APP_PACKAGE_NAME, permission,
                value));
        assertEquals(mDevicePolicyManager.getPermissionGrantState(ADMIN_RECEIVER_COMPONENT,
                SIMPLE_PRE_M_APP_PACKAGE_NAME, permission),
                DevicePolicyManager.PERMISSION_GRANT_STATE_DEFAULT);

        // Install time permissions should always be granted
        PackageInfo packageInfo = mContext.getPackageManager().getPackageInfo(
                SIMPLE_PRE_M_APP_PACKAGE_NAME, 0);
        assertEquals(PERMISSION_GRANTED,
                checkPermission(permission, packageInfo.applicationInfo.uid,
                        SIMPLE_PRE_M_APP_PACKAGE_NAME));
    }

    private void assertCanSetPermissionGrantStateAppPreM(String permission, int value) throws Exception {
        assertTrue(mDevicePolicyManager.setPermissionGrantState(ADMIN_RECEIVER_COMPONENT,
                SIMPLE_PRE_M_APP_PACKAGE_NAME, permission,
                value));
        assertEquals(mDevicePolicyManager.getPermissionGrantState(ADMIN_RECEIVER_COMPONENT,
                SIMPLE_PRE_M_APP_PACKAGE_NAME, permission),
                value);

        // Install time permissions should always be granted
        assertEquals(mPackageManager.checkPermission(permission,
                SIMPLE_PRE_M_APP_PACKAGE_NAME),
                PackageManager.PERMISSION_GRANTED);

        PackageInfo packageInfo = mContext.getPackageManager().getPackageInfo(
                SIMPLE_PRE_M_APP_PACKAGE_NAME, 0);

        // For pre-M apps the access to the data might be prevented via app-ops. Hence check that
        // they are correctly set
        boolean isGranted = (checkPermission(permission, packageInfo.applicationInfo.uid,
                SIMPLE_PRE_M_APP_PACKAGE_NAME) == PERMISSION_GRANTED);
        switch (value) {
            case DevicePolicyManager.PERMISSION_GRANT_STATE_GRANTED:
                assertTrue(isGranted);
                break;
            case DevicePolicyManager.PERMISSION_GRANT_STATE_DENIED:
                assertFalse(isGranted);
                break;
            default:
                fail("unsupported policy value");
        }
    }

    private void pressPermissionPromptButton(String resName) throws Exception {
        if (resName == null) {
            return;
        }

        BySelector selector = By
                .clazz(android.widget.Button.class.getName())
                .res("com.android.packageinstaller", resName);
        mDevice.wait(Until.hasObject(selector), 5000);
        UiObject2 button = mDevice.findObject(selector);
        assertNotNull("Couldn't find button with resource id: " + resName, button);
        button.click();
    }

    private class PermissionBroadcastReceiver extends BroadcastReceiver {
        private BlockingQueue<Integer> mQueue = new ArrayBlockingQueue<Integer> (1);

        @Override
        public void onReceive(Context context, Intent intent) {
            Integer result = new Integer(intent.getIntExtra(EXTRA_GRANT_STATE, PERMISSION_ERROR));
            Log.d(TAG, "Grant state received " + result);
            assertTrue(mQueue.add(result));
        }

        public int waitForBroadcast() throws Exception {
            Integer result = mQueue.poll(30, TimeUnit.SECONDS);
            mQueue.clear();
            assertNotNull(result);
            Log.d(TAG, "Grant state retrieved " + result.intValue());
            return result.intValue();
        }
    }

    private CountDownLatch initLocationPermissionNotificationLatch() {
        CountDownLatch notificationCounterLatch = new CountDownLatch(1);
        NotificationListener.getInstance().addListener((notification) -> {
            if (notification.getPackageName().equals(
                    mPackageManager.getPermissionControllerPackageName()) &&
                    notification.getNotification().getChannelId().equals(
                            AUTO_GRANTED_PERMISSIONS_CHANNEL_ID)) {
                notificationCounterLatch.countDown();
            }
        });
        return notificationCounterLatch;
    }
}
