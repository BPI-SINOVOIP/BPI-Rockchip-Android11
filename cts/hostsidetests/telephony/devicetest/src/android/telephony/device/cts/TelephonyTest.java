/*
 * Copyright (C) 2013 The Android Open Source Project
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

package android.telephony.device.cts;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.os.Handler;
import android.os.HandlerExecutor;
import android.os.HandlerThread;
import android.provider.DeviceConfig;
import android.telephony.PhoneStateListener;
import android.telephony.TelephonyManager;
import android.telephony.TelephonyRegistryManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.ShellIdentityUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;
import java.util.stream.Stream;

@RunWith(AndroidJUnit4.class)
public class TelephonyTest {
    private static final int TEST_TIMEOUT_MILLIS = 1000;

    private Handler mHandler;
    private HandlerThread mHandlerThread;
    private TelephonyManager mTelephonyManager;

    @Before
    public void setUp() {
        mHandlerThread = new HandlerThread(TelephonyTest.class.getSimpleName());
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
        mTelephonyManager = InstrumentationRegistry.getContext()
                .getSystemService(TelephonyManager.class);
    }

    @After
    public void tearDown() {
        mHandlerThread.quitSafely();
    }

    @Test
    public void testListenerRegistrationWithChangeEnabled() throws Throwable {
        // Register a bunch of filler listeners first so that we hit the cap
        List<PhoneStateListener> fillerListeners = registerFillerListeners();
        if (fillerListeners == null) {
            return;
        }

        TelephonyRegistryManager telephonyRegistryManager = InstrumentationRegistry.getContext()
                .getSystemService(TelephonyRegistryManager.class);

        LinkedBlockingQueue<Boolean> queue = new LinkedBlockingQueue<>(1);
        PhoneStateListener testListener = new PhoneStateListener(new HandlerExecutor(mHandler)) {
            @Override
            public void onCallStateChanged(int state, String number) {
                queue.offer(true);
            }
        };

        try {
            mTelephonyManager.listen(testListener, PhoneStateListener.LISTEN_CALL_STATE);
            fail("Expected an IllegalStateException");
        } catch (IllegalStateException e) {
            // expected
        }

        // Now, deregister one of the fillers and try that again. This time it should succeed
        mTelephonyManager.listen(fillerListeners.get(0), PhoneStateListener.LISTEN_NONE);

        mTelephonyManager.listen(testListener, PhoneStateListener.LISTEN_CALL_STATE);
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(telephonyRegistryManager,
                (trm) -> trm.notifyCallStateChangedForAllSubscriptions(
                        TelephonyManager.CALL_STATE_IDLE, "12345678"));

        Boolean result = queue.poll(TEST_TIMEOUT_MILLIS, TimeUnit.MILLISECONDS);
        assertTrue("Never got anything on the extra listener", result);
    }

    @Test
    public void testListenerRegistrationWithChangeDisabled() throws Throwable {
        if (registerFillerListeners() == null) {
            return;
        }

        TelephonyRegistryManager telephonyRegistryManager = InstrumentationRegistry.getContext()
                .getSystemService(TelephonyRegistryManager.class);

        LinkedBlockingQueue<Boolean> queue = new LinkedBlockingQueue<>(1);
        PhoneStateListener testListener = new PhoneStateListener(new HandlerExecutor(mHandler)) {
            @Override
            public void onCallStateChanged(int state, String number) {
                queue.offer(true);
            }
        };

        mTelephonyManager.listen(testListener, PhoneStateListener.LISTEN_CALL_STATE);
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(telephonyRegistryManager,
                (trm) -> trm.notifyCallStateChangedForAllSubscriptions(
                        TelephonyManager.CALL_STATE_IDLE, "12345678"));

        Boolean result = queue.poll(TEST_TIMEOUT_MILLIS, TimeUnit.MILLISECONDS);
        assertTrue("Never got anything on the extra listener", result);
    }

    private List<PhoneStateListener> registerFillerListeners() {
        int registrationLimit = ShellIdentityUtils.invokeStaticMethodWithShellPermissions(() ->
                DeviceConfig.getInt(DeviceConfig.NAMESPACE_TELEPHONY,
                        PhoneStateListener.FLAG_PER_PID_REGISTRATION_LIMIT,
                        PhoneStateListener.DEFAULT_PER_PID_REGISTRATION_LIMIT));
        if (registrationLimit < 1) {
            // Don't test anything if the limit is too small
            return null;
        }
        List<PhoneStateListener> fillerListeners = Stream.generate(
                () -> new PhoneStateListener(new HandlerExecutor(mHandler)))
                .limit(registrationLimit)
                .collect(Collectors.toList());
        fillerListeners.forEach((l) -> mTelephonyManager.listen(
                l, PhoneStateListener.LISTEN_SERVICE_STATE));

        return fillerListeners;
    }
}
