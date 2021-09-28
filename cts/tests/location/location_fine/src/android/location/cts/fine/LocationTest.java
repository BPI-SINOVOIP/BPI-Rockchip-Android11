/*
 * Copyright (C) 2008 The Android Open Source Project
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
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.location.Location;
import android.os.Bundle;
import android.os.Parcel;
import android.util.StringBuilderPrinter;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.text.DecimalFormat;

@RunWith(AndroidJUnit4.class)
public class LocationTest {

    private static final float DELTA = 0.1f;
    private final float TEST_ACCURACY = 1.0f;
    private final float TEST_VERTICAL_ACCURACY = 2.0f;
    private final float TEST_SPEED_ACCURACY = 3.0f;
    private final float TEST_BEARING_ACCURACY = 4.0f;
    private final double TEST_ALTITUDE = 1.0;
    private final double TEST_LATITUDE = 50;
    private final float TEST_BEARING = 1.0f;
    private final double TEST_LONGITUDE = 20;
    private final float TEST_SPEED = 5.0f;
    private final long TEST_TIME = 100;
    private final String TEST_PROVIDER = "LocationProvider";
    private final String TEST_KEY1NAME = "key1";
    private final String TEST_KEY2NAME = "key2";
    private final boolean TEST_KEY1VALUE = false;
    private final byte TEST_KEY2VALUE = 10;

    @Test
    public void testConstructor() {
        new Location("LocationProvider");

        Location l = createTestLocation();
        Location location = new Location(l);
        assertTestLocation(location);

        try {
            new Location((Location) null);
            fail("should throw NullPointerException");
        } catch (NullPointerException e) {
            // expected.
        }
    }

    @Test
    public void testDump() {
        StringBuilder sb = new StringBuilder();
        StringBuilderPrinter printer = new StringBuilderPrinter(sb);
        Location location = new Location("LocationProvider");
        location.dump(printer, "");
        assertNotNull(sb.toString());
    }

    @Test
    public void testBearingTo() {
        Location location = new Location("");
        Location dest = new Location("");

        // set the location to Beijing
        location.setLatitude(39.9);
        location.setLongitude(116.4);
        // set the destination to Chengdu
        dest.setLatitude(30.7);
        dest.setLongitude(104.1);
        assertEquals(-128.66, location.bearingTo(dest), DELTA);

        float bearing;
        Location zeroLocation = new Location("");
        zeroLocation.setLatitude(0);
        zeroLocation.setLongitude(0);

        Location testLocation = new Location("");
        testLocation.setLatitude(0);
        testLocation.setLongitude(150);

        bearing = zeroLocation.bearingTo(zeroLocation);
        assertEquals(0.0f, bearing, DELTA);

        bearing = zeroLocation.bearingTo(testLocation);
        assertEquals(90.0f, bearing, DELTA);

        testLocation.setLatitude(90);
        testLocation.setLongitude(0);
        bearing = zeroLocation.bearingTo(testLocation);
        assertEquals(0.0f, bearing, DELTA);

        try {
            location.bearingTo(null);
            fail("should throw NullPointerException");
        } catch (NullPointerException e) {
            // expected.
        }
    }

    @Test
    public void testConvert_CoordinateToRepresentation() {
        DecimalFormat df = new DecimalFormat("###.#####");
        String result;

        result = Location.convert(-80.0, Location.FORMAT_DEGREES);
        assertEquals("-" + df.format(80.0), result);

        result = Location.convert(-80.085, Location.FORMAT_MINUTES);
        assertEquals("-80:" + df.format(5.1), result);

        result = Location.convert(-80, Location.FORMAT_MINUTES);
        assertEquals("-80:" + df.format(0), result);

        result = Location.convert(-80.075, Location.FORMAT_MINUTES);
        assertEquals("-80:" + df.format(4.5), result);

        result = Location.convert(-80.075, Location.FORMAT_DEGREES);
        assertEquals("-" + df.format(80.075), result);

        result = Location.convert(-80.075, Location.FORMAT_SECONDS);
        assertEquals("-80:4:30", result);

        try {
            Location.convert(-181, Location.FORMAT_SECONDS);
            fail("should throw IllegalArgumentException.");
        } catch (IllegalArgumentException e) {
            // expected.
        }

        try {
            Location.convert(181, Location.FORMAT_SECONDS);
            fail("should throw IllegalArgumentException.");
        } catch (IllegalArgumentException e) {
            // expected.
        }

        try {
            Location.convert(-80.075, -1);
            fail("should throw IllegalArgumentException.");
        } catch (IllegalArgumentException e) {
            // expected.
        }
    }

    @Test
    public void testConvert_RepresentationToCoordinate() {
        double result;

        result = Location.convert("-80.075");
        assertEquals(-80.075, result, DELTA);

        result = Location.convert("-80:05.10000");
        assertEquals(-80.085, result, DELTA);

        result = Location.convert("-80:04:03.00000");
        assertEquals(-80.0675, result, DELTA);

        result = Location.convert("-80:4:3");
        assertEquals(-80.0675, result, DELTA);

        try {
            Location.convert(null);
            fail("should throw NullPointerException.");
        } catch (NullPointerException e){
            // expected.
        }

        try {
            Location.convert(":");
            fail("should throw IllegalArgumentException.");
        } catch (IllegalArgumentException e){
            // expected.
        }

        try {
            Location.convert("190:4:3");
            fail("should throw IllegalArgumentException.");
        } catch (IllegalArgumentException e){
            // expected.
        }

        try {
            Location.convert("-80:60:3");
            fail("should throw IllegalArgumentException.");
        } catch (IllegalArgumentException e){
            // expected.
        }

        try {
            Location.convert("-80:4:60");
            fail("should throw IllegalArgumentException.");
        } catch (IllegalArgumentException e){
            // expected.
        }
    }

    @Test
    public void testDescribeContents() {
        Location location = new Location("");
        location.describeContents();
    }

    @Test
    public void testDistanceBetween() {
        float[] result = new float[3];
        Location.distanceBetween(0, 0, 0, 0, result);
        assertEquals(0.0, result[0], DELTA);
        assertEquals(0.0, result[1], DELTA);
        assertEquals(0.0, result[2], DELTA);

        Location.distanceBetween(20, 30, -40, 140, result);
        assertEquals(1.3094936E7, result[0], 1);
        assertEquals(125.4538, result[1], DELTA);
        assertEquals(93.3971, result[2], DELTA);

        try {
            Location.distanceBetween(20, 30, -40, 140, null);
            fail("should throw IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            // expected.
        }

        try {
            Location.distanceBetween(20, 30, -40, 140, new float[0]);
            fail("should throw IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            // expected.
        }
    }

    @Test
    public void testDistanceTo() {
        float distance;
        Location zeroLocation = new Location("");
        zeroLocation.setLatitude(0);
        zeroLocation.setLongitude(0);

        Location testLocation = new Location("");
        testLocation.setLatitude(30);
        testLocation.setLongitude(50);

        distance = zeroLocation.distanceTo(zeroLocation);
        assertEquals(0, distance, DELTA);

        distance = zeroLocation.distanceTo(testLocation);
        assertEquals(6244139.0, distance, 1);
    }

    @Test
    public void testAccessAccuracy() {
        Location location = new Location("");
        assertFalse(location.hasAccuracy());

        location.setAccuracy(1.0f);
        assertEquals(1.0, location.getAccuracy(), DELTA);
        assertTrue(location.hasAccuracy());
    }

    @Test
    public void testAccessVerticalAccuracy() {
        Location location = new Location("");
        assertFalse(location.hasVerticalAccuracy());

        location.setVerticalAccuracyMeters(1.0f);
        assertEquals(1.0, location.getVerticalAccuracyMeters(), DELTA);
        assertTrue(location.hasVerticalAccuracy());
    }

    @Test
    public void testAccessSpeedAccuracy() {
        Location location = new Location("");
        assertFalse(location.hasSpeedAccuracy());

        location.setSpeedAccuracyMetersPerSecond(1.0f);
        assertEquals(1.0, location.getSpeedAccuracyMetersPerSecond(), DELTA);
        assertTrue(location.hasSpeedAccuracy());
    }

    @Test
    public void testAccessBearingAccuracy() {
        Location location = new Location("");
        assertFalse(location.hasBearingAccuracy());

        location.setBearingAccuracyDegrees(1.0f);
        assertEquals(1.0, location.getBearingAccuracyDegrees(), DELTA);
        assertTrue(location.hasBearingAccuracy());
    }


    @Test
    public void testAccessAltitude() {
        Location location = new Location("");
        assertFalse(location.hasAltitude());

        location.setAltitude(1.0);
        assertEquals(1.0, location.getAltitude(), DELTA);
        assertTrue(location.hasAltitude());
    }

    @Test
    public void testAccessBearing() {
        Location location = new Location("");
        assertFalse(location.hasBearing());

        location.setBearing(1.0f);
        assertEquals(1.0, location.getBearing(), DELTA);
        assertTrue(location.hasBearing());

        location.setBearing(371.0f);
        assertEquals(11.0, location.getBearing(), DELTA);
        assertTrue(location.hasBearing());

        location.setBearing(-361.0f);
        assertEquals(359.0, location.getBearing(), DELTA);
        assertTrue(location.hasBearing());
    }

    @Test
    public void testAccessExtras() {
        Location location = createTestLocation();

        assertTestBundle(location.getExtras());

        location.setExtras(null);
        assertNull(location.getExtras());
    }

    @Test
    public void testAccessLatitude() {
        Location location = new Location("");

        location.setLatitude(0);
        assertEquals(0, location.getLatitude(), DELTA);

        location.setLatitude(90);
        assertEquals(90, location.getLatitude(), DELTA);

        location.setLatitude(-90);
        assertEquals(-90, location.getLatitude(), DELTA);
    }

    @Test
    public void testAccessLongitude() {
        Location location = new Location("");

        location.setLongitude(0);
        assertEquals(0, location.getLongitude(), DELTA);

        location.setLongitude(180);
        assertEquals(180, location.getLongitude(), DELTA);

        location.setLongitude(-180);
        assertEquals(-180, location.getLongitude(), DELTA);
    }

    @Test
    public void testAccessProvider() {
        Location location = new Location("");

        String provider = "Location Provider";
        location.setProvider(provider);
        assertEquals(provider, location.getProvider());

        location.setProvider(null);
        assertNull(location.getProvider());
    }

    @Test
    public void testAccessSpeed() {
        Location location = new Location("");
        assertFalse(location.hasSpeed());

        location.setSpeed(234.0045f);
        assertEquals(234.0045, location.getSpeed(), DELTA);
        assertTrue(location.hasSpeed());
    }

    @Test
    public void testAccessTime() {
        Location location = new Location("");

        location.setTime(0);
        assertEquals(0, location.getTime());

        location.setTime(Long.MAX_VALUE);
        assertEquals(Long.MAX_VALUE, location.getTime());

        location.setTime(12000);
        assertEquals(12000, location.getTime());
    }

    @Test
    public void testAccessElapsedRealtime() {
        Location location = new Location("");

        location.setElapsedRealtimeNanos(0);
        assertEquals(0, location.getElapsedRealtimeNanos());

        location.setElapsedRealtimeNanos(Long.MAX_VALUE);
        assertEquals(Long.MAX_VALUE, location.getElapsedRealtimeNanos());

        location.setElapsedRealtimeNanos(12000);
        assertEquals(12000, location.getElapsedRealtimeNanos());
    }

    @Test
    public void testAccessElapsedRealtimeUncertaintyNanos() {
        Location location = new Location("");
        assertFalse(location.hasElapsedRealtimeUncertaintyNanos());
        assertEquals(0.0, location.getElapsedRealtimeUncertaintyNanos(), DELTA);

        location.setElapsedRealtimeUncertaintyNanos(12000.0);
        assertEquals(12000.0, location.getElapsedRealtimeUncertaintyNanos(), DELTA);
        assertTrue(location.hasElapsedRealtimeUncertaintyNanos());

        location.reset();
        assertFalse(location.hasElapsedRealtimeUncertaintyNanos());
        assertEquals(0.0, location.getElapsedRealtimeUncertaintyNanos(), DELTA);
    }

    @Test
    public void testSet() {
        Location location = new Location("");

        Location loc = createTestLocation();

        location.set(loc);
        assertTestLocation(location);

        location.reset();
        assertNull(location.getProvider());
        assertEquals(0, location.getTime());
        assertEquals(0, location.getLatitude(), DELTA);
        assertEquals(0, location.getLongitude(), DELTA);
        assertEquals(0, location.getAltitude(), DELTA);
        assertFalse(location.hasAltitude());
        assertEquals(0, location.getSpeed(), DELTA);
        assertFalse(location.hasSpeed());
        assertEquals(0, location.getBearing(), DELTA);
        assertFalse(location.hasBearing());
        assertEquals(0, location.getAccuracy(), DELTA);
        assertFalse(location.hasAccuracy());

        assertEquals(0, location.getVerticalAccuracyMeters(), DELTA);
        assertEquals(0, location.getSpeedAccuracyMetersPerSecond(), DELTA);
        assertEquals(0, location.getBearingAccuracyDegrees(), DELTA);

        assertFalse(location.hasVerticalAccuracy());
        assertFalse(location.hasSpeedAccuracy());
        assertFalse(location.hasBearingAccuracy());

        assertNull(location.getExtras());
    }

    @Test
    public void testToString() {
        Location location = createTestLocation();

        assertNotNull(location.toString());
    }

    @Test
    public void testWriteToParcel() {
        Location location = createTestLocation();

        Parcel parcel = Parcel.obtain();
        location.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        Location newLocation = Location.CREATOR.createFromParcel(parcel);
        assertTestLocation(newLocation);

        parcel.recycle();
    }

    private void assertTestLocation(Location l) {
        assertNotNull(l);
        assertEquals(TEST_PROVIDER, l.getProvider());
        assertEquals(TEST_ACCURACY, l.getAccuracy(), DELTA);
        assertEquals(TEST_VERTICAL_ACCURACY, l.getVerticalAccuracyMeters(), DELTA);
        assertEquals(TEST_SPEED_ACCURACY, l.getSpeedAccuracyMetersPerSecond(), DELTA);
        assertEquals(TEST_BEARING_ACCURACY, l.getBearingAccuracyDegrees(), DELTA);
        assertEquals(TEST_ALTITUDE, l.getAltitude(), DELTA);
        assertEquals(TEST_LATITUDE, l.getLatitude(), DELTA);
        assertEquals(TEST_BEARING, l.getBearing(), DELTA);
        assertEquals(TEST_LONGITUDE, l.getLongitude(), DELTA);
        assertEquals(TEST_SPEED, l.getSpeed(), DELTA);
        assertEquals(TEST_TIME, l.getTime());
        assertTestBundle(l.getExtras());
    }

    private Location createTestLocation() {
        Location l = new Location(TEST_PROVIDER);
        l.setAccuracy(TEST_ACCURACY);
        l.setVerticalAccuracyMeters(TEST_VERTICAL_ACCURACY);
        l.setSpeedAccuracyMetersPerSecond(TEST_SPEED_ACCURACY);
        l.setBearingAccuracyDegrees(TEST_BEARING_ACCURACY);

        l.setAltitude(TEST_ALTITUDE);
        l.setLatitude(TEST_LATITUDE);
        l.setBearing(TEST_BEARING);
        l.setLongitude(TEST_LONGITUDE);
        l.setSpeed(TEST_SPEED);
        l.setTime(TEST_TIME);
        Bundle bundle = new Bundle();
        bundle.putBoolean(TEST_KEY1NAME, TEST_KEY1VALUE);
        bundle.putByte(TEST_KEY2NAME, TEST_KEY2VALUE);
        l.setExtras(bundle);

        return l;
    }

    private void assertTestBundle(Bundle bundle) {
        assertFalse(bundle.getBoolean(TEST_KEY1NAME));
        assertEquals(TEST_KEY2VALUE, bundle.getByte(TEST_KEY2NAME));
    }
}
