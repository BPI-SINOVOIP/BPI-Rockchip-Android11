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

import static org.mockito.Mockito.doReturn;

import android.annotation.NonNull;
import android.telephony.CbGeoUtils.Circle;
import android.telephony.CbGeoUtils.Geometry;
import android.telephony.CbGeoUtils.LatLng;
import android.telephony.CbGeoUtils.Polygon;
import android.testing.AndroidTestingRunner;
import android.testing.TestableLooper;

import com.android.cellbroadcastservice.CbSendMessageCalculator;
import com.android.cellbroadcastservice.R;

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
public class CbSendMessageCalculatorTest extends CellBroadcastServiceTestBase {
    LatLng mGooglePlex = new LatLng(
            37.423640, -122.088310);

    private Polygon mSquare;
    private Circle mCircleFarAway;

    @Before
    public void setUp() throws Exception {
        super.setUp();

        doReturn(true).when(mMockedResources)
                .getBoolean(R.bool.use_new_geo_fence_calculation);

        LatLng llWestNorth = addSouth(addEast(mGooglePlex, -1000), -1000);
        LatLng llWestSouth = addSouth(addEast(mGooglePlex, -1000), 1000);
        LatLng llEastSouth = addSouth(addEast(mGooglePlex, 1000), 1000);
        LatLng llEastNorth = addSouth(addEast(mGooglePlex, 1000), -1000);

        mSquare = createPolygon(llWestNorth, llWestSouth, llEastSouth, llEastNorth);
        mCircleFarAway = new Circle(addEast(mGooglePlex, 10000), 10);
    }

    @After
    public void tearDown() throws Exception {
        super.tearDown();
    }

    @Test
    public void testSingleSquareOutside() {
        LatLng coor = addSouth(addEast(mGooglePlex, -2000), -500);
        double threshold = 100;

        //We need to test with accuracies less than and greater than the threshold
        testSquareOutside(coor, threshold - 10, threshold);
        testSquareOutside(coor, threshold + 10, threshold);
    }

    void testSquareOutside(LatLng coor, double accuracy, double threshold) {
        CbSendMessageCalculator calculator = createCalculator(threshold, mSquare);
        calculator.addCoordinate(coor, accuracy);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_DONT_SEND, calculator.getAction());
    }

    @Test
    public void testSingleSquareInside() {
        LatLng coor = mGooglePlex;
        double threshold = 100;
        testSquareInside(coor, threshold - 10, threshold);
        testSquareInside(coor, threshold + 10, threshold);
    }

    void testSquareInside(LatLng coor, double accuracy, double threshold) {
        CbSendMessageCalculator calculator = createCalculator(threshold, mSquare);
        calculator.addCoordinate(coor, accuracy);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_SEND, calculator.getAction());
    }

    @Test
    public void testSingleSquareInsideAndOutsideThreshold() {
        LatLng coor = addEast(addSouth(mGooglePlex, 1005), 1005);
        double threshold = 100;
        testSquareInsideThreshold(coor, 1, threshold);

        //The accuracy is greater than the threshold and not overlapping, and so this is a no go
        testSquareAmbiguous(coor, threshold + 50, threshold);
    }

    private void testSquareAmbiguous(LatLng coor, double accuracy, double threshold) {
        CbSendMessageCalculator calculator = createCalculator(threshold, mSquare);
        calculator.addCoordinate(coor, accuracy);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_AMBIGUOUS,
                calculator.getAction());
    }

    void testSquareInsideThreshold(LatLng coor, double accuracy, double threshold) {
        CbSendMessageCalculator calculator = createCalculator(threshold, mSquare);
        calculator.addCoordinate(coor, accuracy);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_SEND,
                calculator.getAction());
    }

    @Test
    public void testNone() {
        CbSendMessageCalculator calculator = createCalculator(10, null);
        new CbSendMessageCalculator(mMockedContext, new ArrayList<>(), 10);
        calculator.addCoordinate(new LatLng(0, 0), 100);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_SEND,
                calculator.getAction());
    }

    @Test
    public void testMultipleAddsWithOnceSendAlwaysSend() {
        // Once we are set to send, we always go with send.
        // Set to inside
        double threshold = 100;
        CbSendMessageCalculator calculator = createCalculator(threshold, mSquare);

        // Inside
        calculator.addCoordinate(mGooglePlex, threshold + 2000);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_SEND, calculator.getAction());

        // Outside
        calculator.addCoordinate(addSouth(addEast(mGooglePlex, -2000), -500), threshold + 10);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_SEND, calculator.getAction());

        // Ambiguous
        calculator.addCoordinate(addSouth(addEast(mGooglePlex, 1040), 1040), threshold + 50);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_SEND, calculator.getAction());
    }

    @Test
    public void testMultipleAddsWithDontSendThenSend() {
        double threshold = 100;
        CbSendMessageCalculator calculator = createCalculator(threshold, mSquare);

        // Outside
        calculator.addCoordinate(addSouth(addEast(mGooglePlex, -2000), -500), threshold + 10);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_DONT_SEND, calculator.getAction());

        // Ambiguous
        calculator.addCoordinate(addSouth(addEast(mGooglePlex, 1040), 1040), threshold + 50);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_DONT_SEND, calculator.getAction());

        // Outside again
        calculator.addCoordinate(addSouth(addEast(mGooglePlex, -2000), -500), threshold + 10);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_DONT_SEND, calculator.getAction());

        // Inside
        calculator.addCoordinate(mGooglePlex, threshold - 50);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_SEND, calculator.getAction());
    }

    @Test
    public void testMultipleAddsWithAmbiguousToDontSend() {
        double threshold = 100;
        CbSendMessageCalculator calculator = createCalculator(threshold, mSquare);

        // Ambiguous
        calculator.addCoordinate(addSouth(addEast(mGooglePlex, 1040), 1040), threshold + 50);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_AMBIGUOUS, calculator.getAction());

        // Ambiguous again
        calculator.addCoordinate(addSouth(addEast(mGooglePlex, 1040), 1040), threshold + 55);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_AMBIGUOUS, calculator.getAction());

        // Outside
        calculator.addCoordinate(addSouth(addEast(mGooglePlex, -2000), -500), threshold + 10);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_DONT_SEND, calculator.getAction());

        // Inside
        calculator.addCoordinate(mGooglePlex, threshold);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_SEND, calculator.getAction());
    }

    @Test
    public void testMultipleAddsAndDoNewWayFalseWithAmbiguousToDontSend() {
        double threshold = 100;

        //Testing with the geo fence calculation false
        doReturn(false).when(mMockedResources)
                .getBoolean(R.bool.use_new_geo_fence_calculation);
        CbSendMessageCalculator calculator = createCalculator(threshold, mSquare);

        // Ambiguous
        calculator.addCoordinate(addSouth(addEast(mGooglePlex, 1040), 1040), threshold + 50);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_DONT_SEND, calculator.getAction());

        // Ambiguous again
        calculator.addCoordinate(addSouth(addEast(mGooglePlex, 1040), 1040), threshold + 55);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_DONT_SEND, calculator.getAction());

        // Outside
        calculator.addCoordinate(addSouth(addEast(mGooglePlex, -2000), -500), threshold + 10);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_DONT_SEND, calculator.getAction());

        // Inside
        calculator.addCoordinate(mGooglePlex, threshold);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_SEND, calculator.getAction());
    }

    @Test public void testThreshold() {
        //Testing with the geo fence calculation false
        doReturn(1000).when(mMockedResources)
                .getInteger(R.integer.geo_fence_threshold);

        List<Geometry> l = new ArrayList<>();
        l.add(mCircleFarAway);
        CbSendMessageCalculator calculator = new CbSendMessageCalculator(mMockedContext, l);
        assertEquals(calculator.getThreshold(), 1000.0);
    }

    @Test
    public void testManyGeosWithInsideAndOutside() {
        double threshold = 100;
        CbSendMessageCalculator calcOutsideInside =
                createCalculator(threshold, mCircleFarAway, mSquare);

        // Inside
        calcOutsideInside.addCoordinate(mGooglePlex, threshold);
        assertEquals(CbSendMessageCalculator.SEND_MESSAGE_ACTION_SEND,
                calcOutsideInside.getAction());
    }

    private CbSendMessageCalculator createCalculator(double threshold,
            Geometry geo, Geometry... geos) {
        List<Geometry> list = new ArrayList<>(Arrays.asList(geos));
        list.add(geo);
        return new CbSendMessageCalculator(mMockedContext, list, threshold);
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

    private Polygon createPolygon(LatLng... latLngs) {
        return new Polygon(Arrays.stream(latLngs).collect(Collectors.toList()));
    }
}
