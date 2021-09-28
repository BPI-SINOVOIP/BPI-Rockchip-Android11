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

package com.android.car.developeroptions.development.gamedriver;

import android.app.settings.SettingsEnums;
import android.content.Context;
import android.os.Bundle;
import android.provider.SearchIndexableResource;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.SettingsActivity;
import com.android.car.developeroptions.dashboard.DashboardFragment;
import com.android.car.developeroptions.search.BaseSearchIndexProvider;
import com.android.car.developeroptions.widget.SwitchBar;
import com.android.car.developeroptions.widget.SwitchBarController;
import com.android.settingslib.development.DevelopmentSettingsEnabler;
import com.android.settingslib.search.Indexable;
import com.android.settingslib.search.SearchIndexable;

import java.util.ArrayList;
import java.util.List;

/**
 * Dashboard for Game Driver preferences.
 */
@SearchIndexable
public class GameDriverDashboard extends DashboardFragment {

    private static final String TAG = "GameDriverDashboard";

    @Override
    public int getMetricsCategory() {
        return SettingsEnums.SETTINGS_GAME_DRIVER_DASHBOARD;
    }

    @Override
    protected String getLogTag() {
        return TAG;
    }

    @Override
    protected int getPreferenceScreenResId() {
        return R.xml.game_driver_settings;
    }

    @Override
    public int getHelpResource() {
        return 0;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        final SettingsActivity activity = (SettingsActivity) getActivity();
        final SwitchBar switchBar = activity.getSwitchBar();
        final GameDriverGlobalSwitchBarController switchBarController =
                new GameDriverGlobalSwitchBarController(
                        activity, new SwitchBarController(switchBar));
        getSettingsLifecycle().addObserver(switchBarController);
        switchBar.show();
    }

    public static final Indexable.SearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new BaseSearchIndexProvider() {
                @Override
                public List<SearchIndexableResource> getXmlResourcesToIndex(
                        Context context, boolean enabled) {
                    final List<SearchIndexableResource> result = new ArrayList<>();
                    final SearchIndexableResource sir = new SearchIndexableResource(context);
                    sir.xmlResId = R.xml.game_driver_settings;
                    result.add(sir);
                    return result;
                }

                @Override
                protected boolean isPageSearchEnabled(Context context) {
                    return DevelopmentSettingsEnabler.isDevelopmentSettingsEnabled(context);
                }
            };
}
