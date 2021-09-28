/*
 * Copyright (C) 2009 The Android Open Source Project
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
package android.telephony.sdk28.cts;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Looper;
import android.telephony.PhoneStateListener;
import android.telephony.ServiceState;
import android.telephony.TelephonyManager;
import android.util.Log;

import com.android.compatibility.common.util.TestThread;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import static androidx.test.InstrumentationRegistry.getContext;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

public class PhoneStateListenerTest {

    public static final long WAIT_TIME = 1000;
    private boolean mOnServiceStateChangedCalled;
    private TelephonyManager mTelephonyManager;
    private PhoneStateListener mListener;
    private final Object mLock = new Object();
    private boolean mHasTelephony = true;
    private static final String TAG = "android.telephony.cts.PhoneStateListenerTest";

    @Before
    public void setUp() throws Exception {
        mTelephonyManager =
                (TelephonyManager)getContext().getSystemService(Context.TELEPHONY_SERVICE);

        PackageManager packageManager = getContext().getPackageManager();
        if (packageManager != null) {
            if (!packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
                Log.d(TAG, "Some tests requiring FEATURE_TELEPHONY will be skipped");
                mHasTelephony = false;
            }
        }
    }

    @After
    public void tearDown() throws Exception {
        if (mListener != null) {
            // unregister the listener
            mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_NONE);
        }
    }

    @Test
    public void testPhoneStateListener() {
        Looper.prepare();
        new PhoneStateListener();
    }

    @Test
    public void testOnUnRegisterFollowedByRegister() throws Throwable {
        if (!mHasTelephony) {
            return;
        }

        TestThread t = new TestThread(new Runnable() {
            public void run() {
                Looper.prepare();

                mListener = new PhoneStateListener() {
                    @Override
                    public void onServiceStateChanged(ServiceState serviceState) {
                        synchronized(mLock) {
                            mOnServiceStateChangedCalled = true;
                            mLock.notify();
                        }
                    }
                };
                mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_SERVICE_STATE);

                Looper.loop();
            }
        });

        assertFalse(mOnServiceStateChangedCalled);
        t.start();

        synchronized (mLock) {
            if (!mOnServiceStateChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }
        t.checkException();
        assertTrue(mOnServiceStateChangedCalled);

        // reset and un-register
        mOnServiceStateChangedCalled = false;
        if (mListener != null) {
            // un-register the listener
            mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_NONE);
        }
        synchronized (mLock) {
            if (!mOnServiceStateChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }
        assertFalse(mOnServiceStateChangedCalled);

        // re-register the listener
        mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_SERVICE_STATE);
        synchronized (mLock) {
            if (!mOnServiceStateChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }
        t.checkException();
        assertTrue(mOnServiceStateChangedCalled);
    }
}
