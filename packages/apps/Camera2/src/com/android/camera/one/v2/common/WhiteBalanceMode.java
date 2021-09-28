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

package com.android.camera.one.v2.common;

import android.hardware.camera2.CaptureRequest;

import com.android.camera.one.OneCamera;
import com.google.common.base.Supplier;

/**
 * Computes the current AE Mode to use based on the current flash state.
 */
public class WhiteBalanceMode implements Supplier<Integer> {
    private final Supplier<OneCamera.PhotoCaptureParameters.WhiteBalance> mWhitBalance;

    public WhiteBalanceMode(
          Supplier<OneCamera.PhotoCaptureParameters.WhiteBalance> wb) {
        mWhitBalance = wb;
    }

    @Override
    public Integer get() {
        switch (mWhitBalance.get()) {
            case AUTO:
                return CaptureRequest.CONTROL_AWB_MODE_AUTO;
            case CLOUDY_DAYLIGHT:
                return CaptureRequest.CONTROL_AWB_MODE_CLOUDY_DAYLIGHT;
            case DAYLIGHT:
                return CaptureRequest.CONTROL_AWB_MODE_DAYLIGHT;
            case FLUORESCENT:
                return CaptureRequest.CONTROL_AWB_MODE_FLUORESCENT;
            case INCANDESCENT:
                return CaptureRequest.CONTROL_AWB_MODE_INCANDESCENT;
            case SHADE:
                return CaptureRequest.CONTROL_AWB_MODE_SHADE;
            case TWILIGHT:
                return CaptureRequest.CONTROL_AWB_MODE_TWILIGHT;
            case WARM_FLUORESCENT:
                return CaptureRequest.CONTROL_AWB_MODE_WARM_FLUORESCENT;
            default:
                return CaptureRequest.CONTROL_AWB_MODE_AUTO;
        }
    }
}
