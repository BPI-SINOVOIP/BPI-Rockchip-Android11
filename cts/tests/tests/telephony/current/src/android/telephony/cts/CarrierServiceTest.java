/*
 * Copyright (C) 2015 The Android Open Source Project
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

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.PersistableBundle;
import android.service.carrier.CarrierIdentifier;
import android.service.carrier.CarrierService;
import android.telephony.TelephonyManager;
import android.test.ServiceTestCase;
import android.util.Log;

public class CarrierServiceTest extends ServiceTestCase<CarrierServiceTest.TestCarrierService> {
    private static final String TAG = CarrierServiceTest.class.getSimpleName();

    private boolean mHasCellular;

    public CarrierServiceTest() { super(TestCarrierService.class); }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mHasCellular = hasCellular();
        if (!mHasCellular) {
            Log.e(TAG, "No cellular support, all tests will be skipped.");
        }
    }

    private static boolean hasCellular() {
        PackageManager packageManager = getInstrumentation().getContext().getPackageManager();
        TelephonyManager telephonyManager =
                getInstrumentation().getContext().getSystemService(TelephonyManager.class);
        return packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)
                && telephonyManager.getPhoneCount() > 0;
    }

    public void testNotifyCarrierNetworkChange_true() {
        if (!mHasCellular) return;

        notifyCarrierNetworkChange(true);
    }

    public void testNotifyCarrierNetworkChange_false() {
        if (!mHasCellular) return;

        notifyCarrierNetworkChange(false);
    }

    private void notifyCarrierNetworkChange(boolean active) {
        Intent intent = new Intent(getContext(), TestCarrierService.class);
        startService(intent);

        try {
            getService().notifyCarrierNetworkChange(active);
            fail("Expected SecurityException for notifyCarrierNetworkChange(" + active + ")");
        } catch (SecurityException e) { /* Expected */ }
    }

    public static class TestCarrierService extends CarrierService {
        @Override
        public PersistableBundle onLoadConfig(CarrierIdentifier id) { return null; }
    }
}
