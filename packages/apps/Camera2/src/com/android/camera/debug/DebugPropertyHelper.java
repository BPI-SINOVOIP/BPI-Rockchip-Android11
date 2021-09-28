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

package com.android.camera.debug;

import com.android.camera.util.SystemProperties;

public class DebugPropertyHelper {
    private static final String OFF_VALUE = "0";
    private static final String ON_VALUE = "1";

    private static final String PREFIX = "persist.camera";

    /** Enable frame-by-frame focus logging. */
    private static final String PROP_FRAME_LOG = PREFIX + ".frame_log";
    /**
     * Enable additional capture debug UI.
     * For API1/Photomodule: show faces.
     * For API2/Capturemodule: show faces, AF state, AE/AF precise regions.
     */
    private static final String PROP_CAPTURE_DEBUG_UI = PREFIX + ".debug_ui";
    /** Switch between OneCameraImpl and OneCameraZslImpl. */
    private static final String PROP_FORCE_LEGACY_ONE_CAMERA = PREFIX + ".legacy";
    /** Write data about each capture request to disk. */
    private static final String PROP_WRITE_CAPTURE_DATA = PREFIX + ".capture_write";
    /** Is RAW support enabled. */
    private static final String PROP_CAPTURE_DNG = PREFIX + ".capture_dng";
    /** controll preview not rotate. */
    private static final String PROP_PREVIEW_NOT_ROTATE = PREFIX + ".preview_norotate";
    /** controll preview full size. */
    private static final String PROP_PREVIEW_FULLSIZE_ON = PREFIX + ".preview_full";
    /** controll banding mode. */
    private static final String PROP_BANDING_MODE = PREFIX + ".banding";
    /** controll if autofocus before capture. */
    private static final String PROP_AF_BEFORE_CAPTURE = PREFIX + ".capture_af";
    /** controll capture mode. 0:zsl&normal 1:normal-only 2:zsl-only */
    private static final String PROP_CAPTURE_MODE = PREFIX + ".capture_mode";
    public static final String ZSL_NORMAL_MODE = "0";
    public static final String ONLY_NORMAL_MODE = "1";
    public static final String ONLY_ZSL_MODE = "2";
    /** controll debug on. */
    private static final String PROP_DEBUG_ON = PREFIX + ".debug";
    /** controll smile shutter auto on. */
    private static final String PROP_SMILESHUTTER_AUTO = PREFIX + ".smile_auto";
    /** controll video snapshot. */
    private static final String PROP_VIDEO_SNAPSHOT = PREFIX + ".video_capture";

    private static boolean isPropertyOn(String property) {
        return ON_VALUE.equals(SystemProperties.get(property, OFF_VALUE));
    }

    public static boolean showFrameDebugLog() {
        return isPropertyOn(PROP_FRAME_LOG);
    }

    public static boolean showCaptureDebugUI() {
        return isPropertyOn(PROP_CAPTURE_DEBUG_UI);
    }

    public static boolean writeCaptureData() {
        return isPropertyOn(PROP_WRITE_CAPTURE_DATA);
    }

    public static boolean isCaptureDngEnabled() {
        return isPropertyOn(PROP_CAPTURE_DNG);
    }

    public static boolean isPreviewAutoRotate() {
        return !isPropertyOn(PROP_PREVIEW_NOT_ROTATE);
    }

    public static String getDefaultBanding() {
        return SystemProperties.get(PROP_BANDING_MODE, "auto");
    }

    public static boolean isPreviewFullSizeOn() {
        return isPropertyOn(PROP_PREVIEW_FULLSIZE_ON);
    }

    public static boolean isAFDisabledBeforeCapture() {
        return !isPropertyOn(PROP_AF_BEFORE_CAPTURE);
    }

    public static String getCaptureMode() {
        return SystemProperties.get(PROP_CAPTURE_MODE, ZSL_NORMAL_MODE);
    }

    public static boolean isDebugOn() {
        return isPropertyOn(PROP_DEBUG_ON);
    }

    public static boolean isSmileShutterAuto() {
        return isPropertyOn(PROP_SMILESHUTTER_AUTO);
    }

    public static boolean isVideoSnapShotEnabled() {
        return isPropertyOn(PROP_VIDEO_SNAPSHOT);
    }
}
