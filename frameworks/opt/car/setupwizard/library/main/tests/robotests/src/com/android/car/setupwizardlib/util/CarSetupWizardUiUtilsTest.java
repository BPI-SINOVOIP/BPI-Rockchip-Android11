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

package com.android.car.setupwizardlib.util;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;
import android.view.Window;

import com.android.car.setupwizardlib.robotests.R;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

@RunWith(RobolectricTestRunner.class)
public class CarSetupWizardUiUtilsTest {

    private static final int IMMERSIVE_MODE_FLAGS =
            View.SYSTEM_UI_FLAG_IMMERSIVE
                    | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN;

    private static final int NON_IMMERSIVE_MODE_FLAGS =
            View.SYSTEM_UI_FLAG_VISIBLE;

    private static final int IMMERSIVE_WITH_STATUS_MODE_FLAGS =
            View.SYSTEM_UI_FLAG_IMMERSIVE
                    | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;

    private static final int[] RES_ID_NAV_AND_STATUS_BARS = new int[]{
            android.R.attr.statusBarColor,
            android.R.attr.navigationBarColor};

    // Note that these colors are defined in the test theme
    private static final int TEST_THEME = R.style.NavAndStatusBarTestTheme;
    private static final int EXPECTED_COLOR_STATUS_BAR = Color.getHtmlColor("#001");
    private static final int EXPECTED_COLOR_NAVIGATION_BAR = Color.getHtmlColor("#002");

    private Activity mActivity;
    private Window mWindow;

    @Before
    public void setup() {
        mActivity = Robolectric
                .buildActivity(Activity.class)
                .create()
                .get();
        mActivity.setTheme(TEST_THEME);
        mWindow = mActivity.getWindow();
    }

    @Test
    public void maybeHideSystemUI() {
        CarSetupWizardUiUtils.maybeHideSystemUI(mActivity);
        assertThat(mWindow.getDecorView().getSystemUiVisibility())
                .isEqualTo(IMMERSIVE_MODE_FLAGS);
    }

    @Test
    public void enableImmersiveMode() {
        CarSetupWizardUiUtils.enableImmersiveMode(mWindow);
        assertThat(mWindow.getDecorView().getSystemUiVisibility())
                .isEqualTo(IMMERSIVE_MODE_FLAGS);
    }

    @Test
    public void disableImmersiveMode() {
        // Resetting the status bar colors.
        mWindow.setNavigationBarColor(Color.TRANSPARENT);
        mWindow.setStatusBarColor(Color.TRANSPARENT);

        CarSetupWizardUiUtils.disableImmersiveMode(mWindow);

        assertThat(mWindow.getDecorView().getSystemUiVisibility())
                .isEqualTo(NON_IMMERSIVE_MODE_FLAGS);
        assertThat(mWindow.getNavigationBarColor())
                .isEqualTo(EXPECTED_COLOR_NAVIGATION_BAR);
        assertThat(mWindow.getStatusBarColor())
                .isEqualTo(EXPECTED_COLOR_STATUS_BAR);
    }

    @Test
    public void setWindow_Immersive() {
        CarSetupWizardUiUtils.setWindowImmersiveMode(mWindow,
                CarSetupWizardUiUtils.ImmersiveModeTypes.IMMERSIVE.toString());
        assertThat(mWindow.getDecorView().getSystemUiVisibility())
                .isEqualTo(IMMERSIVE_MODE_FLAGS);
    }

    @Test
    public void setWindow_ImmersiveWithStatus() {
        CarSetupWizardUiUtils.setWindowImmersiveMode(mWindow,
                CarSetupWizardUiUtils.ImmersiveModeTypes.IMMERSIVE_WITH_STATUS.toString());
        assertThat(mWindow.getDecorView().getSystemUiVisibility())
                .isEqualTo(IMMERSIVE_WITH_STATUS_MODE_FLAGS);
    }


    @Test
    public void setWindow_NonImmersive() {
        CarSetupWizardUiUtils.setWindowImmersiveMode(mWindow,
                CarSetupWizardUiUtils.ImmersiveModeTypes.NON_IMMERSIVE.toString());
        assertThat(mWindow.getDecorView().getSystemUiVisibility())
                .isEqualTo(NON_IMMERSIVE_MODE_FLAGS);
    }

}
