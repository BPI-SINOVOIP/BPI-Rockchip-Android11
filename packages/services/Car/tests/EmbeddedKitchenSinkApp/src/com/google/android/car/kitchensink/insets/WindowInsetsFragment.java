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

package com.google.android.car.kitchensink.insets;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.google.android.car.kitchensink.R;

/** Test fragment for controlling window insets. */
public final class WindowInsetsFragment extends Fragment {

    private WindowInsetsController mWindowInsetsController;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.window_insets_fragment, container,
                /* attachToRoot= */ false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        mWindowInsetsController = view.getWindowInsetsController();
        initStatusBarInsetsButtons(view);
        initNavigationBarInsetsButtons(view);
        initSystemBarInsetsButtons(view);
        initSystemBarBehaviorButtons(view);
    }

    private void initStatusBarInsetsButtons(View view) {
        view.findViewById(R.id.show_status_bar_insets).setOnClickListener(
                v -> mWindowInsetsController.show(WindowInsets.Type.statusBars()));
        view.findViewById(R.id.hide_status_bar_insets).setOnClickListener(
                v -> mWindowInsetsController.hide(WindowInsets.Type.statusBars()));
    }

    private void initNavigationBarInsetsButtons(View view) {
        view.findViewById(R.id.show_nav_bar_insets).setOnClickListener(
                v -> mWindowInsetsController.show(WindowInsets.Type.navigationBars()));
        view.findViewById(R.id.hide_nav_bar_insets).setOnClickListener(
                v -> mWindowInsetsController.hide(WindowInsets.Type.navigationBars()));
    }

    private void initSystemBarInsetsButtons(View view) {
        view.findViewById(R.id.show_system_bar_insets).setOnClickListener(
                v -> mWindowInsetsController.show(WindowInsets.Type.systemBars()));
        view.findViewById(R.id.hide_system_bar_insets).setOnClickListener(
                v -> mWindowInsetsController.hide(WindowInsets.Type.systemBars()));
    }

    private void initSystemBarBehaviorButtons(View view) {
        view.findViewById(R.id.show_bars_by_touch).setOnClickListener(
                v -> mWindowInsetsController.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_BARS_BY_TOUCH));
        view.findViewById(R.id.show_bars_by_swipe).setOnClickListener(
                v -> mWindowInsetsController.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_BARS_BY_SWIPE));
        view.findViewById(R.id.show_transient_bars_by_swipe).setOnClickListener(
                v -> mWindowInsetsController.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE));
    }
}
