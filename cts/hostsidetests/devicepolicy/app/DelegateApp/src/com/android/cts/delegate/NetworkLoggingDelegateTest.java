/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.cts.delegate;

import static com.android.cts.delegate.DelegateTestUtils.assertExpectException;
import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.app.admin.DelegatedAdminReceiver;
import android.app.admin.DevicePolicyManager;
import android.app.admin.NetworkEvent;
import android.content.Context;
import android.content.Intent;
import android.support.test.uiautomator.UiDevice;
import android.test.InstrumentationTestCase;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests that a delegate app with DELEGATION_NETWORK_LOGGING is able to control and access
 * network logging.
 */
public class NetworkLoggingDelegateTest extends InstrumentationTestCase {

    private static final String TAG = "NetworkLoggingDelegateTest";
    private static final long TIMEOUT_MIN = 1;

    private Context mContext;
    private DevicePolicyManager mDpm;
    private Activity mActivity;
    private UiDevice mDevice;


    private static final String[] URL_LIST = {
            "example.edu",
            "ipv6.google.com",
            "google.co.jp",
            "google.fr",
            "google.com.br",
            "google.com.tr",
            "google.co.uk",
            "google.de"
    };

    public static class NetworkLogsReceiver extends DelegatedAdminReceiver {
        static CountDownLatch mBatchCountDown;
        static Throwable mExceptionFromReceiver;

        @Override
        public void onNetworkLogsAvailable(Context context, Intent intent, long batchToken,
                int networkLogsCount) {
            try {
                DevicePolicyManager dpm = context.getSystemService(DevicePolicyManager.class);
                final List<NetworkEvent> events = dpm.retrieveNetworkLogs(null, batchToken);
                if (events == null || events.size() == 0) {
                    fail("Failed to retrieve batch of network logs with batch token " + batchToken);
                }
            } catch (Throwable e) {
                mExceptionFromReceiver = e;
            } finally {
            mBatchCountDown.countDown();
            }
        }

    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mContext = getInstrumentation().getContext();
        mDpm = mContext.getSystemService(DevicePolicyManager.class);
        NetworkLogsReceiver.mBatchCountDown = new CountDownLatch(1);
        NetworkLogsReceiver.mExceptionFromReceiver = null;
    }

    public void testCanAccessApis() throws Throwable {
        assertThat(mDpm.getDelegatedScopes(null, mContext.getPackageName())).contains(
                DevicePolicyManager.DELEGATION_NETWORK_LOGGING);
        testNetworkLogging();
    }

    public void testCannotAccessApis()throws Exception {
        assertExpectException(SecurityException.class, null,
                () -> mDpm.isNetworkLoggingEnabled(null));

        assertExpectException(SecurityException.class, null,
                () -> mDpm.setNetworkLoggingEnabled(null, true));

        assertExpectException(SecurityException.class, null,
                () -> mDpm.retrieveNetworkLogs(null, 0));
    }

    public void testNetworkLogging() throws Throwable {
        mDpm.setNetworkLoggingEnabled(null, true);
        assertTrue(mDpm.isNetworkLoggingEnabled(null));

        try {
            for (final String url : URL_LIST) {
                connectToWebsite(url);
            }
            mDevice.executeShellCommand("dpm force-network-logs");

            assertTrue("Delegated app did not receive network logs within time limit",
                    NetworkLogsReceiver.mBatchCountDown.await(TIMEOUT_MIN, TimeUnit.MINUTES));
            if (NetworkLogsReceiver.mExceptionFromReceiver != null) {
                // Rethrow any exceptions that might have happened in the receiver.
                throw NetworkLogsReceiver.mExceptionFromReceiver;
            }
        } finally {
            mDpm.setNetworkLoggingEnabled(null, false);
            assertFalse(mDpm.isNetworkLoggingEnabled(null));
        }
    }

    private void connectToWebsite(String server) throws Exception {
        final URL url = new URL("http://" + server);
        HttpURLConnection urlConnection = (HttpURLConnection) url.openConnection();
        try (AutoCloseable ac = () -> urlConnection.disconnect()){
            urlConnection.setConnectTimeout(2000);
            urlConnection.setReadTimeout(2000);
            urlConnection.getResponseCode();
        } catch (IOException e) {
            Log.w(TAG, "Failed to connect to " + server, e);
        }
    }
}
