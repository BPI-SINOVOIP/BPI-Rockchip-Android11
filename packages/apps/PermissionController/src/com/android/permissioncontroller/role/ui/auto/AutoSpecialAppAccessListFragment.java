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

package com.android.permissioncontroller.role.ui.auto;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.permissioncontroller.R;
import com.android.permissioncontroller.auto.AutoSettingsFrameFragment;
import com.android.permissioncontroller.role.ui.TwoTargetPreference;
import com.android.permissioncontroller.role.ui.specialappaccess.SpecialAppAccessListChildFragment;

/** Automotive fragment for the list of role related special app accesses. */
public class AutoSpecialAppAccessListFragment extends AutoSettingsFrameFragment implements
        SpecialAppAccessListChildFragment.Parent {

    /** Returns a new instance of {@link AutoSpecialAppAccessListFragment}. */
    @NonNull
    public static AutoSpecialAppAccessListFragment newInstance() {
        return new AutoSpecialAppAccessListFragment();
    }

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        // Preferences will be added by the child fragment.
    }

    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        if (savedInstanceState == null) {
            SpecialAppAccessListChildFragment fragment =
                    SpecialAppAccessListChildFragment.newInstance();
            getChildFragmentManager().beginTransaction()
                    .add(fragment, /* tag= */ null)
                    .commit();
        }

        setHeaderLabel(getString(R.string.special_app_access));
    }

    @NonNull
    @Override
    public TwoTargetPreference createPreference(@NonNull Context context) {
        return new AutoSettingsPreference(context);
    }

    @Override
    public void onPreferenceScreenChanged() {
    }
}
