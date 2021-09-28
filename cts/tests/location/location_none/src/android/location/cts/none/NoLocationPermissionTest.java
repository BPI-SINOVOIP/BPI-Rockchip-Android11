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

package android.location.cts.none;

import static android.content.pm.PackageManager.FEATURE_TELEPHONY;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.location.Criteria;
import android.location.LocationManager;
import android.location.cts.common.LocationListenerCapture;
import android.location.cts.common.LocationPendingIntentCapture;
import android.os.Looper;
import android.telephony.CellInfo;
import android.telephony.PhoneStateListener;
import android.telephony.TelephonyManager;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;


@RunWith(AndroidJUnit4.class)
public class NoLocationPermissionTest {

    private Context mContext;
    private LocationManager mLocationManager;

    @Before
    public void setUp() throws Exception {
        mContext = ApplicationProvider.getApplicationContext();
        mLocationManager = mContext.getSystemService(LocationManager.class);

        assertNotNull(mLocationManager);
    }

    @SuppressWarnings("deprecation")
    @Test
    public void testGetCellLocation() {
        if (!mContext.getPackageManager().hasSystemFeature(FEATURE_TELEPHONY)) {
            return;
        }

        TelephonyManager telephonyManager = mContext.getSystemService(TelephonyManager.class);
        assertNotNull(telephonyManager);

        try {
            telephonyManager.getCellLocation();
            fail("Should throw SecurityException");
        } catch (SecurityException e) {
            // expected
        }
    }

    @Test
    public void testGetAllCellInfo() {
        if (!mContext.getPackageManager().hasSystemFeature(FEATURE_TELEPHONY)) {
            return;
        }

        TelephonyManager telephonyManager = mContext.getSystemService(TelephonyManager.class);
        assertNotNull(telephonyManager);

        try {
            telephonyManager.getAllCellInfo();
            fail("Should throw SecurityException");
        } catch (SecurityException e) {
            // expected
        }
    }

    @Test
    public void testListenCellLocation() {
        if (!mContext.getPackageManager().hasSystemFeature(FEATURE_TELEPHONY)) {
            return;
        }

        TelephonyManager telephonyManager = mContext.getSystemService(TelephonyManager.class);
        assertNotNull(telephonyManager);

        try {
            telephonyManager.listen(new PhoneStateListener(Runnable::run),
                    PhoneStateListener.LISTEN_CELL_LOCATION);
            fail("Should throw SecurityException");
        } catch (SecurityException e) {
            // expected
        }
    }

    @Test
    public void testRequestCellInfoUpdate() {
        if (!mContext.getPackageManager().hasSystemFeature(FEATURE_TELEPHONY)) {
            return;
        }

        TelephonyManager telephonyManager = mContext.getSystemService(TelephonyManager.class);
        assertNotNull(telephonyManager);

        try {
            telephonyManager.requestCellInfoUpdate(Runnable::run,
                    new TelephonyManager.CellInfoCallback() {
                        @Override
                        public void onCellInfo(List<CellInfo> cellInfos) {
                        }
                    });
            fail("Should throw SecurityException");
        } catch (SecurityException e) {
            // expected
        }
    }

    @Test
    public void testRequestLocationUpdates() {
        for (String provider : mLocationManager.getAllProviders()) {
            try (LocationListenerCapture capture = new LocationListenerCapture(mContext)) {
                mLocationManager.requestLocationUpdates(provider, 0, 0, capture,
                        Looper.getMainLooper());
                fail("Should throw SecurityException for provider " + provider);
            } catch (SecurityException e) {
                // expected
            }

            try (LocationListenerCapture capture = new LocationListenerCapture(mContext)) {
                mLocationManager.requestLocationUpdates(provider, 0, 0, Runnable::run, capture);
                fail("Should throw SecurityException for provider " + provider);
            } catch (SecurityException e) {
                // expected
            }

            try (LocationPendingIntentCapture capture = new LocationPendingIntentCapture(
                    mContext)) {
                mLocationManager.requestLocationUpdates(provider, 0, 0, capture.getPendingIntent());
                fail("Should throw SecurityException for provider " + provider);
            } catch (SecurityException e) {
                // expected
            }
        }
    }

    @Test
    public void testAddProximityAlert() {
        PendingIntent pendingIntent = PendingIntent.getBroadcast(mContext,
                0, new Intent("action"), PendingIntent.FLAG_ONE_SHOT);
        try {
            mLocationManager.addProximityAlert(0, 0, 100, -1, pendingIntent);
            fail("Should throw SecurityException");
        } catch (SecurityException e) {
            // expected
        } finally {
            pendingIntent.cancel();
        }
    }

    @Test
    public void testGetLastKnownLocation() {
        for (String provider : mLocationManager.getAllProviders()) {
            try {
                mLocationManager.getLastKnownLocation(provider);
                fail("Should throw SecurityException for provider " + provider);
            } catch (SecurityException e) {
                // expected
            }
        }
    }

    @Test
    public void testGetProvider() {
        for (String provider : mLocationManager.getAllProviders()) {
            mLocationManager.getProvider(provider);
        }
    }

    @Test
    public void testIsProviderEnabled() {
        for (String provider : mLocationManager.getAllProviders()) {
            mLocationManager.isProviderEnabled(provider);
        }
    }

    @Test
    public void testAddTestProvider() {
        for (String provider : mLocationManager.getAllProviders()) {
            try {
                mLocationManager.addTestProvider(
                        provider,
                        true,
                        true,
                        true,
                        true,
                        true,
                        true,
                        true,
                        Criteria.POWER_LOW,
                        Criteria.ACCURACY_FINE);
                fail("Should throw SecurityException for provider " + provider);
            } catch (SecurityException e) {
                // expected
            }
        }
    }

    @Test
    public void testRemoveTestProvider() {
        for (String provider : mLocationManager.getAllProviders()) {
            try {
                mLocationManager.removeTestProvider(provider);
                fail("Should throw SecurityException for provider " + provider);
            } catch (SecurityException e) {
                // expected
            }
        }
    }
}
