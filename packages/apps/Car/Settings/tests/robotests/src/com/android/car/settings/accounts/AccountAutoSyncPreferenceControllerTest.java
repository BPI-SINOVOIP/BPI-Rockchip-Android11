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

package com.android.car.settings.accounts;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.RuntimeEnvironment.application;

import android.content.ContentResolver;
import android.content.Context;
import android.os.Bundle;
import android.os.UserHandle;

import androidx.lifecycle.Lifecycle;
import androidx.preference.SwitchPreference;

import com.android.car.settings.common.ConfirmationDialogFragment;
import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.testutils.ShadowContentResolver;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

/** Unit tests for {@link AccountAutoSyncPreferenceController}. */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowContentResolver.class})
public class AccountAutoSyncPreferenceControllerTest {
    private final int mUserId = UserHandle.myUserId();
    private final UserHandle mUserHandle = new UserHandle(mUserId);

    private PreferenceControllerTestHelper<AccountAutoSyncPreferenceController> mHelper;
    private SwitchPreference mSwitchPreference;
    private AccountAutoSyncPreferenceController mController;
    private ConfirmationDialogFragment mDialog;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        Context context = RuntimeEnvironment.application;
        mSwitchPreference = new SwitchPreference(application);
        mHelper = new PreferenceControllerTestHelper<>(application,
                AccountAutoSyncPreferenceController.class, mSwitchPreference);
        mController = mHelper.getController();
        mDialog = new ConfirmationDialogFragment.Builder(context).build();
    }

    @Test
    public void testOnCreate_hasPreviousDialog_dialogListenerSet() {
        when(mHelper.getMockFragmentController().findDialogByTag(
                ConfirmationDialogFragment.TAG)).thenReturn(mDialog);
        mHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);

        assertThat(mDialog.getConfirmListener()).isNotNull();
    }

    @Test
    public void refreshUi_masterSyncOn_preferenceShouldBeChecked() {
        mHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);
        ContentResolver.setMasterSyncAutomaticallyAsUser(true, mUserId);

        mController.refreshUi();

        assertThat(mSwitchPreference.isChecked()).isTrue();
    }

    @Test
    public void refreshUi_masterSyncOff_preferenceShouldNotBeChecked() {
        ContentResolver.setMasterSyncAutomaticallyAsUser(false, mUserId);
        mHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);

        mController.refreshUi();

        assertThat(mSwitchPreference.isChecked()).isFalse();
    }

    @Test
    public void onPreferenceClicked_shouldOpenDialog() {
        mSwitchPreference.performClick();

        verify(mHelper.getMockFragmentController()).showDialog(
                any(ConfirmationDialogFragment.class), eq(ConfirmationDialogFragment.TAG));
    }

    @Test
    public void onConfirm_shouldTogglePreference() {
        // Set the preference as unchecked first so that the state is known
        mHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);
        ContentResolver.setMasterSyncAutomaticallyAsUser(false, mUserId);
        mController.refreshUi();

        assertThat(mSwitchPreference.isChecked()).isFalse();

        Bundle arguments = new Bundle();
        arguments.putBoolean(AccountAutoSyncPreferenceController.KEY_ENABLING, true);
        arguments.putParcelable(AccountAutoSyncPreferenceController.KEY_USER_HANDLE, mUserHandle);
        mController.mConfirmListener.onConfirm(arguments);

        assertThat(mSwitchPreference.isChecked()).isTrue();
    }
}
