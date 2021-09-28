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

package android.car.apitest;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;
import static com.android.compatibility.common.util.TestUtils.BooleanSupplierWithThrow;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import android.car.Car;
import android.content.ComponentName;
import android.content.Context;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.Looper;
import android.os.PowerManager;
import android.os.SystemClock;
import android.util.Log;

import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

abstract class CarApiTestBase {

    private static final String TAG = CarApiTestBase.class.getSimpleName();

    protected static final long DEFAULT_WAIT_TIMEOUT_MS = 1_000;

    /**
     * Constant used to wait blindly, when there is no condition that can be checked.
     */
    private static final int SUSPEND_TIMEOUT_MS = 5_000;
    /**
     * How long to sleep (multiple times) while waiting for a condition.
     */
    private static final int SMALL_NAP_MS = 100;

    protected static final Context sContext = InstrumentationRegistry.getInstrumentation()
            .getTargetContext();

    private Car mCar;

    protected final DefaultServiceConnectionListener mConnectionListener =
            new DefaultServiceConnectionListener();

    @Before
    public final void connectToCar() throws Exception {
        mCar = Car.createCar(getContext(), mConnectionListener);
        mCar.connect();
        mConnectionListener.waitForConnection(DEFAULT_WAIT_TIMEOUT_MS);
    }

    @After
    public final void disconnectFromCar() throws Exception {
        mCar.disconnect();
    }

    protected Car getCar() {
        return mCar;
    }

    protected final Context getContext() {
        return sContext;
    }

    protected static void assertMainThread() {
        assertThat(Looper.getMainLooper().isCurrentThread()).isTrue();
    }

    protected static final class DefaultServiceConnectionListener implements ServiceConnection {
        private final Semaphore mConnectionWait = new Semaphore(0);

        public void waitForConnection(long timeoutMs) throws InterruptedException {
            mConnectionWait.tryAcquire(timeoutMs, TimeUnit.MILLISECONDS);
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            assertMainThread();
            mConnectionWait.release();
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            assertMainThread();
        }
    }

    protected static void suspendToRamAndResume() throws Exception {
        Log.d(TAG, "Emulate suspend to RAM and resume");
        PowerManager powerManager = sContext.getSystemService(PowerManager.class);
        runShellCommand("cmd car_service suspend");
        // Check for suspend success
        waitUntil("Suspsend is not successful",
                SUSPEND_TIMEOUT_MS, () -> !powerManager.isScreenOn());

        // Force turn off garage mode
        runShellCommand("cmd car_service garage-mode off");
        runShellCommand("cmd car_service resume");
    }

    protected static boolean waitUntil(String msg, long timeoutMs,
            BooleanSupplierWithThrow condition) {
        long deadline = SystemClock.elapsedRealtime() + timeoutMs;
        do {
            try {
                if (condition.getAsBoolean()) {
                    return true;
                }
            } catch (Exception e) {
                Log.e(TAG, "Exception in waitUntil: " + msg);
                throw new RuntimeException(e);
            }
            SystemClock.sleep(SMALL_NAP_MS);
        } while (SystemClock.elapsedRealtime() < deadline);

        fail(msg + " after: " + timeoutMs + "ms");
        return false;
    }
}
