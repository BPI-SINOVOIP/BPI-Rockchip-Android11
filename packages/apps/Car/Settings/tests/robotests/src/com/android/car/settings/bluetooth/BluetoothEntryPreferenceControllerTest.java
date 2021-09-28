/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.car.settings.bluetooth;

import static android.content.pm.PackageManager.FEATURE_BLUETOOTH;
import static android.os.UserManager.DISALLOW_BLUETOOTH;

import static com.android.car.settings.common.PreferenceController.AVAILABLE;
import static com.android.car.settings.common.PreferenceController.DISABLED_FOR_USER;
import static com.android.car.settings.common.PreferenceController.UNSUPPORTED_ON_DEVICE;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.os.UserHandle;
import android.os.UserManager;

import com.android.car.settings.common.PreferenceControllerTestHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowUserManager;

/** Unit test for {@link BluetoothEntryPreferenceController}. */
@RunWith(RobolectricTestRunner.class)
public class BluetoothEntryPreferenceControllerTest {

    private Context mContext;
    private BluetoothEntryPreferenceController mController;
    private UserHandle mMyUserHandle;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mMyUserHandle = UserHandle.of(UserHandle.myUserId());
        mController = new PreferenceControllerTestHelper<>(RuntimeEnvironment.application,
                BluetoothEntryPreferenceController.class).getController();
    }

    @Test
    public void getAvailabilityStatus_bluetoothAvailable_available() {
        Shadows.shadowOf(mContext.getPackageManager()).setSystemFeature(
                FEATURE_BLUETOOTH, /* supported= */ true);

        assertThat(mController.getAvailabilityStatus()).isEqualTo(AVAILABLE);
    }

    @Test
    public void getAvailabilityStatus_bluetoothAvailable_disallowBluetooth_disabledForUser() {
        Shadows.shadowOf(mContext.getPackageManager()).setSystemFeature(
                FEATURE_BLUETOOTH, /* supported= */ true);
        getShadowUserManager().setUserRestriction(mMyUserHandle, DISALLOW_BLUETOOTH, true);

        assertThat(mController.getAvailabilityStatus()).isEqualTo(DISABLED_FOR_USER);
    }

    @Test
    public void getAvailabilityStatus_bluetoothNotAvailable_unsupportedOnDevice() {
        Shadows.shadowOf(mContext.getPackageManager()).setSystemFeature(
                FEATURE_BLUETOOTH, /* supported= */ false);

        assertThat(mController.getAvailabilityStatus()).isEqualTo(UNSUPPORTED_ON_DEVICE);
    }

    private ShadowUserManager getShadowUserManager() {
        return Shadow.extract(UserManager.get(mContext));
    }
}
