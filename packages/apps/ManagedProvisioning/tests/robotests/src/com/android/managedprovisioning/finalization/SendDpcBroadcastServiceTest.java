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
 * limitations under the License
 */

package com.android.managedprovisioning.finalization;

import static android.app.admin.DeviceAdminReceiver.ACTION_PROFILE_PROVISIONING_COMPLETE;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISIONING_SUCCESSFUL;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_PROFILE;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_ADMIN_EXTRAS_BUNDLE;
import static android.content.pm.UserInfo.FLAG_MANAGED_PROFILE;

import static com.android.managedprovisioning.finalization.SendDpcBroadcastService.EXTRA_PROVISIONING_PARAMS;

import static com.google.common.truth.Truth.assertThat;

import static org.robolectric.Shadows.shadowOf;

import android.annotation.NonNull;
import android.app.Application;
import android.app.ApplicationPackageManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.PersistableBundle;
import android.os.UserManager;

import com.android.managedprovisioning.common.ProvisionLogger;
import com.android.managedprovisioning.model.ProvisioningParams;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.android.controller.ServiceController;

import java.util.List;
import java.util.Objects;

/**
 * Unit tests for {@link SendDpcBroadcastService}.
 */
@RunWith(RobolectricTestRunner.class)
public class SendDpcBroadcastServiceTest {

    private final Context mContext = RuntimeEnvironment.application;
    private final UserManager mUserManager = mContext.getSystemService(UserManager.class);
    private final ApplicationPackageManager mPackageManager =
            (ApplicationPackageManager) mContext.getApplicationContext().getPackageManager();
    private ServiceController<SendDpcBroadcastService> mController;

    private static final int USER_HANDLE = 0;
    private static final int PROFILE_USER_HANDLE = 2;

    private static final String TEST_MDM_PACKAGE_NAME = "mdm.package.name";
    private static final String TEST_MDM_ADMIN_RECEIVER = TEST_MDM_PACKAGE_NAME + ".AdminReceiver";
    private static final ComponentName TEST_MDM_ADMIN = new ComponentName(TEST_MDM_PACKAGE_NAME,
            TEST_MDM_ADMIN_RECEIVER);
    private static final PersistableBundle TEST_MDM_EXTRA_BUNDLE = new PersistableBundle();
    private static final ProvisioningParams PARAMS = ProvisioningParams.Builder.builder()
            .setProvisioningAction(ACTION_PROVISION_MANAGED_PROFILE)
            .setDeviceAdminComponentName(TEST_MDM_ADMIN)
            .setAdminExtrasBundle(TEST_MDM_EXTRA_BUNDLE)
            .build();

    private static final String BOOL_KEY = "mybool";
    private static final boolean BOOL_VALUE = false;
    private static final String STRING_KEY = "mystring";
    private static final String STRING_VALUE = "some string";
    private static final String INT_KEY = "myint";
    private static final int INT_VALUE = 1234;

    static {
        TEST_MDM_EXTRA_BUNDLE.putBoolean(BOOL_KEY, BOOL_VALUE);
        TEST_MDM_EXTRA_BUNDLE.putString(STRING_KEY, STRING_VALUE);
        TEST_MDM_EXTRA_BUNDLE.putInt(INT_KEY, INT_VALUE);
    }

    @Before
    public void setUp() {
        final Intent intent =
                new Intent(RuntimeEnvironment.application, SendDpcBroadcastService.class)
                .putExtra(EXTRA_PROVISIONING_PARAMS, PARAMS);
        mController = Robolectric.buildService(SendDpcBroadcastService.class, intent);

        shadowOf(mUserManager).addProfile(
                /* userHandle= */ USER_HANDLE,
                /* profileUserHandle= */ PROFILE_USER_HANDLE,
                /* profileName= */ "profile",
                /* profileFlags= */ FLAG_MANAGED_PROFILE);
    }

    @Test
    public void onStartCommand_launchesDpc() {
        shadowOf(mPackageManager)
                .addResolveInfoForIntent(createDpcLaunchIntent(), new ResolveInfo());

        mController.startCommand(/* flags= */ 0, /* startId= */ 0);

        Intent launchedActivityIntent = shadowOf((Application) mContext).getNextStartedActivity();
        assertThat(launchedActivityIntent.getPackage()).isEqualTo(TEST_MDM_PACKAGE_NAME);
        assertThat(launchedActivityIntent.getAction()).isEqualTo(ACTION_PROVISIONING_SUCCESSFUL);
        assertThat(launchedActivityIntent.getFlags()).isEqualTo(Intent.FLAG_ACTIVITY_NEW_TASK);
        assertExtras(launchedActivityIntent);
    }

    @Test
    public void onStartCommand_sendsOrderedBroadcast() {
        shadowOf(mPackageManager)
                .addResolveInfoForIntent(createDpcLaunchIntent(), new ResolveInfo());

        mController.startCommand(/* flags= */ 0, /* startId= */ 0);

        Intent broadcastIntent = getBroadcastIntentForAction(ACTION_PROFILE_PROVISIONING_COMPLETE);
        assertThat(broadcastIntent.getComponent()).isEqualTo(TEST_MDM_ADMIN);
        assertExtras(broadcastIntent);
    }

    private Intent getBroadcastIntentForAction(String action) {
        List<Intent> broadcastIntents = shadowOf((Application) mContext).getBroadcastIntents();
        for (Intent intent : broadcastIntents) {
            if (intent.getAction().equals(action)) {
                return intent;
            }
        }
        return null;
    }

    private void assertExtras(Intent intent) {
        final PersistableBundle bundle =
                (PersistableBundle) intent.getExtra(EXTRA_PROVISIONING_ADMIN_EXTRAS_BUNDLE);
        assertThat(
                bundle.getBoolean(BOOL_KEY)).isEqualTo(TEST_MDM_EXTRA_BUNDLE.getBoolean(BOOL_KEY));
        assertThat(
                bundle.getInt(INT_KEY)).isEqualTo(TEST_MDM_EXTRA_BUNDLE.getInt(INT_KEY));
        assertThat(
                bundle.getInt(STRING_KEY)).isEqualTo(TEST_MDM_EXTRA_BUNDLE.getInt(STRING_KEY));
    }

    private Intent createDpcLaunchIntent() {
        final Intent intent = new Intent(ACTION_PROVISIONING_SUCCESSFUL);
        final String packageName = PARAMS.inferDeviceAdminPackageName();
        intent.setPackage(packageName);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(EXTRA_PROVISIONING_ADMIN_EXTRAS_BUNDLE, PARAMS.adminExtrasBundle);
        return intent;
    }
}
