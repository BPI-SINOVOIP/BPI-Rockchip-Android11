/*
 * Copyright (C) 2021 The Android Open Source Project
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

package com.android.car.settings.sound;

import static com.android.car.settings.sound.RingtonePickerPreferenceController.COLUMN_LABEL;
import static com.android.car.settings.sound.RingtonePickerPreferenceController.SILENT_ITEM_POS;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.database.Cursor;
import android.media.AudioAttributes;
import android.media.RingtoneManager;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;

import androidx.lifecycle.LifecycleOwner;
import androidx.preference.Preference;
import androidx.preference.PreferenceGroup;
import androidx.preference.PreferenceManager;
import androidx.preference.PreferenceScreen;
import androidx.test.annotation.UiThreadTest;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.LogicalPreferenceGroup;
import com.android.car.settings.common.PreferenceControllerTestUtil;
import com.android.car.settings.testutils.TestLifecycleOwner;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class RingtonePickerPreferenceControllerTest {

    private static final int TEST_IDX = 10;
    private Context mContext = ApplicationProvider.getApplicationContext();
    private LifecycleOwner mLifecycleOwner;
    private PreferenceManager mPreferenceManager;
    private PreferenceScreen mScreen;

    private PreferenceGroup mPreferenceGroup;
    private RingtonePickerPreferenceController mPreferenceController;
    private CarUxRestrictions mCarUxRestrictions;

    private List<String> mTestValues;
    private int mCurrIdx;

    @Mock
    private FragmentController mFragmentController;
    @Mock
    private RingtoneManager mRingtoneManager;
    @Mock
    private Cursor mCursor;

    @Before
    @UiThreadTest
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mLifecycleOwner = new TestLifecycleOwner();

        mCarUxRestrictions = new CarUxRestrictions.Builder(/* reqOpt= */ true,
                CarUxRestrictions.UX_RESTRICTIONS_BASELINE, /* timestamp= */ 0).build();

        mPreferenceManager = new PreferenceManager(mContext);
        mScreen = mPreferenceManager.createPreferenceScreen(mContext);
        mPreferenceGroup = new LogicalPreferenceGroup(mContext);
        mScreen.addPreference(mPreferenceGroup);

        mPreferenceController = new RingtonePickerPreferenceController(mContext,
                "key", mFragmentController, mCarUxRestrictions);
        mPreferenceController.setRingtoneManager(mRingtoneManager);
        PreferenceControllerTestUtil.assignPreference(mPreferenceController, mPreferenceGroup);

        // Default list of ringtones.
        List<String> ringtoneTitles = new ArrayList<>();
        ringtoneTitles.add("Test Ringtone 1");
        ringtoneTitles.add("Test Ringtone 2");
        ringtoneTitles.add("Test Ringtone 3");
        setUpCursor(ringtoneTitles);
    }

    @Test
    public void onCreate_noArgsSet_invalidRingtoneTypeSet() {
        mPreferenceController.onCreate(mLifecycleOwner);

        verify(mRingtoneManager).setType(eq(-1));
    }

    @Test
    public void onCreate_setArgs_validRingtoneTypeSet() {
        Bundle args = new Bundle();
        args.putInt(RingtoneManager.EXTRA_RINGTONE_TYPE, RingtoneManager.TYPE_RINGTONE);
        args.putBoolean(RingtoneManager.EXTRA_RINGTONE_SHOW_SILENT, false);
        args.putInt(RingtoneManager.EXTRA_RINGTONE_AUDIO_ATTRIBUTES_FLAGS,
                AudioAttributes.FLAG_BYPASS_INTERRUPTION_POLICY);

        mPreferenceController.setArguments(args);
        mPreferenceController.onCreate(mLifecycleOwner);

        verify(mRingtoneManager).setType(eq(RingtoneManager.TYPE_RINGTONE));
    }

    @Test
    public void onCreate_doesNotShowSilent_noExtraPreference() {
        Bundle args = new Bundle();
        args.putInt(RingtoneManager.EXTRA_RINGTONE_TYPE, RingtoneManager.TYPE_RINGTONE);
        args.putBoolean(RingtoneManager.EXTRA_RINGTONE_SHOW_SILENT, false);
        args.putInt(RingtoneManager.EXTRA_RINGTONE_AUDIO_ATTRIBUTES_FLAGS,
                AudioAttributes.FLAG_BYPASS_INTERRUPTION_POLICY);

        // Add 2 ringtones.
        List<String> ringtoneTitles = new ArrayList<>();
        ringtoneTitles.add("Test Ringtone 1");
        ringtoneTitles.add("Test Ringtone 2");
        setUpCursor(ringtoneTitles);

        mPreferenceController.setArguments(args);
        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(2);
    }

    @Test
    public void onCreate_showSilent_hasExtraPreference() {
        Bundle args = new Bundle();
        args.putInt(RingtoneManager.EXTRA_RINGTONE_TYPE, RingtoneManager.TYPE_RINGTONE);
        args.putBoolean(RingtoneManager.EXTRA_RINGTONE_SHOW_SILENT, true);
        args.putInt(RingtoneManager.EXTRA_RINGTONE_AUDIO_ATTRIBUTES_FLAGS,
                AudioAttributes.FLAG_BYPASS_INTERRUPTION_POLICY);

        // Add 2 ringtones.
        List<String> ringtoneTitles = new ArrayList<>();
        ringtoneTitles.add("Test Ringtone 1");
        ringtoneTitles.add("Test Ringtone 2");
        setUpCursor(ringtoneTitles);

        mPreferenceController.setArguments(args);
        mPreferenceController.onCreate(mLifecycleOwner);

        // Has 2 ringtones + silent option.
        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(3);
    }

    @Test
    public void onCreate_showSilent_firstPreferenceIsSilent() {
        Bundle args = new Bundle();
        args.putInt(RingtoneManager.EXTRA_RINGTONE_TYPE, RingtoneManager.TYPE_RINGTONE);
        args.putBoolean(RingtoneManager.EXTRA_RINGTONE_SHOW_SILENT, true);
        args.putInt(RingtoneManager.EXTRA_RINGTONE_AUDIO_ATTRIBUTES_FLAGS,
                AudioAttributes.FLAG_BYPASS_INTERRUPTION_POLICY);

        mPreferenceController.setArguments(args);
        mPreferenceController.onCreate(mLifecycleOwner);

        Preference preference = mPreferenceGroup.getPreference(0);
        int posKey = Integer.parseInt(preference.getKey());
        assertThat(posKey).isEqualTo(SILENT_ITEM_POS);
    }

    @Test
    public void onCreate_hasSilent_defaultRingtoneSelected() {
        Bundle args = new Bundle();
        args.putInt(RingtoneManager.EXTRA_RINGTONE_TYPE, RingtoneManager.TYPE_RINGTONE);
        args.putBoolean(RingtoneManager.EXTRA_RINGTONE_SHOW_SILENT, true);
        args.putInt(RingtoneManager.EXTRA_RINGTONE_AUDIO_ATTRIBUTES_FLAGS,
                AudioAttributes.FLAG_BYPASS_INTERRUPTION_POLICY);

        List<String> ringtoneTitles = new ArrayList<>();
        ringtoneTitles.add("Test Ringtone 1");
        ringtoneTitles.add("Test Ringtone 2");
        setUpCursor(ringtoneTitles);

        // Set the default ringtone to be the first ringtone (not counting 'silent').
        Settings.System.putStringForUser(mContext.getContentResolver(), Settings.System.RINGTONE,
                createUri(ringtoneTitles.get(0)).toString(), mContext.getUserId());

        mPreferenceController.setArguments(args);
        mPreferenceController.onCreate(mLifecycleOwner);

        // Currently selected ringtone is the first ringtone in the list. However, since we also
        // have the "silent" item, it is the second item in the overall list.
        assertThat(mPreferenceController.getCurrentlySelectedPreferencePos()).isEqualTo(1);
    }

    @Test
    public void onPreferenceClick_clickCurrentlySelectedItem_nothingChanges() {
        Bundle args = new Bundle();
        args.putInt(RingtoneManager.EXTRA_RINGTONE_TYPE, RingtoneManager.TYPE_RINGTONE);
        args.putBoolean(RingtoneManager.EXTRA_RINGTONE_SHOW_SILENT, true);
        args.putInt(RingtoneManager.EXTRA_RINGTONE_AUDIO_ATTRIBUTES_FLAGS,
                AudioAttributes.FLAG_BYPASS_INTERRUPTION_POLICY);

        List<String> ringtoneTitles = new ArrayList<>();
        ringtoneTitles.add("Test Ringtone 1");
        ringtoneTitles.add("Test Ringtone 2");
        setUpCursor(ringtoneTitles);

        // Set the default ringtone to be the first ringtone (not counting 'silent').
        Settings.System.putStringForUser(mContext.getContentResolver(), Settings.System.RINGTONE,
                createUri(ringtoneTitles.get(0)).toString(), mContext.getUserId());

        mPreferenceController.setArguments(args);
        mPreferenceController.onCreate(mLifecycleOwner);
        mPreferenceGroup.getPreference(1).performClick();

        // Currently selected ringtone is the first ringtone in the list. However, since we also
        // have the "silent" item, it is the second item in the overall list.
        assertThat(mPreferenceController.getCurrentlySelectedPreferencePos()).isEqualTo(1);
    }

    @Test
    public void onPreferenceClick_clickDifferentItem_selectionChanges() {
        Bundle args = new Bundle();
        args.putInt(RingtoneManager.EXTRA_RINGTONE_TYPE, RingtoneManager.TYPE_RINGTONE);
        args.putBoolean(RingtoneManager.EXTRA_RINGTONE_SHOW_SILENT, true);
        args.putInt(RingtoneManager.EXTRA_RINGTONE_AUDIO_ATTRIBUTES_FLAGS,
                AudioAttributes.FLAG_BYPASS_INTERRUPTION_POLICY);

        List<String> ringtoneTitles = new ArrayList<>();
        ringtoneTitles.add("Test Ringtone 1");
        ringtoneTitles.add("Test Ringtone 2");
        setUpCursor(ringtoneTitles);

        // Set the default ringtone to be the first ringtone (not counting 'silent').
        Settings.System.putStringForUser(mContext.getContentResolver(), Settings.System.RINGTONE,
                createUri(ringtoneTitles.get(0)).toString(), mContext.getUserId());

        mPreferenceController.setArguments(args);
        mPreferenceController.onCreate(mLifecycleOwner);
        mPreferenceGroup.getPreference(2).performClick();

        assertThat(mPreferenceController.getCurrentlySelectedPreferencePos()).isEqualTo(2);
    }

    @Test
    public void onStop_stopRingtone() {
        Bundle args = new Bundle();
        args.putInt(RingtoneManager.EXTRA_RINGTONE_TYPE, RingtoneManager.TYPE_RINGTONE);
        args.putBoolean(RingtoneManager.EXTRA_RINGTONE_SHOW_SILENT, true);
        args.putInt(RingtoneManager.EXTRA_RINGTONE_AUDIO_ATTRIBUTES_FLAGS,
                AudioAttributes.FLAG_BYPASS_INTERRUPTION_POLICY);

        List<String> ringtoneTitles = new ArrayList<>();
        ringtoneTitles.add("Test Ringtone 1");
        ringtoneTitles.add("Test Ringtone 2");
        setUpCursor(ringtoneTitles);

        // Set the default ringtone to be the first ringtone (not counting 'silent').
        Settings.System.putStringForUser(mContext.getContentResolver(), Settings.System.RINGTONE,
                createUri(ringtoneTitles.get(0)).toString(), mContext.getUserId());

        mPreferenceController.setArguments(args);
        mPreferenceController.onCreate(mLifecycleOwner);
        mPreferenceGroup.getPreference(2).performClick();
        mPreferenceController.onStop(mLifecycleOwner);

        verify(mRingtoneManager, atLeastOnce()).stopPreviousRingtone();
    }

    // Set up the mock cursor to retrieve the values provided by {@code ringtoneTitles}.
    private void setUpCursor(List<String> ringtoneTitles) {
        mTestValues = new ArrayList<>(ringtoneTitles);

        for (int i = 0; i < mTestValues.size(); i++) {
            String value = mTestValues.get(i);
            when(mRingtoneManager.getRingtonePosition(eq(createUri(value)))).thenReturn(i);
        }

        when(mRingtoneManager.getCursor()).thenReturn(mCursor);
        when(mCursor.getColumnIndex(eq(COLUMN_LABEL))).thenReturn(TEST_IDX);
        when(mCursor.moveToFirst()).then(invocation -> {
            mCurrIdx = 0;
            return null;
        });
        when(mCursor.moveToNext()).then(invocation -> {
            if (mCurrIdx < mTestValues.size()) {
                mCurrIdx += 1;
            }
            return null;
        });
        when(mCursor.getString(eq(TEST_IDX))).thenReturn(mTestValues.get(mCurrIdx));
        when(mCursor.isAfterLast()).then(invocation -> mCurrIdx >= mTestValues.size());
    }

    private Uri createUri(String ringtoneTitle) {
        return new Uri.Builder()
                .scheme("test")
                .authority("test")
                .appendPath(ringtoneTitle)
                .build();
    }
}
