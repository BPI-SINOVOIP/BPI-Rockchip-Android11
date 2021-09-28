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

package android.location.cts.fine;

import static org.junit.Assert.assertEquals;

import android.location.GnssStatus;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class GnssStatusTest {

    private static final float DELTA = 1e-3f;

    @Test
    public void testGetValues() {
        GnssStatus gnssStatus = getTestGnssStatus();
        verifyTestValues(gnssStatus);
    }

    @Test
    public void testBuilder_ClearSatellites() {
        GnssStatus.Builder builder = new GnssStatus.Builder();
        builder.addSatellite(GnssStatus.CONSTELLATION_GPS,
                /* svid= */ 13,
                /* cn0DbHz= */ 25.5f,
                /* elevation= */ 2.0f,
                /* azimuth= */ 255.1f,
                /* hasEphemeris= */ true,
                /* hasAlmanac= */ false,
                /* usedInFix= */ true,
                /* hasCarrierFrequency= */ true,
                /* carrierFrequency= */ 1575420000f,
                /* hasBasebandCn0DbHz= */ true,
                /* basebandCn0DbHz= */ 20.5f);
        builder.clearSatellites();

        GnssStatus status = builder.build();
        assertEquals(0, status.getSatelliteCount());
    }

    private static GnssStatus getTestGnssStatus() {
        GnssStatus.Builder builder = new GnssStatus.Builder();
        builder.addSatellite(GnssStatus.CONSTELLATION_GPS,
                /* svid= */ 13,
                /* cn0DbHz= */ 25.5f,
                /* elevation= */ 2.0f,
                /* azimuth= */ 255.1f,
                /* hasEphemeris= */ true,
                /* hasAlmanac= */ false,
                /* usedInFix= */ true,
                /* hasCarrierFrequency= */ true,
                /* carrierFrequency= */ 1575420000f,
                /* hasBasebandCn0DbHz= */ true,
                /* basebandCn0DbHz= */ 20.5f);

        builder.addSatellite(GnssStatus.CONSTELLATION_GLONASS,
                /* svid= */ 9,
                /* cn0DbHz= */ 31.0f,
                /* elevation= */ 1.0f,
                /* azimuth= */ 193.8f,
                /* hasEphemeris= */ false,
                /* hasAlmanac= */ true,
                /* usedInFix= */ false,
                /* hasCarrierFrequency= */ false,
                /* carrierFrequency= */ Float.NaN,
                /* hasBasebandCn0DbHz= */ true,
                /* basebandCn0DbHz= */ 26.9f);

        return builder.build();
    }

    private static void verifyTestValues(GnssStatus gnssStatus) {
        assertEquals(2, gnssStatus.getSatelliteCount());
        assertEquals(GnssStatus.CONSTELLATION_GPS, gnssStatus.getConstellationType(0));
        assertEquals(GnssStatus.CONSTELLATION_GLONASS, gnssStatus.getConstellationType(1));

        assertEquals(13, gnssStatus.getSvid(0));
        assertEquals(9, gnssStatus.getSvid(1));

        assertEquals(25.5f, gnssStatus.getCn0DbHz(0), DELTA);
        assertEquals(31.0f, gnssStatus.getCn0DbHz(1), DELTA);

        assertEquals(2.0f, gnssStatus.getElevationDegrees(0), DELTA);
        assertEquals(1.0f, gnssStatus.getElevationDegrees(1), DELTA);

        assertEquals(255.1f, gnssStatus.getAzimuthDegrees(0), DELTA);
        assertEquals(193.8f, gnssStatus.getAzimuthDegrees(1), DELTA);

        assertEquals(true, gnssStatus.hasEphemerisData(0));
        assertEquals(false, gnssStatus.hasEphemerisData(1));

        assertEquals(false, gnssStatus.hasAlmanacData(0));
        assertEquals(true, gnssStatus.hasAlmanacData(1));

        assertEquals(true, gnssStatus.usedInFix(0));
        assertEquals(false, gnssStatus.usedInFix(1));

        assertEquals(true, gnssStatus.hasCarrierFrequencyHz(0));
        assertEquals(false, gnssStatus.hasCarrierFrequencyHz(1));

        assertEquals(1575420000f, gnssStatus.getCarrierFrequencyHz(0), DELTA);

        assertEquals(true, gnssStatus.hasBasebandCn0DbHz(0));
        assertEquals(true, gnssStatus.hasBasebandCn0DbHz(1));

        assertEquals(20.5f, gnssStatus.getBasebandCn0DbHz(0), DELTA);
        assertEquals(26.9f, gnssStatus.getBasebandCn0DbHz(1), DELTA);
    }
}
