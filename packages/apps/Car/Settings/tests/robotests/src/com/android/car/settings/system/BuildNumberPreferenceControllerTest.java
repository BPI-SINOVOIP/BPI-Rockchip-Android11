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

package com.android.car.settings.system;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.pm.UserInfo;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.Settings;

import androidx.lifecycle.Lifecycle;
import androidx.preference.Preference;

import com.android.car.settings.R;
import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.development.DevelopmentSettingsUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowToast;
import org.robolectric.shadows.ShadowUserManager;

@RunWith(RobolectricTestRunner.class)
public class BuildNumberPreferenceControllerTest {

    private Context mContext;
    private PreferenceControllerTestHelper<BuildNumberPreferenceController>
            mPreferenceControllerHelper;
    private BuildNumberPreferenceController mController;
    private Preference mPreference;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mPreference = new Preference(mContext);
        mPreferenceControllerHelper = new PreferenceControllerTestHelper<>(mContext,
                BuildNumberPreferenceController.class, mPreference);
        mController = mPreferenceControllerHelper.getController();

        // By default, user is an admin user.
        setCurrentUserWithFlags(UserInfo.FLAG_ADMIN);

        // By default, no restrictions on debugging features.
        getShadowUserManager()
                .setUserRestriction(
                        UserHandle.of(UserHandle.myUserId()),
                        UserManager.DISALLOW_DEBUGGING_FEATURES,
                        false);

        // By default device is provisioned.
        Settings.Global.putInt(mContext.getContentResolver(),
                Settings.Global.DEVICE_PROVISIONED, 1);

        // By default development settings is disabled.
        Settings.Global.putInt(mContext.getContentResolver(),
                Settings.Global.DEVELOPMENT_SETTINGS_ENABLED, 0);
        mPreferenceControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_RESUME);
    }

    @Test
    public void testHandlePreferenceClicked_notProvisioned_returnFalse() {
        Settings.Global.putInt(mContext.getContentResolver(),
                Settings.Global.DEVICE_PROVISIONED, 0);
        assertThat(mController.handlePreferenceClicked(mPreference)).isFalse();
    }

    @Test
    public void testHandlePreferenceClicked_nonAdmin_returnFalse() {
        setCurrentUserWithFlags(/* flags= */ 0);

        assertThat(mController.handlePreferenceClicked(mPreference)).isFalse();
    }

    @Test
    public void testHandlePreferenceClicked_demoUser_returnsFalse() {
        setCurrentUserWithFlags(UserInfo.FLAG_DEMO);

        assertThat(mController.handlePreferenceClicked(mPreference)).isFalse();
    }

    @Test
    public void testHandlePreferenceClicked_adminUser_returnsTrue() {
        setCurrentUserWithFlags(UserInfo.FLAG_ADMIN);

        assertThat(mController.handlePreferenceClicked(mPreference)).isTrue();
    }

    @Test
    public void testHandlePreferenceClicked_devSettingsDisabled_firstClick_noToast() {
        mPreference.performClick();
        assertThat(ShadowToast.shownToastCount()).isEqualTo(0);
    }

    @Test
    public void testHandlePreferenceClicked_devSettingsDisabled_someClicks_showToast() {
        for (int i = 0; i < getTapsToShowToast(); i++) {
            mPreference.performClick();
        }

        int remainingClicks = getTapsToBecomeDeveloper() - getTapsToShowToast();
        assertThat(ShadowToast.getTextOfLatestToast()).isEqualTo(
                mContext.getResources().getQuantityString(R.plurals.show_dev_countdown,
                        remainingClicks, remainingClicks));
    }

    @Test
    public void testHandlePreferenceClicked_devSettingsDisabled_allClicks_showDevEnabledToast() {
        for (int i = 0; i < getTapsToBecomeDeveloper(); i++) {
            mPreference.performClick();
        }
        assertThat(ShadowToast.getTextOfLatestToast()).isEqualTo(
                mContext.getString(R.string.show_dev_on));
    }

    @Test
    public void testHandlePreferenceClicked_devSettingsDisabled_allClicks_devSettingsEnabled() {
        for (int i = 0; i < getTapsToBecomeDeveloper(); i++) {
            mPreference.performClick();
        }
        assertThat(DevelopmentSettingsUtil.isDevelopmentSettingsEnabled(mContext,
                UserManager.get(mContext))).isTrue();
    }

    @Test
    public void testHandlePreferenceClicked_devSettingsDisabled_extraClicks_noAlreadyDevToast() {
        int extraClicks = 100;
        for (int i = 0; i < getTapsToBecomeDeveloper() + extraClicks; i++) {
            mPreference.performClick();
        }
        assertThat(
                ShadowToast.showedToast(mContext.getString(R.string.show_dev_already))).isFalse();
    }

    @Test
    public void testHandlePreferenceClicked_devSettingsEnabled_click_showAlreadyDevToast() {
        Settings.Global.putInt(mContext.getContentResolver(),
                Settings.Global.DEVELOPMENT_SETTINGS_ENABLED, 1);
        mPreferenceControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_PAUSE);
        mPreferenceControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_RESUME);
        mPreference.performClick();
        assertThat(ShadowToast.showedToast(mContext.getString(R.string.show_dev_already))).isTrue();
    }

    private int getTapsToBecomeDeveloper() {
        return mContext.getResources().getInteger(R.integer.enable_developer_settings_click_count);
    }

    private int getTapsToShowToast() {
        return mContext.getResources().getInteger(
                R.integer.enable_developer_settings_clicks_to_show_toast_count);
    }

    private void setCurrentUserWithFlags(int flags) {
        UserInfo userInfo = new UserInfo(UserHandle.myUserId(), null, flags);
        getShadowUserManager().addUser(userInfo.id, userInfo.name, userInfo.flags);
    }

    private ShadowUserManager getShadowUserManager() {
        return Shadows.shadowOf(UserManager.get(mContext));
    }
}
