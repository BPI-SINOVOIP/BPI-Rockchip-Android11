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

package com.android.car.developeroptions.inputmethod;

import android.app.Activity;
import android.app.admin.DevicePolicyManager;
import android.app.settings.SettingsEnums;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.provider.SearchIndexableResource;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;

import androidx.preference.Preference;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.SettingsPreferenceFragment;
import com.android.car.developeroptions.search.BaseSearchIndexProvider;
import com.android.settingslib.inputmethod.InputMethodAndSubtypeUtilCompat;
import com.android.settingslib.inputmethod.InputMethodPreference;
import com.android.settingslib.search.Indexable;
import com.android.settingslib.search.SearchIndexable;

import java.text.Collator;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;

@SearchIndexable
public final class VirtualKeyboardFragment extends SettingsPreferenceFragment implements Indexable {

    private static final String ADD_VIRTUAL_KEYBOARD_SCREEN = "add_virtual_keyboard_screen";
    private static final Drawable NO_ICON = new ColorDrawable(Color.TRANSPARENT);

    private final ArrayList<InputMethodPreference> mInputMethodPreferenceList = new ArrayList<>();
    private InputMethodManager mImm;
    private DevicePolicyManager mDpm;
    private Preference mAddVirtualKeyboardScreen;

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        Activity activity = Objects.requireNonNull(getActivity());
        addPreferencesFromResource(R.xml.virtual_keyboard_settings);
        mImm = Objects.requireNonNull(activity.getSystemService(InputMethodManager.class));
        mDpm = Objects.requireNonNull(activity.getSystemService(DevicePolicyManager.class));
        mAddVirtualKeyboardScreen = Objects.requireNonNull(
                findPreference(ADD_VIRTUAL_KEYBOARD_SCREEN));
    }

    @Override
    public void onResume() {
        super.onResume();
        // Refresh internal states in mInputMethodSettingValues to keep the latest
        // "InputMethodInfo"s and "InputMethodSubtype"s
        updateInputMethodPreferenceViews();
    }

    @Override
    public int getMetricsCategory() {
        return SettingsEnums.VIRTUAL_KEYBOARDS;
    }

    private void updateInputMethodPreferenceViews() {
        // Clear existing "InputMethodPreference"s
        mInputMethodPreferenceList.clear();
        List<String> permittedList = mDpm.getPermittedInputMethodsForCurrentUser();
        final Context context = getPrefContext();
        final List<InputMethodInfo> imis = mImm.getEnabledInputMethodList();
        final int N = (imis == null ? 0 : imis.size());
        for (int i = 0; i < N; ++i) {
            final InputMethodInfo imi = imis.get(i);
            final boolean isAllowedByOrganization = permittedList == null
                    || permittedList.contains(imi.getPackageName());
            Drawable icon;
            try {
                // TODO: Consider other ways to retrieve an icon to show here.
                icon = getActivity().getPackageManager().getApplicationIcon(imi.getPackageName());
            } catch (Exception e) {
                // TODO: Consider handling the error differently perhaps by showing default icons.
                icon = NO_ICON;
            }
            final InputMethodPreference pref = new InputMethodPreference(
                    context,
                    imi,
                    false,  /* isImeEnabler */
                    isAllowedByOrganization,
                    null  /* this can be null since isImeEnabler is false */);
            pref.setIcon(icon);
            mInputMethodPreferenceList.add(pref);
        }
        final Collator collator = Collator.getInstance();
        mInputMethodPreferenceList.sort((lhs, rhs) -> lhs.compareTo(rhs, collator));
        getPreferenceScreen().removeAll();
        for (int i = 0; i < N; ++i) {
            final InputMethodPreference pref = mInputMethodPreferenceList.get(i);
            pref.setOrder(i);
            getPreferenceScreen().addPreference(pref);
            InputMethodAndSubtypeUtilCompat.removeUnnecessaryNonPersistentPreference(pref);
            pref.updatePreferenceViews();
        }
        mAddVirtualKeyboardScreen.setIcon(R.drawable.ic_add_24dp);
        mAddVirtualKeyboardScreen.setOrder(N);
        getPreferenceScreen().addPreference(mAddVirtualKeyboardScreen);
    }

    public static final Indexable.SearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new BaseSearchIndexProvider() {
                @Override
                public List<SearchIndexableResource> getXmlResourcesToIndex(
                        Context context, boolean enabled) {
                    final SearchIndexableResource sir = new SearchIndexableResource(context);
                    sir.xmlResId = R.xml.virtual_keyboard_settings;
                    return Arrays.asList(sir);
                }
            };
}
