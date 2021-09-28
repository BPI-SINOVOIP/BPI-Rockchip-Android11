/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.camera.widget;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.SeekBar;

import com.android.camera.CaptureLayoutHelper;
import com.android.camera.ShutterButton;
import com.android.camera.debug.Log;
import com.android.camera.ui.PreviewOverlay;
import com.android.camera.ui.TouchCoordinate;
import com.android.camera.util.CameraUtil;
import com.android.camera2.R;

/**
 * ModeOptionsOverlay is a FrameLayout which positions mode options in
 * in the bottom of the preview that is visible above the bottom bar.
 */
public class ModeOptionsOverlay extends FrameLayout
    implements PreviewOverlay.OnPreviewTouchedListener,
               ShutterButton.OnShutterButtonListener {

    private final static Log.Tag TAG = new Log.Tag("ModeOptionsOverlay");

    private static final int BOTTOMBAR_OPTIONS_TIMEOUT_MS = 2000;
    private static final int BOTTOM_RIGHT = Gravity.BOTTOM | Gravity.RIGHT;
    private static final int TOP_RIGHT = Gravity.TOP | Gravity.RIGHT;

    private ModeOptions mModeOptions;
    // need a reference to set the onClickListener and fix the layout gravity on orientation change
    private LinearLayout mModeOptionsToggle;
    // need a reference to fix the rotation on orientation change
    private ImageView mThreeDots;
    private CaptureLayoutHelper mCaptureLayoutHelper = null;

    private ModeOptionsScreenEffects mModeOptionsScreenEffects;

    private SeekBar mManualFocusBar;

    public ModeOptionsOverlay(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Whether the mode options are hidden.
     */
    public boolean isModeOptionsHidden() {
        return mModeOptions.isHiddenOrHiding();
    }

    /**
     * Gets the current width of the mode options toggle including the three dots and various mode
     * option indicators.
     */
    public float getModeOptionsToggleWidth() {
        return mModeOptionsToggle.getWidth();
    }

    /**
     * Sets a capture layout helper to query layout rect from.
     */
    public void setCaptureLayoutHelper(CaptureLayoutHelper helper) {
        mCaptureLayoutHelper = helper;
    }

    public void setToggleClickable(boolean clickable) {
        mModeOptionsToggle.setClickable(clickable);
    }

    public void showExposureOptions() {
        mModeOptions.showExposureOptions();
    }

    /**
     * Sets the mode options listener.
     *
     * @param listener The listener to be set.
     */
    public void setModeOptionsListener(ModeOptions.Listener listener) {
        mModeOptions.setListener(listener);
    }

    @Override
    public void onFinishInflate() {
        mModeOptions = (ModeOptions) findViewById(R.id.mode_options);
        mModeOptions.setClickable(true);
        mModeOptions.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                closeModeOptions();
            }
        });

        mModeOptionsToggle = (LinearLayout) findViewById(R.id.mode_options_toggle);
        mModeOptionsToggle.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mModeOptions.animateVisible();
            }
        });
        mModeOptions.setViewToShowHide(mModeOptionsToggle);

        mModeOptionsScreenEffects = (ModeOptionsScreenEffects) findViewById(R.id.mode_options_screen);
        mModeOptions.setScreenEffectsView(mModeOptionsScreenEffects);

        mThreeDots = (ImageView) findViewById(R.id.three_dots);

        mManualFocusBar = (SeekBar) findViewById(R.id.manual_focus);
        mModeOptions.setViewToOverlay(mManualFocusBar);
        checkOrientation(getResources().getConfiguration().orientation);
    }

    @Override
    public void onPreviewTouched(MotionEvent ev) {
        closeModeOptions();
    }

    @Override
    public void onShutterButtonClick() {
        closeModeOptions();
    }

    @Override
    public void onShutterCoordinate(TouchCoordinate coord) {
        // Do nothing.
    }

    @Override
    public void onShutterButtonFocus(boolean pressed) {
        // noop
    }

    @Override
    public void onShutterButtonLongPressed() {
        // TODO Auto-generated method stub
        closeModeOptions();
    }

    @Override
    public void onShutterButtonLongClickRelease() {
        // TODO Auto-generated method stub
        
    }

    public boolean onBackPressed() {
        if (mModeOptions.getVisibility() == View.VISIBLE || 
                mModeOptionsScreenEffects.getVisibility() == View.VISIBLE) {
            closeModeOptions();
            return true;
        }
        return false;
    }

    /**
     * Schedule (or re-schedule) the options menu to be closed after a number
     * of milliseconds.  If the options menu is already closed, nothing is
     * scheduled.
     */
    public void closeModeOptions() {
        mModeOptions.animateHidden();
        mModeOptionsScreenEffects.animateHidden();
    }

    public void closeModeOptions(boolean showToggle) {
        mModeOptions.animateHidden(showToggle);
        mModeOptionsScreenEffects.animateHidden();
    }

    public void closeManualFocusBar() {
        if (mManualFocusBar != null) {
            View parent = (View) mManualFocusBar.getParent();
            if (parent != null && parent.getVisibility() == View.VISIBLE)
                mManualFocusBar.setVisibility(View.GONE);
        }
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        super.onConfigurationChanged(configuration);
        checkOrientation(configuration.orientation);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mCaptureLayoutHelper == null) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            Log.e(TAG, "Capture layout helper needs to be set first.");
        } else {
            RectF uncoveredPreviewRect = mCaptureLayoutHelper.getUncoveredPreviewRect();
            super.onMeasure(MeasureSpec.makeMeasureSpec(
                            (int) uncoveredPreviewRect.width(), MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec((int) uncoveredPreviewRect.height(),
                            MeasureSpec.EXACTLY)
            );
        }
    }

    /**
     * Set the layout gravity of the child layout to be bottom or top right
     * depending on orientation.
     */
    public void checkOrientation(int orientation) {
        final boolean isPortrait = (Configuration.ORIENTATION_PORTRAIT == orientation);

        final int modeOptionsDimension = (int) getResources()
            .getDimension(R.dimen.mode_options_height);

        FrameLayout.LayoutParams modeOptionsParams
            = (FrameLayout.LayoutParams) mModeOptions.getLayoutParams();
        FrameLayout.LayoutParams modeOptionsToggleParams
            = (FrameLayout.LayoutParams) mModeOptionsToggle.getLayoutParams();

        LinearLayout manualFocusParent = (LinearLayout) mManualFocusBar.getParent();
        FrameLayout.LayoutParams manualFocusParentParams
            = (FrameLayout.LayoutParams) manualFocusParent.getLayoutParams();
        LinearLayout.LayoutParams manulFocusParams
            = (LinearLayout.LayoutParams) mManualFocusBar.getLayoutParams();

        if (isPortrait) {
            modeOptionsParams.height = modeOptionsDimension;
            modeOptionsParams.width = ViewGroup.LayoutParams.MATCH_PARENT;
            modeOptionsParams.gravity = Gravity.BOTTOM;

            modeOptionsToggleParams.gravity = BOTTOM_RIGHT;

            mThreeDots.setImageResource(R.drawable.ic_options_port);

            manualFocusParent.setOrientation(LinearLayout.HORIZONTAL);
            int left = (int) getResources().getDimension(R.dimen.manual_focus_seek_padding);
            int top = 0;
            int right = (int) getResources().getDimension(R.dimen.manual_focus_seek_padding);
            int bottom = 0;
            manualFocusParent.setPadding(left, top, right, bottom);
            manualFocusParent.setBackgroundResource(R.drawable.ic_manual_focus_tip_horizontal);
            manualFocusParentParams.height = modeOptionsDimension;
            manualFocusParentParams.width = ViewGroup.LayoutParams.MATCH_PARENT;
            manualFocusParentParams.gravity = Gravity.BOTTOM;

            manulFocusParams.width = ViewGroup.LayoutParams.MATCH_PARENT;
            manulFocusParams.height = ViewGroup.LayoutParams.WRAP_CONTENT;

            mManualFocusBar.setBackgroundResource(R.drawable.ic_manual_focus_progress_horizontal);
        } else {
            modeOptionsParams.width = modeOptionsDimension;
            modeOptionsParams.height = ViewGroup.LayoutParams.MATCH_PARENT;
            modeOptionsParams.gravity = Gravity.RIGHT;

            modeOptionsToggleParams.gravity = TOP_RIGHT;

            mThreeDots.setImageResource(R.drawable.ic_options_land);

            manualFocusParent.setOrientation(LinearLayout.VERTICAL);
            int left = 0;
            int top = (int) getResources().getDimension(R.dimen.manual_focus_seek_padding);
            int right = 0;
            int bottom = (int) getResources().getDimension(R.dimen.manual_focus_seek_padding);
            manualFocusParent.setPadding(left, top, right, bottom);
            manualFocusParent.setBackgroundResource(R.drawable.ic_manual_focus_tip_vertical);
            manualFocusParentParams.height = ViewGroup.LayoutParams.MATCH_PARENT;
            manualFocusParentParams.width = modeOptionsDimension;
            manualFocusParentParams.gravity = Gravity.RIGHT;

            manulFocusParams.width = ViewGroup.LayoutParams.WRAP_CONTENT;
            manulFocusParams.height = ViewGroup.LayoutParams.MATCH_PARENT;

            mManualFocusBar.setBackgroundResource(R.drawable.ic_manual_focus_progress_vertical);
        }

        requestLayout();
    }

    public void updateUIByOrientation() {
        mModeOptionsScreenEffects.setRotation(CameraUtil.mUIRotated);
        mModeOptions.updateUIByOrientation();
    }

}
