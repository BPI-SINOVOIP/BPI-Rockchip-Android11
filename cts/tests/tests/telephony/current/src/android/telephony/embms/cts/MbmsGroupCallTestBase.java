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
 * limitations under the License
 */

package android.telephony.embms.cts;

import static androidx.test.InstrumentationRegistry.getContext;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.annotation.Nullable;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.RemoteException;
import android.telephony.MbmsGroupCallSession;
import android.telephony.cts.embmstestapp.CtsGroupCallService;
import android.telephony.cts.embmstestapp.ICtsGroupCallMiddlewareControl;
import android.telephony.mbms.MbmsGroupCallSessionCallback;

import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;
import org.junit.After;
import org.junit.Before;

public class MbmsGroupCallTestBase {
    protected static final int ASYNC_TIMEOUT = 10000;

    protected static class TestCallback implements MbmsGroupCallSessionCallback {
        private final BlockingQueue<SomeArgs> mErrorCalls = new LinkedBlockingQueue<>();
        private final BlockingQueue<SomeArgs> mOnAvailableSaisUpdatedCalls =
                new LinkedBlockingQueue<>();
        private final BlockingQueue<SomeArgs> mOnServiceInterfaceAvailableCalls =
                new LinkedBlockingQueue<>();
        private final BlockingQueue<SomeArgs> mMiddlewareReadyCalls = new LinkedBlockingQueue<>();
        private int mNumErrorCalls = 0;

        @Override
        public void onError(int errorCode, @Nullable String message) {
            MbmsGroupCallSessionCallback.super.onError(errorCode, message);
            mNumErrorCalls += 1;
            SomeArgs args = SomeArgs.obtain();
            args.arg1 = errorCode;
            args.arg2 = message;
            mErrorCalls.add(args);
        }

        @Override
        public void onMiddlewareReady() {
            MbmsGroupCallSessionCallback.super.onMiddlewareReady();
            mMiddlewareReadyCalls.add(SomeArgs.obtain());
        }

        @Override
        public void onAvailableSaisUpdated(List<Integer> currentSais,
                List<List<Integer>> availableSais) {
            MbmsGroupCallSessionCallback.super.onAvailableSaisUpdated(currentSais, availableSais);
            SomeArgs args = SomeArgs.obtain();
            args.arg1 = currentSais;
            args.arg2 = availableSais;
            mOnAvailableSaisUpdatedCalls.add(args);
        }

        @Override
        public void onServiceInterfaceAvailable(String interfaceName, int index) {
            MbmsGroupCallSessionCallback.super.onServiceInterfaceAvailable(interfaceName, index);
            SomeArgs args = SomeArgs.obtain();
            args.arg1 = interfaceName;
            args.arg2 = index;
            mOnServiceInterfaceAvailableCalls.add(args);
        }

        public SomeArgs waitOnError() {
            try {
                return mErrorCalls.poll(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                return null;
            }
        }

        public SomeArgs waitOnAvailableSaisUpdatedCalls() {
            try {
                return mOnAvailableSaisUpdatedCalls.poll(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                return null;
            }
        }

        public SomeArgs waitOnServiceInterfaceAvailableCalls() {
            try {
                return mOnServiceInterfaceAvailableCalls.poll(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                return null;
            }
        }

        public boolean waitOnMiddlewareReady() {
            try {
                return mMiddlewareReadyCalls.poll(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS) != null;
            } catch (InterruptedException e) {
                return false;
            }
        }

        public int getNumErrorCalls() {
            return mNumErrorCalls;
        }
    }

    Context mContext;
    HandlerThread mHandlerThread;
    Executor mCallbackExecutor;
    ICtsGroupCallMiddlewareControl mMiddlewareControl;
    MbmsGroupCallSession mGroupCallSession;
    TestCallback mCallback = new TestCallback();

    @Before
    public void setUp() throws Exception {
        mContext = getContext();
        mHandlerThread = new HandlerThread("EmbmsCtsTestWorker");
        mHandlerThread.start();
        mCallbackExecutor = (new Handler(mHandlerThread.getLooper()))::post;
        mCallback = new TestCallback();
        getControlBinder();
        setupGroupCallSession();
    }

    @After
    public void tearDown() throws Exception {
        mHandlerThread.quit();
        mGroupCallSession.close();
        mMiddlewareControl.reset();
    }

    private void setupGroupCallSession() throws Exception {
        mGroupCallSession = MbmsGroupCallSession.create(
                mContext, mCallbackExecutor, mCallback);
        assertNotNull(mGroupCallSession);
        assertTrue(mCallback.waitOnMiddlewareReady());
        assertEquals(0, mCallback.getNumErrorCalls());
        List initializeCall = (List) mMiddlewareControl.getGroupCallSessionCalls().get(0);
        assertEquals(CtsGroupCallService.METHOD_INITIALIZE, initializeCall.get(0));
    }

    private void getControlBinder() throws InterruptedException {
        Intent bindIntent = new Intent(CtsGroupCallService.CONTROL_INTERFACE_ACTION);
        bindIntent.setComponent(CtsGroupCallService.CONTROL_INTERFACE_COMPONENT);
        final CountDownLatch bindLatch = new CountDownLatch(1);

        boolean success = mContext.bindService(bindIntent, new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                mMiddlewareControl = ICtsGroupCallMiddlewareControl.Stub.asInterface(service);
                bindLatch.countDown();
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {
                mMiddlewareControl = null;
            }
        }, Context.BIND_AUTO_CREATE);
        if (!success) {
            fail("Failed to get control interface -- bind error");
        }
        bindLatch.await(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);
    }

    protected List<List<Object>> getMiddlewareCalls(String methodName) throws RemoteException {
        return ((List<List<Object>>) mMiddlewareControl.getGroupCallSessionCalls()).stream()
                .filter((elem) -> elem.get(0).equals(methodName))
                .collect(Collectors.toList());
    }
}