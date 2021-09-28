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

package com.android.cts.useprocess;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.os.SystemClock;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Assert;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests that network can / can not be accessed in the appropriate processes.
 */
@RunWith(AndroidJUnit4.class)
public class AccessNetworkTest {
    private static Context getContext() {
        return InstrumentationRegistry.getTargetContext();
    }

    final class MyConnection implements ServiceConnection {
        boolean mDone;
        String mMsg;
        String mStackTrace;

        public String waitForResult() {
            long waitUntil = SystemClock.uptimeMillis() + 5000;
            synchronized (this) {
                long now = SystemClock.uptimeMillis();
                while (!mDone) {
                    if (now >= waitUntil) {
                        return "Timed out";
                    }
                    try {
                        wait(waitUntil - now);
                    } catch (InterruptedException e) {
                    }
                }
                return mMsg;
            }
        }

        public String getStackTrace() {
            return mStackTrace;
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            Parcel cmd = Parcel.obtain();
            Parcel reply = Parcel.obtain();
            boolean called = false;
            try {
                service.transact(BaseNetworkService.TRANSACT_TEST, cmd, reply, 0);
                called = true;
            } catch (RemoteException e) {
                Assert.fail("Remote service died: " + e.toString());
            } finally {
                synchronized (this) {
                    mDone = true;
                    if (called) {
                        mMsg = BaseNetworkService.readResult(reply);
                        mStackTrace = mMsg != null
                                ? BaseNetworkService.readResultCallstack(reply) : null;
                    } else {
                        mMsg = "Transaction failed for some reason";
                    }
                    notifyAll();
                }

                reply.recycle();
                cmd.recycle();
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
        }
    }

    /**
     * Test that the main process has network access.
     */
    @Test
    public void testMainHasNetwork() {
        doNetworkTest(ServiceWithNetwork1.class);
    }

    /**
     * Test that the first with-network process has network access.
     */
    @Test
    public void testWithNet2HasNetwork() {
        doNetworkTest(ServiceWithNetwork2.class);
    }

    /**
     * Test that the second with-network process has network access.
     */
    @Test
    public void testWithNet3HasNetwork() {
        doNetworkTest(ServiceWithNetwork3.class);
    }

    /**
     * Test that the first without-network process doesn't have network access.
     */
    @Test
    public void testWithoutNet1NoNetwork() {
        doNetworkTest(ServiceWithoutNetwork1.class);
    }

    /**
     * Test that the first without-network process doesn't have network access.
     */
    @Test
    public void testWithoutNet2NoNetwork() {
        doNetworkTest(ServiceWithoutNetwork2.class);
    }

    private void doNetworkTest(Class serviceClass) {
        final MyConnection conn = new MyConnection();
        final Context context = getContext();
        Intent intent = new Intent(context, serviceClass);
        if (!context.bindService(intent, conn, Context.BIND_AUTO_CREATE)) {
            Assert.fail("Failed binding to service " + intent.getComponent());
        }
        String result = conn.waitForResult();
        if (result != null) {
            Assert.fail(result + ", stack trace:\n" + conn.getStackTrace());
        }
    }
}
