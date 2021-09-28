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

package com.android.camera.ui;

import android.content.Context;
import android.content.res.Configuration;
import android.view.animation.Interpolator;
import android.graphics.PointF;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import com.android.camera.CaptureLayoutHelper;
import com.android.camera.ShutterButton;
import com.android.camera.debug.Log;
import com.android.camera.ui.BottomBar.BottomBarButtonClickListener;
import com.android.camera.util.CameraUtil;
import com.android.camera.ui.motion.InterpolatorHelper;
import com.android.camera.widget.ModeOptions;
import com.android.camera.widget.ModeOptionsOverlay;
import com.android.camera.widget.RoundedThumbnailView;
import com.android.camera2.R;

/**
 * The goal of this class is to ensure mode options and capture indicator is
 * always laid out to the left of or above bottom bar in landscape or portrait
 * respectively. All the other children in this view group can be expected to
 * be laid out the same way as they are in a normal FrameLayout.
 */
public class StickyBottomCaptureLayout extends FrameLayout
    implements PreviewOverlay.OnPreviewTouchedListener,
    ShutterButton.OnShutterButtonListener,
    BottomBar.BottomBarButtonClickListener {

    private final static Log.Tag TAG = new Log.Tag("StickyBotCapLayout");
    private RoundedThumbnailView mRoundedThumbnailView;
    private ModeOptionsOverlay mModeOptionsOverlay;
    private View mBottomBar;
    private CaptureLayoutHelper mCaptureLayoutHelper = null;
    
    private AverageRadioOptions mSceneModeOptions;
    private AverageRadioOptions mColorModeOptions;
    
    private int mSceneModesHeight;

    private ModeOptions.Listener mModeOptionsListener = new ModeOptions.Listener() {
        @Override
        public void onBeginToShowModeOptions() {
            final PointF thumbnailViewPosition = getRoundedThumbnailPosition(
                    mCaptureLayoutHelper.getUncoveredPreviewRect(),
                    false,
                    mModeOptionsOverlay.getModeOptionsToggleWidth());
            final int orientation = getResources().getConfiguration().orientation;
            if (orientation == Configuration.ORIENTATION_PORTRAIT) {
                animateCaptureIndicatorToY(thumbnailViewPosition.y);
            } else {
                animateCaptureIndicatorToX(thumbnailViewPosition.x);
            }
        }

        @Override
        public void onBeginToHideModeOptions() {
            final PointF thumbnailViewPosition = getRoundedThumbnailPosition(
                    mCaptureLayoutHelper.getUncoveredPreviewRect(),
                    true,
                    mModeOptionsOverlay.getModeOptionsToggleWidth());
            final int orientation = getResources().getConfiguration().orientation;
            if (orientation == Configuration.ORIENTATION_PORTRAIT) {
                animateCaptureIndicatorToY(thumbnailViewPosition.y);
            } else {
                animateCaptureIndicatorToX(thumbnailViewPosition.x);
            }
        }
    };

    public StickyBottomCaptureLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        mRoundedThumbnailView = (RoundedThumbnailView) findViewById(R.id.rounded_thumbnail_view);
        mModeOptionsOverlay = (ModeOptionsOverlay) findViewById(R.id.mode_options_overlay);
        mModeOptionsOverlay.setModeOptionsListener(mModeOptionsListener);
        mBottomBar = findViewById(R.id.bottom_bar);
        mSceneModeOptions = (AverageRadioOptions) findViewById(R.id.scene_options);
        mColorModeOptions = (AverageRadioOptions) findViewById(R.id.color_options);

        mSceneModesHeight = getContext().getResources().
                getDimensionPixelSize(R.dimen.scene_options_height);
    }

    public boolean onBackPressed() {
        if (mSceneModeOptions.getVisibility() == View.VISIBLE) {
            mSceneModeOptions.animateHidden(true);
            return true;
        } else if (mColorModeOptions.getVisibility() == View.VISIBLE) {
            mColorModeOptions.animateHidden(true);
            return true;
        }
        return ((ModeOptionsOverlay) mModeOptionsOverlay).onBackPressed();
    }

    private void layoutButtonContents(RectF modeOptionRect, RectF bottomBarRect) {
        int displayrotation = CameraUtil.getDisplayRotation();
        int orientation = getContext().getResources().getConfiguration().orientation;
        final boolean isPortrait = Configuration.ORIENTATION_PORTRAIT == orientation;
        //Log.i(TAG,"isPortrait = " + isPortrait + ",displayrotation = " + displayrotation);
        int SceneModesSize = mSceneModesHeight;
        int ColorModesSize = 0;

        //Log.i(TAG,"scene size = " + SceneModesSize);
        //Log.i(TAG,"color size = " + ColorModesSize);
        //Log.i(TAG,"modeOptionRect = " + modeOptionRect.toString());
        //Log.i(TAG,"bottomBarRect = " + bottomBarRect.toString());

        if (isPortrait) {
            int left = (int) modeOptionRect.left;
            int top = (int) (modeOptionRect.bottom - SceneModesSize);
            int right = (int) modeOptionRect.right;
            int bottom = (int) modeOptionRect.bottom;
            mSceneModeOptions.layout(left, top, right, bottom);

            ColorModesSize = (int) (modeOptionRect.width() / mColorModeOptions.getChildCount());
            top = (int) (modeOptionRect.bottom - ColorModesSize);
            mColorModeOptions.layout(left, top, right, bottom);
        } else {
            int right = (int) modeOptionRect.right;
            int top = (int) modeOptionRect.top;
            int left = right - SceneModesSize;
            int bottom = (int) modeOptionRect.bottom;
            mSceneModeOptions.layout(left, top, right, bottom);

            ColorModesSize = (int) (modeOptionRect.height() / mColorModeOptions.getChildCount());
            left = right - ColorModesSize;
            mColorModeOptions.layout(left, top, right, bottom);
        } /*else if (displayrotation == 180) {
            int left = (int) modeOptionRect.left;
            int top = (int) modeOptionRect.top;
            int right = (int) modeOptionRect.right;
            int bottom = top + SceneModesSize;
            mSceneModeOptions.layout(left, top, right, bottom);

            bottom = top + ColorModesSize;
            mColorModeOptions.layout(left, top, right, bottom);
        } else if (displayrotation == 270) {
            int left = (int) modeOptionRect.left;
            int top = (int) modeOptionRect.top;
            int right = left + SceneModesSize;
            int bottom = (int) modeOptionRect.bottom;
            mSceneModeOptions.layout(left, top, right, bottom);

            right = left + ColorModesSize;
            mColorModeOptions.layout(left, top, right, bottom);
        }*/
    }

    /**
     * Sets a capture layout helper to query layout rect from.
     */
    public void setCaptureLayoutHelper(CaptureLayoutHelper helper) {
        mCaptureLayoutHelper = helper;
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        if (mCaptureLayoutHelper == null) {
            Log.e(TAG, "Capture layout helper needs to be set first.");
            return;
        }
        // Layout mode options overlay.
        RectF uncoveredPreviewRect = mCaptureLayoutHelper.getUncoveredPreviewRect();
        mModeOptionsOverlay.layout((int) uncoveredPreviewRect.left, (int) uncoveredPreviewRect.top,
                (int) uncoveredPreviewRect.right, (int) uncoveredPreviewRect.bottom);

        // Layout capture indicator.
        PointF roundedThumbnailViewPosition = getRoundedThumbnailPosition(
                uncoveredPreviewRect,
                mModeOptionsOverlay.isModeOptionsHidden(),
                mModeOptionsOverlay.getModeOptionsToggleWidth());
        mRoundedThumbnailView.layout(
                (int) roundedThumbnailViewPosition.x,
                (int) roundedThumbnailViewPosition.y,
                (int) roundedThumbnailViewPosition.x + mRoundedThumbnailView.getMeasuredWidth(),
                (int) roundedThumbnailViewPosition.y + mRoundedThumbnailView.getMeasuredHeight());

        // Layout bottom bar.
        RectF bottomBarRect = mCaptureLayoutHelper.getBottomBarRect();
        mBottomBar.layout((int) bottomBarRect.left, (int) bottomBarRect.top,
                (int) bottomBarRect.right, (int) bottomBarRect.bottom);

        layoutButtonContents(uncoveredPreviewRect, bottomBarRect);
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        // TODO Auto-generated method stub
        Log.i(TAG, "onConfigurationChanged");
        super.onConfigurationChanged(newConfig);
        /*final boolean isPortrait = (Configuration.ORIENTATION_PORTRAIT == newConfig.orientation);
        if (isPortrait) {
            mSceneModeOptions.setOrientation(LinearLayout.HORIZONTAL);
            mColorModeOptions.setOrientation(LinearLayout.HORIZONTAL);
        } else {
            mSceneModeOptions.setOrientation(LinearLayout.VERTICAL);
            mColorModeOptions.setOrientation(LinearLayout.VERTICAL);
        }
        requestLayout();*/
    }

    @Override
    public void ColorButtonOnClick() {
        // TODO Auto-generated method stub
        Log.i(TAG,"ColorButtonOnClick");
        ((ModeOptionsOverlay) mModeOptionsOverlay).closeModeOptions(false);
        if (mColorModeOptions.getVisibility() == View.VISIBLE) {
            mColorModeOptions.animateHidden(true);
        } else {
            mColorModeOptions.animateVisible();
            mSceneModeOptions.animateHidden(false);
        }
    }

    @Override
    public void SceneButtonOnClick() {
        // TODO Auto-generated method stub
        Log.i(TAG,"SceneButtonOnClick");
        ((ModeOptionsOverlay) mModeOptionsOverlay).closeModeOptions(false);
        if (mSceneModeOptions.getVisibility() == View.VISIBLE) {
            mSceneModeOptions.animateHidden(true);
        } else {
            mSceneModeOptions.animateVisible();
            mColorModeOptions.animateHidden(false);
        }
    }

    @Override
    public void onShutterButtonFocus(boolean pressed) {
        // TODO Auto-generated method stub
        
    }

    @Override
    public void onShutterCoordinate(TouchCoordinate coord) {
        // TODO Auto-generated method stub
    }

    @Override
    public void onShutterButtonClick() {
        // TODO Auto-generated method stub
        mColorModeOptions.animateHidden(true);
        mSceneModeOptions.animateHidden(true);
    }

    @Override
    public void onShutterButtonLongPressed() {
        // TODO Auto-generated method stub
        mColorModeOptions.animateHidden(true);
        mSceneModeOptions.animateHidden(true);
    }

    @Override
    public void onShutterButtonLongClickRelease() {
        // TODO Auto-generated method stub
        
    }

    @Override
    public void onPreviewTouched(MotionEvent ev) {
        // TODO Auto-generated method stub
        mColorModeOptions.animateHidden(true);
        mSceneModeOptions.animateHidden(true);
    }

    /**
     * Calculates the desired layout of capture indicator.
     *
     * @param uncoveredPreviewRect The uncovered preview bound which contains mode option
     *                             overlay and capture indicator.
     * @param isModeOptionsHidden Whether the mode options button are hidden.
     * @param modeOptionsToggleWidth The width of mode options toggle (three dots button).
     * @return the desired view bound for capture indicator.
     */
    private PointF getRoundedThumbnailPosition(
            RectF uncoveredPreviewRect, boolean isModeOptionsHidden, float modeOptionsToggleWidth) {
        final float threeDotsButtonDiameter =
                getResources().getDimension(R.dimen.option_button_circle_size);
        final float threeDotsButtonPadding =
                getResources().getDimension(R.dimen.mode_options_toggle_padding);
        final float modeOptionsHeight = getResources().getDimension(R.dimen.mode_options_height);

        final float roundedThumbnailViewSize = mRoundedThumbnailView.getMeasuredWidth();
        final float roundedThumbnailFinalSize = mRoundedThumbnailView.getThumbnailFinalDiameter();
        final float roundedThumbnailViewPadding = mRoundedThumbnailView.getThumbnailPadding();

        // The view bound is based on the maximal ripple ring diameter. This is the diff of maximal
        // ripple ring radius and the final thumbnail radius.
        final float radiusDiffBetweenViewAndThumbnail =
                (roundedThumbnailViewSize - roundedThumbnailFinalSize) / 2.0f;
        final float distanceFromModeOptions = roundedThumbnailViewPadding +
                roundedThumbnailFinalSize + radiusDiffBetweenViewAndThumbnail;

        final int orientation = getResources().getConfiguration().orientation;

        float x = 0;
        float y = 0;
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            // The view finder of 16:9 aspect ratio might have a black padding.
            x = uncoveredPreviewRect.right - distanceFromModeOptions;

            y = uncoveredPreviewRect.bottom;
            if (isModeOptionsHidden) {
                y -= threeDotsButtonPadding + threeDotsButtonDiameter;
            } else {
                y -= modeOptionsHeight;
            }
            y -= distanceFromModeOptions;
        }
        if (orientation == Configuration.ORIENTATION_LANDSCAPE) {
            if (isModeOptionsHidden) {
                x = uncoveredPreviewRect.right - threeDotsButtonPadding - modeOptionsToggleWidth;
            } else {
                x = uncoveredPreviewRect.right - modeOptionsHeight;
            }
            x -= distanceFromModeOptions;
            y = uncoveredPreviewRect.top + roundedThumbnailViewPadding -
                    radiusDiffBetweenViewAndThumbnail;
        }
        return new PointF(x, y);
    }

    private void animateCaptureIndicatorToX(float x) {
        final Interpolator interpolator =
                InterpolatorHelper.getLinearOutSlowInInterpolator(getContext());
        mRoundedThumbnailView.animate()
                .setDuration(ModeOptions.PADDING_ANIMATION_TIME)
                .setInterpolator(interpolator)
                .x(x)
                .withEndAction(new Runnable() {
                    @Override
                    public void run() {
                        mRoundedThumbnailView.setTranslationX(0.0f);
                        requestLayout();
                    }
                });
    }

    private void animateCaptureIndicatorToY(float y) {
        final Interpolator interpolator =
                InterpolatorHelper.getLinearOutSlowInInterpolator(getContext());
        mRoundedThumbnailView.animate()
                .setDuration(ModeOptions.PADDING_ANIMATION_TIME)
                .setInterpolator(interpolator)
                .y(y)
                .withEndAction(new Runnable() {
                    @Override
                    public void run() {
                        mRoundedThumbnailView.setTranslationY(0.0f);
                        requestLayout();
                    }
                });
    }
}
