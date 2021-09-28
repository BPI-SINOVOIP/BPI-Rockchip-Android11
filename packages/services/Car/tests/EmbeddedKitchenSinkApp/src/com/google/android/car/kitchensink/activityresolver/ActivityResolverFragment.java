/*
 * Copyright (C) 2021 The Android Open Source Project
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

package com.google.android.car.kitchensink.activityresolver;

import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.google.android.car.kitchensink.R;

/**
 * This fragment triggers the activity resolver by sending an intent handled by three test
 * activities.
 */
public final class ActivityResolverFragment extends Fragment {

    private static final String ACTIVITY_RESOLVER_INTENT =
            "com.google.android.car.kitchensink.activityresolver.TRIGGER_ACTIVITY_RESOLVER";

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.activity_resolver_fragment, container, false);
        Button button = view.requireViewById(R.id.trigger_activity_resolver);
        button.setOnClickListener(v -> startActivity(new Intent(ACTIVITY_RESOLVER_INTENT)));
        return view;
    }
}
