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
import android.net.MacAddress;
import android.net.wifi.cts.WifiFeature;
import android.net.wifi.nl80211.NativeWifiClient;
import android.os.Parcel;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** CTS tests for {@link NativeWifiClient}. */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class NativeWifiClientTest {

    private static final byte[] TEST_MAC = { 1, 2, 3, 4, 5, 6 };

    @Before
    public void setUp() {
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        // skip tests if Wifi is not supported
        assumeTrue(WifiFeature.isWifiSupported(context));
    }

    @Test
    public void testGetters() {
        NativeWifiClient client = new NativeWifiClient(MacAddress.fromBytes(TEST_MAC));

        assertThat(client.getMacAddress().toByteArray()).isEqualTo(TEST_MAC);
    }

    @Test
    public void canSerializeAndDeserialize() {
        NativeWifiClient client = new NativeWifiClient(MacAddress.fromBytes(TEST_MAC));

        Parcel parcel = Parcel.obtain();
        client.writeToParcel(parcel, 0);
        // Rewind the pointer to the head of the parcel.
        parcel.setDataPosition(0);
        NativeWifiClient clientDeserialized = NativeWifiClient.CREATOR.createFromParcel(parcel);

        assertThat(clientDeserialized.getMacAddress().toByteArray()).isEqualTo(TEST_MAC);
        assertThat(clientDeserialized).isEqualTo(client);
        assertThat(clientDeserialized.hashCode()).isEqualTo(client.hashCode());
    }

    @Test
    public void testEquals() {
        NativeWifiClient client = new NativeWifiClient(MacAddress.fromBytes(TEST_MAC));
        NativeWifiClient client2 =
                new NativeWifiClient(MacAddress.fromBytes(new byte[] { 7, 8, 9, 10, 11, 12 }));

        assertThat(client2).isNotEqualTo(client);
    }
}
