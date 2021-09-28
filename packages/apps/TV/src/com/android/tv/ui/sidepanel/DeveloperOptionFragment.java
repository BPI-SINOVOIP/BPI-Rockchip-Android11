/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.tv.ui.sidepanel;

import android.app.Activity;

import com.android.tv.MainActivity;
import com.android.tv.R;
import com.android.tv.common.BuildConfig;
import com.android.tv.common.CommonPreferences;
import com.android.tv.common.feature.CommonFeatures;
import com.android.tv.perf.PerformanceMonitor;

import com.google.common.base.Optional;
import com.google.common.collect.ImmutableList;

import dagger.android.AndroidInjection;

import com.android.tv.common.flags.LegacyFlags;

import javax.inject.Inject;

/** Options for developers only */
public class DeveloperOptionFragment extends SideFragment {
    private static final String TRACKER_LABEL = "debug options";

    @Inject Optional<AdditionalDeveloperItemsFactory> mAdditionalDeveloperItemsFactory;
    @Inject PerformanceMonitor mPerformanceMonitor;
    @Inject LegacyFlags mLegacyFlags;

    @Override
    public void onAttach(Activity activity) {
        AndroidInjection.inject(this);
        super.onAttach(activity);
    }

    @Override
    protected String getTitle() {
        return getString(R.string.menu_developer_options);
    }

    @Override
    public String getTrackerLabel() {
        return TRACKER_LABEL;
    }

    @Override
    protected ImmutableList<Item> getItemList() {
        ImmutableList.Builder<Item> items = ImmutableList.builder();
        if (mAdditionalDeveloperItemsFactory.isPresent()) {
            items.addAll(
                    mAdditionalDeveloperItemsFactory
                            .get()
                            .getAdditionalDevItems(getMainActivity()));
            items.add(new DividerItem());
        }
        if (CommonFeatures.DVR.isEnabled(getContext())) {
            items.add(
                    new ActionItem(getString(R.string.dev_item_dvr_history)) {
                        @Override
                        protected void onSelected() {
                            getMainActivity().getOverlayManager().showDvrHistoryDialog();
                        }
                    });
        }
        if (BuildConfig.ENG || mLegacyFlags.enableDeveloperFeatures()) {
            items.add(
                    new ActionItem(getString(R.string.dev_item_watch_history)) {
                        @Override
                        protected void onSelected() {
                            getMainActivity().getOverlayManager().showRecentlyWatchedDialog();
                        }
                    });
        }
        items.add(
                new SwitchItem(
                        getString(R.string.dev_item_store_ts_on),
                        getString(R.string.dev_item_store_ts_off),
                        getString(R.string.dev_item_store_ts_description)) {
                    @Override
                    protected void onUpdate() {
                        super.onUpdate();
                        setChecked(CommonPreferences.getStoreTsStream(getContext()));
                    }

                    @Override
                    protected void onSelected() {
                        super.onSelected();
                        CommonPreferences.setStoreTsStream(getContext(), isChecked());
                    }
                });
        if (BuildConfig.ENG || mLegacyFlags.enableDeveloperFeatures()) {
            items.add(
                    new ActionItem(getString(R.string.dev_item_show_performance_monitor_log)) {
                        @Override
                        protected void onSelected() {
                            mPerformanceMonitor.startPerformanceMonitorEventDebugActivity(
                                    getContext());
                        }
                    });
        }
        return items.build();
    }

    /** Factory to create additional items. */
    public interface AdditionalDeveloperItemsFactory {
        ImmutableList<Item> getAdditionalDevItems(MainActivity mainActivity);
    }
}
