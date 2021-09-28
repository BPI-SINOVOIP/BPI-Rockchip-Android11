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

package com.android.tv.twopanelsettings;

import androidx.leanback.preference.LeanbackPreferenceFragment;

/**
 * Child preference fragment should extend this class to make two panel settings functionality work,
 * otherwise preview panel would not show up.
 */
public abstract class SettingsPreferenceFragmentBase extends LeanbackPreferenceFragment {
    @Override
    public void onResume() {
        super.onResume();
        if (getCallbackFragment() instanceof TwoPanelSettingsFragment) {
            TwoPanelSettingsFragment parentFragment =
                    (TwoPanelSettingsFragment) getCallbackFragment();
            parentFragment.addListenerForFragment(this);
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        if (getCallbackFragment() instanceof TwoPanelSettingsFragment) {
            TwoPanelSettingsFragment parentFragment =
                    (TwoPanelSettingsFragment) getCallbackFragment();
            parentFragment.removeListenerForFragment(this);
        }
    }
}
