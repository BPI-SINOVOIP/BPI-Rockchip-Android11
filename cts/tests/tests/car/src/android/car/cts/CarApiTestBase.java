/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.car.cts;

import static org.junit.Assert.assertTrue;

import android.car.Car;
import android.car.FuelType;
import android.car.PortLocationType;
import android.content.ComponentName;
import android.content.Context;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.Looper;

import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.RequiredFeatureRule;

import org.junit.After;
import org.junit.ClassRule;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

public abstract class CarApiTestBase {
    @ClassRule
    public static final RequiredFeatureRule sRequiredFeatureRule = new RequiredFeatureRule(
            PackageManager.FEATURE_AUTOMOTIVE);

    protected static final long DEFAULT_WAIT_TIMEOUT_MS = 1000;

    private Car mCar;

    // Enums in FuelType
    final static List<Integer> EXPECTED_FUEL_TYPES =
            Arrays.asList(FuelType.UNKNOWN, FuelType.UNLEADED, FuelType.LEADED, FuelType.DIESEL_1,
                    FuelType.DIESEL_2, FuelType.BIODIESEL, FuelType.E85, FuelType.LPG, FuelType.CNG,
                    FuelType.LNG, FuelType.ELECTRIC, FuelType.HYDROGEN, FuelType.OTHER);
    // Enums in PortLocationType
    final static List<Integer> EXPECTED_PORT_LOCATIONS =
            Arrays.asList(PortLocationType.UNKNOWN, PortLocationType.FRONT_LEFT,
                    PortLocationType.FRONT_RIGHT, PortLocationType.REAR_RIGHT,
                    PortLocationType.REAR_LEFT, PortLocationType.FRONT, PortLocationType.REAR);

    private final DefaultServiceConnectionListener mConnectionListener =
            new DefaultServiceConnectionListener();

    protected static final Context sContext = InstrumentationRegistry.getInstrumentation()
            .getContext();

    protected void assertMainThread() {
        assertTrue(Looper.getMainLooper().isCurrentThread());
    }

    protected void setUp() throws Exception {
        mCar = Car.createCar(sContext, mConnectionListener, null);
        mCar.connect();
        mConnectionListener.waitForConnection(DEFAULT_WAIT_TIMEOUT_MS);
    }

    @After
    public void disconnectCar() throws Exception {
        if (mCar != null) {
            mCar.disconnect();
        }
    }

    protected synchronized Car getCar() {
        return mCar;
    }

    protected class DefaultServiceConnectionListener implements ServiceConnection {
        private final Semaphore mConnectionWait = new Semaphore(0);

        public void waitForConnection(long timeoutMs) throws InterruptedException {
            mConnectionWait.tryAcquire(timeoutMs, TimeUnit.MILLISECONDS);
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            assertMainThread();
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            assertMainThread();
            mConnectionWait.release();
        }
    }
}
