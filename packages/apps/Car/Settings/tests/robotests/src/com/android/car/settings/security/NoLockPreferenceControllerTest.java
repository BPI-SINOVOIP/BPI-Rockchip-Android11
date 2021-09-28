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

package com.android.car.settings.security;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.verify;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.os.UserHandle;

import androidx.lifecycle.Lifecycle;
import androidx.preference.Preference;

import com.android.car.settings.common.ConfirmationDialogFragment;
import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.testutils.ShadowLockPatternUtils;
import com.android.internal.widget.LockscreenCredential;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowLockPatternUtils.class})
public class NoLockPreferenceControllerTest {

    private static final LockscreenCredential TEST_CURRENT_PASSWORD =
            LockscreenCredential.createPassword("test_password");

    private Context mContext;
    private PreferenceControllerTestHelper<NoLockPreferenceController> mPreferenceControllerHelper;
    private NoLockPreferenceController mController;
    private Preference mPreference;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mPreference = new Preference(mContext);
        mPreferenceControllerHelper = new PreferenceControllerTestHelper<>(mContext,
                NoLockPreferenceController.class, mPreference);
        mController = mPreferenceControllerHelper.getController();
        mPreferenceControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);
    }

    @After
    public void tearDown() {
        ShadowLockPatternUtils.reset();
    }

    @Test
    public void testHandlePreferenceClicked_returnsTrue() {
        assertThat(mController.handlePreferenceClicked(mPreference)).isTrue();
    }

    @Test
    public void testHandlePreferenceClicked_ifNotSelectedAsLock_goesToNextFragment() {
        mController.setCurrentPasswordQuality(DevicePolicyManager.PASSWORD_QUALITY_NUMERIC);

        mPreference.performClick();

        verify(mPreferenceControllerHelper.getMockFragmentController()).showDialog(
                any(ConfirmationDialogFragment.class), anyString());
    }

    @Test
    public void testHandlePreferenceClicked_ifSelectedAsLock_goesBack() {
        mController.setCurrentPasswordQuality(DevicePolicyManager.PASSWORD_QUALITY_UNSPECIFIED);

        mPreference.performClick();

        verify(mPreferenceControllerHelper.getMockFragmentController()).goBack();
    }

    @Test
    public void testConfirmRemoveScreenLockListener_removesLock() {
        mController.setCurrentPassword(TEST_CURRENT_PASSWORD);

        mController.mConfirmListener.onConfirm(/* arguments= */ null);

        assertThat(ShadowLockPatternUtils.getClearLockCredential()).isEqualTo(
                TEST_CURRENT_PASSWORD);
        assertThat(ShadowLockPatternUtils.getClearLockUser()).isEqualTo(UserHandle.myUserId());
    }

    @Test
    public void testConfirmRemoveScreenLockListener_goesBack() {
        mController.setCurrentPassword(TEST_CURRENT_PASSWORD);

        mController.mConfirmListener.onConfirm(/* arguments= */ null);

        verify(mPreferenceControllerHelper.getMockFragmentController()).goBack();
    }
}
