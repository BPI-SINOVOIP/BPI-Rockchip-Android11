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

package android.net.wifi.nl80211.cts;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;

import android.content.Context;
import android.net.wifi.cts.WifiFeature;
import android.net.wifi.nl80211.NativeScanResult;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** CTS tests for {@link NativeScanResult}. */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class NativeScanResultTest {

    @Before
    public void setUp() {
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        // skip tests if Wifi is not supported
        assumeTrue(WifiFeature.isWifiSupported(context));
    }

    @Test
    public void testGetters() {
        NativeScanResult result = new NativeScanResult();
        assertThat(result.isAssociated()).isFalse();
    }
}
