/*
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
package com.android.tv.tuner.dvb;

import static com.google.common.truth.Truth.assertThat;

import com.android.tv.common.compat.TvInputConstantCompat;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.tuner.tvinput.TunerSessionWorker;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

/** Tests for {@link TunerSessionWorker}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK, application = TestSingletonApp.class)
public class DvbTunerHalTest {
    private int mSignal = 0;

    DvbTunerHal mDvbTunerHal =
            new DvbTunerHal(RuntimeEnvironment.application) {
                @Override
                protected int nativeGetSignalStrength(long deviceId) {
                    return mSignal;
                }
            };

    @Test
    public void getSignalStrength_notUsed() {
        mSignal = -3;
        int signal = mDvbTunerHal.getSignalStrength();
        assertThat(signal).isEqualTo(TvInputConstantCompat.SIGNAL_STRENGTH_NOT_USED);
    }

    @Test
    public void getSignalStrength_errorMax() {
        mSignal = Integer.MAX_VALUE;
        int signal = mDvbTunerHal.getSignalStrength();
        assertThat(signal).isEqualTo(TvInputConstantCompat.SIGNAL_STRENGTH_ERROR);
    }

    @Test
    public void getSignalStrength_errorMin() {
        mSignal = Integer.MIN_VALUE;
        int signal = mDvbTunerHal.getSignalStrength();
        assertThat(signal).isEqualTo(TvInputConstantCompat.SIGNAL_STRENGTH_ERROR);
    }

    @Test
    public void getSignalStrength_error() {
        mSignal = -1;
        int signal = mDvbTunerHal.getSignalStrength();
        assertThat(signal).isEqualTo(TvInputConstantCompat.SIGNAL_STRENGTH_ERROR);
    }

    @Test
    public void getSignalStrength_curvedMax() {
        mSignal = 65535;
        int signal = mDvbTunerHal.getSignalStrength();
        assertThat(signal).isEqualTo(100);
    }

    @Test
    public void getSignalStrength_curvedHalf() {
        mSignal = 58982;
        int signal = mDvbTunerHal.getSignalStrength();
        assertThat(signal).isEqualTo(50);
    }

    @Test
    public void getSignalStrength_curvedMin() {
        mSignal = 0;
        int signal = mDvbTunerHal.getSignalStrength();
        assertThat(signal).isEqualTo(0);
    }
}
