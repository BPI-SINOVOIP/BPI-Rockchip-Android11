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

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.pm.UserInfo;
import android.os.UserManager;

import androidx.lifecycle.Lifecycle;
import androidx.preference.PreferenceGroup;
import androidx.preference.SwitchPreference;

import com.android.car.settings.common.LogicalPreferenceGroup;
import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.testutils.ShadowUserManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

/**
 * Test for the preference controller which populates the various permissions preferences.
 * Note that the switch preference represents the opposite of the restriction it is controlling.
 * i.e. DISALLOW_ADD_USER may be the restriction, but the switch represents "create new users".
 */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowUserManager.class})
public class PermissionsPreferenceControllerTest {

    private static final String TEST_RESTRICTION = UserManager.DISALLOW_ADD_USER;
    private static final UserInfo TEST_USER = new UserInfo(/* id= */ 10,
            "TEST_USER_NAME", /* flags= */ 0);

    private Context mContext;
    private PreferenceControllerTestHelper<PermissionsPreferenceController>
            mPreferenceControllerHelper;
    private PermissionsPreferenceController mController;
    private PreferenceGroup mPreferenceGroup;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mPreferenceControllerHelper = new PreferenceControllerTestHelper<>(mContext,
                PermissionsPreferenceController.class);
        mController = mPreferenceControllerHelper.getController();
        mController.setUserInfo(TEST_USER);
        mPreferenceGroup = new LogicalPreferenceGroup(mContext);
        mPreferenceControllerHelper.setPreference(mPreferenceGroup);
        mPreferenceControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);
    }

    @After
    public void tearDown() {
        ShadowUserManager.reset();
    }

    @Test
    public void testRefreshUi_populatesGroup() {
        mController.refreshUi();
        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(5);
    }

    @Test
    public void testRefreshUi_callingTwice_noDuplicates() {
        mController.refreshUi();
        mController.refreshUi();
        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(5);
    }

    @Test
    public void testRefreshUi_setToFalse() {
        SwitchPreference preference = getPreferenceForRestriction(mPreferenceGroup,
                TEST_RESTRICTION);
        preference.setChecked(true);
        getShadowUserManager().setUserRestriction(
                TEST_USER.getUserHandle(), TEST_RESTRICTION, true);
        mController.refreshUi();
        assertThat(preference.isChecked()).isFalse();
    }

    @Test
    public void testRefreshUi_setToTrue() {
        SwitchPreference preference = getPreferenceForRestriction(mPreferenceGroup,
                TEST_RESTRICTION);
        preference.setChecked(false);
        getShadowUserManager().setUserRestriction(
                TEST_USER.getUserHandle(), TEST_RESTRICTION, false);
        mController.refreshUi();
        assertThat(preference.isChecked()).isTrue();
    }

    @Test
    public void testOnPreferenceChange_changeToFalse() {
        SwitchPreference preference = getPreferenceForRestriction(mPreferenceGroup,
                TEST_RESTRICTION);
        getShadowUserManager().setUserRestriction(
                TEST_USER.getUserHandle(), TEST_RESTRICTION, true);
        preference.callChangeListener(true);
        assertThat(UserManager.get(mContext)
                .hasUserRestriction(TEST_RESTRICTION, TEST_USER.getUserHandle())).isFalse();
    }

    @Test
    public void testOnPreferenceChange_changeToTrue() {
        SwitchPreference preference = getPreferenceForRestriction(mPreferenceGroup,
                TEST_RESTRICTION);
        getShadowUserManager().setUserRestriction(
                TEST_USER.getUserHandle(), TEST_RESTRICTION, false);
        preference.callChangeListener(false);
        assertThat(UserManager.get(mContext)
                .hasUserRestriction(TEST_RESTRICTION, TEST_USER.getUserHandle())).isTrue();
    }

    private SwitchPreference getPreferenceForRestriction(
            PreferenceGroup preferenceGroup, String restriction) {
        for (int i = 0; i < preferenceGroup.getPreferenceCount(); i++) {
            SwitchPreference preference = (SwitchPreference) preferenceGroup.getPreference(i);
            if (restriction.equals(preference.getExtras().getString(
                    PermissionsPreferenceController.PERMISSION_TYPE_KEY))) {
                return preference;
            }
        }
        return null;
    }

    private ShadowUserManager getShadowUserManager() {
        return Shadow.extract(mContext.getSystemService(UserManager.class));
    }
}
