/*
 * Copyright (C) 2020 Google Inc.
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
package android.packageinstaller.uninstall.cts;

import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;

import static com.android.compatibility.common.util.SystemUtil.eventually;
import static com.android.compatibility.common.util.SystemUtil.runShellCommand;
import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;
import static com.android.compatibility.common.util.UiAutomatorUtils.waitFindObject;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.ActivityTaskManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInstaller;
import android.content.pm.PackageManager;
import android.server.wm.WindowManagerStateHelper;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;

import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.AppOpsUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.io.IOException;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

public class UninstallPinnedTest {

    private static final String APK =
            "/data/local/tmp/cts/uninstall/CtsSelfUninstallingTestApp.apk";
    private static final String TEST_PKG_NAME = "android.packageinstaller.selfuninstalling.cts";
    private static final String TEST_ACTIVITY_NAME = TEST_PKG_NAME + ".SelfUninstallActivity";
    private static final String ACTION_SELF_UNINSTALL =
            "android.packageinstaller.selfuninstalling.cts.action.SELF_UNINSTALL";
    private static final ComponentName COMPONENT = new ComponentName(TEST_PKG_NAME, TEST_ACTIVITY_NAME);
    public static final String CALLBACK_ACTION =
            "android.packageinstaller.uninstall.cts.action.UNINSTALL_PINNED_CALLBACK";

    private WindowManagerStateHelper mWmState = new WindowManagerStateHelper();
    private Context mContext;
    private UiDevice mUiDevice;
    private ActivityTaskManager mActivityTaskManager;

    @Before
    public void setup() throws Exception {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mActivityTaskManager = mContext.getSystemService(ActivityTaskManager.class);

        // Unblock UI
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        if (!mUiDevice.isScreenOn()) {
            mUiDevice.wakeUp();
        }
        mUiDevice.executeShellCommand("wm dismiss-keyguard");
        AppOpsUtils.reset(mContext.getPackageName());

        runShellCommand("pm install -r --force-queryable " + APK);

        Intent i = new Intent()
                .setComponent(COMPONENT)
                .addFlags(FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(i);

        pinActivity(COMPONENT);
    }

    @Test
    public void testAppCantUninstallItself() throws Exception {
        mUiDevice.waitForIdle();
        eventually(() -> {
            mContext.sendBroadcast(new Intent(ACTION_SELF_UNINSTALL));
            waitFindObject(By.text("OK")).click();
        }, 60000);

        mUiDevice.waitForIdle();

        Thread.sleep(5000);

        assertTrue("Package was uninstalled.", isInstalled());
    }

    @Test
    public void testCantUninstallAppDirectly() {
        CompletableFuture<Integer> statusFuture = new CompletableFuture<>();
        mContext.registerReceiver(new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                statusFuture.complete(
                        intent.getIntExtra(PackageInstaller.EXTRA_STATUS, Integer.MAX_VALUE));
            }
        }, new IntentFilter(CALLBACK_ACTION));

        runWithShellPermissionIdentity(() -> {
            mContext.getPackageManager().getPackageInstaller().uninstall(TEST_PKG_NAME,
                    PendingIntent.getBroadcast(mContext, 1,
                            new Intent(CALLBACK_ACTION),
                            0).getIntentSender());
        });

        int status = statusFuture.join();
        assertEquals("Wrong code received", PackageInstaller.STATUS_FAILURE_BLOCKED, status);
        assertTrue("Package was uninstalled.", isInstalled());
    }

    @Test
    public void testCantUninstallWithShell() throws Exception {
        mUiDevice.executeShellCommand("pm uninstall " + TEST_PKG_NAME);
        assertTrue("Package was uninstalled.", isInstalled());
    }

    @After
    public void unpinAndUninstall() throws IOException {
        runWithShellPermissionIdentity(() -> mActivityTaskManager.stopSystemLockTaskMode());
        mUiDevice.executeShellCommand("pm uninstall " + TEST_PKG_NAME);
    }

    private void pinActivity(ComponentName component) {
        mWmState.computeState();

        int stackId = mWmState.getStackIdByActivity(component);

        runWithShellPermissionIdentity(() -> {
            mActivityTaskManager.startSystemLockTaskMode(
                    stackId);
        });
    }

    private boolean isInstalled() {
        try {
            mContext.getPackageManager().getPackageInfo(TEST_PKG_NAME, 0);
            return true;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }
}
