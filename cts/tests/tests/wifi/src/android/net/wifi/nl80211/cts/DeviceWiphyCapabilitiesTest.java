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
import android.net.wifi.ScanResult;
import android.net.wifi.cts.WifiFeature;
import android.net.wifi.nl80211.DeviceWiphyCapabilities;
import android.os.Parcel;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** CTS tests for {@link DeviceWiphyCapabilities}. */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class DeviceWiphyCapabilitiesTest {

    @Before
    public void setUp() {
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        // skip tests if Wifi is not supported
        assumeTrue(WifiFeature.isWifiSupported(context));
    }

    /**
     *  Test that a {@link DeviceWiphyCapabilities} object can be serialized and deserialized,
     *  while keeping its values unchanged.
     */
    @Test
    public void canSerializeAndDeserialize() {
        DeviceWiphyCapabilities capa = new DeviceWiphyCapabilities();

        capa.setWifiStandardSupport(ScanResult.WIFI_STANDARD_11N, true);
        capa.setWifiStandardSupport(ScanResult.WIFI_STANDARD_11AC, true);
        capa.setWifiStandardSupport(ScanResult.WIFI_STANDARD_11AX, false);

        Parcel parcel = Parcel.obtain();
        capa.writeToParcel(parcel, 0);
        // Rewind the pointer to the head of the parcel.
        parcel.setDataPosition(0);
        DeviceWiphyCapabilities capaDeserialized =
                DeviceWiphyCapabilities.CREATOR.createFromParcel(parcel);

        assertThat(capaDeserialized.isWifiStandardSupported(ScanResult.WIFI_STANDARD_11N)).isTrue();
        assertThat(capaDeserialized.isWifiStandardSupported(ScanResult.WIFI_STANDARD_11AC))
                .isTrue();
        assertThat(capaDeserialized.isWifiStandardSupported(ScanResult.WIFI_STANDARD_11AX))
                .isFalse();
        assertThat(capaDeserialized).isEqualTo(capa);
        assertThat(capaDeserialized.hashCode()).isEqualTo(capa.hashCode());
    }

    /** Test mapping wifi standard support into channel width support */
    @Test
    public void testMappingWifiStandardIntoChannelWidthSupport() {
        DeviceWiphyCapabilities capa = new DeviceWiphyCapabilities();

        capa.setWifiStandardSupport(ScanResult.WIFI_STANDARD_11N, false);
        capa.setWifiStandardSupport(ScanResult.WIFI_STANDARD_11AC, false);
        capa.setWifiStandardSupport(ScanResult.WIFI_STANDARD_11AX, false);
        assertThat(capa.isChannelWidthSupported(ScanResult.CHANNEL_WIDTH_20MHZ)).isTrue();
        assertThat(capa.isChannelWidthSupported(ScanResult.CHANNEL_WIDTH_40MHZ)).isFalse();
        assertThat(capa.isChannelWidthSupported(ScanResult.CHANNEL_WIDTH_80MHZ)).isFalse();

        capa.setWifiStandardSupport(ScanResult.WIFI_STANDARD_11N, true);
        assertThat(capa.isChannelWidthSupported(ScanResult.CHANNEL_WIDTH_20MHZ)).isTrue();
        assertThat(capa.isChannelWidthSupported(ScanResult.CHANNEL_WIDTH_40MHZ)).isTrue();
        assertThat(capa.isChannelWidthSupported(ScanResult.CHANNEL_WIDTH_80MHZ)).isFalse();

        capa.setWifiStandardSupport(ScanResult.WIFI_STANDARD_11AC, true);
        assertThat(capa.isChannelWidthSupported(ScanResult.CHANNEL_WIDTH_20MHZ)).isTrue();
        assertThat(capa.isChannelWidthSupported(ScanResult.CHANNEL_WIDTH_40MHZ)).isTrue();
        assertThat(capa.isChannelWidthSupported(ScanResult.CHANNEL_WIDTH_80MHZ)).isTrue();
    }
}
