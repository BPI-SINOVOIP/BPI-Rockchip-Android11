/**
 * Copyright (c) 2019 The Android Open Source Project
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

package com.android.car.secondaryhome.launcher;

import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.GridView;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.lifecycle.ViewModelProviders;

import com.android.car.secondaryhome.R;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * {@link Fragment} that shows a grid of app installed.
 * It will launch app into AppFragment.
 * Note: a new task will be launched every time for app to run in multiple display.
 */
public final class HomeFragment extends Fragment {
    private static final String TAG = "SecHome.HomeFragment";

    private final Set<String> mHiddenApps = new HashSet<>();

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {

        View view = inflater.inflate(R.layout.home_fragment, container, false);
        GridView gridView = view.findViewById(R.id.app_grid);

        FragmentManager fragmentManager = getActivity().getSupportFragmentManager();
        AppListAdapter appListAdapter = new AppListAdapter(getActivity());

        gridView.setAdapter(appListAdapter);
        gridView.setOnItemClickListener((adapterView, v, position, id) -> {
            AppEntry entry = appListAdapter.getItem(position);
            AppFragment mAppFragment = (AppFragment) fragmentManager
                    .findFragmentByTag(((LauncherActivity) getActivity()).APP_FRAGMENT_TAG);

            if (mAppFragment != null) {
                // Equivalent to setting in AndroidManifest: documentLaunchMode="always"
                Intent newIntent = entry.getLaunchIntent()
                        .setFlags(~Intent.FLAG_ACTIVITY_SINGLE_TOP)
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT
                                | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
                mAppFragment.addLaunchIntentToStack(newIntent);
                ((LauncherActivity) getActivity()).navigateApp();

            } else if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "AppFragment is not found...");
            }
        });

        AppListViewModel appListViewModel = ViewModelProviders.of(this)
                .get(AppListViewModel.class);

        mHiddenApps.addAll(Arrays.asList(getResources().getStringArray(R.array.hidden_apps)));
        appListViewModel.getAppList().observe(this, data ->
                appListAdapter.setData(data, mHiddenApps));
        return view;
    }
}
