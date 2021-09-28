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

package com.android.car.settings.users;

import static com.android.car.ui.core.CarUi.requireToolbar;

import static com.google.common.truth.Truth.assertThat;

import android.content.pm.UserInfo;

import com.android.car.settings.testutils.BaseTestActivity;
import com.android.car.settings.testutils.FragmentController;
import com.android.car.settings.testutils.ShadowUserIconProvider;
import com.android.car.settings.testutils.ShadowUserManager;
import com.android.car.ui.core.testsupport.CarUiInstallerRobolectric;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.ToolbarController;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/**
 * Tests for ChooseNewAdminFragment.
 */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowUserIconProvider.class, ShadowUserManager.class})
public class ChooseNewAdminFragmentTest {

    private static final UserInfo TEST_ADMIN_USER = new UserInfo(/* id= */ 10,
            "TEST_USER_NAME", /* flags= */ 0);

    private FragmentController<ChooseNewAdminFragment> mFragmentController;
    private ChooseNewAdminFragment mFragment;

    @Before
    public void setUpTestActivity() {
        MockitoAnnotations.initMocks(this);

        // Needed to install Install CarUiLib BaseLayouts Toolbar for test activity
        CarUiInstallerRobolectric.install();

        mFragment = ChooseNewAdminFragment.newInstance(TEST_ADMIN_USER);
        mFragmentController = FragmentController.of(mFragment);
        mFragmentController.setup();
    }

    @After
    public void tearDown() {
        ShadowUserManager.reset();
    }

    @Test
    public void testBackButtonPressed_whenRemoveCancelled() {
        MenuItem actionButton = ((ToolbarController) requireToolbar(mFragment.requireActivity()))
                .getMenuItems().get(0);

        actionButton.performClick();
        assertThat(((BaseTestActivity) mFragment.requireActivity()).getOnBackPressedFlag())
                .isTrue();
    }
}
