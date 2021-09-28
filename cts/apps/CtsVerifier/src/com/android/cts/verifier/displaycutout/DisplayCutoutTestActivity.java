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

package com.android.cts.verifier.displaycutout;

import android.graphics.Color;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.DisplayCutout;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.Toast;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

public class DisplayCutoutTestActivity extends PassFailButtons.Activity {

    private View mDescription;
    private View mLeftButtons;
    private View mTopButtons;
    private View mRightButtons;
    private View mBottomButtons;
    private View mPassFailButton;

    private Toast mToast;
    private int mButtonSize;

    final private static int FLAG_PASS_BUTTON_ENABLE = 0xFFFF;
    private int mPassButtonEnableFlag = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getWindow().requestFeature(Window.FEATURE_NO_TITLE);
        WindowManager.LayoutParams lp = getWindow().getAttributes();
        lp.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        getWindow().setAttributes(lp);
        getWindow().getDecorView().getWindowInsetsController().hide(WindowInsets.Type.systemBars());
        getWindow().getDecorView().getWindowInsetsController().setSystemBarsBehavior(
                WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);

        setContentView(R.layout.display_cutout);

        mDescription = findViewById(R.id.enable_buttons_desc);
        mPassFailButton = findViewById(R.id.pass_fail_buttons);
        mLeftButtons = findViewById(R.id.left_buttons);
        mTopButtons = findViewById(R.id.top_buttons);
        mRightButtons = findViewById(R.id.right_buttons);
        mBottomButtons = findViewById(R.id.bottom_buttons);
        mButtonSize = getResources().getDimensionPixelSize(R.dimen.display_cutout_test_button_size);

        setPassFailButtonClickListeners();
        // only enable pass button when all test buttons are clicked.
        getPassButton().setEnabled(false);
        getWindow().getDecorView().setOnApplyWindowInsetsListener((v, insets) -> {
                updateButtons(insets.getDisplayCutout());
                return insets;
            });

    }

    public void updateButtons(DisplayCutout cutout) {
        Rect safeInsets = new Rect();
        if (cutout != null) {
            safeInsets.set(cutout.getSafeInsetLeft(),
                    cutout.getSafeInsetTop(),
                    cutout.getSafeInsetRight(),
                    cutout.getSafeInsetBottom());
        }

        // update left buttons
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) mLeftButtons.getLayoutParams();
        lp.leftMargin = safeInsets.left;
        lp.topMargin = safeInsets.top + mButtonSize;
        lp.bottomMargin = safeInsets.bottom + mButtonSize;
        mLeftButtons.setLayoutParams(lp);
        mLeftButtons.setVisibility(View.VISIBLE);

        // update top buttons
        lp = (ViewGroup.MarginLayoutParams) mTopButtons.getLayoutParams();
        lp.leftMargin = safeInsets.left + mButtonSize;
        lp.topMargin = safeInsets.top;
        lp.rightMargin = safeInsets.right + mButtonSize;
        mTopButtons.setLayoutParams(lp);
        mTopButtons.setVisibility(View.VISIBLE);

        // update right buttons
        lp = (ViewGroup.MarginLayoutParams) mRightButtons.getLayoutParams();
        lp.topMargin = safeInsets.top + mButtonSize;;
        lp.rightMargin = safeInsets.right;
        lp.bottomMargin = safeInsets.bottom + mButtonSize;;
        mRightButtons.setLayoutParams(lp);
        mRightButtons.setVisibility(View.VISIBLE);

        // update bottom buttons
        lp = (ViewGroup.MarginLayoutParams) mBottomButtons.getLayoutParams();
        lp.leftMargin = safeInsets.left + mButtonSize;
        lp.rightMargin = safeInsets.right + mButtonSize;
        lp.bottomMargin = safeInsets.bottom;
        mBottomButtons.setLayoutParams(lp);
        mBottomButtons.setVisibility(View.VISIBLE);

        // update description view
        lp = (ViewGroup.MarginLayoutParams) mDescription.getLayoutParams();
        lp.leftMargin = safeInsets.left + mButtonSize * 2;
        lp.rightMargin = safeInsets.right + mButtonSize * 2;
        mDescription.setLayoutParams(lp);

        // update pass fail button view
        lp = (ViewGroup.MarginLayoutParams) mPassFailButton.getLayoutParams();
        lp.leftMargin = safeInsets.left+ mButtonSize * 2;
        lp.rightMargin = safeInsets.right + mButtonSize * 2;
        mPassFailButton.setLayoutParams(lp);
    }

    public void onButtonClicked(View v) {
        final Button button = (Button) v;
        final int buttonNumber = Integer.valueOf((button.getText()).toString());
        String toastText = "Button #" + buttonNumber + "  clicked";
        if (mToast != null) {
            mToast.cancel();
        }
        mToast = Toast.makeText(getApplicationContext(), toastText, Toast.LENGTH_SHORT);
        mToast.show();

        // Indicate this button has been clicked.
        button.setTextColor(Color.BLUE);

        enablePassButtonIfNeeded(buttonNumber);
    }

    private void enablePassButtonIfNeeded(int buttonNumber) {
        mPassButtonEnableFlag |= (1 << buttonNumber);
        if (mPassButtonEnableFlag == FLAG_PASS_BUTTON_ENABLE) {
            getPassButton().setEnabled(true);
        }
    }
}
