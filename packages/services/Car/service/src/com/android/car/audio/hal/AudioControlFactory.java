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

package com.android.car.audio.hal;

import android.util.Log;

/**
 * Factory for constructing wrappers around IAudioControl HAL instances.
 */
public final class AudioControlFactory {
    private static final String TAG = AudioControlWrapper.class.getSimpleName();

    /**
     * Generates {@link AudioControlWrapper} for interacting with IAudioControl HAL service. Checks
     * for V2.0 first, and then falls back to V1.0 if that is not available. Will throw if none is
     * registered on the manifest.
     * @return {@link AudioControlWrapper} for registered IAudioControl service.
     */
    public static AudioControlWrapper newAudioControl() {
        android.hardware.automotive.audiocontrol.V2_0.IAudioControl audioControlV2 =
                AudioControlWrapperV2.getService();
        if (audioControlV2 != null) {
            return new AudioControlWrapperV2(audioControlV2);
        }
        Log.i(TAG, "IAudioControl@V2.0 not in the manifest");

        android.hardware.automotive.audiocontrol.V1_0.IAudioControl audioControlV1 =
                AudioControlWrapperV1.getService();
        if (audioControlV1 != null) {
            Log.w(TAG, "IAudioControl V1.0 is deprecated. Consider upgrading to V2.0");
            return new AudioControlWrapperV1(audioControlV1);
        }

        throw new IllegalStateException("No version of AudioControl HAL in the manifest");
    }
}
