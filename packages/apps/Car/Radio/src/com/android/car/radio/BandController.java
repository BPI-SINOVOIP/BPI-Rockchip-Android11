/**
 * Copyright (C) 2018 The Android Open Source Project
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

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.car.radio.bands.ProgramType;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/**
 * Keeps track of the current band and list of supported program types/bands (AM/FM/DAB etc).
 */
public class BandController {
    private final Object mLock = new Object();

    @NonNull private List<ProgramType> mSupportedBands = new ArrayList<>();
    @Nullable private ProgramType mCurrentBand;

    /**
     * Sets supported program types.
     */
    public void setSupportedProgramTypes(@NonNull List<ProgramType> supported) {
        synchronized (mLock) {
            mSupportedBands = Objects.requireNonNull(supported);
        }
    }

    /**
     * Switches to the next supported band.
     *
     * @return The current band after the switch
     */
    public ProgramType switchToNext() {
        synchronized (mLock) {
            if (mSupportedBands.isEmpty()) {
                return null;
            }
            mCurrentBand = mSupportedBands.get(
                    (mSupportedBands.indexOf(mCurrentBand) + 1) % mSupportedBands.size());
            return mCurrentBand;
        }
    }

    /**
     * Sets the current band.
     *
     * @param programType Program type to set.
     */
    public void setType(@NonNull ProgramType programType) {
        synchronized (mLock) {
            mCurrentBand = programType;
        }
    }

    /**
     * Retrieves the current band.
     */
    public ProgramType getCurrentBand() {
        return mCurrentBand;
    }
}

