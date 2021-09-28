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

package com.android.cellbroadcastservice.tests;

import static com.google.common.truth.Truth.assertThat;

import android.annotation.NonNull;
import android.telephony.CbGeoUtils.Circle;
import android.telephony.CbGeoUtils.Geometry;
import android.telephony.CbGeoUtils.LatLng;
import android.testing.AndroidTestingRunner;
import android.testing.TestableLooper;

import com.android.cellbroadcastservice.CbGeoUtils;

import junit.framework.AssertionFailedError;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;

@RunWith(AndroidTestingRunner.class)
@TestableLooper.RunWithLooper
public class CbGeoUtilsTest extends CellBroadcastServiceTestBase {

    private static final double ASSERT_EQUALS_PRECISION = .005;

    LatLng mGooglePlex = new LatLng(37.423640, -122.088310);

    @Before
    public void setUp() throws Exception {
        super.setUp();
    }

    @After
    public void tearDown() throws Exception {
        super.tearDown();
    }

    @Test
    public void testEncodeAndParseString() {
        // create list of geometries
        List<Geometry> geometries = new ArrayList<>();
        geometries.add(new Circle(new LatLng(10, 10), 3000));
        geometries.add(new Circle(new LatLng(12, 10), 3000));
        geometries.add(new Circle(new LatLng(40, 40), 3000));

        // encode to string
        String geoString = CbGeoUtils.encodeGeometriesToString(geometries);

        // parse from string
        List<Geometry> parsedGeometries = CbGeoUtils.parseGeometriesFromString(geoString);

        // assert equality
        assertEquals(geometries, parsedGeometries);
    }

    @Test
    public void testAddEast() {
        LatLng east100m = addEast(mGooglePlex, 100);
        double distance = mGooglePlex.distance(east100m);
        assertThat(mGooglePlex.lat).isEqualTo(east100m.lat);
        assertEqualsWithinPrecision(100, distance);
    }

    @Test
    public void testAddNorth() {
        LatLng north120m = addSouth(mGooglePlex, -120);
        double distance = mGooglePlex.distance(north120m);
        assertThat(mGooglePlex.lng).isEqualTo(north120m.lng);
        assertEqualsWithinPrecision(120, distance);
    }

    @Test
    public void testExistingLatLngConversionToPoint() {
        {
            Circle circle = new Circle(new LatLng(37.42331, -122.08636), 500);

            // ~ 622 meters according to mapping utility
            LatLng ll622 =
                    new LatLng(37.41807, -122.08389);
            assertThat(circle.contains(ll622)).isFalse();
            assertEqualsWithinPrecision(622.0, ll622.distance(circle.getCenter()));

            LatLng origin = circle.getCenter();
            CbGeoUtils.Point ptCenter = convert(origin, circle.getCenter());
            CbGeoUtils.Point pt622 = convert(origin, ll622);
            assertEqualsWithinPrecision(622.0, ptCenter.distance(pt622));
        }

        {
            LatLng alaska = new LatLng(61.219005, -149.899929);
            LatLng llSouth100 = addSouth(alaska, 100);
            assertEqualsWithinPrecision(100,
                    convert(alaska, llSouth100).distance(convert(alaska, alaska)));

            LatLng llEast100 = addSouth(alaska, 100);
            assertEqualsWithinPrecision(100,
                    convert(alaska, llEast100).distance(convert(alaska, alaska)));

            LatLng llSouthEast100 = addSouth(addEast(alaska, 100), 100);
            assertEqualsWithinPrecision(141.42,
                    convert(alaska, llSouthEast100).distance(convert(alaska, alaska)));
        }
    }

    @Test
    public void testDistanceFromSegmentToPerpendicularPoint() {
        CbGeoUtils.LineSegment seg =
                new CbGeoUtils.LineSegment(
                        newPoint(0.0, 0.0), newPoint(0.0, 100.0));
        double dX = seg.distance(newPoint(50.0, 50));
        assertEqualsWithinPrecision(50.0, dX);

        double dY = seg.distance(newPoint(0.0, 200.0));
        assertEqualsWithinPrecision(100.0, dY);
    }

    @Test
    public void testDistanceFromSegmentToAngledPoint() {
        {
            CbGeoUtils.LineSegment seg =
                    new CbGeoUtils.LineSegment(newPoint(0.0, 0.0),
                            newPoint(0.0, 100.0));
            double hypTriangleOf50 = Math.sqrt(Math.pow(50, 2) + Math.pow(50, 2));

            double upperLeftDistanceSegment = seg.distance(newPoint(-50.0, -50.0));
            assertEqualsWithinPrecision(hypTriangleOf50, upperLeftDistanceSegment);

            double bottomRightDistanceSegment = seg.distance(newPoint(50.0, 150));
            assertEqualsWithinPrecision(hypTriangleOf50, bottomRightDistanceSegment);
        }

        {
            //Test segment starting with latlng
            LatLng llWestNorth = mGooglePlex;
            LatLng llWestSouth = addSouth(mGooglePlex, 500);

            LatLng origin = llWestNorth;
            CbGeoUtils.Point ptWestNorth = convert(origin, llWestNorth);
            CbGeoUtils.Point ptWestSouth = convert(origin, llWestSouth);
            double distancePoints = ptWestNorth.distance(ptWestSouth);
            assertEqualsWithinPrecision(500, distancePoints);
        }
    }

    @Test
    public void testDistanceWithSquareToPoint() {

        LatLng llWestNorth = addSouth(addEast(mGooglePlex, -100), -100);
        LatLng llWestSouth = addSouth(addEast(mGooglePlex, -100), 100);
        LatLng llEastSouth = addSouth(addEast(mGooglePlex, 100), 100);
        LatLng llEastNorth = addSouth(addEast(mGooglePlex, 100), -100);
        CbGeoUtils.DistancePolygon square = createPolygon(
                llWestNorth, llWestSouth, llEastSouth, llEastNorth);

        {
            LatLng llDueEast = addEast(mGooglePlex, 200);
            double distance = square.distance(llDueEast);
            assertEqualsWithinPrecision(100, distance);
        }

        {
            LatLng llDueEastNorth = addSouth(addEast(mGooglePlex, 200), -350);
            double llDistance = square.distance(llDueEastNorth);
            assertEqualsWithinPrecision(269, llDistance);
            square.distance(llDueEastNorth);
        }
    }

    private CbGeoUtils.DistancePolygon createPolygon(LatLng... latLngs) {
        return new CbGeoUtils.DistancePolygon(
                new android.telephony.CbGeoUtils.Polygon(
                        Arrays.stream(latLngs).collect(Collectors.toList())));
    }

    @NonNull
    LatLng addEast(LatLng latLng, double meters) {
        double offset = scaleMeters(meters)
                / Math.cos(Math.toRadians(latLng.lat));
        return new LatLng(latLng.lat, latLng.lng + offset);
    }

    @NonNull
    LatLng addSouth(LatLng latLng, double meters) {
        return new LatLng(latLng.lat + scaleMeters(meters),
                latLng.lng);
    }

    private static final int EARTH_RADIUS_METER = 6371 * 1000;
    static double scaleMeters(double meters) {
        return (meters / EARTH_RADIUS_METER) * (180 / Math.PI);
    }

    private CbGeoUtils.Point newPoint(double x, double y) {
        return new CbGeoUtils.Point(x, y);
    }

    public void assertEqualsWithinPrecision(double expected, double actual) {
        double precision = expected * ASSERT_EQUALS_PRECISION;
        if (Math.abs(expected - actual) >= precision) {
            String message = "assertEqualsWithinPrecision FAILED!\n"
                    + "Expected: " + expected + ", but Actual is: " + actual + "\n";
            throw new AssertionFailedError(message);
        }
    }

    private CbGeoUtils.Point convert(LatLng origin, LatLng latLng) {
        return CbGeoUtils.convertToDistanceFromOrigin(origin, latLng);
    }
}
