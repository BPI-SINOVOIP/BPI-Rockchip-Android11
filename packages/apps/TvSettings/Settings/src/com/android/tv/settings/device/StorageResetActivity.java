/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.tv.settings.device;

import android.app.Fragment;
import android.text.TextUtils;

import com.android.tv.settings.R;
import com.android.tv.settings.TvSettingsActivity;
import com.android.tv.settings.overlay.FeatureFactory;

/**
 * Activity to view storage consumption and factory reset device.
 */
public class StorageResetActivity extends TvSettingsActivity {

    @Override
    protected Fragment createSettingsFragment() {
        String fragmentName = StorageSummaryFragment.class.getName();
        if (!TextUtils.isEmpty(getString(R.string.storage_summary_fragment_name))) {
            fragmentName = getString(R.string.storage_summary_fragment_name);
        }
        return FeatureFactory.getFactory(this).getSettingsFragmentProvider()
            .newSettingsFragment(fragmentName, null);
    }
}
