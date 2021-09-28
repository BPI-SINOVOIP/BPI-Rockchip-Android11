/*
 * Copyright (C) 2019 The Android Open Source Project
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
package com.android.car.radio;

import androidx.annotation.Nullable;

/**
 * Enum used to define which Radio property should be affected by the media keys.
 */
public enum SkipMode {

    FAVORITES, TUNE, BROWSE;

    /**
     * Gets the default mode.
     */
    public static final SkipMode DEFAULT_MODE = TUNE;

    /**
     * Converts an {@code int} value to a valid {@link SkipMode} {or {@code null} if invalid).
     */
    @Nullable
    public static SkipMode valueOf(int mode) {
        if (mode < 0 || mode > SkipMode.values().length - 1) {
            return null;
        }
        return values()[mode];
    }
}
