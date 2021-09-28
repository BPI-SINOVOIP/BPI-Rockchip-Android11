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


import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.location.Geocoder;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.util.Locale;

@RunWith(AndroidJUnit4.class)
public class GeocoderTest {

    private static final int MAX_NUM_RETRIES = 2;
    private static final int TIME_BETWEEN_RETRIES_MS = 2000;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    public void testConstructor() {
        new Geocoder(mContext);

        new Geocoder(mContext, Locale.ENGLISH);

        try {
            new Geocoder(mContext, null);
            fail("should throw NullPointerException.");
        } catch (NullPointerException e) {
            // expected.
        }
    }

    @Test
    public void testIsPresent() {
        if (isServiceMissing()) {
            assertFalse(Geocoder.isPresent());
        } else {
            assertTrue(Geocoder.isPresent());
        }
    }

    @Test
    public void testGetFromLocation() throws IOException, InterruptedException {
        Geocoder geocoder = new Geocoder(mContext);

        // There is no guarantee that geocoder.getFromLocation returns accurate results
        // Thus only test that calling the method with valid arguments doesn't produce
        // an unexpected exception
        // Note: there is a risk this test will fail if device under test does not have
        // a network connection. This is why we try the geocode 5 times if it fails due
        // to a network error.
        int numRetries = 0;
        while (numRetries < MAX_NUM_RETRIES) {
            try {
                geocoder.getFromLocation(60, 30, 5);
                break;
            } catch (IOException e) {
                Thread.sleep(TIME_BETWEEN_RETRIES_MS);
                numRetries++;
            }
        }
        if (numRetries >= MAX_NUM_RETRIES) {
            fail("Failed to geocode location " + MAX_NUM_RETRIES + " times.");
        }


        try {
            // latitude is less than -90
            geocoder.getFromLocation(-91, 30, 5);
            fail("should throw IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            // pass
        }

        try {
            // latitude is greater than 90
            geocoder.getFromLocation(91, 30, 5);
            fail("should throw IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            // pass
        }

        try {
            // longitude is less than -180
            geocoder.getFromLocation(10, -181, 5);
            fail("should throw IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            // pass
        }

        try {
            // longitude is greater than 180
            geocoder.getFromLocation(10, 181, 5);
            fail("should throw IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            // pass
        }
    }

    @Test
    public void testGetFromLocationName() throws IOException, InterruptedException {
        Geocoder geocoder = new Geocoder(mContext, Locale.US);

        // There is no guarantee that geocoder.getFromLocationName returns accurate results.
        // Thus only test that calling the method with valid arguments doesn't produce
        // an unexpected exception
        // Note: there is a risk this test will fail if device under test does not have
        // a network connection. This is why we try the geocode 5 times if it fails due
        // to a network error.
        int numRetries = 0;
        while (numRetries < MAX_NUM_RETRIES) {
            try {
                geocoder.getFromLocationName("Dalvik,Iceland", 5);
                break;
            } catch (IOException e) {
                Thread.sleep(TIME_BETWEEN_RETRIES_MS);
                numRetries++;
            }
        }
        if (numRetries >= MAX_NUM_RETRIES) {
            fail("Failed to geocode location name " + MAX_NUM_RETRIES + " times.");
        }

        try {
            geocoder.getFromLocationName(null, 5);
            fail("should throw IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            // pass
        }

        try {
            geocoder.getFromLocationName("Beijing", 5, -91, 100, 45, 130);
            fail("should throw IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            // pass
        }

        try {
            geocoder.getFromLocationName("Beijing", 5, 25, 190, 45, 130);
            fail("should throw IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            // pass
        }

        try {
            geocoder.getFromLocationName("Beijing", 5, 25, 100, 91, 130);
            fail("should throw IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            // pass
        }

        try {
            geocoder.getFromLocationName("Beijing", 5, 25, 100, 45, -181);
            fail("should throw IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            // pass
        }
    }

    private boolean isServiceMissing() {
        PackageManager pm = mContext.getPackageManager();

        final Intent intent = new Intent("com.android.location.service.GeocodeProvider");
        final int flags = PackageManager.MATCH_DIRECT_BOOT_AWARE
                | PackageManager.MATCH_DIRECT_BOOT_UNAWARE;
        return pm.queryIntentServices(intent, flags).isEmpty();
    }
}
