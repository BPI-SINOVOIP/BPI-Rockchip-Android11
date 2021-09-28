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
import android.net.wifi.nl80211.PnoNetwork;
import android.os.Parcel;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** CTS tests for {@link PnoNetwork}. */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class PnoNetworkTest {

    private static final byte[] TEST_SSID = { 's', 's', 'i', 'd' };
    private static final int[] TEST_FREQUENCIES = { 2412, 2417, 5035 };

    @Before
    public void setUp() {
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        // skip tests if Wifi is not supported
        assumeTrue(WifiFeature.isWifiSupported(context));
    }

    @Test
    public void testGetters() {
        PnoNetwork network = new PnoNetwork();
        network.setSsid(TEST_SSID);
        network.setFrequenciesMhz(TEST_FREQUENCIES);
        network.setHidden(true);

        assertThat(network.getSsid()).isEqualTo(TEST_SSID);
        assertThat(network.getFrequenciesMhz()).isEqualTo(TEST_FREQUENCIES);
        assertThat(network.isHidden()).isTrue();
    }

    @Test
    public void canSerializeAndDeserialize() {
        PnoNetwork network = new PnoNetwork();
        network.setSsid(TEST_SSID);
        network.setFrequenciesMhz(TEST_FREQUENCIES);
        network.setHidden(true);

        Parcel parcel = Parcel.obtain();
        network.writeToParcel(parcel, 0);
        // Rewind the pointer to the head of the parcel.
        parcel.setDataPosition(0);
        PnoNetwork networkDeserialized = PnoNetwork.CREATOR.createFromParcel(parcel);

        assertThat(networkDeserialized.getSsid()).isEqualTo(TEST_SSID);
        assertThat(networkDeserialized.getFrequenciesMhz()).isEqualTo(TEST_FREQUENCIES);
        assertThat(networkDeserialized.isHidden()).isTrue();
        assertThat(networkDeserialized).isEqualTo(network);
        assertThat(networkDeserialized.hashCode()).isEqualTo(network.hashCode());
    }

    @Test
    public void testEquals() {
        PnoNetwork network = new PnoNetwork();
        network.setSsid(TEST_SSID);
        network.setFrequenciesMhz(TEST_FREQUENCIES);
        network.setHidden(true);

        PnoNetwork network2 = new PnoNetwork();
        network.setSsid(new byte[] { 'a', 's', 'd', 'f'});
        network.setFrequenciesMhz(new int[] { 1, 2, 3 });
        network.setHidden(false);

        assertThat(network2).isNotEqualTo(network);
    }
}
