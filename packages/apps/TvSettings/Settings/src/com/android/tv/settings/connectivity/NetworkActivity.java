/*
 * Copyright (C) 2016 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.tv.settings.connectivity;

import android.app.Fragment;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;

import com.android.tv.settings.TvSettingsActivity;
import com.android.tv.settings.overlay.FeatureFactory;

public class NetworkActivity extends TvSettingsActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Intent callingIntent = getIntent();
        if (FeatureFactory.getFactory(this).getOfflineFeatureProvider().isOfflineMode(this)
                && callingIntent != null
                && Settings.ACTION_WIFI_SETTINGS.equals(callingIntent.getAction())) {
            if (FeatureFactory.getFactory(this).getOfflineFeatureProvider()
                    .startOfflineExitActivity(this)) {
                finish();
                return;
            }
        }
        super.onCreate(savedInstanceState);
    }

    @Override
    protected Fragment createSettingsFragment()  {
        return FeatureFactory.getFactory(this).getSettingsFragmentProvider()
                .newSettingsFragment(NetworkFragment.class.getName(), null);
    }
}
