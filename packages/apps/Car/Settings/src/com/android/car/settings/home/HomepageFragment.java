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

package com.android.car.settings.home;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;

import androidx.annotation.XmlRes;

import com.android.car.settings.R;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.Toolbar;
import com.android.car.ui.toolbar.ToolbarController;

import java.util.Collections;
import java.util.List;

/**
 * Homepage for settings for car.
 */
public class HomepageFragment extends SettingsFragment {
    private static final int REQUEST_CODE = 501;

    private MenuItem mSearchButton;

    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        return R.xml.homepage_fragment;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        // TODO: Re-enable suggestions once more use cases are supported.
        // use(SuggestionsPreferenceController.class, R.string.pk_suggestions).setLoaderManager(
        //        LoaderManager.getInstance(/* owner= */ this));
    }

    @Override
    protected List<MenuItem> getToolbarMenuItems() {
        return Collections.singletonList(mSearchButton);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mSearchButton = new MenuItem.Builder(getContext())
                .setToSearch()
                .setOnClickListener(i -> onSearchButtonClicked())
                .setUxRestrictions(CarUxRestrictions.UX_RESTRICTIONS_NO_KEYBOARD)
                .build();
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        ToolbarController toolbar = getToolbar();
        if (toolbar != null) {
            // If the fragment is root, change the back button to settings icon.
            if (!getContext().getResources().getBoolean(R.bool.config_is_quick_settings_root)) {
                toolbar.setState(Toolbar.State.HOME);
                toolbar.setLogo(getContext().getResources()
                        .getBoolean(R.bool.config_show_settings_root_exit_icon)
                        ? R.drawable.ic_launcher_settings
                        : 0);
            } else {
                toolbar.setState(Toolbar.State.SUBPAGE);
            }
        }
    }

    private void onSearchButtonClicked() {
        Intent intent = new Intent(Settings.ACTION_APP_SEARCH_SETTINGS)
                .setPackage(getSettingsIntelligencePkgName(getContext()));
        if (intent.resolveActivity(getContext().getPackageManager()) == null) {
            return;
        }
        startActivityForResult(intent, REQUEST_CODE);
    }

    private String getSettingsIntelligencePkgName(Context context) {
        return context.getString(R.string.config_settingsintelligence_package_name);
    }

}
