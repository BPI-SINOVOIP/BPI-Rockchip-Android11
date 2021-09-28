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

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.android.camera.ButtonManager;
import com.android.camera.app.AppController;
import com.android.camera.debug.DebugPropertyHelper;
import com.android.camera.debug.Log;
import com.android.camera.settings.Keys;
import com.android.camera.settings.SettingsManager;
import com.android.camera.util.CameraUtil;
import com.android.camera.util.Gusterpolator;
import com.android.camera.util.PhotoSphereHelper;
import com.android.ex.camera2.portability.CameraCapabilities;
import com.android.camera2.R;

/**
 * IndicatorIconController sets the visibility and icon state of
 * on screen indicators.
 *
 * Indicators are only visible if they are in a non-default state.  The
 * visibility of an indicator is set when an indicator's setting changes.
 */
public class IndicatorIconController
    implements SettingsManager.OnSettingChangedListener,
               ButtonManager.ButtonStatusListener {

    private final static Log.Tag TAG = new Log.Tag("IndicatorIconCtrlr");

    private ImageView mFlashIndicator;
    private ImageView mHdrIndicator;
    private ImageView mPanoIndicator;
    private ImageView mCountdownTimerIndicator;
    private ImageView mZslIndicator;
    private ImageView mSmileShutterIndicator;
    private ImageView m3dnrIndicator;

    private ImageView mExposureIndicatorN2;
    private ImageView mExposureIndicatorN1;
    private ImageView mExposureIndicatorP1;
    private ImageView mExposureIndicatorP2;
    
    private ImageView mWbIndicatorCloudy;
    private ImageView mWbIndicatorFluorescent;
    private ImageView mWbIndicatorTungsten;
    private ImageView mWbIndicatorDaylight;

    private LinearLayout mModeOptionsToggle;

    private TypedArray mFlashIndicatorPhotoIcons;
    private TypedArray mFlashIndicatorVideoIcons;
    private TypedArray mHdrPlusIndicatorIcons;
    private TypedArray mHdrIndicatorIcons;
    private TypedArray mPanoIndicatorIcons;
    private TypedArray mCountdownTimerIndicatorIcons;
    private TypedArray mZslIndicatorIcons;
    private TypedArray mSmileShutterIndicatorIcons;
    private TypedArray m3dnrIndicatorIcons;

    private AppController mController;
    
    private TextView mSmileShutterTip;
    private AnimatorSet mSmileShutterAnimator;

    public IndicatorIconController(AppController controller, View root) {
        mController = controller;
        Context context = controller.getAndroidContext();

        mModeOptionsToggle = (LinearLayout) root.findViewById(R.id.mode_options_toggle);

        mFlashIndicator = (ImageView) root.findViewById(R.id.flash_indicator);
        mFlashIndicatorPhotoIcons = context.getResources().obtainTypedArray(
            R.array.camera_flashmode_indicator_icons);
        mFlashIndicatorVideoIcons = context.getResources().obtainTypedArray(
            R.array.video_flashmode_indicator_icons);

        mHdrIndicator = (ImageView) root.findViewById(R.id.hdr_indicator);
        mHdrPlusIndicatorIcons = context.getResources().obtainTypedArray(
            R.array.pref_camera_hdr_plus_indicator_icons);
        mHdrIndicatorIcons = context.getResources().obtainTypedArray(
            R.array.pref_camera_hdr_indicator_icons);

        int panoIndicatorArrayId = PhotoSphereHelper.getPanoramaOrientationIndicatorArrayId();
        if (panoIndicatorArrayId > 0) {
            mPanoIndicator = (ImageView) root.findViewById(R.id.pano_indicator);
            mPanoIndicatorIcons =
                context.getResources().obtainTypedArray(panoIndicatorArrayId);
        }

        mCountdownTimerIndicator = (ImageView) root.findViewById(R.id.countdown_timer_indicator);
        mCountdownTimerIndicatorIcons = context.getResources().obtainTypedArray(
                R.array.pref_camera_countdown_indicators);
        
        mZslIndicator = (ImageView) root.findViewById(R.id.zsl_indicator);
        mZslIndicatorIcons = context.getResources().obtainTypedArray(
                R.array.zsl_indicator_icons);
        
        mSmileShutterIndicator = (ImageView) root.findViewById(R.id.smile_shutter_indicator);
        mSmileShutterIndicatorIcons = context.getResources().obtainTypedArray(
                R.array.smile_shutter_indicator_icons);

        m3dnrIndicator = (ImageView) root.findViewById(R.id.threednr_indicator);
        m3dnrIndicatorIcons = context.getResources().obtainTypedArray(
                R.array.threednr_indicator_icons);

        mExposureIndicatorN2 = (ImageView) root.findViewById(R.id.exposure_n2_indicator);
        mExposureIndicatorN1 = (ImageView) root.findViewById(R.id.exposure_n1_indicator);
        mExposureIndicatorP1 = (ImageView) root.findViewById(R.id.exposure_p1_indicator);
        mExposureIndicatorP2 = (ImageView) root.findViewById(R.id.exposure_p2_indicator);
        
        mWbIndicatorCloudy = (ImageView) root.findViewById(R.id.wb_cloudy_indicator);
        mWbIndicatorFluorescent = (ImageView) root.findViewById(R.id.wb_fluorescent_indicator);
        mWbIndicatorTungsten = (ImageView) root.findViewById(R.id.wb_tungsten_indicator);
        mWbIndicatorDaylight = (ImageView) root.findViewById(R.id.wb_daylight_indicator);
        
        mSmileShutterTip = (TextView) root.findViewById(R.id.smile_shutter_tip);
        if (DebugPropertyHelper.isSmileShutterAuto())
            mSmileShutterTip.setText(R.string.smile_shutter_auto_tip);
        setupSmileShutterAnimator();
    }

    @Override
    public void onButtonVisibilityChanged(ButtonManager buttonManager, int buttonId) {
        syncIndicatorWithButton(buttonId);
    }

    @Override
    public void onButtonEnabledChanged(ButtonManager buttonManager, int buttonId) {
        syncIndicatorWithButton(buttonId);
    }

    /**
     * Syncs a specific indicator's icon and visibility
     * based on the enabled state and visibility of a button.
     */
    private void syncIndicatorWithButton(int buttonId) {
        switch (buttonId) {
            case ButtonManager.BUTTON_FLASH: {
                syncFlashIndicator();
                break;
            }
            case ButtonManager.BUTTON_TORCH: {
                syncFlashIndicator();
                break;
            }
            case ButtonManager.BUTTON_HDR_PLUS: {
                syncHdrIndicator();
                break;
            }
            case ButtonManager.BUTTON_HDR: {
                syncHdrIndicator();
                break;
            }
            case ButtonManager.BUTTON_EXPOSURE_COMPENSATION: {
                syncExposureIndicator();
                break;
            }
            case ButtonManager.BUTTON_WHITEBALANCE: {
                syncWhiteBalanceIndicator();
                break;
            }
            default:
                // Do nothing.  The indicator doesn't care
                // about button that don't correspond to indicators.
        }
    }

    /**
     * Sets all indicators to the correct resource and visibility
     * based on the current settings.
     */
    public void syncIndicators() {
        syncFlashIndicator();
        syncHdrIndicator();
        syncPanoIndicator();
        syncExposureIndicator();
        syncCountdownTimerIndicator();
        syncZslIndicator();
        syncSmileShutterIndicator();
        syncWhiteBalanceIndicator();
        sync3dnrIndicator();
    }

    /**
     * If the new visibility is different from the current visibility
     * on a view, change the visibility and call any registered
     * {@link OnIndicatorVisibilityChangedListener}.
     */
    private static void changeVisibility(View view, int visibility) {
        if (view.getVisibility() != visibility) {
            view.setVisibility(visibility);
        }
    }

    /**
     * Sync the icon and visibility of the flash indicator.
     */
    private void syncFlashIndicator() {
        ButtonManager buttonManager = mController.getButtonManager();
        // If flash isn't an enabled and visible option,
        // do not show the indicator.
        if ((buttonManager.isEnabled(ButtonManager.BUTTON_FLASH)
                && buttonManager.isVisible(ButtonManager.BUTTON_FLASH))
                || (buttonManager.isEnabled(ButtonManager.BUTTON_TORCH)
                && buttonManager.isVisible(ButtonManager.BUTTON_TORCH))) {

            int modeIndex = mController.getCurrentModuleIndex();
            if (modeIndex == mController.getAndroidContext().getResources()
                    .getInteger(R.integer.camera_mode_video)) {
                setIndicatorState(mController.getCameraScope(),
                                  Keys.KEY_VIDEOCAMERA_FLASH_MODE, mFlashIndicator,
                                  mFlashIndicatorVideoIcons, false);
            } else if (modeIndex == mController.getAndroidContext().getResources()
                    .getInteger(R.integer.camera_mode_gcam)) {
                setIndicatorState(mController.getCameraScope(),
                                  Keys.KEY_HDR_PLUS_FLASH_MODE, mFlashIndicator,
                                  mFlashIndicatorPhotoIcons, false);
            } else {
                setIndicatorState(mController.getCameraScope(),
                                  Keys.KEY_FLASH_MODE, mFlashIndicator,
                                  mFlashIndicatorPhotoIcons, false);
            }
        } else {
            changeVisibility(mFlashIndicator, View.GONE);
        }
    }

    /**
     * Sync the icon and the visibility of the hdr/hdrplus indicator.
     */
    private void syncHdrIndicator() {
        ButtonManager buttonManager = mController.getButtonManager();
        // If hdr isn't an enabled and visible option,
        // do not show the indicator.
        if (buttonManager.isEnabled(ButtonManager.BUTTON_HDR_PLUS)
                && buttonManager.isVisible(ButtonManager.BUTTON_HDR_PLUS)) {
            setIndicatorState(SettingsManager.SCOPE_GLOBAL,
                              Keys.KEY_CAMERA_HDR_PLUS, mHdrIndicator,
                              mHdrPlusIndicatorIcons, false);
        } else if (buttonManager.isEnabled(ButtonManager.BUTTON_HDR)
                && buttonManager.isVisible(ButtonManager.BUTTON_HDR)) {
            setIndicatorState(SettingsManager.SCOPE_GLOBAL,
                              Keys.KEY_CAMERA_HDR, mHdrIndicator,
                              mHdrIndicatorIcons, false);
        } else {
            changeVisibility(mHdrIndicator, View.GONE);
        }
    }

    /**
     * Sync the icon and the visibility of the pano indicator.
     */
    private void syncPanoIndicator() {
        if (mPanoIndicator == null) {
            Log.w(TAG, "Trying to sync a pano indicator that is not initialized.");
            return;
        }

        ButtonManager buttonManager = mController.getButtonManager();
        if (buttonManager.isPanoEnabled()) {
            setIndicatorState(SettingsManager.SCOPE_GLOBAL,
                              Keys.KEY_CAMERA_PANO_ORIENTATION, mPanoIndicator,
                              mPanoIndicatorIcons, true);
        } else {
            changeVisibility(mPanoIndicator, View.GONE);
        }
    }

    private void syncExposureIndicator() {
        if (mExposureIndicatorN2 == null
            || mExposureIndicatorN1 == null
            || mExposureIndicatorP1 == null
            || mExposureIndicatorP2 == null) {
            Log.w(TAG, "Trying to sync exposure indicators that are not initialized.");
            return;
        }


        // Reset all exposure indicator icons.
        changeVisibility(mExposureIndicatorN2, View.GONE);
        changeVisibility(mExposureIndicatorN1, View.GONE);
        changeVisibility(mExposureIndicatorP1, View.GONE);
        changeVisibility(mExposureIndicatorP2, View.GONE);

        ButtonManager buttonManager = mController.getButtonManager();
        if (buttonManager.isEnabled(ButtonManager.BUTTON_EXPOSURE_COMPENSATION)
                && buttonManager.isVisible(ButtonManager.BUTTON_EXPOSURE_COMPENSATION)) {

            int compValue = mController.getSettingsManager().getInteger(
                    mController.getCameraScope(), Keys.KEY_EXPOSURE);
            int comp = Math.round(compValue * buttonManager.getExposureCompensationStep());

            // Turn on the appropriate indicator.
            switch (comp) {
                case -2:
                    changeVisibility(mExposureIndicatorN2, View.VISIBLE);
                    break;
                case -1:
                    changeVisibility(mExposureIndicatorN1, View.VISIBLE);
                    break;
                case 0:
                    // Do nothing.
                    break;
                case 1:
                    changeVisibility(mExposureIndicatorP1, View.VISIBLE);
                    break;
                case 2:
                    changeVisibility(mExposureIndicatorP2, View.VISIBLE);
            }
        }
    }

    private void syncWhiteBalanceIndicator() {
        if (mWbIndicatorCloudy == null
            || mWbIndicatorFluorescent == null
            || mWbIndicatorTungsten == null
            || mWbIndicatorDaylight == null) {
            Log.w(TAG, "Trying to sync whitebalance indicators that are not initialized.");
            return;
        }

        changeVisibility(mWbIndicatorCloudy, View.GONE);
        changeVisibility(mWbIndicatorFluorescent, View.GONE);
        changeVisibility(mWbIndicatorTungsten, View.GONE);
        changeVisibility(mWbIndicatorDaylight, View.GONE);

        ButtonManager buttonManager = mController.getButtonManager();
        if (buttonManager.isEnabled(ButtonManager.BUTTON_WHITEBALANCE)
                && buttonManager.isVisible(ButtonManager.BUTTON_WHITEBALANCE)) {
            String value = mController.getSettingsManager().getString(
                    mController.getCameraScope(), Keys.KEY_WHITEBALANCE);
            if (ButtonManager.toApiCase(CameraCapabilities.WhiteBalance.CLOUDY_DAYLIGHT.name())
                    .equals(value)) {
                changeVisibility(mWbIndicatorCloudy, View.VISIBLE);
            } else if (ButtonManager.toApiCase(CameraCapabilities.WhiteBalance.FLUORESCENT.name())
                        .equals(value)) {
                changeVisibility(mWbIndicatorFluorescent, View.VISIBLE);
            } else if (ButtonManager.toApiCase(CameraCapabilities.WhiteBalance.INCANDESCENT.name())
                        .equals(value)) {
                changeVisibility(mWbIndicatorTungsten, View.VISIBLE);
            } else if (ButtonManager.toApiCase(CameraCapabilities.WhiteBalance.DAYLIGHT.name())
                        .equals(value)) {
                changeVisibility(mWbIndicatorDaylight, View.VISIBLE);
            }
        }
    }

    private void syncCountdownTimerIndicator() {
        ButtonManager buttonManager = mController.getButtonManager();

        if (buttonManager.isEnabled(ButtonManager.BUTTON_COUNTDOWN)
            && buttonManager.isVisible(ButtonManager.BUTTON_COUNTDOWN)) {
            setIndicatorState(SettingsManager.SCOPE_GLOBAL,
                              Keys.KEY_COUNTDOWN_DURATION, mCountdownTimerIndicator,
                              mCountdownTimerIndicatorIcons, false);
        } else {
            changeVisibility(mCountdownTimerIndicator, View.GONE);
        }
    }

    private void syncZslIndicator() {
        ButtonManager buttonManager = mController.getButtonManager();

        if (buttonManager.isEnabled(ButtonManager.BUTTON_ZSL)
            && buttonManager.isVisible(ButtonManager.BUTTON_ZSL)) {
            setIndicatorState(SettingsManager.SCOPE_GLOBAL,
                              Keys.KEY_BURST_CAPTURE_ON, mZslIndicator,
                              mZslIndicatorIcons, false);
            SettingsManager settingsManager = mController.getSettingsManager();
            if (settingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL, Keys.KEY_BURST_CAPTURE_ON))
                changeVisibility(mZslIndicator, View.VISIBLE);
            else
                changeVisibility(mZslIndicator, View.GONE);
        } else {
            changeVisibility(mZslIndicator, View.GONE);
        }
    }

    private void sync3dnrIndicator() {
        ButtonManager buttonManager = mController.getButtonManager();

        if (buttonManager.isEnabled(ButtonManager.BUTTON_3DNR)
            && buttonManager.isVisible(ButtonManager.BUTTON_3DNR)) {
            setIndicatorState(SettingsManager.SCOPE_GLOBAL,
                              Keys.KEY_3DNR_ON, m3dnrIndicator,
                              m3dnrIndicatorIcons, false);
            SettingsManager settingsManager = mController.getSettingsManager();
            if (settingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL, Keys.KEY_3DNR_ON))
                changeVisibility(m3dnrIndicator, View.VISIBLE);
            else
                changeVisibility(m3dnrIndicator, View.GONE);
        } else {
            changeVisibility(m3dnrIndicator, View.GONE);
        }
    }

    private void syncSmileShutterIndicator() {
        ButtonManager buttonManager = mController.getButtonManager();

        if (buttonManager.isEnabled(ButtonManager.BUTTON_SMILE_SHUTTER)
            && buttonManager.isVisible(ButtonManager.BUTTON_SMILE_SHUTTER)) {
            setIndicatorState(SettingsManager.SCOPE_GLOBAL,
                              Keys.KEY_SMILE_SHUTTER_ON, mSmileShutterIndicator,
                              mSmileShutterIndicatorIcons, false);
        } else {
            changeVisibility(mSmileShutterIndicator, View.GONE);
        }
        if (mSmileShutterIndicator.getVisibility() == View.VISIBLE) {
            mSmileShutterTip.setVisibility(View.VISIBLE);
        } else {
            mSmileShutterTip.setVisibility(View.GONE);
        }
    }

    private void setupSmileShutterAnimator() {
        if (mSmileShutterAnimator != null) {
            mSmileShutterAnimator.end();
        }
        final ValueAnimator alphaAnimator = ValueAnimator.ofFloat(1.0f, 0.0f);
        alphaAnimator.setDuration(800);
        alphaAnimator.setRepeatCount(ValueAnimator.INFINITE);
        alphaAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                mSmileShutterIndicator.setAlpha((Float) animation.getAnimatedValue());
                mSmileShutterIndicator.invalidate();
            }
        });
        alphaAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mSmileShutterIndicator.setAlpha(0.0f);
                mSmileShutterIndicator.invalidate();
            }
        });
        
        mSmileShutterAnimator = new AnimatorSet();
        mSmileShutterAnimator.setInterpolator(Gusterpolator.INSTANCE);
        mSmileShutterAnimator.play(alphaAnimator);
    }

    public void smileShutterAnimator(boolean on) {
        Log.i(TAG,"smileShutterAnimator on = " + on);
        if (mSmileShutterAnimator == null) return;
        if (on) {
            mSmileShutterIndicator.setAlpha(1.0f);
            mSmileShutterAnimator.end();
            mSmileShutterAnimator.start();
        } else {
            mSmileShutterAnimator.end();
            mSmileShutterIndicator.setAlpha(1.0f);
        }
    }

    /**
     * Sets the image resource and visibility of the indicator
     * based on the indicator's corresponding setting state.
     */
    private void setIndicatorState(String scope, String key, ImageView imageView,
                                   TypedArray iconArray, boolean showDefault) {
        SettingsManager settingsManager = mController.getSettingsManager();

        int valueIndex = settingsManager.getIndexOfCurrentValue(scope, key);
        if (valueIndex < 0) {
            // This can happen when the setting is camera dependent
            // and the camera is not yet open.  CameraAppUI.onChangeCamera()
            // will call this again when the camera is open.
            Log.w(TAG, "The setting for this indicator is not available.");
            imageView.setVisibility(View.GONE);
            return;
        }
        Drawable drawable = iconArray.getDrawable(valueIndex);
        if (drawable == null) {
            throw new IllegalStateException("Indicator drawable is null.");
        }
        imageView.setImageDrawable(drawable);

        // Set the indicator visible if not in default state.
        boolean visibilityChanged = false;
        if (!showDefault && settingsManager.isDefault(scope, key)) {
            changeVisibility(imageView, View.GONE);
        } else {
            changeVisibility(imageView, View.VISIBLE);
        }
    }

    @Override
    public void onSettingChanged(SettingsManager settingsManager, String key) {
        if (key.equals(Keys.KEY_FLASH_MODE)) {
            syncFlashIndicator();
            return;
        }
        if (key.equals(Keys.KEY_VIDEOCAMERA_FLASH_MODE)) {
            syncFlashIndicator();
            return;
        }
        if (key.equals(Keys.KEY_CAMERA_HDR_PLUS)) {
            syncHdrIndicator();
            return;
        }
        if (key.equals(Keys.KEY_CAMERA_HDR)) {
            syncHdrIndicator();
            return;
        }
        if (key.equals(Keys.KEY_CAMERA_PANO_ORIENTATION)) {
            syncPanoIndicator();
            return;
        }
        if (key.equals(Keys.KEY_EXPOSURE)) {
            syncExposureIndicator();
            return;
        }
        if (key.equals(Keys.KEY_COUNTDOWN_DURATION)) {
            syncCountdownTimerIndicator();
            return;
        }
        if (key.equals(Keys.KEY_BURST_CAPTURE_ON)) {
            syncZslIndicator();
            return;
        }
        if (key.equals(Keys.KEY_SMILE_SHUTTER_ON)) {
            syncSmileShutterIndicator();
            return;
        }
        if (key.equals(Keys.KEY_WHITEBALANCE)) {
            syncWhiteBalanceIndicator();
            return;
        }
        if (key.equals(Keys.KEY_3DNR_ON)) {
            sync3dnrIndicator();
            return;
        }
    }

    public void updateUIByOrientation() {
        if (CameraUtil.AUTO_ROTATE_SENSOR) return;
        if (CameraUtil.mIsPortrait) {
            mModeOptionsToggle.setTranslationX(0.0f);
            mModeOptionsToggle.setTranslationY(0.0f);
        } else {
            mModeOptionsToggle.setTranslationX((mModeOptionsToggle.getWidth() - mModeOptionsToggle.getHeight())
                    / 2.0f);
            mModeOptionsToggle.setTranslationY((mModeOptionsToggle.getHeight() - mModeOptionsToggle.getWidth())
                    / 2.0f);
        }
        mModeOptionsToggle.setRotation(CameraUtil.mUIRotated);
    }

}
