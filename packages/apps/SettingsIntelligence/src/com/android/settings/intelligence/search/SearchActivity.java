/*
 * Copyright (C) 2017 The Android Open Source Project
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
 *
 */

package com.android.settings.intelligence.search;

import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import android.content.pm.PackageManager;
import android.os.Bundle;
import android.view.WindowManager;

import com.android.settings.intelligence.R;
import com.android.settings.intelligence.search.car.CarSearchFragment;

public class SearchActivity extends FragmentActivity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        if (isAutomotive()) {
            // Automotive relies on a different theme. Apply before calling super so that
            // fragments are restored properly on configuration changes.
            setTheme(R.style.Theme_CarSettings);
        }
        super.onCreate(savedInstanceState);
        setContentView(R.layout.search_main);

        FragmentManager fragmentManager = getSupportFragmentManager();
        Fragment fragment = fragmentManager.findFragmentById(R.id.main_content);
        if (fragment == null) {
            fragment = isAutomotive() ?
                    new CarSearchFragment() : new SearchFragment();
            fragmentManager.beginTransaction()
                    .add(R.id.main_content, fragment)
                    .commit();
        }
    }

    @Override
    public boolean onNavigateUp() {
        finish();
        return true;
    }

    private boolean isAutomotive() {
        return getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE);
    }
}
