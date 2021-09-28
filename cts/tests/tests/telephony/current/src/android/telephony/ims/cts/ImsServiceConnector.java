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

import android.app.Instrumentation;
import android.app.role.RoleManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.telephony.cts.TelephonyUtils;
import android.telephony.cts.externalimsservice.ITestExternalImsService;
import android.telephony.cts.externalimsservice.TestExternalImsService;
import android.telephony.ims.feature.ImsFeature;
import android.telephony.ims.stub.ImsFeatureConfiguration;
import android.util.Log;

import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.ShellIdentityUtils;

import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * Connects The CTS test ImsService to the Telephony Framework.
 */
class ImsServiceConnector {

    private static final String TAG = "CtsImsServiceConnector";

    private static final String PACKAGE_NAME =
            InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageName();
    private static final String EXTERNAL_PACKAGE_NAME =
            TestExternalImsService.class.getPackage().getName();

    private static final String COMMAND_BASE = "cmd phone ";
    private static final String COMMAND_SET_IMS_SERVICE = "ims set-ims-service ";
    private static final String COMMAND_GET_IMS_SERVICE = "ims get-ims-service ";
    private static final String COMMAND_CARRIER_SERVICE_IDENTIFIER = "-c ";
    private static final String COMMAND_DEVICE_SERVICE_IDENTIFIER = "-d ";
    private static final String COMMAND_SLOT_IDENTIFIER = "-s ";
    private static final String COMMAND_FEATURE_IDENTIFIER = "-f ";
    private static final String COMMAND_ENABLE_IMS = "ims enable ";
    private static final String COMMAND_DISABLE_IMS = "ims disable ";

    private class TestCarrierServiceConnection implements ServiceConnection {

        private final CountDownLatch mLatch;

        TestCarrierServiceConnection(CountDownLatch latch) {
            mLatch = latch;
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            mCarrierService = ((TestImsService.LocalBinder) service).getService();
            mLatch.countDown();
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            mCarrierService = null;
        }
    }

    private class TestDeviceServiceConnection implements ServiceConnection {

        private final CountDownLatch mLatch;

        TestDeviceServiceConnection(CountDownLatch latch) {
            mLatch = latch;
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            mExternalService = ITestExternalImsService.Stub.asInterface(service);
            mLatch.countDown();
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            mCarrierService = null;
        }
    }

    public class Connection {

        private static final int CONNECTION_TYPE_IMS_SERVICE_DEVICE = 1;
        private static final int CONNECTION_TYPE_IMS_SERVICE_CARRIER = 2;
        private static final int CONNECTION_TYPE_DEFAULT_SMS_APP = 3;

        private boolean mIsServiceOverridden = false;
        private String mOrigMmTelServicePackage;
        private String mOrigRcsServicePackage;
        private String mOrigSmsPackage;
        private int mConnectionType;
        private int mSlotId;
        Connection(int connectionType, int slotId) {
            mConnectionType = connectionType;
            mSlotId = slotId;
        }

        void clearPackage() throws Exception {
            mIsServiceOverridden = true;
            switch (mConnectionType) {
                case CONNECTION_TYPE_IMS_SERVICE_CARRIER: {
                    setCarrierImsService("none");
                    break;
                }
                case CONNECTION_TYPE_IMS_SERVICE_DEVICE: {
                    setDeviceImsService("");
                    break;
                }
                case CONNECTION_TYPE_DEFAULT_SMS_APP: {
                    // We don't need to clear anything for default SMS app.
                    break;
                }
            }
        }

        boolean overrideService(ImsFeatureConfiguration config) throws Exception {
            switch (mConnectionType) {
                case CONNECTION_TYPE_IMS_SERVICE_CARRIER: {
                    return bindCarrierImsService(config, PACKAGE_NAME);
                }
                case CONNECTION_TYPE_IMS_SERVICE_DEVICE: {
                    return bindDeviceImsService(config, EXTERNAL_PACKAGE_NAME);
                }
                case CONNECTION_TYPE_DEFAULT_SMS_APP: {
                    setDefaultSmsApp(PACKAGE_NAME);
                    break;
                }
            }
            return false;
        }

        void restoreOriginalPackage() throws Exception {
            if (!mIsServiceOverridden) {
                return;
            }

            if (mOrigRcsServicePackage == null) {
                mOrigRcsServicePackage = "";
            }

            if (mOrigMmTelServicePackage == null) {
                mOrigMmTelServicePackage = "";
            }

            switch (mConnectionType) {
                case CONNECTION_TYPE_IMS_SERVICE_CARRIER: {
                    setCarrierImsService(mOrigMmTelServicePackage, ImsFeature.FEATURE_MMTEL);
                    setCarrierImsService(mOrigRcsServicePackage, ImsFeature.FEATURE_RCS);
                    break;
                }
                case CONNECTION_TYPE_IMS_SERVICE_DEVICE: {
                    setDeviceImsService(mOrigMmTelServicePackage, ImsFeature.FEATURE_MMTEL);
                    setDeviceImsService(mOrigRcsServicePackage, ImsFeature.FEATURE_RCS);
                    break;
                }
                case CONNECTION_TYPE_DEFAULT_SMS_APP: {
                    setDefaultSmsApp(mOrigSmsPackage);
                    break;
                }
            }
        }

        private void storeOriginalPackage() throws Exception {
            switch (mConnectionType) {
                case CONNECTION_TYPE_IMS_SERVICE_CARRIER: {
                    mOrigMmTelServicePackage = getOriginalMmTelCarrierService();
                    mOrigRcsServicePackage = getOriginalRcsCarrierService();
                    break;
                }
                case CONNECTION_TYPE_IMS_SERVICE_DEVICE: {
                    mOrigMmTelServicePackage = getOriginalMmTelDeviceService();
                    mOrigRcsServicePackage = getOriginalRcsDeviceService();
                    break;
                }
                case CONNECTION_TYPE_DEFAULT_SMS_APP: {
                    mOrigSmsPackage = getDefaultSmsApp();
                    break;
                }
            }
        }

        private boolean setDeviceImsService(String packageName) throws Exception {
            String result = TelephonyUtils.executeShellCommand(mInstrumentation,
                    constructSetImsServiceOverrideCommand(false, packageName, new int[] {
                            ImsFeature.FEATURE_MMTEL, ImsFeature.FEATURE_RCS}));
            if (ImsUtils.VDBG) {
                Log.d(TAG, "setDeviceMmTelImsService result: " + result);
            }
            return "true".equals(result);
        }

        private boolean setCarrierImsService(String packageName) throws Exception {
            String result = TelephonyUtils.executeShellCommand(mInstrumentation,
                    constructSetImsServiceOverrideCommand(true, packageName, new int[] {
                            ImsFeature.FEATURE_MMTEL, ImsFeature.FEATURE_RCS}));
            if (ImsUtils.VDBG) {
                Log.d(TAG, "setCarrierMmTelImsService result: " + result);
            }
            return "true".equals(result);
        }

        private boolean setDeviceImsService(String packageName, int featureType) throws Exception {
            String result = TelephonyUtils.executeShellCommand(mInstrumentation,
                    constructSetImsServiceOverrideCommand(false, packageName,
                            new int[]{featureType}));
            if (ImsUtils.VDBG) {
                Log.d(TAG, "setDeviceMmTelImsService result: " + result);
            }
            return "true".equals(result);
        }

        private boolean setCarrierImsService(String packageName, int featureType) throws Exception {
            String result = TelephonyUtils.executeShellCommand(mInstrumentation,
                    constructSetImsServiceOverrideCommand(true, packageName,
                            new int[]{featureType}));
            if (ImsUtils.VDBG) {
                Log.d(TAG, "setCarrierMmTelImsService result: " + result);
            }
            return "true".equals(result);
        }

        private void setDefaultSmsApp(String packageName) throws Exception {
            RoleManager roleManager = mInstrumentation.getContext()
                    .getSystemService(RoleManager.class);
            Boolean result;
            LinkedBlockingQueue<Boolean> queue = new LinkedBlockingQueue<>(1);
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(roleManager,
                    (m) -> m.addRoleHolderAsUser(RoleManager.ROLE_SMS, packageName, 0,
                            android.os.Process.myUserHandle(),
                            // Run on calling binder thread.
                            Runnable::run, queue::offer));
            result = queue.poll(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            if (ImsUtils.VDBG) {
                Log.d(TAG, "setDefaultSmsApp result: " + result);
            }
        }

        private String getDefaultSmsApp() throws Exception {
            RoleManager roleManager = mInstrumentation.getContext()
                    .getSystemService(RoleManager.class);
            List<String> result = ShellIdentityUtils.invokeMethodWithShellPermissions(roleManager,
                    (m) -> m.getRoleHolders(RoleManager.ROLE_SMS));
            if (ImsUtils.VDBG) {
                Log.d(TAG, "getDefaultSmsApp result: " + result);
            }
            // There should only be one default sms app
            return result.get(0);
        }

        private boolean bindCarrierImsService(ImsFeatureConfiguration config, String packageName)
                throws Exception {
            getCarrierService().setFeatureConfig(config);
            return setCarrierImsService(packageName) && getCarrierService().waitForLatchCountdown(
                            TestImsService.LATCH_FEATURES_READY);
        }

        private boolean bindDeviceImsService(ImsFeatureConfiguration config, String packageName)
                throws Exception {
            getExternalService().setFeatureConfig(config);
            return setDeviceImsService(packageName) && getExternalService().waitForLatchCountdown(
                    TestImsService.LATCH_FEATURES_READY);
        }

        private String getOriginalMmTelCarrierService() throws Exception {
            String result = TelephonyUtils.executeShellCommand(mInstrumentation,
                    constructGetImsServiceCommand(true, ImsFeature.FEATURE_MMTEL));
            if (ImsUtils.VDBG) {
                Log.d(TAG, "getOriginalMmTelCarrierService result: " + result);
            }
            return result;
        }

        private String getOriginalRcsCarrierService() throws Exception {
            String result = TelephonyUtils.executeShellCommand(mInstrumentation,
                    constructGetImsServiceCommand(true, ImsFeature.FEATURE_RCS));
            if (ImsUtils.VDBG) {
                Log.d(TAG, "getOriginalRcsCarrierService result: " + result);
            }
            return result;
        }

        private String getOriginalMmTelDeviceService() throws Exception {
            String result = TelephonyUtils.executeShellCommand(mInstrumentation,
                    constructGetImsServiceCommand(false, ImsFeature.FEATURE_MMTEL));
            if (ImsUtils.VDBG) {
                Log.d(TAG, "getOriginalMmTelDeviceService result: " + result);
            }
            return result;
        }

        private String getOriginalRcsDeviceService() throws Exception {
            String result = TelephonyUtils.executeShellCommand(mInstrumentation,
                    constructGetImsServiceCommand(false, ImsFeature.FEATURE_RCS));
            if (ImsUtils.VDBG) {
                Log.d(TAG, "getOriginalRcsDeviceService result: " + result);
            }
            return result;
        }

        private String constructSetImsServiceOverrideCommand(boolean isCarrierService,
                String packageName, int[] featureTypes) {
            return COMMAND_BASE + COMMAND_SET_IMS_SERVICE + COMMAND_SLOT_IDENTIFIER + mSlotId + " "
                    + (isCarrierService
                        ? COMMAND_CARRIER_SERVICE_IDENTIFIER : COMMAND_DEVICE_SERVICE_IDENTIFIER)
                    + COMMAND_FEATURE_IDENTIFIER + getFeatureTypesString(featureTypes) + " "
                    + packageName;
        }

        private String constructGetImsServiceCommand(boolean isCarrierService, int featureType) {
            return COMMAND_BASE + COMMAND_GET_IMS_SERVICE + COMMAND_SLOT_IDENTIFIER + mSlotId + " "
                    + (isCarrierService
                        ? COMMAND_CARRIER_SERVICE_IDENTIFIER : COMMAND_DEVICE_SERVICE_IDENTIFIER)
                    + COMMAND_FEATURE_IDENTIFIER + featureType;
        }

        private String getFeatureTypesString(int[] featureTypes) {
            if (featureTypes.length == 0) return "";
            StringBuilder builder = new StringBuilder();
            builder.append(featureTypes[0]);
            for (int i = 1; i < featureTypes.length; i++) {
                builder.append(",");
                builder.append(featureTypes[i]);
            }
            return builder.toString();
        }
    }

    private Instrumentation mInstrumentation;

    private TestImsService mCarrierService;
    private TestCarrierServiceConnection mCarrierServiceConn;
    private ITestExternalImsService mExternalService;
    private TestDeviceServiceConnection mExternalServiceConn;

    private Connection mDeviceServiceConnection;
    private Connection mCarrierServiceConnection;
    private Connection mDefaultSmsAppConnection;

    ImsServiceConnector(Instrumentation instrumentation) {
        mInstrumentation = instrumentation;
    }

    void clearAllActiveImsServices(int slotId) throws Exception {
        mDeviceServiceConnection = new Connection(Connection.CONNECTION_TYPE_IMS_SERVICE_DEVICE,
                slotId);
        mDeviceServiceConnection.storeOriginalPackage();
        mDeviceServiceConnection.clearPackage();

        mCarrierServiceConnection = new Connection(Connection.CONNECTION_TYPE_IMS_SERVICE_CARRIER,
                slotId);
        mCarrierServiceConnection.storeOriginalPackage();
        mCarrierServiceConnection.clearPackage();

        mDefaultSmsAppConnection = new Connection(Connection.CONNECTION_TYPE_DEFAULT_SMS_APP,
                slotId);
        mDefaultSmsAppConnection.storeOriginalPackage();
        // No need to clear SMS App, only replace when necessary.
    }

    boolean connectCarrierImsService(ImsFeatureConfiguration config) throws Exception {
        if (!setupLocalCarrierImsService()) {
            Log.w(TAG, "connectCarrierImsService: couldn't set up service.");
            return false;
        }
        mCarrierService.resetState();
        return mCarrierServiceConnection.overrideService(config);
    }

    boolean connectDeviceImsService(ImsFeatureConfiguration config) throws Exception {
        if (!setupExternalImsService()) {
            Log.w(TAG, "connectDeviceImsService: couldn't set up service.");
            return false;
        }
        mExternalService.resetState();
        return mDeviceServiceConnection.overrideService(config);
    }

    void setDefaultSmsApp() throws Exception {
        mDefaultSmsAppConnection.overrideService(null);
    }

    void disconnectCarrierImsService() throws Exception {
        mCarrierServiceConnection.clearPackage();
    }

    void disconnectDeviceImsService() throws Exception {
        mDeviceServiceConnection.clearPackage();
    }

    private boolean setupLocalCarrierImsService() {
        if (mCarrierService != null) {
            return true;
        }
        CountDownLatch latch = new CountDownLatch(1);
        mCarrierServiceConn = new TestCarrierServiceConnection(latch);
        mInstrumentation.getContext().bindService(new Intent(mInstrumentation.getContext(),
                        TestImsService.class), mCarrierServiceConn, Context.BIND_AUTO_CREATE);
        try {
            return latch.await(5000, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            return false;
        }
    }

    private boolean setupExternalImsService() {
        if (mExternalService != null) {
            return true;
        }
        CountDownLatch latch = new CountDownLatch(1);
        mExternalServiceConn = new TestDeviceServiceConnection(latch);
        Intent deviceIntent = new Intent();
        deviceIntent.setComponent(new ComponentName(EXTERNAL_PACKAGE_NAME,
                TestExternalImsService.class.getName()));
        mInstrumentation.getContext().bindService(deviceIntent, mExternalServiceConn,
                Context.BIND_AUTO_CREATE);
        try {
            return latch.await(5000, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            return false;
        }
    }

    // Detect and disconnect all active services.
    void disconnectServices() throws Exception {
        // Remove local connection
        if (mCarrierServiceConn != null) {
            mInstrumentation.getContext().unbindService(mCarrierServiceConn);
            mCarrierService = null;
        }
        if (mExternalServiceConn != null) {
            mInstrumentation.getContext().unbindService(mExternalServiceConn);
            mExternalService = null;
        }
        mDeviceServiceConnection.restoreOriginalPackage();
        mCarrierServiceConnection.restoreOriginalPackage();
        mDefaultSmsAppConnection.restoreOriginalPackage();
    }

    void enableImsService(int slot) throws Exception {
        TelephonyUtils.executeShellCommand(mInstrumentation, COMMAND_BASE + COMMAND_ENABLE_IMS
                + COMMAND_SLOT_IDENTIFIER + slot);
    }

    void disableImsService(int slot) throws Exception {
        TelephonyUtils.executeShellCommand(mInstrumentation, COMMAND_BASE + COMMAND_DISABLE_IMS
                + COMMAND_SLOT_IDENTIFIER + slot);
    }

    TestImsService getCarrierService() {
        return mCarrierService;
    }

    ITestExternalImsService getExternalService() {
        return mExternalService;
    }
}
