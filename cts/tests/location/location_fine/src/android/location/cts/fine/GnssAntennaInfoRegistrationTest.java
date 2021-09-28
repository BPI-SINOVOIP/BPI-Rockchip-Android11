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

package android.location.cts.fine;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.location.GnssAntennaInfo;
import android.location.LocationManager;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/**
 * End to end test of GNSS Antenna Info. Test first attempts to register a GNSS Antenna Info
 * Callback and then sleeps for a timeout period. The callback status is then checked. If the
 * status is not STATUS_READY, the test is skipped. Otherwise, the test proceeds. We verify that
 * the callback has been called and has received at least GnssAntennaInfo object.
 */
@RunWith(AndroidJUnit4.class)
public class GnssAntennaInfoRegistrationTest {

    private static final String TAG = "GnssAntennaInfoCallbackTest";

    private static final int ANTENNA_INFO_TIMEOUT_SEC = 10;

    private LocationManager mManager;
    private Context mContext;
    CountDownLatch mAntennaInfoReciept = new CountDownLatch(1);

    @Before
    public void setUp() throws Exception {

        mContext = ApplicationProvider.getApplicationContext();
        mManager = mContext.getSystemService(LocationManager.class);

        assertThat(mManager).isNotNull();
    }

    @After
    public void tearDown() throws Exception {
    }

    @Test
    public void testGnssAntennaInfoCallbackRegistration() {
        // TODO(skz): check that version code is greater than R

        if(!mManager.getGnssCapabilities().hasGnssAntennaInfo()) {
            // GnssAntennaInfo is not supported
            return;
        }

        TestGnssAntennaInfoListener listener = new TestGnssAntennaInfoListener();

        mManager.registerAntennaInfoListener(Executors.newSingleThreadExecutor(), listener);
        try {
            mAntennaInfoReciept.await(ANTENNA_INFO_TIMEOUT_SEC, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Log.e(TAG, "Test was interrupted.");
        }

        listener.verifyRegistration();

        mManager.unregisterAntennaInfoListener(listener);
    }

    private class TestGnssAntennaInfoListener implements GnssAntennaInfo.Listener {
        private boolean receivedAntennaInfo = false;
        private int numResults = 0;

        @Override
        public void onGnssAntennaInfoReceived(@NonNull List<GnssAntennaInfo> gnssAntennaInfos) {
            receivedAntennaInfo = true;
            numResults = gnssAntennaInfos.size();
            mAntennaInfoReciept.countDown();

            for (GnssAntennaInfo gnssAntennaInfo: gnssAntennaInfos) {
                Log.d(TAG, gnssAntennaInfo.toString() + "\n");
            }
        }

        public void verifyRegistration() {
            assertTrue(receivedAntennaInfo);
            assertThat(numResults).isGreaterThan(0);
        }
    }
}
