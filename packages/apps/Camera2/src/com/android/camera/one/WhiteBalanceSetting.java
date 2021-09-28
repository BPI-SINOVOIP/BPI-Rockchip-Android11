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

package com.android.camera.one;

import android.hardware.camera2.CaptureRequest;
import android.widget.Switch;

import com.android.camera.async.ForwardingObservable;
import com.android.camera.async.Observable;
import com.android.camera.async.Observables;
import com.android.camera.one.OneCamera.PhotoCaptureParameters.WhiteBalance;
import com.google.common.base.Function;

/**
 * Translates from the WhiteBalance Mode setting (stored as a string) to the
 * appropriate {@link OneCamera.PhotoCaptureParameters.WhiteBalance} value.
 */
public class WhiteBalanceSetting extends ForwardingObservable<OneCamera.PhotoCaptureParameters.WhiteBalance> {
    private static class WhiteBalanceStringToEnum implements
            Function<String, OneCamera.PhotoCaptureParameters.WhiteBalance> {
        @Override
        public OneCamera.PhotoCaptureParameters.WhiteBalance apply(String settingString) {
            return OneCamera.PhotoCaptureParameters.WhiteBalance.decodeSettingsString(settingString);
        }
    }

    public WhiteBalanceSetting(Observable<String> wbSettingString) {
        super(Observables.transform(wbSettingString, new WhiteBalanceStringToEnum()));
    }
}
