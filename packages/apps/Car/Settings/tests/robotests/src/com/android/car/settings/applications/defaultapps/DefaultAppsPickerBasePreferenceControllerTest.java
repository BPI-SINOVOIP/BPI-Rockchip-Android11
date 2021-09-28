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
 * limitations under the License.
 */

package com.android.car.settings.applications.defaultapps;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;

import androidx.lifecycle.Lifecycle;
import androidx.preference.Preference;
import androidx.preference.PreferenceGroup;

import com.android.car.settings.R;
import com.android.car.settings.common.ConfirmationDialogFragment;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.LogicalPreferenceGroup;
import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.ui.preference.CarUiRadioButtonPreference;
import com.android.settingslib.applications.DefaultAppInfo;

import com.google.android.collect.Lists;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.List;

@RunWith(RobolectricTestRunner.class)
public class DefaultAppsPickerBasePreferenceControllerTest {

    private static class TestDefaultAppsPickerBasePreferenceController extends
            DefaultAppsPickerBasePreferenceController {

        private List<DefaultAppInfo> mCandidates;
        private String mKey;
        private CharSequence mMessage;
        private boolean mIncludeNone = true;

        TestDefaultAppsPickerBasePreferenceController(Context context, String preferenceKey,
                FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
            super(context, preferenceKey, fragmentController, uxRestrictions);
        }

        public void setTestCandidates(List<DefaultAppInfo> candidates) {
            mCandidates = candidates;
        }

        public void setTestMessage(String s) {
            mMessage = s;
        }

        public void setIncludeNonePreference(boolean include) {
            mIncludeNone = include;
        }

        @Override
        protected List<DefaultAppInfo> getCandidates() {
            return mCandidates;
        }

        @Override
        protected String getCurrentDefaultKey() {
            return mKey;
        }

        @Override
        protected void setCurrentDefault(String key) {
            mKey = key;
        }

        @Override
        protected CharSequence getConfirmationMessage(DefaultAppInfo info) {
            return mMessage;
        }

        @Override
        protected boolean includeNonePreference() {
            return mIncludeNone;
        }
    }

    private Context mContext;
    private PreferenceGroup mPreferenceGroup;
    private PreferenceControllerTestHelper<TestDefaultAppsPickerBasePreferenceController>
            mControllerHelper;
    private TestDefaultAppsPickerBasePreferenceController mController;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mPreferenceGroup = new LogicalPreferenceGroup(mContext);
        mControllerHelper = new PreferenceControllerTestHelper<>(mContext,
                TestDefaultAppsPickerBasePreferenceController.class, mPreferenceGroup);
        mController = mControllerHelper.getController();
    }

    @Test
    public void refreshUi_noCandidates_hasSingleNoneElement() {
        mController.setCurrentDefault("");
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);
        mController.refreshUi();

        // Has the "None" element.
        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(1);

        Preference preference = mPreferenceGroup.getPreference(0);
        assertThat(preference.getTitle()).isEqualTo(
                mContext.getString(R.string.app_list_preference_none));
        assertThat(preference.getIcon()).isNotNull();
    }

    @Test
    public void refreshUi_noCandidates_noNoneElement() {
        mController.setCurrentDefault("");
        mController.setIncludeNonePreference(false);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);
        mController.refreshUi();

        // None element removed.
        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(0);
    }

    @Test
    public void refreshUi_hasAdditionalCandidate_hasTwoElements() {
        String testKey = "testKey";

        DefaultAppInfo testApp = mock(DefaultAppInfo.class);
        when(testApp.getKey()).thenReturn(testKey);
        mController.setTestCandidates(Lists.newArrayList(testApp));

        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);
        mController.refreshUi();

        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(2);
    }

    @Test
    public void refreshUi_hasAdditionalCandidateAsDefault_secondElementIsSelected() {
        String testKey = "testKey";

        DefaultAppInfo testApp = mock(DefaultAppInfo.class);
        when(testApp.getKey()).thenReturn(testKey);
        mController.setTestCandidates(Lists.newArrayList(testApp));
        mController.setCurrentDefault(testKey);

        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);
        mController.refreshUi();

        CarUiRadioButtonPreference preference = mPreferenceGroup.findPreference(testKey);
        assertThat(preference.isChecked()).isTrue();
    }

    @Test
    public void performClick_currentDefaultApp_nothingHappened() {
        String testKey = "testKey";

        DefaultAppInfo testApp = mock(DefaultAppInfo.class);
        when(testApp.getKey()).thenReturn(testKey);
        mController.setTestCandidates(Lists.newArrayList(testApp));
        mController.setCurrentDefault(testKey);

        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);
        mController.refreshUi();

        CarUiRadioButtonPreference currentDefault = mPreferenceGroup.findPreference(testKey);
        CarUiRadioButtonPreference otherOption =
                (CarUiRadioButtonPreference) mPreferenceGroup.getPreference(0);

        assertThat(currentDefault.isChecked()).isTrue();
        assertThat(otherOption.isChecked()).isFalse();

        currentDefault.performClick();

        currentDefault = mPreferenceGroup.findPreference(testKey);
        otherOption = (CarUiRadioButtonPreference) mPreferenceGroup.getPreference(0);

        assertThat(currentDefault.isChecked()).isTrue();
        assertThat(otherOption.isChecked()).isFalse();
    }

    @Test
    public void performClick_otherOptionNoMessage_otherOptionSelected() {
        String testKey = "testKey";

        DefaultAppInfo testApp = mock(DefaultAppInfo.class);
        when(testApp.getKey()).thenReturn(testKey);
        mController.setTestCandidates(Lists.newArrayList(testApp));
        mController.setCurrentDefault(testKey);
        mController.setTestMessage(null);

        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);
        mController.refreshUi();

        CarUiRadioButtonPreference currentDefault = mPreferenceGroup.findPreference(testKey);
        CarUiRadioButtonPreference otherOption =
                (CarUiRadioButtonPreference) mPreferenceGroup.getPreference(0);

        assertThat(currentDefault.isChecked()).isTrue();
        assertThat(otherOption.isChecked()).isFalse();

        otherOption.performClick();

        currentDefault = mPreferenceGroup.findPreference(testKey);
        otherOption = (CarUiRadioButtonPreference) mPreferenceGroup.getPreference(0);

        assertThat(currentDefault.isChecked()).isFalse();
        assertThat(otherOption.isChecked()).isTrue();
    }

    @Test
    public void performClick_otherOptionHasMessage_dialogOpened() {
        String testKey = "testKey";

        DefaultAppInfo testApp = mock(DefaultAppInfo.class);
        when(testApp.getKey()).thenReturn(testKey);
        mController.setTestCandidates(Lists.newArrayList(testApp));
        mController.setCurrentDefault(testKey);
        mController.setTestMessage("Non-empty message");

        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);
        mController.refreshUi();

        CarUiRadioButtonPreference currentDefault = mPreferenceGroup.findPreference(testKey);
        CarUiRadioButtonPreference otherOption =
                (CarUiRadioButtonPreference) mPreferenceGroup.getPreference(0);

        assertThat(currentDefault.isChecked()).isTrue();
        assertThat(otherOption.isChecked()).isFalse();

        otherOption.performClick();

        verify(mControllerHelper.getMockFragmentController()).showDialog(
                any(ConfirmationDialogFragment.class),
                eq(ConfirmationDialogFragment.TAG));
    }

    @Test
    public void performClick_otherOptionNoMessage_newKeySet() {
        String testKey = "testKey";

        DefaultAppInfo testApp = mock(DefaultAppInfo.class);
        when(testApp.getKey()).thenReturn(testKey);
        mController.setTestCandidates(Lists.newArrayList(testApp));

        // Currently, the testApp is the default selection.
        mController.setCurrentDefault(testKey);

        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);
        mController.refreshUi();

        // This preference represents the "None" option.
        CarUiRadioButtonPreference otherOption =
                (CarUiRadioButtonPreference) mPreferenceGroup.getPreference(0);
        assertThat(mController.getCurrentDefaultKey()).isEqualTo(testKey);

        otherOption.performClick();
        assertThat(mController.getCurrentDefaultKey()).isEqualTo("");
    }
}
