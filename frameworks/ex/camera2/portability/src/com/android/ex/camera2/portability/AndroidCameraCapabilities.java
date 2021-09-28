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

package com.android.ex.camera2.portability;

import android.hardware.Camera;
import android.text.TextUtils;

import com.android.ex.camera2.portability.debug.Log;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * The subclass of {@link CameraCapabilities} for Android Camera 1 API.
 */
class AndroidCameraCapabilities extends CameraCapabilities {

    private static Log.Tag TAG = new Log.Tag("AndCamCapabs");

    /** Conversion from ratios to percentages. */
    public static final float ZOOM_MULTIPLIER = 100f;

    private FpsComparator mFpsComparator = new FpsComparator();
    private SizeComparator mSizeComparator = new SizeComparator();

    AndroidCameraCapabilities(Camera.Parameters p) {
        super(new Stringifier());
        mMaxExposureCompensation = p.getMaxExposureCompensation();
        mMinExposureCompensation = p.getMinExposureCompensation();
        mExposureCompensationStep = p.getExposureCompensationStep();
        mMaxNumOfFacesSupported = p.getMaxNumDetectedFaces();
        mMaxNumOfMeteringArea = p.getMaxNumMeteringAreas();
        mPreferredPreviewSizeForVideo = new Size(p.getPreferredPreviewSizeForVideo());
        mSupportedPreviewFormats.addAll(p.getSupportedPreviewFormats());
        mSupportedPhotoFormats.addAll(p.getSupportedPictureFormats());
        mHorizontalViewAngle = p.getHorizontalViewAngle();
        mVerticalViewAngle = p.getVerticalViewAngle();
        buildPreviewFpsRange(p);
        buildPreviewSizes(p);
        buildVideoSizes(p);
        buildPictureSizes(p);
        buildSceneModes(p);
        buildFlashModes(p);
        buildFocusModes(p);
        buildWhiteBalances(p);

        buildColorEffects(p);
        buildSaturations(p);
        buildContrasts(p);
        buildSharpnesses(p);
        buildBrightnesses(p);
        buildHues(p);
        buildAntiBandings(p);

        if (p.isZoomSupported()) {
            mMaxZoomRatio = p.getZoomRatios().get(p.getMaxZoom()) / ZOOM_MULTIPLIER;
            mSupportedFeatures.add(Feature.ZOOM);
        }
        if (p.isVideoSnapshotSupported()) {
            mSupportedFeatures.add(Feature.VIDEO_SNAPSHOT);
        }
        if (p.isAutoExposureLockSupported()) {
            mSupportedFeatures.add(Feature.AUTO_EXPOSURE_LOCK);
        }
        if (p.isAutoWhiteBalanceLockSupported()) {
            mSupportedFeatures.add(Feature.AUTO_WHITE_BALANCE_LOCK);
        }
        if (supports(FocusMode.AUTO)) {
            mMaxNumOfFocusAreas = p.getMaxNumFocusAreas();
            if (mMaxNumOfFocusAreas > 0) {
                mSupportedFeatures.add(Feature.FOCUS_AREA);
            }
        }
        if (mMaxNumOfMeteringArea > 0) {
            mSupportedFeatures.add(Feature.METERING_AREA);
        }
    }

    AndroidCameraCapabilities(AndroidCameraCapabilities src) {
        super(src);
    }

    private void buildPreviewFpsRange(Camera.Parameters p) {
        List<int[]> supportedPreviewFpsRange = p.getSupportedPreviewFpsRange();
        if (supportedPreviewFpsRange != null) {
            mSupportedPreviewFpsRange.addAll(supportedPreviewFpsRange);
        }
        Collections.sort(mSupportedPreviewFpsRange, mFpsComparator);
    }

    private void buildPreviewSizes(Camera.Parameters p) {
        List<Camera.Size> supportedPreviewSizes = p.getSupportedPreviewSizes();
        if (supportedPreviewSizes != null) {
            for (Camera.Size s : supportedPreviewSizes) {
                mSupportedPreviewSizes.add(new Size(s.width, s.height));
            }
        }
        Collections.sort(mSupportedPreviewSizes, mSizeComparator);
    }

    private void buildVideoSizes(Camera.Parameters p) {
        List<Camera.Size> supportedVideoSizes = p.getSupportedVideoSizes();
        if (supportedVideoSizes != null) {
            for (Camera.Size s : supportedVideoSizes) {
                mSupportedVideoSizes.add(new Size(s.width, s.height));
            }
        }
        Collections.sort(mSupportedVideoSizes, mSizeComparator);
    }

    private void buildPictureSizes(Camera.Parameters p) {
        List<Camera.Size> supportedPictureSizes = p.getSupportedPictureSizes();
        if (supportedPictureSizes != null) {
            for (Camera.Size s : supportedPictureSizes) {
                mSupportedPhotoSizes.add(new Size(s.width, s.height));
            }
        }
        Collections.sort(mSupportedPhotoSizes, mSizeComparator);

    }

    private void buildSceneModes(Camera.Parameters p) {
        List<String> supportedSceneModes = p.getSupportedSceneModes();
        if (supportedSceneModes != null) {
            for (String scene : supportedSceneModes) {
                if (Camera.Parameters.SCENE_MODE_AUTO.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.AUTO);
                } else if (Camera.Parameters.SCENE_MODE_ACTION.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.ACTION);
                } else if (Camera.Parameters.SCENE_MODE_BARCODE.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.BARCODE);
                } else if (Camera.Parameters.SCENE_MODE_BEACH.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.BEACH);
                } else if (Camera.Parameters.SCENE_MODE_CANDLELIGHT.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.CANDLELIGHT);
                } else if (Camera.Parameters.SCENE_MODE_FIREWORKS.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.FIREWORKS);
                } else if (Camera.Parameters.SCENE_MODE_HDR.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.HDR);
                } else if (Camera.Parameters.SCENE_MODE_LANDSCAPE.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.LANDSCAPE);
                } else if (Camera.Parameters.SCENE_MODE_NIGHT.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.NIGHT);
                } else if (Camera.Parameters.SCENE_MODE_NIGHT_PORTRAIT.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.NIGHT_PORTRAIT);
                } else if (Camera.Parameters.SCENE_MODE_PARTY.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.PARTY);
                } else if (Camera.Parameters.SCENE_MODE_PORTRAIT.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.PORTRAIT);
                } else if (Camera.Parameters.SCENE_MODE_SNOW.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.SNOW);
                } else if (Camera.Parameters.SCENE_MODE_SPORTS.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.SPORTS);
                } else if (Camera.Parameters.SCENE_MODE_STEADYPHOTO.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.STEADYPHOTO);
                } else if (Camera.Parameters.SCENE_MODE_SUNSET.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.SUNSET);
                } else if (Camera.Parameters.SCENE_MODE_THEATRE.equals(scene)) {
                    mSupportedSceneModes.add(SceneMode.THEATRE);
                }
            }
        }
    }

    private void buildFlashModes(Camera.Parameters p) {
        List<String> supportedFlashModes = p.getSupportedFlashModes();
        if (supportedFlashModes == null) {
            // Camera 1 will return NULL if no flash mode is supported.
            mSupportedFlashModes.add(FlashMode.NO_FLASH);
        } else {
            for (String flash : supportedFlashModes) {
                if (Camera.Parameters.FLASH_MODE_AUTO.equals(flash)) {
                    mSupportedFlashModes.add(FlashMode.AUTO);
                } else if (Camera.Parameters.FLASH_MODE_OFF.equals(flash)) {
                    mSupportedFlashModes.add(FlashMode.OFF);
                } else if (Camera.Parameters.FLASH_MODE_ON.equals(flash)) {
                    mSupportedFlashModes.add(FlashMode.ON);
                } else if (Camera.Parameters.FLASH_MODE_RED_EYE.equals(flash)) {
                    mSupportedFlashModes.add(FlashMode.RED_EYE);
                } else if (Camera.Parameters.FLASH_MODE_TORCH.equals(flash)) {
                    mSupportedFlashModes.add(FlashMode.TORCH);
                }
            }
        }
    }

    private void buildFocusModes(Camera.Parameters p) {
        List<String> supportedFocusModes = p.getSupportedFocusModes();
        if (supportedFocusModes != null) {
            for (String focus : supportedFocusModes) {
                if (Camera.Parameters.FOCUS_MODE_AUTO.equals(focus)) {
                    mSupportedFocusModes.add(FocusMode.AUTO);
                } else if (Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE.equals(focus)) {
                    mSupportedFocusModes.add(FocusMode.CONTINUOUS_PICTURE);
                } else if (Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO.equals(focus)) {
                    mSupportedFocusModes.add(FocusMode.CONTINUOUS_VIDEO);
                } else if (Camera.Parameters.FOCUS_MODE_EDOF.equals(focus)) {
                    mSupportedFocusModes.add(FocusMode.EXTENDED_DOF);
                } else if (Camera.Parameters.FOCUS_MODE_FIXED.equals(focus)) {
                    mSupportedFocusModes.add(FocusMode.FIXED);
                } else if (Camera.Parameters.FOCUS_MODE_INFINITY.equals(focus)) {
                    mSupportedFocusModes.add(FocusMode.INFINITY);
                } else if (Camera.Parameters.FOCUS_MODE_MACRO.equals(focus)) {
                    mSupportedFocusModes.add(FocusMode.MACRO);
                }
            }
        }
    }

    private void buildWhiteBalances(Camera.Parameters p) {
        List<String> supportedWhiteBalances = p.getSupportedWhiteBalance();
        if (supportedWhiteBalances != null) {
            for (String wb : supportedWhiteBalances) {
                if (Camera.Parameters.WHITE_BALANCE_AUTO.equals(wb)) {
                    mSupportedWhiteBalances.add(WhiteBalance.AUTO);
                } else if (Camera.Parameters.WHITE_BALANCE_CLOUDY_DAYLIGHT.equals(wb)) {
                    mSupportedWhiteBalances.add(WhiteBalance.CLOUDY_DAYLIGHT);
                } else if (Camera.Parameters.WHITE_BALANCE_DAYLIGHT.equals(wb)) {
                    mSupportedWhiteBalances.add(WhiteBalance.DAYLIGHT);
                } else if (Camera.Parameters.WHITE_BALANCE_FLUORESCENT.equals(wb)) {
                    mSupportedWhiteBalances.add(WhiteBalance.FLUORESCENT);
                } else if (Camera.Parameters.WHITE_BALANCE_INCANDESCENT.equals(wb)) {
                    mSupportedWhiteBalances.add(WhiteBalance.INCANDESCENT);
                } else if (Camera.Parameters.WHITE_BALANCE_SHADE.equals(wb)) {
                    mSupportedWhiteBalances.add(WhiteBalance.SHADE);
                } else if (Camera.Parameters.WHITE_BALANCE_TWILIGHT.equals(wb)) {
                    mSupportedWhiteBalances.add(WhiteBalance.TWILIGHT);
                } else if (Camera.Parameters.WHITE_BALANCE_WARM_FLUORESCENT.equals(wb)) {
                    mSupportedWhiteBalances.add(WhiteBalance.WARM_FLUORESCENT);
                }
            }
        }
    }

    // Splits a comma delimited string to an ArrayList of String.
    // Return null if the passing string is null or the size is 0.
    private ArrayList<String> split(String str) {
        if (str == null) return null;

        TextUtils.StringSplitter splitter = new TextUtils.SimpleStringSplitter(',');
        splitter.setString(str);
        ArrayList<String> substrings = new ArrayList<String>();
        for (String s : splitter) {
            substrings.add(s);
        }
        return substrings;
    }

    private void buildColorEffects(Camera.Parameters p) {
        List<String> supportedColorEffects = p.getSupportedColorEffects();
        if (supportedColorEffects != null) {
            for (String coloreffect : supportedColorEffects) {
                mSupportedColorEffects.add(coloreffect);
            }
        }
    }

    private void buildSaturations(Camera.Parameters p) {
        String SUPPORTED_VALUES_SUFFIX = "-values";
        String KEY_SATURATION_MODE = "saturation-mode";
        String str = p.get(KEY_SATURATION_MODE + SUPPORTED_VALUES_SUFFIX);
        List<String> supportedSaturations = split(str);
        if (supportedSaturations != null) {
            for (String saturation : supportedSaturations) {
                mSupportedSaturations.add(saturation);
            }
        }
    }

    private void buildContrasts(Camera.Parameters p) {
        String SUPPORTED_VALUES_SUFFIX = "-values";
        String KEY_CONTRAST_MODE = "contrast-mode";
        String str = p.get(KEY_CONTRAST_MODE + SUPPORTED_VALUES_SUFFIX);
        List<String> supportedContrasts = split(str);
        if (supportedContrasts != null) {
            for (String contrast : supportedContrasts) {
                mSupportedContrasts.add(contrast);
            }
        }
    }

    private void buildSharpnesses(Camera.Parameters p) {
        String SUPPORTED_VALUES_SUFFIX = "-values";
        String KEY_SHARPNESS_MODE = "sharpness-mode";
        String str = p.get(KEY_SHARPNESS_MODE + SUPPORTED_VALUES_SUFFIX);
        List<String> supportedSharpnesses = split(str);
        if (supportedSharpnesses != null) {
            for (String sharpness : supportedSharpnesses) {
                mSupportedSharpnesses.add(sharpness);
            }
        }
    }

    private void buildBrightnesses(Camera.Parameters p) {
        String SUPPORTED_VALUES_SUFFIX = "-values";
        String KEY_BRIGHTNESS_MODE = "brightness-mode";
        String str = p.get(KEY_BRIGHTNESS_MODE + SUPPORTED_VALUES_SUFFIX);
        List<String> supportedBrightnesses = split(str);
        if (supportedBrightnesses != null) {
            for (String brightness : supportedBrightnesses) {
                mSupportedBrightnesses.add(brightness);
            }
        }
    }

    private void buildHues(Camera.Parameters p) {
        String SUPPORTED_VALUES_SUFFIX = "-values";
        String KEY_HUE_MODE = "hue-mode";
        String str = p.get(KEY_HUE_MODE + SUPPORTED_VALUES_SUFFIX);
        List<String> supportedHues = split(str);
        if (supportedHues != null) {
            for (String hue : supportedHues) {
                mSupportedHues.add(hue);
            }
        }
    }

    private void buildAntiBandings(Camera.Parameters p) {
        List<String> supportedAntiBandings = p.getSupportedAntibanding();
        if (supportedAntiBandings != null) {
            for (String antiBanding : supportedAntiBandings) {
                mSupportedAntiBandings.add(antiBanding);
            }
        }
    }

    private static class FpsComparator implements Comparator<int[]> {
        @Override
        public int compare(int[] fps1, int[] fps2) {
            return (fps1[0] == fps2[0] ? fps1[1] - fps2[1] : fps1[0] - fps2[0]);
        }
    }

    private static class SizeComparator implements Comparator<Size> {

        @Override
        public int compare(Size size1, Size size2) {
            return (size1.width() == size2.width() ? size1.height() - size2.height() :
                    size1.width() - size2.width());
        }
    }
}
