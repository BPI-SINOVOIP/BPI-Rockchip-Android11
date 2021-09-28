/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.server.wm;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;
import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;

import android.app.ActivityManager;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.RemoteCallback;
import android.os.UserHandle;
import android.os.UserManager;
import android.platform.test.annotations.Presubmit;

import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@Presubmit
public class StartActivityAsUserTests {
    static final String EXTRA_CALLBACK = "callback";
    static final String KEY_USER_ID = "user id";

    private static final String PACKAGE = "android.server.wm.cts";
    private static final String CLASS = "android.server.wm.StartActivityAsUserActivity";
    private static final int INVALID_STACK = -1;
    private static final boolean SUPPORTS_MULTIPLE_USERS = UserManager.supportsMultipleUsers();

    private final Context mContext = InstrumentationRegistry.getInstrumentation().getContext();
    private final ActivityManager mAm = mContext.getSystemService(ActivityManager.class);

    private int mSecondUserId;
    private WindowManagerStateHelper mAmWmState = new WindowManagerStateHelper();

    @Before
    public void createSecondUser() {
        assumeTrue(SUPPORTS_MULTIPLE_USERS);

        final String output = runShellCommand("pm create-user --profileOf " + mContext.getUserId()
                + " user2");
        mSecondUserId = Integer.parseInt(output.substring(output.lastIndexOf(" ")).trim());
        assertThat(mSecondUserId).isNotEqualTo(0);
        runShellCommand("pm install-existing --user " + mSecondUserId + " android.server.wm.cts");
    }

    @After
    public void removeSecondUser() {
        runShellCommand("pm remove-user " + mSecondUserId);
    }

    @Test
    public void startActivityValidUser() throws Throwable {
        int[] secondUser= {-1};
        CountDownLatch latch = new CountDownLatch(1);
        RemoteCallback cb = new RemoteCallback((Bundle result) -> {
            secondUser[0] = result.getInt(KEY_USER_ID);
            latch.countDown();
        });

        final Intent intent = new Intent(mContext, StartActivityAsUserActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(EXTRA_CALLBACK, cb);

        final CountDownLatch returnToOriginalUserLatch = new CountDownLatch(1);
        mContext.registerReceiver(new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                mContext.unregisterReceiver(this);
                returnToOriginalUserLatch.countDown();
            }
        }, new IntentFilter(Intent.ACTION_USER_FOREGROUND));

        UserHandle secondUserHandle = UserHandle.of(mSecondUserId);

        try {
            runWithShellPermissionIdentity(() -> {
                mContext.startActivityAsUser(intent, secondUserHandle);
                mAm.switchUser(secondUserHandle);
                try {
                    latch.await(5, TimeUnit.SECONDS);
                } finally {
                    mAm.switchUser(mContext.getUser());
                }
            });
        } catch (RuntimeException e) {
            throw e.getCause();
        }

        assertThat(secondUser[0]).isEqualTo(mSecondUserId);

        // Avoid the race between switch-user and remove-user.
        returnToOriginalUserLatch.await(20, TimeUnit.SECONDS);
    }

    @Test
    public void startActivityInvalidUser() {
        UserHandle secondUserHandle = UserHandle.of(mSecondUserId * 100);
        int[] stackId = {-1};

        final Intent intent = new Intent(mContext, StartActivityAsUserActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        runWithShellPermissionIdentity(() -> {
            mContext.startActivityAsUser(intent, secondUserHandle);
            WindowManagerState amState = mAmWmState;
            amState.computeState();
            ComponentName componentName = ComponentName.createRelative(PACKAGE, CLASS);
            stackId[0] = amState.getStackIdByActivity(componentName);
        });

        assertThat(stackId[0]).isEqualTo(INVALID_STACK);
    }
}
