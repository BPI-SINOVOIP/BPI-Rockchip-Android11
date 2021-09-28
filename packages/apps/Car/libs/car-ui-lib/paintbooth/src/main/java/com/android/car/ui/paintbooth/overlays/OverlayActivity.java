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

package com.android.car.ui.paintbooth.overlays;

import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.preference.SwitchPreference;

import com.android.car.ui.paintbooth.R;
import com.android.car.ui.preference.PreferenceFragment;

import java.util.List;
import java.util.Map;

/**
 * Activity representing lists of RROs ppackage name as title and the corresponding target package
 * name as the summary with a toggle switch to enable/disable the overlays.
 */
public class OverlayActivity extends AppCompatActivity {
    private static final String TAG = OverlayActivity.class.getSimpleName();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Display the fragment as the main content.
        if (savedInstanceState == null) {
            getSupportFragmentManager()
                    .beginTransaction()
                    .replace(android.R.id.content, new OverlayFragment())
                    .commit();
        }
    }

    /** PreferenceFragmentCompat that sets the preference hierarchy from XML */
    public static class OverlayFragment extends PreferenceFragment {
        private OverlayManager mOverlayManager;

        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
            try {
                mOverlayManager = OverlayManager.getInstance(getContext());
                setPreferencesFromResource(R.xml.preference_overlays, rootKey);

                Map<String, List<OverlayManager.OverlayInfo>> overlays =
                        OverlayManager.getInstance(getContext()).getOverlays();

                for (String targetPackage : overlays.keySet()) {
                    for (OverlayManager.OverlayInfo overlayPackage : overlays.get(targetPackage)) {
                        SwitchPreference switchPreference = new SwitchPreference(getContext());
                        switchPreference.setKey(overlayPackage.getPackageName());
                        switchPreference.setTitle(overlayPackage.getPackageName());
                        switchPreference.setSummary(targetPackage);
                        switchPreference.setChecked(overlayPackage.isEnabled());

                        switchPreference.setOnPreferenceChangeListener((preference, newValue) -> {
                            applyOverlay(overlayPackage.getPackageName(), (boolean) newValue);
                            return true;
                        });

                        getPreferenceScreen().addPreference(switchPreference);
                    }
                }
            } catch (Exception e) {
                Toast.makeText(getContext(), "Error: " + e.getMessage(),
                        Toast.LENGTH_LONG).show();
                Log.e(TAG, "Can't load overlays: ", e);
            }
        }

        private void applyOverlay(String overlayPackage, boolean enableOverlay) {
            try {
                mOverlayManager.applyOverlay(overlayPackage, enableOverlay);
            } catch (Exception e) {
                Toast.makeText(getContext(), "Error: " + e.getMessage(),
                        Toast.LENGTH_LONG).show();
                Log.e(TAG, "Can't apply overlay: ", e);
            }
        }
    }
}
