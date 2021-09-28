/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.cts.launchertests;

import static com.android.server.pm.shortcutmanagertest.ShortcutManagerTestUtils.setDefaultLauncher;

import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertNull;
import static junit.framework.Assert.assertTrue;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertThrows;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.os.UserHandle;
import android.os.UserManager;
import android.support.test.uiautomator.UiDevice;
import android.text.TextUtils;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.BlockingBroadcastReceiver;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * Test that runs {@link UserManager#trySetQuietModeEnabled(boolean, UserHandle)} API
 * against valid target user.
 */
@RunWith(AndroidJUnit4.class)
public class QuietModeTest {
    private static final String PARAM_TARGET_USER = "TARGET_USER";
    private static final String PARAM_ORIGINAL_DEFAULT_LAUNCHER = "ORIGINAL_DEFAULT_LAUNCHER";
    private static final ComponentName LAUNCHER_ACTIVITY =
            new ComponentName(
                    "com.android.cts.launchertests.support",
                    "com.android.cts.launchertests.support.LauncherActivity");

    private static final ComponentName COMMAND_RECEIVER =
            new ComponentName(
                    "com.android.cts.launchertests.support",
                    "com.android.cts.launchertests.support.QuietModeCommandReceiver");
    private static final String EXTRA_FLAGS = "quiet_mode_flags";

    private UserManager mUserManager;
    private UserHandle mTargetUser;
    private Context mContext;
    private String mOriginalLauncher;
    private UiDevice mUiDevice;

    @Before
    public void setupUserManager() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mUserManager = mContext.getSystemService(UserManager.class);
    }

    @Before
    public void readParams() {
        Context context = InstrumentationRegistry.getContext();
        Bundle arguments = InstrumentationRegistry.getArguments();
        UserManager userManager = context.getSystemService(UserManager.class);
        final int userSn = Integer.parseInt(arguments.getString(PARAM_TARGET_USER));
        mTargetUser = userManager.getUserForSerialNumber(userSn);
        mOriginalLauncher = arguments.getString(PARAM_ORIGINAL_DEFAULT_LAUNCHER);
    }

    @Before
    public void wakeupDeviceAndUnlock() throws Exception {
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mUiDevice.wakeUp();
        mUiDevice.pressMenu();
    }

    @Before
    @After
    public void revertToDefaultLauncher() throws Exception {
        if (TextUtils.isEmpty(mOriginalLauncher)) {
            return;
        }
        setDefaultLauncher(InstrumentationRegistry.getInstrumentation(), mOriginalLauncher);
        startActivitySync(mOriginalLauncher);
    }

    @Test
    public void testTryEnableQuietMode_defaultForegroundLauncher() throws Exception {
        setTestAppAsDefaultLauncher();
        startLauncherActivityInTestApp();

        Intent intent = trySetQuietModeEnabled(true);
        assertNotNull("Failed to receive ACTION_MANAGED_PROFILE_UNAVAILABLE broadcast", intent);
        assertTrue(mUserManager.isQuietModeEnabled(mTargetUser));

        intent = trySetQuietModeEnabled(false);
        assertNotNull("Failed to receive ACTION_MANAGED_PROFILE_AVAILABLE broadcast", intent);
        assertFalse(mUserManager.isQuietModeEnabled(mTargetUser));
    }

    @Test
    public void testTryEnableQuietMode_notForegroundLauncher() throws InterruptedException {
        setTestAppAsDefaultLauncher();

        assertThrows(SecurityException.class, () -> trySetQuietModeEnabled(true));
        assertFalse(mUserManager.isQuietModeEnabled(mTargetUser));
    }

    @Test
    public void testTryEnableQuietMode_notDefaultLauncher() throws Exception {
        startLauncherActivityInTestApp();

        assertThrows(SecurityException.class, () -> trySetQuietModeEnabled(true));
        assertFalse(mUserManager.isQuietModeEnabled(mTargetUser));
    }

    @Test
    public void testTryEnableQuietMode_noCredentialRequest() throws Exception {
        setTestAppAsDefaultLauncher();
        startLauncherActivityInTestApp();

        Intent intent = trySetQuietModeEnabled(true,
                UserManager.QUIET_MODE_DISABLE_ONLY_IF_CREDENTIAL_NOT_REQUIRED, true);
        assertNotNull("Failed to receive ACTION_MANAGED_PROFILE_UNAVAILABLE broadcast", intent);
        assertTrue(mUserManager.isQuietModeEnabled(mTargetUser));

        waitForUserNotRunning(30);

        intent = trySetQuietModeEnabled(false,
                UserManager.QUIET_MODE_DISABLE_ONLY_IF_CREDENTIAL_NOT_REQUIRED, false);
        assertNull("Received ACTION_MANAGED_PROFILE_AVAILABLE broadcast", intent);
        assertTrue(mUserManager.isQuietModeEnabled(mTargetUser));
    }

    // Note: The timeout is in seconds.
    private void waitForUserNotRunning(int timeout) {
        for(int i = 0 ; i < 2 * timeout ; i++) {
            if (!mUserManager.isUserRunning(mTargetUser)) {
                break;
            }
            SystemClock.sleep(500);
        }
        assertFalse("Cannot get the profile stopped.", mUserManager.isUserRunning(mTargetUser));
    }

    private Intent trySetQuietModeEnabled(boolean enabled, int flags,
            boolean expectsCredentialsNotNeeded) throws Exception {
        return trySetQuietModeEnabled(enabled, true, flags, expectsCredentialsNotNeeded);
    }

    @Test
    public void testTryEnableQuietMode() throws Exception {
        setTestAppAsDefaultLauncher();
        startLauncherActivityInTestApp();

        Intent intent = trySetQuietModeEnabled(true);
        assertNotNull("Failed to receive ACTION_MANAGED_PROFILE_UNAVAILABLE broadcast", intent);
        assertTrue(mUserManager.isQuietModeEnabled(mTargetUser));
    }

    @Test
    public void testTryDisableQuietMode() throws Exception {
        setTestAppAsDefaultLauncher();
        startLauncherActivityInTestApp();

        Intent intent = trySetQuietModeEnabled(false);
        assertNotNull("Failed to receive ACTION_MANAGED_PROFILE_AVAILABLE broadcast", intent);
        assertFalse(mUserManager.isQuietModeEnabled(mTargetUser));
    }

    private Intent trySetQuietModeEnabled(boolean enabled) throws Exception {
        return trySetQuietModeEnabled(enabled, false, 0, true);
    }

    private Intent trySetQuietModeEnabled(boolean enabled, boolean hasFlags, int flags,
            boolean expectsCredentialsNotNeeded) throws Exception {
        final String action = enabled
                ? Intent.ACTION_MANAGED_PROFILE_UNAVAILABLE
                : Intent.ACTION_MANAGED_PROFILE_AVAILABLE;

        BlockingBroadcastReceiver receiver =
                new BlockingBroadcastReceiver(mContext, action);
        try {
            receiver.register();

            boolean credentialNotNeeded = askLauncherSupportAppToSetQuietMode(enabled, hasFlags,
                    flags);
            assertEquals(credentialNotNeeded, expectsCredentialsNotNeeded);
            return receiver.awaitForBroadcast();
        } finally {
            receiver.unregisterQuietly();
        }
    }

    /**
     * Ask launcher support test app to set quiet mode by sending broadcast.
     * <p>
     * We cannot simply make this package the launcher and call the API because instrumentation
     * process would always considered to be in the foreground. The trick here is to send
     * broadcast to another test app which is launcher itself and call the API through it.
     * The receiver will then send back the result, and it should be either true, false or
     * security-exception.
     * <p>
     * All the constants defined here should be aligned with
     * com.android.cts.launchertests.support.QuietModeCommandReceiver.
     */
    private boolean askLauncherSupportAppToSetQuietMode(boolean enabled, boolean hasFlags, int flags) throws Exception {
        Intent intent = new Intent("toggle_quiet_mode");
        intent.setComponent(COMMAND_RECEIVER);
        intent.putExtra("quiet_mode", enabled);
        intent.putExtra(Intent.EXTRA_USER, mTargetUser);
        if (hasFlags) {
            intent.putExtra(EXTRA_FLAGS, flags);
        }

        // Ask launcher support app to set quiet mode by sending broadcast.
        LinkedBlockingQueue<String> blockingQueue = new LinkedBlockingQueue<>();
        mContext.sendOrderedBroadcast(intent, null, new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                blockingQueue.offer(getResultData());
            }
        }, null, 0, "", null);

        // Wait for the result.
        String result = null;
        for (int i = 0; i < 10; i++) {
            // Broadcast won't be delivered when the device is sleeping, so wake up the device
            // in between each attempt.
            wakeupDeviceAndUnlock();
            result = blockingQueue.poll(10, TimeUnit.SECONDS);
            if (!TextUtils.isEmpty(result)) {
                break;
            }
        }

        // Parse the result.
        assertNotNull(result);
        if ("true".equalsIgnoreCase(result)) {
            return true;
        } else if ("false".equalsIgnoreCase(result)) {
            return false;
        } else if ("security-exception".equals(result)) {
            throw new SecurityException();
        }
        throw new IllegalStateException("Unexpected result : " + result);
    }

    private void startActivitySync(String activity) throws Exception {
        mUiDevice.executeShellCommand("am start -W -n " + activity);
    }

    /**
     * Start the launcher activity in the test app to make it foreground.
     */
    private void startLauncherActivityInTestApp() throws Exception {
        startActivitySync(LAUNCHER_ACTIVITY.flattenToString());
    }

    private void setTestAppAsDefaultLauncher() {
        setDefaultLauncher(
                InstrumentationRegistry.getInstrumentation(),
                LAUNCHER_ACTIVITY.flattenToString());
    }
}

