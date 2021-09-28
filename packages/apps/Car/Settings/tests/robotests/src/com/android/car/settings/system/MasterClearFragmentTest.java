/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.car.settings.system;

import static android.app.Activity.RESULT_CANCELED;
import static android.app.Activity.RESULT_OK;

import static com.android.car.settings.system.MasterClearFragment.CHECK_LOCK_REQUEST_CODE;
import static com.android.car.ui.core.CarUi.requireToolbar;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.UserHandle;
import android.os.UserManager;

import androidx.fragment.app.Fragment;

import com.android.car.settings.R;
import com.android.car.settings.security.CheckLockActivity;
import com.android.car.settings.testutils.FragmentController;
import com.android.car.settings.testutils.ShadowAccountManager;
import com.android.car.settings.testutils.ShadowUserManager;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.ToolbarController;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

/** Unit test for {@link MasterClearFragment}. */
@Ignore
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowAccountManager.class, ShadowUserManager.class})
public class MasterClearFragmentTest {

    private MasterClearFragment mFragment;

    @Before
    public void setUp() {
        // Setup needed by instantiated PreferenceControllers.
        MockitoAnnotations.initMocks(this);
        Context context = RuntimeEnvironment.application;
        int userId = UserHandle.myUserId();
        Shadows.shadowOf(UserManager.get(context)).addUser(userId, "User Name", /* flags= */ 0);
        Shadows.shadowOf(UserManager.get(context)).addProfile(userId, userId,
                "Profile Name", /* profileFlags= */ 0);
        Shadows.shadowOf(context.getPackageManager())
                .setSystemFeature(PackageManager.FEATURE_AUTOMOTIVE, true);

        mFragment = FragmentController.of(new MasterClearFragment()).setup();
    }

    @After
    public void tearDown() {
        ShadowUserManager.reset();
    }

    @Test
    public void masterClearButtonClicked_launchesCheckLockActivity() {
        MenuItem masterClearButton = findMasterClearButton(mFragment.requireActivity());
        masterClearButton.setEnabled(true);
        masterClearButton.performClick();

        Intent startedIntent = ShadowApplication.getInstance().getNextStartedActivity();
        assertThat(startedIntent.getComponent().getClassName()).isEqualTo(
                CheckLockActivity.class.getName());
    }

    @Test
    public void processActivityResult_resultOk_launchesMasterClearConfirmFragment() {
        mFragment.processActivityResult(CHECK_LOCK_REQUEST_CODE, RESULT_OK, /* data= */ null);

        Fragment launchedFragment = mFragment.getFragmentManager().findFragmentById(
                R.id.fragment_container);

        assertThat(launchedFragment).isInstanceOf(MasterClearConfirmFragment.class);
    }

    @Test
    public void processActivityResult_otherResultCode_doesNothing() {
        mFragment.processActivityResult(CHECK_LOCK_REQUEST_CODE, RESULT_CANCELED, /* data= */ null);

        Fragment launchedFragment = mFragment.getFragmentManager().findFragmentById(
                R.id.fragment_container);

        assertThat(launchedFragment).isInstanceOf(MasterClearFragment.class);
    }

    private MenuItem findMasterClearButton(Activity activity) {
        ToolbarController toolbar = requireToolbar(activity);
        return toolbar.getMenuItems().get(0);
    }
}
