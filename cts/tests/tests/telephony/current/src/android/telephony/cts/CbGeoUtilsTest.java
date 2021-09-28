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

package android.telephony.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.telephony.CbGeoUtils;

import org.junit.Test;

import java.util.ArrayList;

public class CbGeoUtilsTest {

    // latitude is in range -90, 90
    private static final double LAT1 = 30;
    // longitude is in range -180, 180
    private static final double LNG1 = 100;

    private static final double LAT2 = 10;
    private static final double LNG2 = 120;

    private static final double LAT3 = -10;
    private static final double LNG3 = -90;

    // distance in meters between (LAT1, LNG1) and (LAT2, LNG2)
    private static final double DIST = 3040602;

    // max allowed error in calculations
    private static final double DELTA = 1;

    @Test
    public void testLatLong() {
        CbGeoUtils.LatLng p1 = new CbGeoUtils.LatLng(LAT1, LNG1);
        CbGeoUtils.LatLng p2 = new CbGeoUtils.LatLng(LAT2, LNG2);

        CbGeoUtils.LatLng difference = new CbGeoUtils.LatLng(LAT1 - LAT2, LNG1 - LNG2);
        assertEquals(difference.lat, p1.subtract(p2).lat, DELTA);
        assertEquals(difference.lng, p1.subtract(p2).lng, DELTA);

        assertEquals(DIST, p1.distance(p2), DELTA);
    }

    @Test
    public void testPolygon() {
        CbGeoUtils.LatLng p1 = new CbGeoUtils.LatLng(LAT1, LNG1);
        CbGeoUtils.LatLng p2 = new CbGeoUtils.LatLng(LAT2, LNG2);
        CbGeoUtils.LatLng p3 = new CbGeoUtils.LatLng(LAT3, LNG3);

        ArrayList<CbGeoUtils.LatLng> vertices = new ArrayList<>();
        vertices.add(p1);
        vertices.add(p2);
        vertices.add(p3);

        CbGeoUtils.Polygon polygon = new CbGeoUtils.Polygon((vertices));
        assertEquals(vertices, polygon.getVertices());

        assertTrue(polygon.contains(p1));
        assertTrue(polygon.contains(p2));
        assertTrue(polygon.contains(p3));
    }

    @Test
    public void testCircle() {
        CbGeoUtils.LatLng p1 = new CbGeoUtils.LatLng(LAT1, LNG1);
        double radius = 1000;
        CbGeoUtils.Circle circle = new CbGeoUtils.Circle(p1, radius);

        assertEquals(radius, circle.getRadius(), DELTA);
        assertEquals(p1, circle.getCenter());
        // circle should always contain its center
        assertTrue(circle.contains(p1));
    }
}
