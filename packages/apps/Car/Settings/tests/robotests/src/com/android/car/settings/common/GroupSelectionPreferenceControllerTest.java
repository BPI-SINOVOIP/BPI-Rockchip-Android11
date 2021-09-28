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

package com.android.car.settings.common;

import static com.google.common.truth.Truth.assertThat;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;

import androidx.annotation.NonNull;
import androidx.lifecycle.Lifecycle;
import androidx.preference.PreferenceGroup;
import androidx.preference.SwitchPreference;
import androidx.preference.TwoStatePreference;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.ArrayList;
import java.util.List;

@RunWith(RobolectricTestRunner.class)
public class GroupSelectionPreferenceControllerTest {

    private static class TestGroupSelectionPreferenceController extends
            GroupSelectionPreferenceController {

        private String mCurrentCheckedKey;
        private boolean mAllowPassThrough = true;
        private List<TwoStatePreference> mGroupPreferences;

        TestGroupSelectionPreferenceController(Context context, String preferenceKey,
                FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
            super(context, preferenceKey, fragmentController, uxRestrictions);
            mGroupPreferences = new ArrayList<>();
        }

        @Override
        protected String getCurrentCheckedKey() {
            return mCurrentCheckedKey;
        }

        @Override
        @NonNull
        protected List<TwoStatePreference> getGroupPreferences() {
            return mGroupPreferences;
        }

        @Override
        protected boolean handleGroupItemSelected(TwoStatePreference preference) {
            if (mAllowPassThrough) {
                mCurrentCheckedKey = preference.getKey();
                return true;
            } else {
                return false;
            }
        }

        public void setCurrentCheckedKey(String key) {
            mCurrentCheckedKey = key;
        }

        public void setAllowPassThrough(boolean allow) {
            mAllowPassThrough = allow;
        }

        public void setGroupPreferences(List<TwoStatePreference> preferences) {
            mGroupPreferences = preferences;
        }
    }

    private Context mContext;
    private PreferenceGroup mPreferenceGroup;
    private PreferenceControllerTestHelper<TestGroupSelectionPreferenceController>
            mPreferenceControllerHelper;
    private TestGroupSelectionPreferenceController mController;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mPreferenceGroup = new LogicalPreferenceGroup(mContext);
        mPreferenceControllerHelper = new PreferenceControllerTestHelper<>(mContext,
                TestGroupSelectionPreferenceController.class, mPreferenceGroup);
        mController = mPreferenceControllerHelper.getController();

        mPreferenceControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);
    }

    @Test
    public void refreshUi_hasTwoElements() {
        List<TwoStatePreference> entries = new ArrayList<>();
        entries.add(createPreference("key1"));
        entries.add(createPreference("key2"));

        mController.setGroupPreferences(entries);
        mController.refreshUi();

        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(2);
    }

    @Test
    public void refreshUi_elementChecked() {
        List<TwoStatePreference> entries = new ArrayList<>();
        entries.add(createPreference("key1"));
        entries.add(createPreference("key2"));

        mController.setGroupPreferences(entries);
        mController.setCurrentCheckedKey("key2");
        mController.refreshUi();

        assertThat(findPreference("key1").isChecked()).isFalse();
        assertThat(findPreference("key2").isChecked()).isTrue();
    }

    @Test
    public void performClick_allowPassThrough_checkedElementChanged() {
        List<TwoStatePreference> entries = new ArrayList<>();
        entries.add(createPreference("key1"));
        entries.add(createPreference("key2"));

        mController.setGroupPreferences(entries);
        mController.setCurrentCheckedKey("key2");
        mController.setAllowPassThrough(true);
        mController.refreshUi();

        assertThat(findPreference("key1").isChecked()).isFalse();
        assertThat(findPreference("key2").isChecked()).isTrue();

        findPreference("key1").performClick();

        assertThat(findPreference("key1").isChecked()).isTrue();
        assertThat(findPreference("key2").isChecked()).isFalse();
    }

    @Test
    public void performClick_disallowPassThrough_checkedElementNotChanged() {
        List<TwoStatePreference> entries = new ArrayList<>();
        entries.add(createPreference("key1"));
        entries.add(createPreference("key2"));

        mController.setGroupPreferences(entries);
        mController.setCurrentCheckedKey("key2");
        mController.setAllowPassThrough(false);
        mController.refreshUi();

        assertThat(findPreference("key1").isChecked()).isFalse();
        assertThat(findPreference("key2").isChecked()).isTrue();

        findPreference("key1").performClick();

        assertThat(findPreference("key1").isChecked()).isFalse();
        assertThat(findPreference("key2").isChecked()).isTrue();
    }

    @Test
    public void performClick_disallowPassThrough_notifyKeyChanged_checkedElementChanged() {
        List<TwoStatePreference> entries = new ArrayList<>();
        entries.add(createPreference("key1"));
        entries.add(createPreference("key2"));

        mController.setGroupPreferences(entries);
        mController.setCurrentCheckedKey("key2");
        mController.setAllowPassThrough(false);
        mController.refreshUi();

        assertThat(findPreference("key1").isChecked()).isFalse();
        assertThat(findPreference("key2").isChecked()).isTrue();

        findPreference("key1").performClick();

        assertThat(findPreference("key1").isChecked()).isFalse();
        assertThat(findPreference("key2").isChecked()).isTrue();

        mController.setCurrentCheckedKey("key1");
        mController.notifyCheckedKeyChanged();

        assertThat(findPreference("key1").isChecked()).isTrue();
        assertThat(findPreference("key2").isChecked()).isFalse();
    }

    private TwoStatePreference findPreference(String key) {
        return mPreferenceGroup.findPreference(key);
    }

    private TwoStatePreference createPreference(String key) {
        TwoStatePreference preference = new SwitchPreference(mContext);
        preference.setKey(key);
        return preference;
    }
}
