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

package android.net.wifi.cts;

import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_TIMEOUT;
import static android.net.wifi.WifiConfiguration.SECURITY_TYPE_PSK;
import static android.net.wifi.WifiManager.EASY_CONNECT_NETWORK_ROLE_STA;

import android.app.UiAutomation;
import android.content.Context;
import android.net.wifi.EasyConnectStatusCallback;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;
import android.os.Handler;
import android.os.HandlerExecutor;
import android.os.HandlerThread;
import android.platform.test.annotations.AppModeFull;
import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.SparseArray;
import androidx.test.platform.app.InstrumentationRegistry;

import java.util.concurrent.Executor;

@AppModeFull(reason = "Cannot get WifiManager in instant app mode")
@SmallTest
public class EasyConnectStatusCallbackTest extends WifiJUnit3TestBase {
    private static final String TEST_SSID = "\"testSsid\"";
    private static final String TEST_PASSPHRASE = "\"testPassword\"";
    private static final int TEST_WAIT_DURATION_MS = 12_000; // Long delay is necessary, see below
    private WifiManager mWifiManager;
    private static final String TEST_DPP_URI =
            "DPP:C:81/1;I:Easy_Connect_Demo;M:000102030405;"
                    + "K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgACDmtXD1Sz6/5B4YRdmTkbkkFLDwk8f0yRnfm1Go"
                    + "kpx/0=;;";
    private final HandlerThread mHandlerThread = new HandlerThread("EasyConnectTest");
    protected final Executor mExecutor;
    {
        mHandlerThread.start();
        mExecutor = new HandlerExecutor(new Handler(mHandlerThread.getLooper()));
    }
    private final Object mLock = new Object();
    private boolean mOnFailureCallback = false;
    private int mErrorCode;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }

        mWifiManager = (WifiManager) getContext().getSystemService(Context.WIFI_SERVICE);
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
    }

    private EasyConnectStatusCallback mEasyConnectStatusCallback = new EasyConnectStatusCallback() {
        @Override
        public void onEnrolleeSuccess(int newNetworkId) {

        }

        @Override
        public void onConfiguratorSuccess(int code) {

        }

        @Override
        public void onProgress(int code) {

        }

        @Override
        public void onFailure(int code) {
            synchronized (mLock) {
                mOnFailureCallback = true;
                mErrorCode = code;
                mLock.notify();
            }
        }

        public void onFailure(int code, String ssid, SparseArray<int[]> channelListArray,
                int[] operatingClassArray) {
            synchronized (mLock) {
                mOnFailureCallback = true;
                mErrorCode = code;
                mLock.notify();
            }
        }
    };

    /**
     * Tests {@link android.net.wifi.EasyConnectStatusCallback} class.
     *
     * Since Easy Connect requires 2 devices, start Easy Connect session and expect an error.
     */
    public void testConfiguratorInitiatorOnFailure() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        if (!mWifiManager.isEasyConnectSupported()) {
            // skip the test if Easy Connect is not supported
            return;
        }
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        try {
            uiAutomation.adoptShellPermissionIdentity();
            WifiConfiguration config;
            config = new WifiConfiguration();
            config.SSID = TEST_SSID;
            config.preSharedKey = TEST_PASSPHRASE;
            config.setSecurityParams(SECURITY_TYPE_PSK);
            int networkId = mWifiManager.addNetwork(config);
            assertFalse(networkId == -1);
            synchronized (mLock) {
                mWifiManager.startEasyConnectAsConfiguratorInitiator(TEST_DPP_URI, networkId,
                        EASY_CONNECT_NETWORK_ROLE_STA, mExecutor, mEasyConnectStatusCallback);
                // Note: A long delay is necessary because there is no enrollee, and the system
                // tries to discover it. We will wait for a timeout error to occur.
                mLock.wait(TEST_WAIT_DURATION_MS);
            }
            mWifiManager.removeNetwork(networkId);
            assertTrue(mOnFailureCallback);
            assertEquals(EASY_CONNECT_EVENT_FAILURE_TIMEOUT, mErrorCode);
            mWifiManager.stopEasyConnectSession();
        } finally {
            uiAutomation.dropShellPermissionIdentity();
        }
    }

    /**
     * Tests {@link android.net.wifi.EasyConnectStatusCallback} class.
     *
     * Since Easy Connect requires 2 devices, start Easy Connect session and expect an error.
     */
    public void testEnrolleeInitiatorOnFailure() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        if (!mWifiManager.isEasyConnectSupported()) {
            // skip the test if Easy Connect is not supported
            return;
        }
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        try {
            uiAutomation.adoptShellPermissionIdentity();
            synchronized (mLock) {
                mWifiManager.startEasyConnectAsEnrolleeInitiator(TEST_DPP_URI, mExecutor,
                        mEasyConnectStatusCallback);
                // Note: A long delay is necessary because there is no configurator, and the system
                // tries to discover it. We will wait for a timeout error to occur.
                mLock.wait(TEST_WAIT_DURATION_MS);
            }
            assertTrue(mOnFailureCallback);
            assertEquals(EASY_CONNECT_EVENT_FAILURE_TIMEOUT, mErrorCode);
            mWifiManager.stopEasyConnectSession();
        } finally {
            uiAutomation.dropShellPermissionIdentity();
        }
    }
}
