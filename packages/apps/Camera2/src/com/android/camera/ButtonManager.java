/*
 * Copyright (C) 2013 The Android Open Source Project
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

package com.android.camera;

import java.util.List;
import java.util.Locale;
import java.util.Set;

import android.content.Context;
import android.content.res.TypedArray;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import com.android.camera.app.AppController;
import com.android.camera.app.CameraAppUI;
import com.android.camera.debug.Log;
import com.android.camera.settings.Keys;
import com.android.camera.settings.SettingsManager;
import com.android.camera.ui.AverageRadioOptions;
import com.android.camera.ui.HorizontalRadioOptions;
import com.android.camera.ui.RadioOptions;
import com.android.camera.ui.TopRightWeightedLayout;
import com.android.camera.util.PhotoSphereHelper;
import com.android.camera.widget.ModeOptions;
import com.android.ex.camera2.portability.CameraCapabilities;
import com.android.camera2.R;

/**
 * A  class for generating pre-initialized
 * {@link #android.widget.ImageButton}s.
 */
public class ButtonManager implements SettingsManager.OnSettingChangedListener {
    public static final int BUTTON_FLASH = 0;
    public static final int BUTTON_TORCH = 1;
    public static final int BUTTON_HDR_PLUS_FLASH = 2;
    public static final int BUTTON_CAMERA = 3;
    public static final int BUTTON_HDR_PLUS = 4;
    public static final int BUTTON_HDR = 5;
    public static final int BUTTON_CANCEL = 6;
    public static final int BUTTON_DONE = 7;
    public static final int BUTTON_RETAKE = 8;
    public static final int BUTTON_REVIEW = 9;
    public static final int BUTTON_GRID_LINES = 10;
    public static final int BUTTON_EXPOSURE_COMPENSATION = 11;
    public static final int BUTTON_COUNTDOWN = 12;
    public static final int BUTTON_WHITEBALANCE = 13;
    public static final int BUTTON_SCENE = 14;
    public static final int BUTTON_COLOR = 15;
    public static final int BUTTON_ZSL = 16;
    public static final int BUTTON_SMILE_SHUTTER = 17;
    public static final int BUTTON_3DNR = 18;

    /** For two state MultiToggleImageButtons, the off index. */
    public static final int OFF = 0;
    /** For two state MultiToggleImageButtons, the on index. */
    public static final int ON = 1;

    private static final int NO_RESOURCE = -1;

    /** A reference to the application's settings manager. */
    private final SettingsManager mSettingsManager;

    /** Bottom bar options toggle buttons. */
    private MultiToggleImageButton mButtonCamera;
    private MultiToggleImageButton mButtonFlash;
    private MultiToggleImageButton mButtonHdr;
    private MultiToggleImageButton mButtonGridlines;
    private MultiToggleImageButton mButtonCountdown;
    private MultiToggleImageButton mButtonZsl;
    private MultiToggleImageButton mButtonSmileShutter;
    private MultiToggleImageButton mButton3dnr;

    /** Intent UI buttons. */
    private ImageButton mButtonCancel;
    private ImageButton mButtonDone;
    private ImageButton mButtonRetake; // same as review.

    private ImageButton mButtonExposureCompensation;
    private ImageButton mExposureN2;
    private ImageButton mExposureN1;
    private ImageButton mExposure0;
    private ImageButton mExposureP1;
    private ImageButton mExposureP2;
    private RadioOptions mModeOptionsExposure;
    private RadioOptions mModeOptionsPano;
    private View mModeOptionsButtons;
    private ModeOptions mModeOptions;

    private ImageButton mExposureN2OfScreen;
    private ImageButton mExposureN1OfScreen;
    private ImageButton mExposure0OfScreen;
    private ImageButton mExposureP1OfScreen;
    private ImageButton mExposureP2OfScreen;
    private HorizontalRadioOptions mModeOptionsExposureOfScreen;

    private ImageButton mContrastN1;
    private ImageButton mContrast0;
    private ImageButton mContrastP1;
    private HorizontalRadioOptions mModeOptionsContrast;

    private ImageButton mSaturationN1;
    private ImageButton mSaturation0;
    private ImageButton mSaturationP1;
    private HorizontalRadioOptions mModeOptionsSaturation;
    
    private ImageButton mSharpnessN1;
    private ImageButton mSharpness0;
    private ImageButton mSharpnessP1;;
    private HorizontalRadioOptions mModeOptionsSharpness;

    private ImageButton mBrightnessN1;
    private ImageButton mBrightness0;
    private ImageButton mBrightnessP1;
    private HorizontalRadioOptions mModeOptionsBrightness;

    private ImageButton mHueN1;
    private ImageButton mHue0;
    private ImageButton mHueP1;
    private HorizontalRadioOptions mModeOptionsHue;

    private ImageButton mButtonWhiteBalance;
    private ImageButton mWbCloudy;
    private ImageButton mWbFluorescent;
    private ImageButton mWbAuto;
    private ImageButton mWbIncandescent;
    private ImageButton mWbSunlight;
    private RadioOptions mModeOptionsWhiteBalance;

    private ImageButton mButtonColor;
    private FrameLayout mColorNone;
    private FrameLayout mColorMono;
    private FrameLayout mColorNegative;
    private FrameLayout mColorSepia;
    private AverageRadioOptions mModeOptionsColor;

    private ImageButton mButtonScene;
    private LinearLayout mSceneAuto;
    private LinearLayout mScenePortrait;
    private LinearLayout mSceneLandscape;
    private LinearLayout mSceneNight;
    private LinearLayout mSceneSports;
    private LinearLayout mSceneBarcode;
    private AverageRadioOptions mModeOptionsScene; 

    private int mMinExposureCompensation;
    private int mMaxExposureCompensation;
    private float mExposureCompensationStep;

    /** A listener for button enabled and visibility
        state changes. */
    private ButtonStatusListener mListener;

    /** An reference to the gcam mode index. */
    private static int sGcamIndex;

    /** Whether Camera Button can be enabled by generic operations. */
    private boolean mIsCameraButtonBlocked;

    private final AppController mAppController;

    private CameraCapabilities mCameraCapabilities;

    private boolean mButtonLoaded = false;

    /**
     * Get a new global ButtonManager.
     */
    public ButtonManager(AppController app) {
        mAppController = app;

        Context context = app.getAndroidContext();
        sGcamIndex = context.getResources().getInteger(R.integer.camera_mode_gcam);

        mSettingsManager = app.getSettingsManager();
        mSettingsManager.addListener(this);
    }

    /**
     * Load references to buttons under a root View.
     * Call this after the root clears/reloads all of its children
     * to prevent stale references button views.
     */
    public void load(View root) {
        getButtonsReferences(root);
        mButtonLoaded = true;
    }
    
    public static String toApiCase(String enumCase) {
        return enumCase.toLowerCase(Locale.US).replaceAll("_", "-");
    }

    /**
     * ButtonStatusListener provides callbacks for when button's
     * visibility changes and enabled status changes.
     */
    public interface ButtonStatusListener {
        /**
         * A button's visibility has changed.
         */
        public void onButtonVisibilityChanged(ButtonManager buttonManager, int buttonId);

        /**
         * A button's enabled state has changed.
         */
        public void onButtonEnabledChanged(ButtonManager buttonManager, int buttonId);
    }

    /**
     * Sets the ButtonStatusListener.
     */
    public void setListener(ButtonStatusListener listener) {
        mListener = listener;
    }
    
    public void setCameraCapabilities(CameraCapabilities cameraCapabilities) {
        mCameraCapabilities = cameraCapabilities;
    }

    public CameraCapabilities getCameraCapabilities() {
        return mCameraCapabilities;
    }

    public void setScreenEffectOptionsEnabled(boolean enable) {
        mModeOptions.setScreenEffectOptionsEnabled(enable);
    }

    /**
     * Gets references to all known buttons.
     */
    private void getButtonsReferences(View root) {
        mButtonCamera
            = (MultiToggleImageButton) root.findViewById(R.id.camera_toggle_button);
        mButtonFlash
            = (MultiToggleImageButton) root.findViewById(R.id.flash_toggle_button);
        mButtonHdr
            = (MultiToggleImageButton) root.findViewById(R.id.hdr_plus_toggle_button);
        mButtonGridlines
            = (MultiToggleImageButton) root.findViewById(R.id.grid_lines_toggle_button);
        mButtonCancel
            = (ImageButton) root.findViewById(R.id.cancel_button);
        mButtonDone
            = (ImageButton) root.findViewById(R.id.done_button);
        mButtonRetake
            = (ImageButton) root.findViewById(R.id.retake_button);
        mButtonZsl
            = (MultiToggleImageButton) root.findViewById(R.id.zsl_toggle_button);
        mButtonSmileShutter
            = (MultiToggleImageButton) root.findViewById(R.id.smile_shutter_toggle_button);
        mButton3dnr
            = (MultiToggleImageButton) root.findViewById(R.id.threednr_toggle_button);

        mButtonExposureCompensation =
            (ImageButton) root.findViewById(R.id.exposure_button);
        mExposureN2 = (ImageButton) root.findViewById(R.id.exposure_n2);
        mExposureN1 = (ImageButton) root.findViewById(R.id.exposure_n1);
        mExposure0 = (ImageButton) root.findViewById(R.id.exposure_0);
        mExposureP1 = (ImageButton) root.findViewById(R.id.exposure_p1);
        mExposureP2 = (ImageButton) root.findViewById(R.id.exposure_p2);
        mModeOptionsExposure = (RadioOptions) root.findViewById(R.id.mode_options_exposure);
        mModeOptionsPano = (RadioOptions) root.findViewById(R.id.mode_options_pano);
        mModeOptionsButtons = root.findViewById(R.id.mode_options_buttons);
        mModeOptions = (ModeOptions) root.findViewById(R.id.mode_options);

        mButtonCountdown = (MultiToggleImageButton) root.findViewById(R.id.countdown_toggle_button);

        mExposureN2OfScreen = (ImageButton) root.findViewById(R.id.exposure_n2_of_screen);
        mExposureN1OfScreen = (ImageButton) root.findViewById(R.id.exposure_n1_of_screen);
        mExposure0OfScreen = (ImageButton) root.findViewById(R.id.exposure_0_of_screen);
        mExposureP1OfScreen = (ImageButton) root.findViewById(R.id.exposure_p1_of_screen);
        mExposureP2OfScreen = (ImageButton) root.findViewById(R.id.exposure_p2_of_screen);
        mModeOptionsExposureOfScreen = (HorizontalRadioOptions) root.findViewById(R.id.mode_options_exposure_of_screen);

        mSaturationN1 = (ImageButton) root.findViewById(R.id.saturation_n1);
        mSaturation0 = (ImageButton) root.findViewById(R.id.saturation_0);
        mSaturationP1 = (ImageButton) root.findViewById(R.id.saturation_p1);
        mModeOptionsSaturation = (HorizontalRadioOptions) root.findViewById(R.id.mode_options_saturation);

        mContrastN1 = (ImageButton) root.findViewById(R.id.contrast_n1);
        mContrast0 = (ImageButton) root.findViewById(R.id.contrast_0);
        mContrastP1 = (ImageButton) root.findViewById(R.id.contrast_p1);
        mModeOptionsContrast = (HorizontalRadioOptions) root.findViewById(R.id.mode_options_contrast);

        mSharpnessN1 = (ImageButton) root.findViewById(R.id.sharpness_n1);
        mSharpness0 = (ImageButton) root.findViewById(R.id.sharpness_0);
        mSharpnessP1 = (ImageButton) root.findViewById(R.id.sharpness_p1);
        mModeOptionsSharpness = (HorizontalRadioOptions) root.findViewById(R.id.mode_options_sharpness);

        mBrightnessN1 = (ImageButton) root.findViewById(R.id.brightness_n1);
        mBrightness0 = (ImageButton) root.findViewById(R.id.brightness_0);
        mBrightnessP1 = (ImageButton) root.findViewById(R.id.brightness_p1);
        mModeOptionsBrightness = (HorizontalRadioOptions) root.findViewById(R.id.mode_options_brightness);

        mHueN1 = (ImageButton) root.findViewById(R.id.hue_n1);
        mHue0 = (ImageButton) root.findViewById(R.id.hue_0);
        mHueP1 = (ImageButton) root.findViewById(R.id.hue_p1);
        mModeOptionsHue = (HorizontalRadioOptions) root.findViewById(R.id.mode_options_hue);

        mButtonWhiteBalance = (ImageButton) root.findViewById(R.id.wb_button);
        mWbCloudy = (ImageButton) root.findViewById(R.id.wb_cloudy);
        mWbFluorescent = (ImageButton) root.findViewById(R.id.wb_fluorescent);
        mWbAuto = (ImageButton) root.findViewById(R.id.wb_auto);
        mWbIncandescent = (ImageButton) root.findViewById(R.id.wb_incandescent);
        mWbSunlight = (ImageButton) root.findViewById(R.id.wb_sunlight);
        mModeOptionsWhiteBalance = (RadioOptions) root.findViewById(R.id.mode_options_whitebalance);

        mButtonScene = (ImageButton) root.findViewById(R.id.scene_button);
        mSceneAuto = (LinearLayout) root.findViewById(R.id.scene_auto);
        mScenePortrait = (LinearLayout) root.findViewById(R.id.scene_portrait);
        mSceneLandscape = (LinearLayout) root.findViewById(R.id.scene_landscape);
        mSceneNight = (LinearLayout) root.findViewById(R.id.scene_night);
        mSceneSports = (LinearLayout) root.findViewById(R.id.scene_sports);
        mSceneBarcode = (LinearLayout) root.findViewById(R.id.scene_barcode);
        mModeOptionsScene = (AverageRadioOptions) root.findViewById(R.id.scene_options);

        mButtonColor = (ImageButton) root.findViewById(R.id.color_button);
        mColorNone = (FrameLayout) root.findViewById(R.id.color_none);
        mColorMono = (FrameLayout) root.findViewById(R.id.color_mono);
        mColorNegative = (FrameLayout) root.findViewById(R.id.color_negative);
        mColorSepia = (FrameLayout) root.findViewById(R.id.color_sepia);
        mModeOptionsColor = (AverageRadioOptions) root.findViewById(R.id.color_options);
    }

    @Override
    public void onSettingChanged(SettingsManager settingsManager, String key) {
        if (!mButtonLoaded) return;
        MultiToggleImageButton button = null;
        int index = 0;

        if (key.equals(Keys.KEY_FLASH_MODE)) {
            index = mSettingsManager.getIndexOfCurrentValue(mAppController.getCameraScope(),
                                                            Keys.KEY_FLASH_MODE);
            button = getButtonOrError(BUTTON_FLASH);
        } else if (key.equals(Keys.KEY_VIDEOCAMERA_FLASH_MODE)) {
            index = mSettingsManager.getIndexOfCurrentValue(mAppController.getCameraScope(),
                                                            Keys.KEY_VIDEOCAMERA_FLASH_MODE);
            button = getButtonOrError(BUTTON_TORCH);
        } else if (key.equals(Keys.KEY_HDR_PLUS_FLASH_MODE)) {
            index = mSettingsManager.getIndexOfCurrentValue(mAppController.getModuleScope(),
                                                            Keys.KEY_HDR_PLUS_FLASH_MODE);
            button = getButtonOrError(BUTTON_HDR_PLUS_FLASH);
        } else if (key.equals(Keys.KEY_CAMERA_ID)) {
            index = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                                                            Keys.KEY_CAMERA_ID);
            button = getButtonOrError(BUTTON_CAMERA);
        } else if (key.equals(Keys.KEY_CAMERA_HDR_PLUS)) {
            index = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                                                            Keys.KEY_CAMERA_HDR_PLUS);
            button = getButtonOrError(BUTTON_HDR_PLUS);
        } else if (key.equals(Keys.KEY_CAMERA_HDR)) {
            index = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                                                            Keys.KEY_CAMERA_HDR);
            button = getButtonOrError(BUTTON_HDR);
        } else if (key.equals(Keys.KEY_CAMERA_GRID_LINES)) {
            index = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                                                            Keys.KEY_CAMERA_GRID_LINES);
            button = getButtonOrError(BUTTON_GRID_LINES);
        } else if (key.equals(Keys.KEY_CAMERA_PANO_ORIENTATION)) {
            updatePanoButtons();
        } else if (key.equals(Keys.KEY_EXPOSURE)) {
            updateExposureButtons();
        } else if (key.equals(Keys.KEY_COUNTDOWN_DURATION)) {
            index = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                                                            Keys.KEY_COUNTDOWN_DURATION);
            button = getButtonOrError(BUTTON_COUNTDOWN);
        } else if (key.equals(Keys.KEY_WHITEBALANCE)) {
            updateWhiteBalanceButtons();
        } else if (key.equals(Keys.KEY_SCENE_MODE)) {
            updateSceneButtons();
        } else if (key.equals(Keys.KEY_COLOR_EFFECT)) {
            updateColorEffectButtons();
        } else if (key.equals(Keys.KEY_BURST_CAPTURE_ON)) {
            index = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                    Keys.KEY_BURST_CAPTURE_ON);
            button = getButtonOrError(BUTTON_ZSL);
        } else if (key.equals(Keys.KEY_SMILE_SHUTTER_ON)) {
            index = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                    Keys.KEY_SMILE_SHUTTER_ON);
            button = getButtonOrError(BUTTON_SMILE_SHUTTER);
        } else if (key.equals(Keys.KEY_SATURATION)) {
            updateSaturationButtons();
        } else if (key.equals(Keys.KEY_CONTRAST)) {
            updateContrastButtons();
        } else if (key.equals(Keys.KEY_SHARPNESS)) {
            updateSharpnessButtons();
        } else if (key.equals(Keys.KEY_BRIGHTNESS)) {
            updateBrightnessButtons();
        } else if (key.equals(Keys.KEY_HUE)) {
            updateHueButtons();
        } else if (key.equals(Keys.KEY_3DNR_ON)) {
            index = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                    Keys.KEY_3DNR_ON);
            button = getButtonOrError(BUTTON_3DNR);
        }

        if (button != null && button.getState() != index) {
            button.setState(Math.max(index, 0), false);
        }
    }

    /**
     * A callback executed in the state listener of a button.
     *
     * Used by a module to set specific behavior when a button's
     * state changes.
     */
    public interface ButtonCallback {
        public void onStateChanged(int state);
    }

    /**
     * Returns the appropriate {@link com.android.camera.MultiToggleImageButton}
     * based on button id.  An IllegalStateException will be throw if the
     * button could not be found in the view hierarchy.
     */
    private MultiToggleImageButton getButtonOrError(int buttonId) {
        switch (buttonId) {
            case BUTTON_FLASH:
                if (mButtonFlash == null) {
                    throw new IllegalStateException("Flash button could not be found.");
                }
                return mButtonFlash;
            case BUTTON_TORCH:
                if (mButtonFlash == null) {
                    throw new IllegalStateException("Torch button could not be found.");
                }
                return mButtonFlash;
            case BUTTON_HDR_PLUS_FLASH:
                if (mButtonFlash == null) {
                    throw new IllegalStateException("Hdr plus torch button could not be found.");
                }
                return mButtonFlash;
            case BUTTON_CAMERA:
                if (mButtonCamera == null) {
                    throw new IllegalStateException("Camera button could not be found.");
                }
                return mButtonCamera;
            case BUTTON_HDR_PLUS:
                if (mButtonHdr == null) {
                    throw new IllegalStateException("Hdr plus button could not be found.");
                }
                return mButtonHdr;
            case BUTTON_HDR:
                if (mButtonHdr == null) {
                    throw new IllegalStateException("Hdr button could not be found.");
                }
                return mButtonHdr;
            case BUTTON_GRID_LINES:
                if (mButtonGridlines == null) {
                    throw new IllegalStateException("Grid lines button could not be found.");
                }
                return mButtonGridlines;
            case BUTTON_COUNTDOWN:
                if (mButtonCountdown == null) {
                    throw new IllegalStateException("Countdown button could not be found.");
                }
                return mButtonCountdown;
            case BUTTON_ZSL:
                if (mButtonZsl == null) {
                    throw new IllegalStateException("Zsl button could not be found.");
                }
                return mButtonZsl;
            case BUTTON_SMILE_SHUTTER:
                if (mButtonSmileShutter == null) {
                    throw new IllegalStateException("Smile Shutter button could not be found.");
                }
                return mButtonSmileShutter;
            case BUTTON_3DNR:
                if (mButton3dnr == null) {
                    throw new IllegalAccessError("3dnr button could not be found.");
                }
                return mButton3dnr;
            default:
                throw new IllegalArgumentException("button not known by id=" + buttonId);
        }
    }

    /**
     * Returns the appropriate {@link android.widget.ImageButton}
     * based on button id.  An IllegalStateException will be throw if the
     * button could not be found in the view hierarchy.
     */
    private ImageButton getImageButtonOrError(int buttonId) {
        switch (buttonId) {
            case BUTTON_CANCEL:
                if (mButtonCancel == null) {
                    throw new IllegalStateException("Cancel button could not be found.");
                }
                return mButtonCancel;
            case BUTTON_DONE:
                if (mButtonDone == null) {
                    throw new IllegalStateException("Done button could not be found.");
                }
                return mButtonDone;
            case BUTTON_RETAKE:
                if (mButtonRetake == null) {
                    throw new IllegalStateException("Retake button could not be found.");
                }
                return mButtonRetake;
            case BUTTON_REVIEW:
                if (mButtonRetake == null) {
                    throw new IllegalStateException("Review button could not be found.");
                }
                return mButtonRetake;
            case BUTTON_EXPOSURE_COMPENSATION:
                if (mButtonExposureCompensation == null) {
                    throw new IllegalStateException("Exposure Compensation button could not be found.");
                }
                return mButtonExposureCompensation;
            case BUTTON_WHITEBALANCE:
                if (mButtonWhiteBalance == null) {
                    throw new IllegalStateException("WhiteBalance button could not be found.");
                }
                return mButtonWhiteBalance;
            case BUTTON_SCENE:
                if (mButtonScene == null) {
                    throw new IllegalStateException("Scene button could not be found.");
                }
                return mButtonScene;
            case BUTTON_COLOR:
                if (mButtonColor == null) {
                    throw new IllegalStateException("Color button could not be found.");
                }
                return mButtonColor;
            default:
                throw new IllegalArgumentException("button not known by id=" + buttonId);
        }
    }

    /**
     * Initialize a known button by id with a state change callback, and then
     * enable the button.
     *
     * @param buttonId The id if the button to be initialized.
     * @param cb The callback to be executed after the button state change.
     */
    public void initializeButton(int buttonId, ButtonCallback cb) {
        initializeButton(buttonId, cb, null);
    }

    /**
     * Initialize a known button by id, with a state change callback and a state
     * pre-change callback, and then enable the button.
     *
     * @param buttonId The id if the button to be initialized.
     * @param cb The callback to be executed after the button state change.
     * @param preCb The callback to be executed before the button state change.
     */
    public void initializeButton(int buttonId, ButtonCallback cb, ButtonCallback preCb) {
        MultiToggleImageButton button = getButtonOrError(buttonId);
        switch (buttonId) {
            case BUTTON_FLASH:
                initializeFlashButton(button, cb, preCb, R.array.camera_flashmode_icons);
                break;
            case BUTTON_TORCH:
                initializeTorchButton(button, cb, preCb, R.array.video_flashmode_icons);
                break;
            case BUTTON_HDR_PLUS_FLASH:
                initializeHdrPlusFlashButton(button, cb, preCb, R.array.camera_flashmode_icons);
                break;
            case BUTTON_CAMERA:
                initializeCameraButton(button, cb, preCb, R.array.camera_id_icons);
                break;
            case BUTTON_HDR_PLUS:
                initializeHdrPlusButton(button, cb, preCb, R.array.pref_camera_hdr_plus_icons);
                break;
            case BUTTON_HDR:
                initializeHdrButton(button, cb, preCb, R.array.pref_camera_hdr_icons);
                break;
            case BUTTON_GRID_LINES:
                initializeGridLinesButton(button, cb, preCb, R.array.grid_lines_icons);
                break;
            case BUTTON_COUNTDOWN:
                initializeCountdownButton(button, cb, preCb, R.array.countdown_duration_icons);
                break;
            case BUTTON_ZSL:
                initializeZslButton(button, cb, R.array.zsl_icons);
                break;
            case BUTTON_SMILE_SHUTTER:
                initializeSmileShutterButton(button, cb, R.array.smile_shutter_icons);
                break;
            case BUTTON_3DNR:
                initialize3dnrButton(button, cb, R.array.threednr_icons);
                break;
            default:
                throw new IllegalArgumentException("button not known by id=" + buttonId);
        }

        showButton(buttonId);
        enableButton(buttonId);
    }

    /**
     * Initialize a known button with a click listener and a drawable resource id,
     * and a content description resource id.
     * Sets the button visible.
     */
    public void initializePushButton(int buttonId, View.OnClickListener cb,
            int imageId, int contentDescriptionId) {
        ImageButton button = getImageButtonOrError(buttonId);
        if (cb != null)
            button.setOnClickListener(cb);
        if (imageId != NO_RESOURCE) {
            button.setImageResource(imageId);
        }
        if (contentDescriptionId != NO_RESOURCE) {
            button.setContentDescription(mAppController
                    .getAndroidContext().getResources().getString(contentDescriptionId));
        }

        if (!button.isEnabled()) {
            button.setEnabled(true);
            if (mListener != null) {
                mListener.onButtonEnabledChanged(this, buttonId);
            }
        }
        button.setTag(R.string.tag_enabled_id, buttonId);

        if (button.getVisibility() != View.VISIBLE) {
            button.setVisibility(View.VISIBLE);
            if (mListener != null) {
                mListener.onButtonVisibilityChanged(this, buttonId);
            }
        }

        
        switch (buttonId) {
        case BUTTON_WHITEBALANCE:
            initializeWhiteBalanceButton(button);
            break;

        default:
            break;
        }
    }

    /**
     * Initialize a known button with a click listener and a resource id.
     * Sets the button visible.
     */
    public void initializePushButton(int buttonId, View.OnClickListener cb,
            int imageId) {
        initializePushButton(buttonId, cb, imageId, NO_RESOURCE);
    }

    /**
     * Initialize a known button with a click listener. Sets the button visible.
     */
    public void initializePushButton(int buttonId, View.OnClickListener cb) {
        initializePushButton(buttonId, cb, NO_RESOURCE, NO_RESOURCE);
    }

    /**
     * Sets the camera button in its disabled (greyed out) state and blocks it
     * so no generic operation can enable it until it's explicitly re-enabled by
     * calling {@link #enableCameraButton()}.
     */
    public void disableCameraButtonAndBlock() {
        mIsCameraButtonBlocked = true;
        disableButton(BUTTON_CAMERA);
    }

    /**
     * Sets a button in its disabled (greyed out) state.
     */
    public void disableButton(int buttonId) {
        ImageButton button;
        try {
            button = getButtonOrError(buttonId);
        } catch (IllegalArgumentException e) {
            button = getImageButtonOrError(buttonId);
        }
        // HDR and HDR+ buttons share the same button object,
        // but change actual image icons at runtime.
        // This extra check is to ensure the correct icons are used
        // in the case of the HDR[+] button being disabled at startup,
        // e.g. app startup with front-facing camera.
        // b/18104680
        if (buttonId == BUTTON_HDR_PLUS) {
            initializeHdrPlusButtonIcons((MultiToggleImageButton) button, R.array.pref_camera_hdr_plus_icons);
        } else if (buttonId == BUTTON_HDR) {
            initializeHdrButtonIcons((MultiToggleImageButton) button, R.array.pref_camera_hdr_icons);
        }

        if (button.isEnabled()) {
            button.setEnabled(false);
            if (mListener != null) {
                mListener.onButtonEnabledChanged(this, buttonId);
            }
        }
        button.setTag(R.string.tag_enabled_id, null);
    }

    /**
     * Enables the camera button and removes the block that was set by
     * {@link #disableCameraButtonAndBlock()}.
     */
    public void enableCameraButton() {
        mIsCameraButtonBlocked = false;
        enableButton(BUTTON_CAMERA);
    }

    /**
     * Enables a button that has already been initialized.
     */
    public void enableButton(int buttonId) {
        // If Camera Button is blocked, ignore the request.
        if(buttonId == BUTTON_CAMERA && mIsCameraButtonBlocked) {
            return;
        }
        ImageButton button;
        // Manual exposure uses a regular image button instead of a
        // MultiToggleImageButton, so it requires special handling.
        // TODO: Redesign ButtonManager's button getter methods into one method.
        try {
            button = getButtonOrError(buttonId);
        } catch (IllegalArgumentException e) {
            button = getImageButtonOrError(buttonId);
        }
        if (!button.isEnabled()) {
            button.setEnabled(true);
            if (mListener != null) {
                mListener.onButtonEnabledChanged(this, buttonId);
            }
        }
        button.setTag(R.string.tag_enabled_id, buttonId);
    }

    /**
     * Disable click reactions for a button without affecting visual state.
     * For most cases you'll want to use {@link #disableButton(int)}.
     * @param buttonId The id of the button.
     */
    public void disableButtonClick(int buttonId) {
        ImageButton button = getButtonOrError(buttonId);
        if (button instanceof MultiToggleImageButton) {
            ((MultiToggleImageButton) button).setClickEnabled(false);
        }
    }

    /**
     * Enable click reactions for a button without affecting visual state.
     * For most cases you'll want to use {@link #enableButton(int)}.
     * @param buttonId The id of the button.
     */
    public void enableButtonClick(int buttonId) {
        ImageButton button = getButtonOrError(buttonId);
        if (button instanceof MultiToggleImageButton) {
            ((MultiToggleImageButton) button).setClickEnabled(true);
        }
    }

    /**
     * Hide a button by id.
     */
    public void hideButton(int buttonId) {
        View button;
        try {
            button = getButtonOrError(buttonId);
        } catch (IllegalArgumentException e) {
            button = getImageButtonOrError(buttonId);
        }
        if (button.getVisibility() == View.VISIBLE) {
            if (buttonId == ButtonManager.BUTTON_SCENE
                    || buttonId == ButtonManager.BUTTON_COLOR)
                button.setVisibility(View.INVISIBLE);
            else
                button.setVisibility(View.GONE);
            if (mListener != null) {
                mListener.onButtonVisibilityChanged(this, buttonId);
            }
        }
    }

    /**
     * Show a button by id.
     */
    public void showButton(int buttonId) {
        View button;
        try {
            button = getButtonOrError(buttonId);
        } catch (IllegalArgumentException e) {
            button = getImageButtonOrError(buttonId);
        }
        if (button.getVisibility() != View.VISIBLE) {
            button.setVisibility(View.VISIBLE);
            if (mListener != null) {
                mListener.onButtonVisibilityChanged(this, buttonId);
            }
        }
    }

    public void hideModeOptions(int optionId) {
        View parent = null;
        switch (optionId) {
        case R.id.mode_options_exposure_of_screen:
            if (mModeOptionsExposureOfScreen != null) {
                parent = (View) mModeOptionsExposureOfScreen.getParent();
                if (parent.getVisibility() == View.VISIBLE)
                    parent.setVisibility(View.GONE);
            }
            break;

        case R.id.mode_options_saturation:
            if (mModeOptionsSaturation != null) {
                parent = (View) mModeOptionsSaturation.getParent();
                if (parent.getVisibility() == View.VISIBLE)
                    parent.setVisibility(View.GONE);
            }
            break;

        case R.id.mode_options_contrast:
            if (mModeOptionsContrast != null) {
                parent = (View) mModeOptionsContrast.getParent();
                if (parent.getVisibility() == View.VISIBLE)
                    parent.setVisibility(View.GONE);
            }
            break;

        case R.id.mode_options_sharpness:
            if (mModeOptionsSharpness != null) {
                parent = (View) mModeOptionsSharpness.getParent();
                if (parent.getVisibility() == View.VISIBLE)
                    parent.setVisibility(View.GONE);
            }
            break;

        case R.id.mode_options_brightness:
            if (mModeOptionsBrightness != null) {
                parent = (View) mModeOptionsBrightness.getParent();
                if (parent.getVisibility() == View.VISIBLE)
                    parent.setVisibility(View.GONE);
            }
            break;

        case R.id.mode_options_hue:
            if (mModeOptionsHue != null) {
                parent = (View) mModeOptionsHue.getParent();
                if (parent.getVisibility() == View.VISIBLE)
                    parent.setVisibility(View.GONE);
            }
            break;

        default:
            break;
        }
    }

    public void showModeOptions(int optionId) {
        View parent = null;
        switch (optionId) {
        case R.id.mode_options_exposure_of_screen:
            if (mModeOptionsExposureOfScreen != null) {
                parent = (View) mModeOptionsExposureOfScreen.getParent();
                if (parent.getVisibility() != View.VISIBLE)
                    parent.setVisibility(View.VISIBLE);
            }
            break;

        case R.id.mode_options_saturation:
            if (mModeOptionsSaturation != null) {
                parent = (View) mModeOptionsSaturation.getParent();
                if (parent.getVisibility() != View.VISIBLE)
                    parent.setVisibility(View.VISIBLE);
            }
            break;

        case R.id.mode_options_contrast:
            if (mModeOptionsContrast != null) {
                parent = (View) mModeOptionsContrast.getParent();
                if (parent.getVisibility() != View.VISIBLE)
                    parent.setVisibility(View.VISIBLE);
            }
            break;

        case R.id.mode_options_sharpness:
            if (mModeOptionsSharpness != null) {
                parent = (View) mModeOptionsSharpness.getParent();
                if (parent.getVisibility() != View.VISIBLE)
                    parent.setVisibility(View.VISIBLE);
            }
            break;

        case R.id.mode_options_brightness:
            if (mModeOptionsBrightness != null) {
                parent = (View) mModeOptionsBrightness.getParent();
                if (parent.getVisibility() != View.VISIBLE)
                    parent.setVisibility(View.VISIBLE);
            }
            break;

        case R.id.mode_options_hue:
            if (mModeOptionsHue != null) {
                parent = (View) mModeOptionsHue.getParent();
                if (parent.getVisibility() != View.VISIBLE)
                    parent.setVisibility(View.VISIBLE);
            }
            break;

        default:
            break;
        }
    }

    public void setToInitialState() {
        mModeOptions.setMainBar(ModeOptions.BAR_STANDARD);
    }

    public void setExposureCompensationCallback(final CameraAppUI.BottomBarUISpec
                                        .ExposureCompensationSetCallback cb) {
        if (cb == null) {
            mModeOptionsExposure.setOnOptionClickListener(null);
        } else {
            mModeOptionsExposure
                .setOnOptionClickListener(new RadioOptions.OnOptionClickListener() {
                    @Override
                    public void onOptionClicked(View v) {
                        int comp = Integer.parseInt((String)(v.getTag()));

                        if (mExposureCompensationStep != 0.0f) {
                            int compValue =
                                Math.round(comp / mExposureCompensationStep);
                            cb.setExposure(compValue);
                        }
                    }
                });
        }
    }

    public void setWhiteBalanceCallback(final CameraAppUI.BottomBarUISpec
            .WhiteBalanceSetCallback cb) {
        if (cb == null) {
            mModeOptionsWhiteBalance.setOnOptionClickListener(null);
        } else {
            mModeOptionsWhiteBalance
            .setOnOptionClickListener(new RadioOptions.OnOptionClickListener() {
                @Override
                public void onOptionClicked(View v) {
                    String value = (String) v.getTag();
                    cb.setWhiteBalance(value);
                    if (toApiCase(CameraCapabilities.WhiteBalance.CLOUDY_DAYLIGHT.name())
                            .equals(value)) {
                        mButtonWhiteBalance.setImageResource(R.drawable.ic_whitebalance_cloudy);
                    } else if (toApiCase(CameraCapabilities.WhiteBalance.INCANDESCENT.name())
                            .equals(value)) {
                        mButtonWhiteBalance.setImageResource(R.drawable.ic_whitebalance_incandescent);
                    } else if (toApiCase(CameraCapabilities.WhiteBalance.FLUORESCENT.name())
                            .equals(value)) {
                        mButtonWhiteBalance.setImageResource(R.drawable.ic_whitebalance_fluorescent);
                    } else if (toApiCase(CameraCapabilities.WhiteBalance.DAYLIGHT.name())
                            .equals(value)) {
                        mButtonWhiteBalance.setImageResource(R.drawable.ic_whitebalance_daylight);
                    } else if (toApiCase(CameraCapabilities.WhiteBalance.AUTO.name())
                            .equals(value)) {
                        mButtonWhiteBalance.setImageResource(R.drawable.ic_whitebalance_auto);
                    }
                }
            });
        }
    }

    public void setSceneCallBack(final CameraAppUI.BottomBarUISpec.SceneSetCallback cb) {
        if (cb == null) {
            mModeOptionsScene.setOnOptionClickListener(null);
        } else {
            mModeOptionsScene.setOnOptionClickListener(new AverageRadioOptions.OnOptionClickListener() {

                @Override
                public void onOptionClicked(View v) {
                    // TODO Auto-generated method stub
                    cb.setScene((String) v.getTag());
                }
            });
        }
    }

    public void setColorEffectCallBack(final CameraAppUI.BottomBarUISpec.ColorEffectSetCallback cb) {
        if (cb == null) {
            mModeOptionsColor.setOnOptionClickListener(null);
        } else {
            mModeOptionsColor.setOnOptionClickListener(new AverageRadioOptions.OnOptionClickListener() {

                @Override
                public void onOptionClicked(View v) {
                    // TODO Auto-generated method stub
                    cb.setColorEffect((String) v.getTag());
                }
            });
        }
    }

    public void setExposureCompensationOfScreenCallback(final CameraAppUI.BottomBarUISpec
            .ExposureCompensationSetCallback cb) {
        if (cb == null) {
            mModeOptionsExposureOfScreen.setOnOptionClickListener(null);
        } else {
            mModeOptionsExposureOfScreen
            .setOnOptionClickListener(new HorizontalRadioOptions.OnOptionClickListener() {
                @Override
                public void onOptionClicked(View v) {
                    int comp = Integer.parseInt((String)(v.getTag()));

                    if (mExposureCompensationStep != 0.0f) {
                        int compValue =
                                Math.round(comp / mExposureCompensationStep);
                        cb.setExposure(compValue);
                    }
                }
            });
        }
    }

    public void setSaturationCallBack(final CameraAppUI.BottomBarUISpec
            .SaturationSetCallback cb) {
        if (cb == null) {
            mModeOptionsSaturation.setOnOptionClickListener(null);
        } else {
            mModeOptionsSaturation
            .setOnOptionClickListener(new HorizontalRadioOptions.OnOptionClickListener() {
                @Override
                public void onOptionClicked(View v) {
                    cb.setSaturation((String) v.getTag());
                }
            });
        }
    }

    public void setContrastCallBack(final CameraAppUI.BottomBarUISpec
            .ContrastSetCallback cb) {
        if (cb == null) {
            mModeOptionsContrast.setOnOptionClickListener(null);
        } else {
            mModeOptionsContrast
            .setOnOptionClickListener(new HorizontalRadioOptions.OnOptionClickListener() {
                @Override
                public void onOptionClicked(View v) {
                    cb.setContrast((String) v.getTag());
                }
            });
        }
    }

    public void setSharpnessCallBack(final CameraAppUI.BottomBarUISpec
            .SharpnessSetCallback cb) {
        if (cb == null) {
            mModeOptionsSharpness.setOnOptionClickListener(null);
        } else {
            mModeOptionsSharpness
            .setOnOptionClickListener(new HorizontalRadioOptions.OnOptionClickListener() {
                @Override
                public void onOptionClicked(View v) {
                    cb.setSharpness((String) v.getTag());
                }
            });
        }
    }

    public void setBrightnessCallBack(final CameraAppUI.BottomBarUISpec
            .BrightnessSetCallback cb) {
        if (cb == null) {
            mModeOptionsBrightness.setOnOptionClickListener(null);
        } else {
            mModeOptionsBrightness
            .setOnOptionClickListener(new HorizontalRadioOptions.OnOptionClickListener() {
                @Override
                public void onOptionClicked(View v) {
                    cb.setBrightness((String) v.getTag());
                }
            });
        }
    }

    public void setHueCallBack(final CameraAppUI.BottomBarUISpec
            .HueSetCallback cb) {
        if (cb == null) {
            mModeOptionsHue.setOnOptionClickListener(null);
        } else {
            mModeOptionsHue
            .setOnOptionClickListener(new HorizontalRadioOptions.OnOptionClickListener() {
                @Override
                public void onOptionClicked(View v) {
                    cb.setHue((String) v.getTag());
                }
            });
        }
    }

    /**
     * Set the exposure compensation parameters supported by the current camera mode.
     * @param min Minimum exposure compensation value.
     * @param max Maximum exposure compensation value.
     * @param step Expsoure compensation step value.
     */
    public void setExposureCompensationParameters(int min, int max, float step) {
        mMaxExposureCompensation = max;
        mMinExposureCompensation = min;
        mExposureCompensationStep = step;


        setVisible(mExposureN2, (Math.round(min * step) <= -2));
        setVisible(mExposureN1, (Math.round(min * step) <= -1));
        setVisible(mExposureP1, (Math.round(max * step) >= 1));
        setVisible(mExposureP2, (Math.round(max * step) >= 2));

        updateExposureButtons();
    }

    public void setWhiteBalanceParameters(Set<CameraCapabilities.WhiteBalance> sets) {
        setVisible(mWbCloudy, sets.contains(CameraCapabilities.WhiteBalance.CLOUDY_DAYLIGHT));
        setVisible(mWbSunlight, sets.contains(CameraCapabilities.WhiteBalance.DAYLIGHT));
        setVisible(mWbAuto, sets.contains(CameraCapabilities.WhiteBalance.AUTO));
        setVisible(mWbFluorescent, sets.contains(CameraCapabilities.WhiteBalance.FLUORESCENT));
        setVisible(mWbIncandescent, sets.contains(CameraCapabilities.WhiteBalance.INCANDESCENT));

        updateWhiteBalanceButtons();
    }

    public void setSceneParameters(Set<CameraCapabilities.SceneMode> sets) {
        setVisible(mSceneAuto, sets.contains(CameraCapabilities.SceneMode.AUTO));
        setVisible(mScenePortrait, sets.contains(CameraCapabilities.SceneMode.PORTRAIT));
        setVisible(mSceneLandscape, sets.contains(CameraCapabilities.SceneMode.LANDSCAPE));
        setVisible(mSceneNight, sets.contains(CameraCapabilities.SceneMode.NIGHT));
        setVisible(mSceneSports, sets.contains(CameraCapabilities.SceneMode.SPORTS));
        setVisible(mSceneBarcode, sets.contains(CameraCapabilities.SceneMode.BARCODE));

        updateSceneButtons();
    }

    public void setColorEffectParameters(List<String> lists) {
        setVisible(mColorNone, lists.contains(android.hardware.Camera.Parameters.EFFECT_NONE));
        setVisible(mColorMono, lists.contains(android.hardware.Camera.Parameters.EFFECT_MONO));
        setVisible(mColorNegative, lists.contains(android.hardware.Camera.Parameters.EFFECT_NEGATIVE));
        setVisible(mColorSepia, lists.contains(android.hardware.Camera.Parameters.EFFECT_SEPIA));

        updateColorEffectButtons();
    }

    /**
     * Set the exposure compensation parameters supported by the current camera mode.
     * @param min Minimum exposure compensation value.
     * @param max Maximum exposure compensation value.
     * @param step Expsoure compensation step value.
     */
    public void setExposureCompensationOfScreenParameters(int min, int max, float step) {
        mMaxExposureCompensation = max;
        mMinExposureCompensation = min;
        mExposureCompensationStep = step;

        setVisible(mExposureN2OfScreen, (Math.round(min * step) <= -2));
        setVisible(mExposureN1OfScreen, (Math.round(min * step) <= -1));
        setVisible(mExposureP1OfScreen, (Math.round(max * step) >= 1));
        setVisible(mExposureP2OfScreen, (Math.round(max * step) >= 2));

        updateExposureButtonsOfScreen();
    }

    public void setSaturationParameters(List<String> lists) {
        setVisible(mSaturationN1, lists.contains("low"));
        setVisible(mSaturationP1, lists.contains("high"));

        updateSaturationButtons();
    }

    public void setContrastParameters(List<String> lists) {
        setVisible(mContrastN1, lists.contains("soft"));
        setVisible(mContrastP1, lists.contains("hard"));

        updateContrastButtons();
    }

    public void setSharpnessParameters(List<String> lists) {
        setVisible(mSharpnessN1, lists.contains("low"));
        setVisible(mSharpnessP1, lists.contains("high"));

        updateSharpnessButtons();
    }

    public void setBrightnessParameters(List<String> lists) {
        setVisible(mBrightnessN1, lists.contains("low"));
        setVisible(mBrightnessP1, lists.contains("high"));

        updateBrightnessButtons();
    }

    public void setHueParameters(List<String> lists) {
        setVisible(mHueN1, lists.contains("low"));
        setVisible(mHueP1, lists.contains("high"));

        updateHueButtons();
    }

    private static void setVisible(View v, boolean visible) {
        if (visible) {
            v.setVisibility(View.VISIBLE);
        } else {
            v.setVisibility(View.INVISIBLE);
        }
    }

    /**
     * @return The exposure compensation step value.
     **/
    public float getExposureCompensationStep() {
        return mExposureCompensationStep;
    }

    /**
     * Check if a button is enabled with the given button id..
     */
    public boolean isEnabled(int buttonId) {
        View button;
        try {
            button = getButtonOrError(buttonId);
        } catch (IllegalArgumentException e) {
            button = getImageButtonOrError(buttonId);
        }

        Integer enabledId = (Integer) button.getTag(R.string.tag_enabled_id);
        if (enabledId != null) {
            return (enabledId.intValue() == buttonId) && button.isEnabled();
        } else {
            return false;
        }
    }

    /**
     * Check if a button is visible.
     */
    public boolean isVisible(int buttonId) {
        View button;
        try {
            button = getButtonOrError(buttonId);
        } catch (IllegalArgumentException e) {
            button = getImageButtonOrError(buttonId);
        }
        return (button.getVisibility() == View.VISIBLE);
    }

    /**
     * Initialize a flash button.
     */
    private void initializeFlashButton(MultiToggleImageButton button,
            final ButtonCallback cb, final ButtonCallback preCb, int resIdImages) {

        if (resIdImages > 0) {
            button.overrideImageIds(resIdImages);
        }
        button.overrideContentDescriptions(R.array.camera_flash_descriptions);

        int index = mSettingsManager.getIndexOfCurrentValue(mAppController.getCameraScope(),
                                                            Keys.KEY_FLASH_MODE);
        button.setState(index >= 0 ? index : 0, false);

        setPreChangeCallback(button, preCb);

        button.setOnStateChangeListener(new MultiToggleImageButton.OnStateChangeListener() {
            @Override
            public void stateChanged(View view, int state) {
                mSettingsManager.setValueByIndex(mAppController.getCameraScope(),
                                                 Keys.KEY_FLASH_MODE, state);
                if (cb != null) {
                    cb.onStateChanged(state);
                }
            }
        });
    }

    /**
     * Initialize video torch button
     */
    private void initializeTorchButton(MultiToggleImageButton button,
            final ButtonCallback cb, final ButtonCallback preCb, int resIdImages) {

        if (resIdImages > 0) {
            button.overrideImageIds(resIdImages);
        }
        button.overrideContentDescriptions(R.array.video_flash_descriptions);

        int index = mSettingsManager.getIndexOfCurrentValue(mAppController.getCameraScope(),
                                                            Keys.KEY_VIDEOCAMERA_FLASH_MODE);
        button.setState(index >= 0 ? index : 0, false);

        setPreChangeCallback(button, preCb);

        button.setOnStateChangeListener(new MultiToggleImageButton.OnStateChangeListener() {
            @Override
            public void stateChanged(View view, int state) {
                mSettingsManager.setValueByIndex(mAppController.getCameraScope(),
                                                 Keys.KEY_VIDEOCAMERA_FLASH_MODE, state);
                if (cb != null) {
                    cb.onStateChanged(state);
                }
            }
        });
    }

    /**
     * Initialize hdr plus flash button
     */
    private void initializeHdrPlusFlashButton(MultiToggleImageButton button,
            final ButtonCallback cb, final ButtonCallback preCb, int resIdImages) {

        if (resIdImages > 0) {
            button.overrideImageIds(resIdImages);
        }
        button.overrideContentDescriptions(R.array.hdr_plus_flash_descriptions);

        int index = mSettingsManager.getIndexOfCurrentValue(mAppController.getModuleScope(),
                                                            Keys.KEY_HDR_PLUS_FLASH_MODE);
        button.setState(index >= 0 ? index : 0, false);

        setPreChangeCallback(button, preCb);

        button.setOnStateChangeListener(new MultiToggleImageButton.OnStateChangeListener() {
            @Override
            public void stateChanged(View view, int state) {
                mSettingsManager.setValueByIndex(mAppController.getModuleScope(),
                                                 Keys.KEY_HDR_PLUS_FLASH_MODE, state);
                if (cb != null) {
                    cb.onStateChanged(state);
                }
            }
        });
    }

    /**
     * Initialize a camera button.
     */
    private void initializeCameraButton(final MultiToggleImageButton button,
            final ButtonCallback cb, final ButtonCallback preCb, int resIdImages) {

        if (resIdImages > 0) {
            button.overrideImageIds(resIdImages);
        }

        int index = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                                                            Keys.KEY_CAMERA_ID);
        button.setState(index >= 0 ? index : 0, false);

        setPreChangeCallback(button, preCb);

        button.setOnStateChangeListener(new MultiToggleImageButton.OnStateChangeListener() {
            @Override
            public void stateChanged(View view, int state) {
                mSettingsManager.setValueByIndex(SettingsManager.SCOPE_GLOBAL,
                                                 Keys.KEY_CAMERA_ID, state);
                int cameraId = mSettingsManager.getInteger(SettingsManager.SCOPE_GLOBAL,
                                                           Keys.KEY_CAMERA_ID);
                // This is a quick fix for ISE in Gcam module which can be
                // found by rapid pressing camera switch button. The assumption
                // here is that each time this button is clicked, the listener
                // will do something and then enable this button again.
                button.setEnabled(false);
                if (cb != null) {
                    cb.onStateChanged(cameraId);
                }
                mAppController.getCameraAppUI().onChangeCamera();
            }
        });
    }

    /**
     * Initialize an hdr plus button.
     */
    private void initializeHdrPlusButton(MultiToggleImageButton button,
            final ButtonCallback cb, final ButtonCallback preCb, int resIdImages) {

        initializeHdrPlusButtonIcons(button, resIdImages);

        int index = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                                                            Keys.KEY_CAMERA_HDR_PLUS);
        button.setState(index >= 0 ? index : 0, false);

        setPreChangeCallback(button, preCb);

        button.setOnStateChangeListener(new MultiToggleImageButton.OnStateChangeListener() {
            @Override
            public void stateChanged(View view, int state) {
                mSettingsManager.setValueByIndex(SettingsManager.SCOPE_GLOBAL,
                                                 Keys.KEY_CAMERA_HDR_PLUS, state);
                if (cb != null) {
                    cb.onStateChanged(state);
                }
            }
        });
    }

    private void initializeHdrPlusButtonIcons(MultiToggleImageButton button, int resIdImages) {
        if (resIdImages > 0) {
            button.overrideImageIds(resIdImages);
        }
        button.overrideContentDescriptions(R.array.hdr_plus_descriptions);
    }

    /**
     * Initialize an hdr button.
     */
    private void initializeHdrButton(MultiToggleImageButton button,
            final ButtonCallback cb, final ButtonCallback preCb, int resIdImages) {

        initializeHdrButtonIcons(button, resIdImages);

        int index = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                                                            Keys.KEY_CAMERA_HDR);
        button.setState(index >= 0 ? index : 0, false);

        setPreChangeCallback(button, preCb);

        button.setOnStateChangeListener(new MultiToggleImageButton.OnStateChangeListener() {
            @Override
            public void stateChanged(View view, int state) {
                mSettingsManager.setValueByIndex(SettingsManager.SCOPE_GLOBAL,
                                                 Keys.KEY_CAMERA_HDR, state);
                if (cb != null) {
                    cb.onStateChanged(state);
                }
            }
        });
    }

    private void initializeHdrButtonIcons(MultiToggleImageButton button, int resIdImages) {
        if (resIdImages > 0) {
            button.overrideImageIds(resIdImages);
        }
        button.overrideContentDescriptions(R.array.hdr_descriptions);
    }

    /**
     * Initialize a countdown timer button.
     */
    private void initializeCountdownButton(MultiToggleImageButton button,
            final ButtonCallback cb, final ButtonCallback preCb, int resIdImages) {
        if (resIdImages > 0) {
            button.overrideImageIds(resIdImages);
        }

        int index = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                                                            Keys.KEY_COUNTDOWN_DURATION);
        button.setState(index >= 0 ? index : 0, false);

        setPreChangeCallback(button, preCb);

        button.setOnStateChangeListener(new MultiToggleImageButton.OnStateChangeListener() {
            @Override
            public void stateChanged(View view, int state) {
                mSettingsManager.setValueByIndex(SettingsManager.SCOPE_GLOBAL,
                                                 Keys.KEY_COUNTDOWN_DURATION, state);
                if(cb != null) {
                    cb.onStateChanged(state);
                }
            }
        });
    }

    private void initializeWhiteBalanceButton(ImageButton button) {
        String value = mSettingsManager.getString(
                mAppController.getCameraScope(), Keys.KEY_WHITEBALANCE);
        if (toApiCase(CameraCapabilities.WhiteBalance.CLOUDY_DAYLIGHT.name())
                .equals(value)) {
            button.setImageResource(R.drawable.ic_whitebalance_cloudy);
        } else if (toApiCase(CameraCapabilities.WhiteBalance.INCANDESCENT.name())
                .equals(value)) {
            button.setImageResource(R.drawable.ic_whitebalance_incandescent);
        } else if (toApiCase(CameraCapabilities.WhiteBalance.FLUORESCENT.name())
                .equals(value)) {
            button.setImageResource(R.drawable.ic_whitebalance_fluorescent);
        } else if (toApiCase(CameraCapabilities.WhiteBalance.DAYLIGHT.name())
                .equals(value)) {
            button.setImageResource(R.drawable.ic_whitebalance_daylight);
        } else if (toApiCase(CameraCapabilities.WhiteBalance.AUTO.name())
                .equals(value)) {
            button.setImageResource(R.drawable.ic_whitebalance_auto);
        }
    }

    /**
     * Update the visual state of the manual exposure buttons
     */
    public void updateExposureButtons() {
        int compValue = mSettingsManager.getInteger(mAppController.getCameraScope(),
                                                    Keys.KEY_EXPOSURE);
        if (mExposureCompensationStep != 0.0f) {
            int comp = Math.round(compValue * mExposureCompensationStep);
            mModeOptionsExposure.setSelectedOptionByTag(String.valueOf(comp));
        }
    }

    public void updateWhiteBalanceButtons() {
        String value = mSettingsManager.getString(mAppController.getCameraScope(),
                Keys.KEY_WHITEBALANCE);
        mModeOptionsWhiteBalance.setSelectedOptionByTag(value);
    }

    public void updateSceneButtons() {
        String value = mSettingsManager.getString(mAppController.getCameraScope(),
                Keys.KEY_SCENE_MODE);
        mModeOptionsScene.setSelectedOptionByTag(value);
    }

    public void updateColorEffectButtons() {
        String value = mSettingsManager.getString(mAppController.getCameraScope(),
                Keys.KEY_COLOR_EFFECT);
        mModeOptionsColor.setSelectedOptionByTag(value);
    }

    /**
     * Update the visual state of the manual exposure buttons
     */
    public void updateExposureButtonsOfScreen() {
        int compValue = mSettingsManager.getInteger(mAppController.getCameraScope(),
                                                    Keys.KEY_EXPOSURE);
        if (mExposureCompensationStep != 0.0f) {
            int comp = Math.round(compValue * mExposureCompensationStep);
            mModeOptionsExposureOfScreen.setSelectedOptionByTag(String.valueOf(comp));
        }
    }

    public void updateSaturationButtons() {
        String value = mSettingsManager.getString(mAppController.getCameraScope(),
                Keys.KEY_SATURATION);
        mModeOptionsSaturation.setSelectedOptionByTag(value);
    }

    public void updateContrastButtons() {
        String value = mSettingsManager.getString(mAppController.getCameraScope(),
                Keys.KEY_CONTRAST);
        mModeOptionsContrast.setSelectedOptionByTag(value);
    }

    public void updateSharpnessButtons() {
        String value = mSettingsManager.getString(mAppController.getCameraScope(),
                Keys.KEY_SHARPNESS);
        mModeOptionsSharpness.setSelectedOptionByTag(value);
    }

    public void updateBrightnessButtons() {
        String value = mSettingsManager.getString(mAppController.getCameraScope(),
                Keys.KEY_BRIGHTNESS);
        mModeOptionsBrightness.setSelectedOptionByTag(value);
    }

    public void updateHueButtons() {
        String value = mSettingsManager.getString(mAppController.getCameraScope(),
                Keys.KEY_HUE);
        mModeOptionsHue.setSelectedOptionByTag(value);
    }

    public void resetModeOptionsButtonScroll() {
        if (mModeOptionsButtons != null)
            mModeOptionsButtons.scrollTo(0, 0);
    }

    /**
     * Initialize a grid lines button.
     */
    private void initializeGridLinesButton(MultiToggleImageButton button,
            final ButtonCallback cb, final ButtonCallback preCb, int resIdImages) {

        if (resIdImages > 0) {
            button.overrideImageIds(resIdImages);
        }
        button.overrideContentDescriptions(R.array.grid_lines_descriptions);

        setPreChangeCallback(button, preCb);

        button.setOnStateChangeListener(new MultiToggleImageButton.OnStateChangeListener() {
            @Override
            public void stateChanged(View view, int state) {
                mSettingsManager.setValueByIndex(SettingsManager.SCOPE_GLOBAL,
                                                 Keys.KEY_CAMERA_GRID_LINES, state);
                if (cb != null) {
                    cb.onStateChanged(state);
                }
            }
        });

        int index = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                                                            Keys.KEY_CAMERA_GRID_LINES);
        button.setState(index >= 0 ? index : 0, true);
    }
    
    /**
     * Initialize a zsl button.
     */
    private void initializeZslButton(MultiToggleImageButton button,
            final ButtonCallback cb, int resIdImages) {

        if (resIdImages > 0) {
            button.overrideImageIds(resIdImages);
        }
        button.overrideContentDescriptions(R.array.zsl_descriptions);

        button.setOnStateChangeListener(new MultiToggleImageButton.OnStateChangeListener() {
            @Override
            public void stateChanged(View view, int state) {
                mSettingsManager.setValueByIndex(SettingsManager.SCOPE_GLOBAL,
                                                 Keys.KEY_BURST_CAPTURE_ON, state);
                if (cb != null) {
                    cb.onStateChanged(state);
                }
            }
        });

        int index = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                                                            Keys.KEY_BURST_CAPTURE_ON);
        button.setState(index >= 0 ? index : 0, true);
    }

    public boolean isZslButtonChanging() {
        MultiToggleImageButton zslButton = getButtonOrError(BUTTON_ZSL);
        return zslButton.isStateChanging();
    }

    public void setZslButtonChangeOver() {
        MultiToggleImageButton zslButton = getButtonOrError(BUTTON_ZSL);
        zslButton.setStateChangeOver(true);
    }

    /**
     * Initialize a smile shutter button.
     */
    private void initializeSmileShutterButton(MultiToggleImageButton button,
            final ButtonCallback cb, int resIdImages) {

        if (resIdImages > 0) {
            button.overrideImageIds(resIdImages);
        }
        button.overrideContentDescriptions(R.array.smile_shutter_descriptions);

        button.setOnStateChangeListener(new MultiToggleImageButton.OnStateChangeListener() {
            @Override
            public void stateChanged(View view, int state) {
                mSettingsManager.setValueByIndex(SettingsManager.SCOPE_GLOBAL,
                                                 Keys.KEY_SMILE_SHUTTER_ON, state);
                if (cb != null) {
                    cb.onStateChanged(state);
                }
            }
        });

        int index = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                                                            Keys.KEY_SMILE_SHUTTER_ON);
        button.setState(index >= 0 ? index : 0, true);
    }

    private void initialize3dnrButton(MultiToggleImageButton button,
            final ButtonCallback cb, int resIdImages) {

        if (resIdImages > 0) {
            button.overrideImageIds(resIdImages);
        }
        button.overrideContentDescriptions(R.array.threednr_descriptions);

        button.setOnStateChangeListener(new MultiToggleImageButton.OnStateChangeListener() {
            @Override
            public void stateChanged(View view, int state) {
                mSettingsManager.setValueByIndex(SettingsManager.SCOPE_GLOBAL,
                                                 Keys.KEY_3DNR_ON, state);
                if (cb != null) {
                    cb.onStateChanged(state);
                }
            }
        });

        int index = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                                                            Keys.KEY_3DNR_ON);
        button.setState(index >= 0 ? index : 0, true);
    }

    public boolean isPanoEnabled() {
        return mModeOptions.getMainBar() == ModeOptions.BAR_PANO;
    }

   /**
     * Initialize a panorama orientation buttons.
     */
    public void initializePanoOrientationButtons(final ButtonCallback cb) {
        int resIdImages = PhotoSphereHelper.getPanoramaOrientationOptionArrayId();
        int resIdDescriptions = PhotoSphereHelper.getPanoramaOrientationDescriptions();
        if (resIdImages > 0) {
            TypedArray imageIds = null;
            TypedArray descriptionIds = null;
            try {
                mModeOptions.setMainBar(ModeOptions.BAR_PANO);
                imageIds = mAppController
                    .getAndroidContext().getResources().obtainTypedArray(resIdImages);
                descriptionIds = mAppController
                    .getAndroidContext().getResources().obtainTypedArray(resIdDescriptions);
                mModeOptionsPano.removeAllViews();
                final boolean isHorizontal =
                    (mModeOptionsPano.getOrientation() == LinearLayout.HORIZONTAL);
                final int numImageIds = imageIds.length();
                for (int index = 0; index < numImageIds; index++) {
                    int i;
                    // if in portrait orientation (pano bar horizonal), order buttons normally
                    // if in landscape orientation (pano bar vertical), reverse button order
                    if (isHorizontal) {
                        i = index;
                    } else {
                        i = numImageIds - index - 1;
                    }

                    int imageId = imageIds.getResourceId(i, 0);
                    if (imageId > 0) {
                        ImageButton imageButton = (ImageButton) LayoutInflater
                            .from(mAppController.getAndroidContext())
                            .inflate(R.layout.mode_options_imagebutton_template,
                                     mModeOptionsPano, false);
                        imageButton.setImageResource(imageId);
                        imageButton.setTag(String.valueOf(i));
                        mModeOptionsPano.addView(imageButton);

                        int descriptionId = descriptionIds.getResourceId(i, 0);
                        if (descriptionId > 0) {
                            imageButton.setContentDescription(
                                    mAppController.getAndroidContext().getString(descriptionId));
                        }
                    }
                }
                mModeOptionsPano.updateListeners();
                mModeOptionsPano
                    .setOnOptionClickListener(new RadioOptions.OnOptionClickListener() {
                        @Override
                        public void onOptionClicked(View v) {
                            if (cb != null) {
                                int state = Integer.parseInt((String)v.getTag());
                                mSettingsManager.setValueByIndex(SettingsManager.SCOPE_GLOBAL,
                                                                 Keys.KEY_CAMERA_PANO_ORIENTATION,
                                                                 state);
                                cb.onStateChanged(state);
                            }
                        }
                    });
                updatePanoButtons();
            } finally {
                if (imageIds != null) {
                    imageIds.recycle();
                }
                if (descriptionIds != null) {
                    descriptionIds.recycle();
                }
            }
        }
    }

    private void updatePanoButtons() {
        int modeIndex = mSettingsManager.getIndexOfCurrentValue(SettingsManager.SCOPE_GLOBAL,
                                                                Keys.KEY_CAMERA_PANO_ORIENTATION);
        mModeOptionsPano.setSelectedOptionByTag(String.valueOf(modeIndex));
    }

    private void setPreChangeCallback(MultiToggleImageButton button, final ButtonCallback preCb) {
        button.setOnPreChangeListener(new MultiToggleImageButton.OnStateChangeListener() {
            @Override
            public void stateChanged(View view, int state) {
                if(preCb != null) {
                    preCb.onStateChanged(state);
                }
            }
        });
    }
}
