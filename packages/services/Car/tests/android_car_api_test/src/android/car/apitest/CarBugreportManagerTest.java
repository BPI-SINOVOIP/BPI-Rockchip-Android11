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
package android.car.apitest;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;
import static org.testng.Assert.expectThrows;

import android.Manifest;
import android.annotation.FloatRange;
import android.car.Car;
import android.car.CarBugreportManager;
import android.car.CarBugreportManager.CarBugreportManagerCallback;
import android.os.ParcelFileDescriptor;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;

import java.io.File;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
@MediumTest
public class CarBugreportManagerTest extends CarApiTestBase {
    // TODO: Use "dumpstate.dry_run" to make dumpstate faster.
    // dumpstate runs around 3 minutes on emulator on a pretty fast computer.
    private static final int BUGREPORT_TIMEOUT_MILLIS = 360_000;
    private static final int NO_ERROR = -1;

    @Rule public TestName testName = new TestName();

    private CarBugreportManager mManager;
    private FakeCarBugreportCallback mFakeCallback;
    private ParcelFileDescriptor mOutput;
    private ParcelFileDescriptor mExtraOutput;

    @Before
    public void setUp() throws Exception {
        mManager = (CarBugreportManager) getCar().getCarManager(Car.CAR_BUGREPORT_SERVICE);
        mFakeCallback = new FakeCarBugreportCallback();
        mOutput = createParcelFdInCache("bugreport", "zip");
        mExtraOutput = createParcelFdInCache("screenshot", "png");
    }

    @After
    public void tearDown() throws Exception {
        getPermissions();  // For cancelBugreport()
        mManager.cancelBugreport();
        dropPermissions();
    }

    @Test
    public void test_requestBugreport_failsWhenNoPermission() throws Exception {
        dropPermissions();

        SecurityException expected =
                expectThrows(SecurityException.class,
                        () -> mManager.requestBugreport(mOutput, mExtraOutput, mFakeCallback));
        assertThat(expected).hasMessageThat().contains(
                "nor current process has android.permission.DUMP.");
    }

    @Test
    @LargeTest
    public void test_requestBugreport_works() throws Exception {
        getPermissions();

        mManager.requestBugreport(mOutput, mExtraOutput, mFakeCallback);

        // The FDs are duped and closed in requestBugreport().
        assertFdIsClosed(mOutput);
        assertFdIsClosed(mExtraOutput);

        mFakeCallback.waitTillDoneOrTimeout(BUGREPORT_TIMEOUT_MILLIS);
        assertThat(mFakeCallback.isFinishedSuccessfully()).isEqualTo(true);
        assertThat(mFakeCallback.getReceivedProgress()).isTrue();
        // TODO: Check the contents of the zip file and the extra output.
    }

    @Test
    public void test_requestBugreport_cannotRunMultipleBugreports() throws Exception {
        getPermissions();
        FakeCarBugreportCallback callback2 = new FakeCarBugreportCallback();
        ParcelFileDescriptor output2 = createParcelFdInCache("bugreport2", "zip");
        ParcelFileDescriptor extraOutput2 = createParcelFdInCache("screenshot2", "png");

        // 1st bugreport.
        mManager.requestBugreport(mOutput, mExtraOutput, mFakeCallback);

        // 2nd bugreport.
        mManager.requestBugreport(output2, extraOutput2, callback2);

        callback2.waitTillDoneOrTimeout(BUGREPORT_TIMEOUT_MILLIS);
        assertThat(callback2.getErrorCode()).isEqualTo(
                CarBugreportManagerCallback.CAR_BUGREPORT_IN_PROGRESS);
    }

    @Test
    @LargeTest
    public void test_cancelBugreport_works() throws Exception {
        getPermissions();
        FakeCarBugreportCallback callback2 = new FakeCarBugreportCallback();
        ParcelFileDescriptor output2 = createParcelFdInCache("bugreport2", "zip");
        ParcelFileDescriptor extraOutput2 = createParcelFdInCache("screenshot2", "png");

        // 1st bugreport.
        mManager.requestBugreport(mOutput, mExtraOutput, mFakeCallback);
        mManager.cancelBugreport();

        // Allow the system to finish the bugreport cancellation, 0.5 seconds is enough.
        Thread.sleep(500);

        // 2nd bugreport must work, because 1st bugreport was cancelled.
        mManager.requestBugreport(output2, extraOutput2, callback2);

        callback2.waitTillProgressOrTimeout(BUGREPORT_TIMEOUT_MILLIS);
        assertThat(callback2.getErrorCode()).isEqualTo(NO_ERROR);
        assertThat(callback2.getReceivedProgress()).isEqualTo(true);
    }

    private static void getPermissions() {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity(Manifest.permission.DUMP);
    }

    private static void dropPermissions() {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
    }

    /** Creates a temp file in cache dir and returns file descriptor. */
    private ParcelFileDescriptor createParcelFdInCache(String prefix, String extension)
            throws Exception {
        File f = File.createTempFile(
                prefix + "_" + testName.getMethodName(), extension, getContext().getCacheDir());
        f.setReadable(true, true);
        f.setWritable(true, true);

        return ParcelFileDescriptor.open(f,
                ParcelFileDescriptor.MODE_WRITE_ONLY | ParcelFileDescriptor.MODE_APPEND);
    }

    private static void assertFdIsClosed(ParcelFileDescriptor pfd) {
        try {
            int fd = pfd.getFd();
            fail("Expected ParcelFileDescriptor argument to be closed, but got: " + fd);
        } catch (IllegalStateException expected) {
        }
    }

    private static class FakeCarBugreportCallback extends CarBugreportManagerCallback {
        private final Object mLock = new Object();
        private final CountDownLatch mEndedLatch = new CountDownLatch(1);
        private final CountDownLatch mProgressLatch = new CountDownLatch(1);
        private boolean mReceivedProgress = false;
        private int mErrorCode = NO_ERROR;

        @Override
        public void onProgress(@FloatRange(from = 0f, to = 100f) float progress) {
            synchronized (mLock) {
                mReceivedProgress = true;
            }
            mProgressLatch.countDown();
        }

        @Override
        public void onError(
                @CarBugreportManagerCallback.CarBugreportErrorCode int errorCode) {
            synchronized (mLock) {
                mErrorCode = errorCode;
            }
            mEndedLatch.countDown();
            mProgressLatch.countDown();
        }

        @Override
        public void onFinished() {
            mEndedLatch.countDown();
            mProgressLatch.countDown();
        }

        int getErrorCode() {
            synchronized (mLock) {
                return mErrorCode;
            }
        }

        boolean getReceivedProgress() {
            synchronized (mLock) {
                return mReceivedProgress;
            }
        }

        boolean isFinishedSuccessfully() {
            return mEndedLatch.getCount() == 0 && getErrorCode() == NO_ERROR;
        }

        void waitTillDoneOrTimeout(long timeoutMillis) throws InterruptedException {
            mEndedLatch.await(timeoutMillis, TimeUnit.MILLISECONDS);
        }

        void waitTillProgressOrTimeout(long timeoutMillis) throws InterruptedException {
            mProgressLatch.await(timeoutMillis, TimeUnit.MILLISECONDS);
        }
    }
}
