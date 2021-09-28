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
 * limitations under the License.
 */

package com.android.cts.verifier.wifi;

import android.annotation.NonNull;
import android.content.Context;
import android.content.res.Resources;
import android.net.wifi.WifiManager;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;

import com.android.cts.verifier.R;

/**
 * Base class for all Wifi test cases.
 */
public abstract class BaseTestCase {
    private static final String TAG = "BaseTestCase";

    protected Context mContext;
    protected Resources mResources;
    protected Listener mListener;

    private Thread mThread;
    private HandlerThread mHandlerThread;
    protected Handler mHandler;
    protected WifiManager mWifiManager;
    protected TestUtils mTestUtils;

    protected String mSsid;
    protected String mPsk;

    public BaseTestCase(Context context) {
        mContext = context;
        mResources = mContext.getResources();
    }

    /**
     * Set up the test case. Executed once before test starts.
     */
    protected void setUp() {
        mWifiManager = (WifiManager) mContext.getSystemService(Context.WIFI_SERVICE);
        mTestUtils = new TestUtils(mContext, mListener);

        // Ensure we're not connected to any wifi network before we start the tests.
        if (mTestUtils.isConnected(null, null)) {
            mListener.onTestFailed(mContext.getString(
                    R.string.wifi_status_connected_to_other_network));
            throw new IllegalStateException("Should not be connected to any network");
        }
        /**
         * TODO: Clear the state before each test. This needs to be an instrumentation to
         * run the below shell commands.
        SystemUtil.runShellCommand("wifi network-suggestions-set-user-approved "
                + mContext.getPackageName() + " no");
        SystemUtil.runShellCommand("wifi network-requests-remove-user-approved-access-points "
                + mContext.getPackageName());
        */
    }

    /**
     * Tear down the test case. Executed after test finishes - whether on success or failure.
     */
    protected void tearDown() {
        mWifiManager = null;
    }

    /**
     * Execute test case.
     *
     * @return true on success, false on failure. In case of failure
     */
    protected abstract boolean executeTest() throws InterruptedException;

    /**
     * Returns a String describing the failure reason of the most recent test failure (not valid
     * in other scenarios). Override to customize the failure string.
     */
    protected String getFailureReason() {
        return mContext.getString(R.string.wifi_unexpected_error);
    }

    /**
     * Start running the test case.
     * <p>
     * Test case is executed in another thread.
     */
    public void start(@NonNull Listener listener, @NonNull String ssid, @NonNull String psk) {
        mListener = listener;
        mSsid = ssid;
        mPsk = psk;

        stop();
        mHandlerThread = new HandlerThread("CtsVerifier-Wifi");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
        mThread = new Thread(
                new Runnable() {
                    @Override
                    public void run() {
                        mListener.onTestStarted();
                        try {
                            setUp();
                        } catch (Exception e) {
                            Log.e(TAG, "Setup failed", e);
                            mListener.onTestFailed(mContext.getString(R.string.wifi_setup_error));
                            return;
                        }

                        try {
                            if (executeTest()) {
                                mListener.onTestSuccess();
                            } else {
                                mListener.onTestFailed(getFailureReason());
                            }
                        } catch (Exception e) {
                            Log.e(TAG, "Execute failed", e);
                            mListener.onTestFailed(
                                    mContext.getString(R.string.wifi_unexpected_error));
                        } finally {
                            tearDown();
                        }
                    }
                });
        mThread.start();
    }

    /**
     * Stop the currently running test case.
     */
    public void stop() {
        if (mThread != null) {
            mThread.interrupt();
            mThread = null;
        }
        if (mHandlerThread != null) {
            mHandlerThread.quitSafely();
            mHandlerThread = null;
            mHandler = null;
        }
    }

    /**
     * Listener interface used to communicate the state and status of the test case. It should
     * be implemented by any activity encompassing a test case.
     */
    public interface Listener {
        /**
         * This function is invoked when the test case starts.
         */
        void onTestStarted();

        /**
         * This function is invoked by the test to send a message to listener.
         */
        void onTestMsgReceived(String msg);

        /**
         * This function is invoked when the test finished successfully.
         */
        void onTestSuccess();

        /**
         * This function is invoked when the test failed (test is done).
         */
        void onTestFailed(String reason);
    }
}

