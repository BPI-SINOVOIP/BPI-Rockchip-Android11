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
package com.android.car.rotaryplayground;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

/**
 * Fragment for the menu.
 *
 * On focus of a menu item, the associated fragment will start in the R.id.rotary_content container.
 */
public class RotaryMenu extends Fragment {

    private Fragment mRotaryCards = null;
    private Fragment mRotaryGrid = null;
    private Fragment mDirectManipulation = null;
    private Fragment mSysUiDirectManipulation = null;
    private Fragment mNotificationFragment = null;
    private Fragment mScrollFragment = null;
    private Fragment mWebViewFragment = null;
    private Fragment mCustomFocusAreasFragment = null;

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.rotary_menu, container, false);

        Button cardButton = view.findViewById(R.id.cards);
        cardButton.setOnClickListener(v -> {
            selectTab(v);
            showRotaryCards();
        });

        Button gridButton = view.findViewById(R.id.grid);
        gridButton.setOnClickListener(v -> {
            selectTab(v);
            showGridExample();
        });

        Button directManipulationButton = view.findViewById(R.id.direct_manipulation);
        directManipulationButton.setOnClickListener(v -> {
            selectTab(v);
            showDirectManipulationExamples();
        });

        Button sysUiDirectManipulationButton = view.findViewById(R.id.sys_ui_direct_manipulation);
        sysUiDirectManipulationButton.setOnClickListener(v -> {
            selectTab(v);
            showSysUiDirectManipulationExamples();
        });

        Button notificationButton = view.findViewById(R.id.notification);
        notificationButton.setOnClickListener(v -> {
            selectTab(v);
            showNotificationExample();
        });

        Button scrollButton = view.findViewById(R.id.scroll);
        scrollButton.setOnClickListener(v -> {
            selectTab(v);
            showScrollFragment();
        });

        Button webViewButton = view.findViewById(R.id.web_view);
        webViewButton.setOnClickListener(v -> {
            selectTab(v);
            showWebViewFragment();
        });

        Button customFocusAreasButton = view.findViewById(R.id.custom_focus_areas);
        customFocusAreasButton.setOnClickListener(v -> {
            selectTab(v);
            showCustomFocusAreasFragment();
        });

        return view;
    }

    private void selectTab(View view) {
        ViewGroup container = (ViewGroup) view.getParent();
        for (int i = 0; i < container.getChildCount(); i++) {
            container.getChildAt(i).setSelected(false);
        }
        view.setSelected(true);
    }

    private void showRotaryCards() {
        if (mRotaryCards == null) {
            mRotaryCards = new RotaryCards();
        }
        showFragment(mRotaryCards);
    }

    private void showGridExample() {
        if (mRotaryGrid == null) {
            mRotaryGrid = new RotaryGrid();
        }
        showFragment(mRotaryGrid);
    }

    // TODO(agathaman): refactor this and the showRotaryCards above into a
    //  showFragment(Fragment fragment, boolean hasFocus); method.
    private void showDirectManipulationExamples() {
        if (mDirectManipulation == null) {
            mDirectManipulation = new RotaryDirectManipulationWidgets();
        }
        showFragment(mDirectManipulation);
    }

    private void showSysUiDirectManipulationExamples() {
        if (mSysUiDirectManipulation == null) {
            mSysUiDirectManipulation = new RotarySysUiDirectManipulationWidgets();
        }
        showFragment(mSysUiDirectManipulation);
    }

    private void showNotificationExample() {
        if (mNotificationFragment == null) {
            mNotificationFragment = new HeadsUpNotificationFragment();
        }
        showFragment(mNotificationFragment);
    }

    private void showScrollFragment() {
        if (mScrollFragment == null) {
            mScrollFragment = new ScrollFragment();
        }
        showFragment(mScrollFragment);
    }

    private void showWebViewFragment() {
        if (mWebViewFragment == null) {
            mWebViewFragment = new WebViewFragment();
        }
        showFragment(mWebViewFragment);
    }

    private void showCustomFocusAreasFragment() {
        if (mCustomFocusAreasFragment == null) {
            mCustomFocusAreasFragment = new CustomFocusAreasFragment();
        }
        showFragment(mCustomFocusAreasFragment);
    }

    private void showFragment(Fragment fragment) {
        getFragmentManager().beginTransaction()
                .replace(R.id.rotary_content, fragment)
                .commit();
    }
}
