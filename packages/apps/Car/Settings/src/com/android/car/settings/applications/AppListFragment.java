/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.car.settings.applications;

import android.os.Bundle;

import com.android.car.settings.R;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.ui.toolbar.MenuItem;

import java.util.Collections;
import java.util.List;

/**
 * Fragment base class for use in cases where a list of applications is displayed with the option to
 * show/hide system apps in the list.
 */
public abstract class AppListFragment extends SettingsFragment {

    private static final String KEY_SHOW_SYSTEM = "showSystem";

    private boolean mShowSystem;
    private MenuItem mSystemButton;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (savedInstanceState != null) {
            mShowSystem = savedInstanceState.getBoolean(KEY_SHOW_SYSTEM, false);
        }

        mSystemButton = new MenuItem.Builder(getContext())
                .setOnClickListener(i -> {
                    mShowSystem = !mShowSystem;
                    onToggleShowSystemApps(mShowSystem);
                    i.setTitle(mShowSystem ? R.string.hide_system : R.string.show_system);
                })
                .setTitle(mShowSystem ? R.string.hide_system : R.string.show_system)
                .build();
    }

    @Override
    public List<MenuItem> getToolbarMenuItems() {
        return Collections.singletonList(mSystemButton);
    }

    @Override
    public void onStart() {
        super.onStart();
        onToggleShowSystemApps(mShowSystem);
    }

    /** Called when a user toggles the option to show system applications in the list. */
    protected abstract void onToggleShowSystemApps(boolean showSystem);

    /** Returns {@code true} if system applications should be shown in the list. */
    protected final boolean shouldShowSystemApps() {
        return mShowSystem;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putBoolean(KEY_SHOW_SYSTEM, mShowSystem);
    }
}
