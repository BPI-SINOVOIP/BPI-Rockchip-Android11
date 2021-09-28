/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.google.android.car.kitchensink.systemfeatures;

import android.annotation.Nullable;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;

import com.google.android.car.kitchensink.R;

import java.util.Arrays;
import java.util.Comparator;
import java.util.Objects;

/**
 * Shows system features as available by PackageManager
 */
public class SystemFeaturesFragment extends Fragment {
    private static final String TAG = "CAR.SYSFEATURES.KS";

    private ListView mSysFeaturesList;
    private PackageManager mPackageManager;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        mPackageManager = Objects.requireNonNull(
                getContext().getPackageManager());

        super.onCreate(savedInstanceState);
    }

    @Nullable
    @Override
    public View onCreateView(
            @NonNull LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        mSysFeaturesList = (ListView) inflater.inflate(R.layout.system_feature_fragment,
                container, false);
        return mSysFeaturesList;
    }

    @Override
    public void onResume() {
        super.onResume();
        refresh();
    }

    @Override
    public void onPause() {
        super.onPause();
    }

    private void refresh() {
        final FeatureInfo[] features = mPackageManager.getSystemAvailableFeatures();
        if (features != null) {
            final String[] descriptions = Arrays.stream(features)
                .filter(fi -> fi != null && fi.name != null)
                .sorted(Comparator.<FeatureInfo, String>comparing(fi -> fi.name)
                                  .thenComparing(fi -> fi.version))
                .map(fi -> String.format("%s (v=%d)", fi.name, fi.version))
                .toArray(String[]::new);
            mSysFeaturesList.setAdapter(new ArrayAdapter<>(getContext(),
                    android.R.layout.simple_list_item_1, descriptions));
        } else {
            Log.e(TAG, "no features available on this device!");
        }
    }
}
