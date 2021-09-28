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
package com.android.tradefed.device.helper;

import com.android.ddmlib.IDevice;
import com.android.ddmlib.testrunner.IRemoteAndroidTestRunner;
import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.WifiHelper;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestResult;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.result.ddmlib.DefaultRemoteAndroidTestRunner;
import com.android.tradefed.util.FileUtil;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

/** A utility to use and get information related to the telephony. */
public class TelephonyHelper {

    private static final String TELEPHONY_UTIL_APK_NAME = "TelephonyUtility.apk";
    private static final String TELEPHONY_APK_RES_PATH = "/apks/telephonyutil/";
    public static final String PACKAGE_NAME = "android.telephony.utility";
    private static final String CLASS_NAME = ".SimCardUtil";
    private static final String METHOD_NAME = "getSimCardInformation";

    private static final String AJUR_RUNNER = "androidx.test.runner.AndroidJUnitRunner";

    public static final String SIM_STATE_KEY = "sim_state";
    public static final String CARRIER_PRIVILEGES_KEY = "has_carried_privileges";
    public static final String SECURED_ELEMENT_KEY = "has_secured_element";
    public static final String SE_SERVICE_KEY = "has_se_service";

    public static final TestDescription SIM_TEST =
            new TestDescription(PACKAGE_NAME + CLASS_NAME, METHOD_NAME);

    /** An information holder for the sim card related information. */
    public static class SimCardInformation {

        public boolean mHasTelephonySupport;
        public String mSimState;
        public boolean mCarrierPrivileges;
        public boolean mHasSecuredElement;
        public boolean mHasSeService;

        @Override
        public String toString() {
            return "SimCardInformation [mHasTelephonySupport="
                    + mHasTelephonySupport
                    + ", mSimState="
                    + mSimState
                    + ", mCarrierPrivileges="
                    + mCarrierPrivileges
                    + ", mHasSecuredElement="
                    + mHasSecuredElement
                    + ", mHasSeService="
                    + mHasSeService
                    + "]";
        }
    }

    private TelephonyHelper() {}

    /**
     * Get the information related to sim card from a given device.
     *
     * @param device The device under tests
     * @return A {@link SimCardInformation} object populated with the sim card info or null if
     *     anything goes wrong.
     */
    public static SimCardInformation getSimInfo(ITestDevice device)
            throws DeviceNotAvailableException {
        File apkFile = null;
        boolean wasInstalled = false;
        try {
            apkFile = extractTelephonyUtilApk();
            if (apkFile == null) {
                return null;
            }
            final String error = device.installPackage(apkFile, true);
            if (error != null) {
                CLog.e(error);
                return null;
            }

            wasInstalled = true;
            CollectingTestListener listener = new CollectingTestListener();
            IRemoteAndroidTestRunner runner = createTestRunner(device.getIDevice());
            runner.setMethodName(PACKAGE_NAME + CLASS_NAME, METHOD_NAME);
            device.runInstrumentationTests(runner, listener);
            TestRunResult runResult = listener.getCurrentRunResults();
            if (!runResult.isRunComplete()) {
                CLog.e("Run did not complete.");
                return null;
            }
            if (runResult.isRunFailure()) {
                CLog.e("TelephonyHelper run failure: %s", runResult.getRunFailureMessage());
                return null;
            }
            TestResult testResult = runResult.getTestResults().get(SIM_TEST);
            if (testResult == null) {
                CLog.e("getSimCardInformation did not run");
                return null;
            }
            SimCardInformation info = new SimCardInformation();
            info.mHasTelephonySupport = !TestStatus.FAILURE.equals(testResult.getStatus());
            info.mSimState = testResult.getMetrics().get(SIM_STATE_KEY);
            info.mCarrierPrivileges =
                    stringToBool(testResult.getMetrics().get(CARRIER_PRIVILEGES_KEY));
            info.mHasSecuredElement =
                    stringToBool(testResult.getMetrics().get(SECURED_ELEMENT_KEY));
            info.mHasSeService = stringToBool(testResult.getMetrics().get(SE_SERVICE_KEY));
            CLog.d("%s", info);
            return info;
        } finally {
            FileUtil.deleteFile(apkFile);
            if (wasInstalled) {
                device.uninstallPackage(PACKAGE_NAME);
            }
        }
    }

    /** Helper method to extract the wifi util apk from the classpath */
    private static File extractTelephonyUtilApk() {
        File apkTempFile = null;
        try {
            apkTempFile = FileUtil.createTempFile(TELEPHONY_UTIL_APK_NAME, ".apk");
            InputStream apkStream =
                    WifiHelper.class.getResourceAsStream(
                            TELEPHONY_APK_RES_PATH + TELEPHONY_UTIL_APK_NAME);
            FileUtil.writeToFile(apkStream, apkTempFile);
        } catch (IOException e) {
            CLog.e(e);
            FileUtil.deleteFile(apkTempFile);
            return null;
        }
        return apkTempFile;
    }

    private static IRemoteAndroidTestRunner createTestRunner(IDevice idevice) {
        return new DefaultRemoteAndroidTestRunner(PACKAGE_NAME, AJUR_RUNNER, idevice);
    }

    private static boolean stringToBool(String boolString) {
        if (boolString == null) {
            return false;
        }
        return Boolean.parseBoolean(boolString);
    }
}
