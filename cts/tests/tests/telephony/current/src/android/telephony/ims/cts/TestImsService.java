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

package android.telephony.ims.cts;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.telephony.ims.ImsService;
import android.telephony.ims.feature.MmTelFeature;
import android.telephony.ims.feature.RcsFeature;
import android.telephony.ims.stub.ImsConfigImplBase;
import android.telephony.ims.stub.ImsFeatureConfiguration;
import android.telephony.ims.stub.ImsRegistrationImplBase;
import android.util.Log;


import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * A Test ImsService that will verify ImsService functionality.
 */
public class TestImsService extends Service {

    private static final String TAG = "GtsImsTestImsService";

    private static ImsRegistrationImplBase sImsRegistrationImplBase = new ImsRegistrationImplBase();

    private TestRcsFeature mTestRcsFeature;
    private TestMmTelFeature mTestMmTelFeature;
    private TestImsConfig mTestImsConfig;
    private ImsService mTestImsService;
    private boolean mIsEnabled = false;
    private ImsFeatureConfiguration mFeatureConfig;
    private final Object mLock = new Object();

    public static final int LATCH_FEATURES_READY = 0;
    public static final int LATCH_ENABLE_IMS = 1;
    public static final int LATCH_DISABLE_IMS = 2;
    public static final int LATCH_CREATE_MMTEL = 3;
    public static final int LATCH_CREATE_RCS = 4;
    public static final int LATCH_REMOVE_MMTEL = 5;
    public static final int LATCH_REMOVE_RCS = 6;
    public static final int LATCH_MMTEL_READY = 7;
    public static final int LATCH_RCS_READY = 8;
    public static final int LATCH_MMTEL_CAP_SET = 9;
    public static final int LATCH_RCS_CAP_SET = 10;
    private static final int LATCH_MAX = 11;
    protected static final CountDownLatch[] sLatches = new CountDownLatch[LATCH_MAX];
    static {
        for (int i = 0; i < LATCH_MAX; i++) {
            sLatches[i] = new CountDownLatch(1);
        }
    }

    interface RemovedListener {
        void onRemoved();
    }
    interface ReadyListener {
        void onReady();
    }
    interface CapabilitiesSetListener {
        void onSet();
    }

    // This is defined here instead TestImsService extending ImsService directly because the GTS
    // tests were failing to run on pre-P devices. Not sure why, but TestImsService is loaded
    // even if it isn't used.
    private class ImsServiceUT extends ImsService {

        ImsServiceUT(Context context) {
            // As explained above, ImsServiceUT is created in order to get around classloader
            // restrictions. Attach the base context from the wrapper ImsService.
            if (getBaseContext() == null) {
                attachBaseContext(context);
            }
            mTestImsConfig = new TestImsConfig();
        }

        @Override
        public ImsFeatureConfiguration querySupportedImsFeatures() {
            return getFeatureConfig();
        }

        @Override
        public void readyForFeatureCreation() {
            synchronized (mLock) {
                countDownLatch(LATCH_FEATURES_READY);
            }
        }

        @Override
        public void enableIms(int slotId) {
            synchronized (mLock) {
                countDownLatch(LATCH_ENABLE_IMS);
                setIsEnabled(true);
            }
        }

        @Override
        public void disableIms(int slotId) {
            synchronized (mLock) {
                countDownLatch(LATCH_DISABLE_IMS);
                setIsEnabled(false);
            }
        }

        @Override
        public RcsFeature createRcsFeature(int slotId) {
            synchronized (mLock) {
                countDownLatch(LATCH_CREATE_RCS);
                mTestRcsFeature = new TestRcsFeature(
                        //onReady
                        () -> {
                            synchronized (mLock) {
                                countDownLatch(LATCH_RCS_READY);
                            }
                        },
                        //onRemoved
                        () -> {
                            synchronized (mLock) {
                                countDownLatch(LATCH_REMOVE_RCS);
                                mTestRcsFeature = null;
                            }
                        },
                        //onCapabilitiesSet
                        () -> {
                            synchronized (mLock) {
                                countDownLatch(LATCH_RCS_CAP_SET);
                            }
                        }
                        );
                return mTestRcsFeature;
            }
        }

        @Override
        public ImsConfigImplBase getConfig(int slotId) {
            return mTestImsConfig;
        }

        @Override
        public MmTelFeature createMmTelFeature(int slotId) {
            synchronized (mLock) {
                countDownLatch(LATCH_CREATE_MMTEL);
                mTestMmTelFeature = new TestMmTelFeature(
                        //onReady
                        () -> {
                            synchronized (mLock) {
                                countDownLatch(LATCH_MMTEL_READY);
                            }
                        },
                        //onRemoved
                        () -> {
                            synchronized (mLock) {
                                countDownLatch(LATCH_REMOVE_MMTEL);
                                mTestMmTelFeature = null;
                            }
                        },
                        //onCapabilitiesSet
                        () -> {
                            synchronized (mLock) {
                                countDownLatch(LATCH_MMTEL_CAP_SET);
                            }
                        }
                        );
                return mTestMmTelFeature;
            }
        }

        @Override
        public ImsRegistrationImplBase getRegistration(int slotId) {
            return sImsRegistrationImplBase;
        }
    }

    private final LocalBinder mBinder = new LocalBinder();
    // For local access of this Service.
    class LocalBinder extends Binder {
        TestImsService getService() {
            return TestImsService.this;
        }
    }

    protected ImsService getImsService() {
        synchronized (mLock) {
            if (mTestImsService != null) {
                return mTestImsService;
            }
            mTestImsService = new ImsServiceUT(this);
            return mTestImsService;
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        if ("android.telephony.ims.ImsService".equals(intent.getAction())) {
            if (ImsUtils.VDBG) {
                Log.d(TAG, "onBind-Remote");
            }
            return getImsService().onBind(intent);
        }
        if (ImsUtils.VDBG) {
            Log.i(TAG, "onBind-Local");
        }
        return mBinder;
    }

    public void resetState() {
        synchronized (mLock) {
            mTestMmTelFeature = null;
            mTestRcsFeature = null;
            mIsEnabled = false;
            for (int i = 0; i < LATCH_MAX; i++) {
                sLatches[i] = new CountDownLatch(1);
            }
        }
    }

    // Sets the feature configuration. Make sure to call this before initiating Bind to this
    // ImsService.
    public void setFeatureConfig(ImsFeatureConfiguration f) {
        synchronized (mLock) {
            mFeatureConfig = f;
        }
    }

    public ImsFeatureConfiguration getFeatureConfig() {
        synchronized (mLock) {
            return mFeatureConfig;
        }
    }

    public boolean isEnabled() {
        synchronized (mLock) {
            return mIsEnabled;
        }
    }

    public void setIsEnabled(boolean isEnabled) {
        synchronized (mLock) {
            mIsEnabled = isEnabled;
        }
    }

    public boolean waitForLatchCountdown(int latchIndex) {
        boolean complete = false;
        try {
            CountDownLatch latch;
            synchronized (mLock) {
                latch = sLatches[latchIndex];
            }
            complete = latch.await(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            // complete == false
        }
        synchronized (mLock) {
            sLatches[latchIndex] = new CountDownLatch(1);
        }
        return complete;
    }

    public void countDownLatch(int latchIndex) {
        synchronized (mLock) {
            sLatches[latchIndex].countDown();
        }
    }

    public TestMmTelFeature getMmTelFeature() {
        synchronized (mLock) {
            return mTestMmTelFeature;
        }
    }

    public TestRcsFeature getRcsFeature() {
        synchronized (mLock) {
            return mTestRcsFeature;
        }
    }

    public ImsRegistrationImplBase getImsRegistration() {
        synchronized (mLock) {
            return sImsRegistrationImplBase;
        }
    }

    public ImsConfigImplBase getConfig() {
        return mTestImsConfig;
    }
}
