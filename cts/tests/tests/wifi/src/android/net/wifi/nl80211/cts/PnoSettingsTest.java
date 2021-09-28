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
import android.net.wifi.nl80211.PnoSettings;
import android.os.Parcel;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;
import java.util.List;

/** CTS tests for {@link PnoSettings}. */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class PnoSettingsTest {

    private static List<PnoNetwork> createTestNetworks() {
        PnoNetwork network1 = new PnoNetwork();
        network1.setSsid(new byte[] { 's', 's', 'i', 'd' });
        network1.setFrequenciesMhz(new int[] { 2412, 2417, 5035 });
        network1.setHidden(true);

        PnoNetwork network2 = new PnoNetwork();
        network2.setSsid(new byte[] { 'a', 's', 'd', 'f' });
        network2.setFrequenciesMhz(new int[] { 2422, 2427, 5040 });
        network2.setHidden(false);

        return Arrays.asList(network1, network2);
    }

    @Before
    public void setUp() {
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        // skip tests if Wifi is not supported
        assumeTrue(WifiFeature.isWifiSupported(context));
    }

    @Test
    public void testGetters() {
        PnoSettings settings = new PnoSettings();
        settings.setIntervalMillis(1000);
        settings.setMin2gRssiDbm(-70);
        settings.setMin5gRssiDbm(-60);
        settings.setMin6gRssiDbm(-50);
        settings.setPnoNetworks(createTestNetworks());

        assertThat(settings.getIntervalMillis()).isEqualTo(1000);
        assertThat(settings.getMin2gRssiDbm()).isEqualTo(-70);
        assertThat(settings.getMin5gRssiDbm()).isEqualTo(-60);
        assertThat(settings.getMin6gRssiDbm()).isEqualTo(-50);
        assertThat(settings.getPnoNetworks()).isEqualTo(createTestNetworks());
    }

    @Test
    public void canSerializeAndDeserialize() {
        PnoSettings settings = new PnoSettings();
        settings.setIntervalMillis(1000);
        settings.setMin2gRssiDbm(-70);
        settings.setMin5gRssiDbm(-60);
        settings.setMin6gRssiDbm(-50);
        settings.setPnoNetworks(createTestNetworks());

        Parcel parcel = Parcel.obtain();
        settings.writeToParcel(parcel, 0);
        // Rewind the pointer to the head of the parcel.
        parcel.setDataPosition(0);
        PnoSettings settingsDeserialized = PnoSettings.CREATOR.createFromParcel(parcel);

        assertThat(settingsDeserialized.getIntervalMillis()).isEqualTo(1000);
        assertThat(settingsDeserialized.getMin2gRssiDbm()).isEqualTo(-70);
        assertThat(settingsDeserialized.getMin5gRssiDbm()).isEqualTo(-60);
        assertThat(settingsDeserialized.getMin6gRssiDbm()).isEqualTo(-50);
        assertThat(settingsDeserialized.getPnoNetworks()).isEqualTo(createTestNetworks());
        assertThat(settingsDeserialized).isEqualTo(settings);
        assertThat(settingsDeserialized.hashCode()).isEqualTo(settings.hashCode());
    }

    @Test
    public void testEquals() {
        PnoSettings settings = new PnoSettings();
        settings.setIntervalMillis(1000);
        settings.setMin2gRssiDbm(-70);
        settings.setMin5gRssiDbm(-60);
        settings.setMin6gRssiDbm(-50);
        settings.setPnoNetworks(createTestNetworks());

        PnoSettings settings2 = new PnoSettings();
        settings.setIntervalMillis(2000);
        settings.setMin2gRssiDbm(-70);
        settings.setMin5gRssiDbm(-60);
        settings.setMin6gRssiDbm(-50);
        settings.setPnoNetworks(createTestNetworks());

        assertThat(settings2).isNotEqualTo(settings);
    }
}
