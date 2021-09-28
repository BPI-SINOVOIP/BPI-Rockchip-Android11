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

package android.net.cts;

import static com.google.common.truth.Truth.assertThat;

import android.net.RssiCurve;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/** CTS tests for {@link RssiCurve}. */
@RunWith(AndroidJUnit4.class)
public class RssiCurveTest {

    @Test
    public void lookupScore_constantCurve() {
        // One bucket from rssi=-100 to 100 with score 10.
        RssiCurve curve = new RssiCurve(-100, 200, new byte[] { 10 });
        assertThat(curve.lookupScore(-200)).isEqualTo(10);
        assertThat(curve.lookupScore(-100)).isEqualTo(10);
        assertThat(curve.lookupScore(0)).isEqualTo(10);
        assertThat(curve.lookupScore(100)).isEqualTo(10);
        assertThat(curve.lookupScore(200)).isEqualTo(10);
    }

    @Test
    public void lookupScore_changingCurve() {
        // One bucket from -100 to 0 with score -10, and one bucket from 0 to 100 with score 10.
        RssiCurve curve = new RssiCurve(-100, 100, new byte[] { -10, 10 });
        assertThat(curve.lookupScore(-200)).isEqualTo(-10);
        assertThat(curve.lookupScore(-100)).isEqualTo(-10);
        assertThat(curve.lookupScore(-50)).isEqualTo(-10);
        assertThat(curve.lookupScore(0)).isEqualTo(10);
        assertThat(curve.lookupScore(50)).isEqualTo(10);
        assertThat(curve.lookupScore(100)).isEqualTo(10);
        assertThat(curve.lookupScore(200)).isEqualTo(10);
    }

    @Test
    public void lookupScore_linearCurve() {
        // Curve starting at -110, with 15 buckets of width 10 whose scores increases by 10 with
        // each bucket. The current active network gets a boost of 15 to its RSSI.
        RssiCurve curve = new RssiCurve(
                -110,
                10,
                new byte[] { -20, -10, 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120 },
                15);

        assertThat(curve.lookupScore(-120)).isEqualTo(-20);
        assertThat(curve.lookupScore(-120, false)).isEqualTo(-20);
        assertThat(curve.lookupScore(-120, true)).isEqualTo(-20);

        assertThat(curve.lookupScore(-111)).isEqualTo(-20);
        assertThat(curve.lookupScore(-111, false)).isEqualTo(-20);
        assertThat(curve.lookupScore(-111, true)).isEqualTo(-10);

        assertThat(curve.lookupScore(-110)).isEqualTo(-20);
        assertThat(curve.lookupScore(-110, false)).isEqualTo(-20);
        assertThat(curve.lookupScore(-110, true)).isEqualTo(-10);

        assertThat(curve.lookupScore(-105)).isEqualTo(-20);
        assertThat(curve.lookupScore(-105, false)).isEqualTo(-20);
        assertThat(curve.lookupScore(-105, true)).isEqualTo(0);

        assertThat(curve.lookupScore(-100)).isEqualTo(-10);
        assertThat(curve.lookupScore(-100, false)).isEqualTo(-10);
        assertThat(curve.lookupScore(-100, true)).isEqualTo(0);

        assertThat(curve.lookupScore(-50)).isEqualTo(40);
        assertThat(curve.lookupScore(-50, false)).isEqualTo(40);
        assertThat(curve.lookupScore(-50, true)).isEqualTo(50);

        assertThat(curve.lookupScore(0)).isEqualTo(90);
        assertThat(curve.lookupScore(0, false)).isEqualTo(90);
        assertThat(curve.lookupScore(0, true)).isEqualTo(100);

        assertThat(curve.lookupScore(30)).isEqualTo(120);
        assertThat(curve.lookupScore(30, false)).isEqualTo(120);
        assertThat(curve.lookupScore(30, true)).isEqualTo(120);

        assertThat(curve.lookupScore(40)).isEqualTo(120);
        assertThat(curve.lookupScore(40, false)).isEqualTo(120);
        assertThat(curve.lookupScore(40, true)).isEqualTo(120);
    }
}
