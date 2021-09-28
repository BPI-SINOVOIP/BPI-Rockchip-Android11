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
package com.android.cts.appbinding;


import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.IBuildReceiver;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

@RunWith(DeviceJUnit4ClassRunner.class)
public class AppBindingHostTest extends BaseHostJUnit4Test implements IBuildReceiver {

    private static final boolean SKIP_UNINSTALL = false;

    private static final String APK_1 = "CtsAppBindingService1.apk";
    private static final String APK_2 = "CtsAppBindingService2.apk";
    private static final String APK_3 = "CtsAppBindingService3.apk";
    private static final String APK_4 = "CtsAppBindingService4.apk";
    private static final String APK_5 = "CtsAppBindingService5.apk";
    private static final String APK_6 = "CtsAppBindingService6.apk";
    private static final String APK_7 = "CtsAppBindingService7.apk";
    private static final String APK_B = "CtsAppBindingServiceB.apk";

    private static final String PACKAGE_A = "com.android.cts.appbinding.app";
    private static final String PACKAGE_B = "com.android.cts.appbinding.app.b";

    private static final String PACKAGE_A_PROC = PACKAGE_A + ":persistent";

    private static final String APP_BINDING_SETTING = "app_binding_constants";

    private static final String SERVICE_1 = "com.android.cts.appbinding.app.MyService";
    private static final String SERVICE_2 = "com.android.cts.appbinding.app.MyService2";

    private IBuildInfo mCtsBuild;

    private static final int USER_SYSTEM = 0;

    private static final int DEFAULT_TIMEOUT_SEC = 30;

    private interface ThrowingRunnable {
        void run() throws Throwable;
    }

    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mCtsBuild = buildInfo;
    }

    private void installAppAsUser(String appFileName, boolean grantPermissions, int userId)
            throws Exception {
        CLog.d("Installing app " + appFileName + " for user " + userId);
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        String result = getDevice().installPackageForUser(
                buildHelper.getTestFile(appFileName), true, grantPermissions, userId, "-t");
        assertNull("Failed to install " + appFileName + " for user " + userId + ": " + result,
                result);

        waitForBroadcastIdle();
    }

    private void waitForBroadcastIdle() throws Exception {
        runCommand("am wait-for-broadcast-idle");
        Thread.sleep(100); // Just wait a bit to make sure the system isn't too busy...
    }

    private String runCommand(String command) throws Exception {
        return runCommand(command, "", true);
    }

    private String runCommand(String command, String expectedOutputPattern) throws Exception {
        return runCommand(command, expectedOutputPattern, true);
    }

    private String runCommandAndNotMatch(String command, String expectedOutputPattern)
            throws Exception {
        return runCommand(command, expectedOutputPattern, false);
    }

    private String runCommand(String command, String expectedOutputPattern,
            boolean shouldMatch) throws Exception {
        CLog.d("Executing command: " + command);
        final String output = getDevice().executeShellCommand(command);

        CLog.d("Output:\n"
                + "====================\n"
                + output
                + "====================");

        final Pattern pat = Pattern.compile(
                expectedOutputPattern, Pattern.MULTILINE | Pattern.COMMENTS);
        if (pat.matcher(output.trim()).find() != shouldMatch) {
            fail("Output from \"" + command + "\" "
                    + (shouldMatch ? "didn't match" : "unexpectedly matched")
                    + " \"" + expectedOutputPattern + "\"");
        }
        return output;
    }

    private String runCommandAndExtract(String command,
            String startPattern, boolean startInclusive,
            String endPattern, boolean endInclusive) throws Exception {
        final String[] output = runCommand(command).split("\\n");
        final StringBuilder sb = new StringBuilder();

        final Pattern start = Pattern.compile(startPattern, Pattern.COMMENTS);
        final Pattern end = Pattern.compile(endPattern, Pattern.COMMENTS);

        boolean in = false;
        for (String s : output) {
            if (in) {
                if (end.matcher(s.trim()).find()) {
                    if (endInclusive) {
                        sb.append(s);
                        sb.append("\n");
                    }
                    break;
                }
                sb.append(s);
                sb.append("\n");
            } else {
                if (start.matcher(s.trim()).find()) {
                    if (startInclusive) {
                        sb.append(s);
                        sb.append("\n");
                    }
                    continue;
                }
                in = true;
            }
        }

        return sb.toString();
    }

    private void updateConstants(String settings) throws Exception {
        runCommand("settings put global " + APP_BINDING_SETTING + " '" + settings + "'");
    }

    private boolean isSmsCapable() throws Exception {
        String output = runCommand("dumpsys phone");
        if (output.contains("isSmsCapable=true")) {
            CLog.d("Device is SMS capable");
            return true;
        }
        CLog.d("Device is not SMS capable");
        return false;
    }

    private void setSmsApp(String pkg, int userId) throws Exception {
        runCommand("cmd role add-role-holder --user " + userId + " android.app.role.SMS " + pkg);
    }

    private void uninstallTestApps(boolean always) throws Exception {
        if (SKIP_UNINSTALL && !always) {
            return;
        }
        getDevice().uninstallPackage(PACKAGE_A);
        getDevice().uninstallPackage(PACKAGE_B);

        waitForBroadcastIdle();
    }

    private void runWithRetries(int timeoutSeconds, ThrowingRunnable r) throws Throwable {
        final long timeout = System.currentTimeMillis() + timeoutSeconds * 1000;
        Throwable lastThrowable = null;

        int sleep = 200;
        while (System.currentTimeMillis() < timeout) {
            try {
                r.run();
                return;
            } catch (Throwable th) {
                lastThrowable = th;
            }
            Thread.sleep(sleep);
            sleep = Math.min(1000, sleep * 2);
        }
        throw lastThrowable;
    }

    @Before
    public void setUp() throws Exception {
        // Reset to the default setting.
        updateConstants(",");

        uninstallTestApps(true);
    }

    @After
    public void tearDown() throws Exception {
        uninstallTestApps(false);

        // Reset to the default setting.
        updateConstants(",");
    }

    private void installAndCheckBound(String apk, String packageName,
            String serviceClass, int userId) throws Throwable {
        // Install
        installAppAsUser(apk, true, userId);

        // Set as the default app
        setSmsApp(packageName, userId);

        checkBound(packageName, serviceClass, userId);
    }

    private void checkBound(String packageName, String serviceClass, int userId) throws Throwable {
        runWithRetries(DEFAULT_TIMEOUT_SEC, () -> {
            runCommand("dumpsys activity service " + packageName + "/" + serviceClass,
                    Pattern.quote("[" + packageName + "]") + " .* "
                    + Pattern.quote("[" + serviceClass + "]"));
        });

        // This should contain:
        // "conn,0,[Default SMS app],PACKAGE,CLASS,bound,connected"

        // The binding information is propagated asynchronously, so we need a retry here too.
        // (Even though the activity manager said it's already bound.)
        runWithRetries(DEFAULT_TIMEOUT_SEC, () -> {
            runCommand("dumpsys app_binding -s",
                    "^" + Pattern.quote("conn,[Default SMS app]," + userId + "," + packageName + ","
                            + serviceClass + ",bound,connected,"));
        });
    }

    private void installAndCheckNotBound(String apk, String packageName, int userId,
            String expectedErrorPattern) throws Throwable {
        // Install
        installAppAsUser(apk, true, userId);

        // Set as the default app
        setSmsApp(packageName, userId);

        checkNotBoundWithError(packageName, userId, expectedErrorPattern);
    }

    private void checkNotBoundWithError(String packageName, int userId,
            String expectedErrorPattern) throws Throwable {
        // This should contain:
        // "finder,0,[Default SMS app],PACKAGE,null,ERROR-MESSAGE"
        runWithRetries(DEFAULT_TIMEOUT_SEC, () -> {
            runCommand("dumpsys app_binding -s",
                    "^" + Pattern.quote("finder,[Default SMS app]," + userId + ","
                            + packageName + ",null,") + ".*"
                            + Pattern.quote(expectedErrorPattern) + ".*$");
        });
    }

    private void checkPackageNotBound(String packageName, int userId) throws Throwable {
        // This should contain:
        // "finder,0,[Default SMS app],DIFFERENT-PACKAGE,..."
        runWithRetries(DEFAULT_TIMEOUT_SEC, () -> {
            runCommand("dumpsys app_binding -s",
                    "^" + Pattern.quote("finder,[Default SMS app]," + userId + ",")
                            + "(?!" // Negative look ahead
                            + Pattern.quote(packageName + ",")
                            + ")");
        });
    }

    private void assertOomAdjustment(String packageName, String processName, int oomAdj)
            throws Exception {
        final String output = runCommandAndExtract("dumpsys activity -a p " + packageName,
                "\\sProcessRecord\\{.*\\:" + Pattern.quote(processName) + "\\/", false,
                "^\\s*oom:", true);
        /* Example:
ACTIVITY MANAGER RUNNING PROCESSES (dumpsys activity processes)
  All known processes:
  *APP* UID 10196 ProcessRecord{ef7dd8f 29993:com.android.cts.appbinding.app:persistent/u0a196}
    user #0 uid=10196 gids={50196, 20196, 9997}
    mRequiredAbi=arm64-v8a instructionSet=null
    dir=/data/app/com.android.cts.appbinding.app-zvJ1Z44jYKxm-K0HLBRtLA==/base.apk publicDir=/da...
    packageList={com.android.cts.appbinding.app}
    compat={560dpi}
    thread=android.app.IApplicationThread$Stub$Proxy@a5181c
    pid=29993 starting=false
    lastActivityTime=-14s282ms lastPssTime=-14s316ms pssStatType=0 nextPssTime=+5s718ms
    adjSeq=35457 lruSeq=0 lastPss=0.00 lastSwapPss=0.00 lastCachedPss=0.00 lastCachedSwapPss=0.00
    procStateMemTracker: best=4 () / pending state=2 highest=2 1.0x
    cached=false empty=true
    oom: max=1001 curRaw=200 setRaw=200 cur=200 set=200
    mCurSchedGroup=2 setSchedGroup=2 systemNoUi=false trimMemoryLevel=0
    curProcState=4 mRepProcState=4 pssProcState=19 setProcState=4 lastStateTime=-14s282ms
    reportedInteraction=true time=-14s284ms
    startSeq=369
    lastRequestedGc=-14s283ms lastLowMemory=-14s283ms reportLowMemory=false
     Configuration={1.0 ?mcc?mnc [en_US] ldltr sw411dp w411dp h746dp 560dpi nrml long widecg ...
     OverrideConfiguration={0.0 ?mcc?mnc ?localeList ?layoutDir ?swdp ?wdp ?hdp ?density ?lsize ...
     mLastReportedConfiguration={0.0 ?mcc?mnc ?localeList ?layoutDir ?swdp ?wdp ?hdp ?density ...
    Services:
      - ServiceRecord{383eb86 u0 com.android.cts.appbinding.app/.MyService}
    Connected Providers:
      - 54bfc25/com.android.providers.settings/.SettingsProvider->29993:com.android.cts....

  Process LRU list (sorted by oom_adj, 50 total, non-act at 4, non-svc at 4):
    Proc #10: prcp  F/ /BFGS trm: 0 29993:com.android.cts.appbinding.app:persistent/u0a196 (service)
        com.android.cts.appbinding.app/.MyService<=Proc{1332:system/1000}
         */
        final Pattern pat = Pattern.compile("\\soom:\\s.* set=(\\d+)$", Pattern.MULTILINE);
        final Matcher m = pat.matcher(output);
        if (!m.find()) {
            fail("Unable to fild the oom: line for process " + processName);
        }
        final String oom = m.group(1);
        assertEquals("Unexpected oom adjustment:", String.valueOf(oomAdj), oom);
    }

    /**
     * Install APK 1 and make it the default SMS app and make sure the service gets bound.
     */
    @Test
    public void testSimpleBind1() throws Throwable {
        if (!isSmsCapable()) {
            // device not supporting sms. cannot run the test.
            return;
        }

        installAndCheckBound(APK_1, PACKAGE_A, SERVICE_1, USER_SYSTEM);
    }

    /**
     * Install APK 2 and make it the default SMS app and make sure the service gets bound.
     */
    @Test
    public void testSimpleBind2() throws Throwable {
        if (!isSmsCapable()) {
            // device not supporting sms. cannot run the test.
            return;
        }

        installAndCheckBound(APK_2, PACKAGE_A, SERVICE_2, USER_SYSTEM);
    }

    /**
     * Install APK B and make it the default SMS app and make sure the service gets bound.
     */
    @Test
    public void testSimpleBindB() throws Throwable {
        if (!isSmsCapable()) {
            // device not supporting sms. cannot run the test.
            return;
        }

        installAndCheckBound(APK_B, PACKAGE_B, SERVICE_1, USER_SYSTEM);
    }

    /**
     * APK 3 doesn't have a valid service to be bound.
     */
    @Test
    public void testSimpleNotBound3() throws Throwable {
        if (!isSmsCapable()) {
            // device not supporting sms. cannot run the test.
            return;
        }

        installAndCheckNotBound(APK_3, PACKAGE_A, USER_SYSTEM,
                "must be protected with android.permission.BIND_CARRIER_MESSAGING_CLIENT_SERVICE");
    }

    /**
     * APK 4 doesn't have a valid service to be bound.
     */
    @Test
    public void testSimpleNotBound4() throws Throwable {
        if (!isSmsCapable()) {
            // device not supporting sms. cannot run the test.
            return;
        }

        installAndCheckNotBound(APK_4, PACKAGE_A, USER_SYSTEM, "More than one");
    }

    /**
     * APK 5 doesn't have a valid service to be bound.
     */
    @Test
    public void testSimpleNotBound5() throws Throwable {
        if (!isSmsCapable()) {
            // device not supporting sms. cannot run the test.
            return;
        }

        installAndCheckNotBound(APK_5, PACKAGE_A, USER_SYSTEM,
                "Service with android.telephony.action.CARRIER_MESSAGING_CLIENT_SERVICE not found");
    }

    /**
     * APK 6's service doesn't have android:process.
     */
    @Test
    public void testSimpleNotBound6() throws Throwable {
        if (!isSmsCapable()) {
            // device not supporting sms. cannot run the test.
            return;
        }

        installAndCheckNotBound(APK_6, PACKAGE_A, USER_SYSTEM,
                "Service must not run on the main process");
    }

    /**
     * Make sure when the SMS app gets updated, the service still gets bound correctly.
     */
    @Test
    public void testUpgrade() throws Throwable {
        if (!isSmsCapable()) {
            // device not supporting sms. cannot run the test.
            return;
        }

        // Replace existing package without uninstalling.
        installAndCheckBound(APK_1, PACKAGE_A, SERVICE_1, USER_SYSTEM);
        installAndCheckBound(APK_2, PACKAGE_A, SERVICE_2, USER_SYSTEM);
        installAndCheckNotBound(APK_3, PACKAGE_A, USER_SYSTEM,
                "must be protected with android.permission.BIND_CARRIER_MESSAGING_CLIENT_SERVICE");
        installAndCheckBound(APK_1, PACKAGE_A, SERVICE_1, USER_SYSTEM);
        installAndCheckNotBound(APK_4, PACKAGE_A, USER_SYSTEM, "More than one");
    }

    private void enableTargetService(boolean enable) throws DeviceNotAvailableException {
        runDeviceTests(PACKAGE_A, "com.android.cts.appbinding.app.MyEnabler",
                enable ? "enableService" : "disableService");
    }

    /**
     * Make sure the service responds to setComponentEnabled.
     */
    @Test
    public void testServiceEnabledByDefault() throws Throwable {
        if (!isSmsCapable()) {
            // device not supporting sms. cannot run the test.
            return;
        }

        installAndCheckBound(APK_1, PACKAGE_A, SERVICE_1, USER_SYSTEM);

        // Disable the component and now it should be unbound.

        enableTargetService(false);

        Thread.sleep(2); // Technically not needed, but allow the system to handle the broadcast.

        checkNotBoundWithError(PACKAGE_A, USER_SYSTEM,
                "Service with android.telephony.action.CARRIER_MESSAGING_CLIENT_SERVICE not found");

        // Enable the component and now it should be bound.
        enableTargetService(true);

        Thread.sleep(2); // Technically not needed, but allow the system to handle the broadcast.

        checkBound(PACKAGE_A, SERVICE_1, USER_SYSTEM);
    }

    /**
     * Make sure the service responds to setComponentEnabled.
     */
    @Test
    public void testServiceDisabledByDefault() throws Throwable {
        if (!isSmsCapable()) {
            // device not supporting sms. cannot run the test.
            return;
        }

        // The service is disabled by default, so not bound.
        installAndCheckNotBound(APK_7, PACKAGE_A, USER_SYSTEM,
                "Service with android.telephony.action.CARRIER_MESSAGING_CLIENT_SERVICE not found");

        // Enable the component and now it should be bound.
        enableTargetService(true);

        Thread.sleep(2); // Technically not needed, but allow the system to handle the broadcast.

        checkBound(PACKAGE_A, SERVICE_1, USER_SYSTEM);

        // Disable the component and now it should be unbound.

        enableTargetService(false);

        Thread.sleep(2); // Technically not needed, but allow the system to handle the broadcast.

        checkNotBoundWithError(PACKAGE_A, USER_SYSTEM,
                "Service with android.telephony.action.CARRIER_MESSAGING_CLIENT_SERVICE not found");
    }

    /**
     * Make sure when the SMS app is uninstalled, the binding will be gone.
     */
    @Test
    public void testUninstall() throws Throwable {
        if (!isSmsCapable()) {
            // device not supporting sms. cannot run the test.
            return;
        }

        // Replace existing package without uninstalling.
        installAndCheckBound(APK_1, PACKAGE_A, SERVICE_1, USER_SYSTEM);
        getDevice().uninstallPackage(PACKAGE_A);
        checkPackageNotBound(PACKAGE_A, USER_SYSTEM);

        // Try with different APKs, just to make sure.
        installAndCheckBound(APK_B, PACKAGE_B, SERVICE_1, USER_SYSTEM);
        getDevice().uninstallPackage(PACKAGE_B);
        checkPackageNotBound(PACKAGE_B, USER_SYSTEM);

        installAndCheckBound(APK_2, PACKAGE_A, SERVICE_2, USER_SYSTEM);
        getDevice().uninstallPackage(PACKAGE_A);
        checkPackageNotBound(PACKAGE_A, USER_SYSTEM);
    }

    /**
     * Make sure when the SMS app changes, the service still gets bound correctly.
     */
    @Test
    public void testSwitchDefaultApp() throws Throwable {
        if (!isSmsCapable()) {
            // device not supporting sms. cannot run the test.
            return;
        }

        installAndCheckBound(APK_1, PACKAGE_A, SERVICE_1, USER_SYSTEM);
        installAndCheckBound(APK_B, PACKAGE_B, SERVICE_1, USER_SYSTEM);
        installAndCheckBound(APK_2, PACKAGE_A, SERVICE_2, USER_SYSTEM);
    }

    private void assertUserHasNoConnection(int userId) throws Throwable {
        runWithRetries(DEFAULT_TIMEOUT_SEC, () -> {
            runCommandAndNotMatch("dumpsys app_binding -s",
                    "^conn,\\[Default\\sSMS\\sapp\\]," + userId + ",");
        });
    }

    private void assertUserHasNoFinder(int userId) throws Throwable {
        runWithRetries(DEFAULT_TIMEOUT_SEC, () -> {
            runCommandAndNotMatch("dumpsys app_binding -s",
                    "^finder,\\[Default\\sSMS\\sapp\\]," + userId + ",");
        });
    }

    @Test
    public void testSecondaryUser() throws Throwable {
        if (!isSmsCapable()) {
            // device not supporting sms. cannot run the test.
            return;
        }

        if (!getDevice().isMultiUserSupported()) {
            // device do not support multi-user.
            return;
        }

        installAndCheckBound(APK_1, PACKAGE_A, SERVICE_1, USER_SYSTEM);

        final int userId = getDevice().createUser("test-user");
        try {
            getDevice().startUser(userId);

            // Install SMS app on the secondary user.
            installAndCheckBound(APK_B, PACKAGE_B, SERVICE_1, userId);

            // Package A should still be bound on user-0.
            checkBound(PACKAGE_A, SERVICE_1, USER_SYSTEM);

            // Replace the app on the primary user with an invalid one.
            installAndCheckNotBound(APK_3, PACKAGE_A, USER_SYSTEM,
                    "must be protected with android.permission.BIND_CARRIER_MESSAGING_CLIENT_SERVICE");

            // Secondary user should still have a valid connection.
            checkBound(PACKAGE_B, SERVICE_1, userId);

            // Upgrade test: Try with apk 1, and then upgrade to apk 2.
            installAndCheckBound(APK_1, PACKAGE_A, SERVICE_1, userId);
            installAndCheckBound(APK_2, PACKAGE_A, SERVICE_2, userId);

            // Stop the secondary user, now the binding should be gone.
            getDevice().stopUser(userId);

            // Now the connection should be removed.
            assertUserHasNoConnection(userId);

            // Start the secondary user again.
            getDevice().startUser(userId);

            // Now the binding should recover.
            runWithRetries(DEFAULT_TIMEOUT_SEC, () -> {
                checkBound(PACKAGE_A, SERVICE_2, userId);
            });

        } finally {
            getDevice().removeUser(userId);
        }
        assertUserHasNoConnection(userId);
        assertUserHasNoFinder(userId);
    }

    @Test
    public void testCrashAndAutoRebind() throws Throwable {
        if (!isSmsCapable()) {
            // device not supporting sms. cannot run the test.
            return;
        }

        updateConstants(
                "service_reconnect_backoff_sec=5"
                + ",service_reconnect_backoff_increase=2"
                + ",service_reconnect_max_backoff_sec=1000"
                + ",service_stable_connection_threshold_sec=10");

        installAndCheckBound(APK_1, PACKAGE_A, SERVICE_1, USER_SYSTEM);

        // Ensure the expected status.
        runWithRetries(DEFAULT_TIMEOUT_SEC, () -> {
            runCommand("dumpsys app_binding -s",
                    "^conn,\\[Default\\sSMS\\sapp\\],0,.*,bound,connected"
                    + ",\\#con=1,\\#dis=0,\\#died=0,backoff=5000");
        });

        // Let the service crash.
        runCommand("dumpsys activity service " + PACKAGE_A + "/" + SERVICE_1 + " crash");

        // Now the connection disconnected and re-connected, so the counters increase.
        // In this case, because binder-died isn't called, so backoff won't increase.
        runWithRetries(DEFAULT_TIMEOUT_SEC, () -> {
            runCommand("dumpsys app_binding -s",
                    "^conn,\\[Default\\sSMS\\sapp\\],0,.*,bound,connected"
                    + ",\\#con=2,\\#dis=1,\\#died=0,backoff=5000");
        });

        // Force-stop the app.
        runCommand("am force-stop " + PACKAGE_A);

        // Force-stop causes a disconnect and a binder-died. Then it doubles the backoff.
        runWithRetries(DEFAULT_TIMEOUT_SEC, () -> {
            runCommand("dumpsys app_binding -s",
                    "^conn,\\[Default\\sSMS\\sapp\\],0,.*,not-bound,not-connected"
                    + ",\\#con=2,\\#dis=2,\\#died=1,backoff=10000");
        });

        Thread.sleep(5000);

        // It should re-bind.
        runWithRetries(10, () -> {
            runCommand("dumpsys app_binding -s",
                    "^conn,\\[Default\\sSMS\\sapp\\],0,.*,bound,connected"
                            + ",\\#con=3,\\#dis=2,\\#died=1,backoff=10000");
        });

        // Force-stop again.
        runCommand("am force-stop " + PACKAGE_A);

        runWithRetries(10, () -> {
            runCommand("dumpsys app_binding -s",
                    "^conn,\\[Default\\sSMS\\sapp\\],0,.*,not-bound,not-connected"
                            + ",\\#con=3,\\#dis=3,\\#died=2,backoff=20000");
        });

        Thread.sleep(10000);

        runWithRetries(10, () -> {
            runCommand("dumpsys app_binding -s",
                    "^conn,\\[Default\\sSMS\\sapp\\],0,.*,bound,connected"
                            + ",\\#con=4,\\#dis=3,\\#died=2,backoff=20000");
        });

        // If the connection lasts more than service_stable_connection_threshold_sec seconds,
        // the backoff resets.
        Thread.sleep(10000);

        runWithRetries(10, () -> {
            runCommand("dumpsys app_binding -s",
                    "^conn,\\[Default\\sSMS\\sapp\\],0,.*,bound,connected"
                            + ",\\#con=4,\\#dis=3,\\#died=2,backoff=5000");
        });
    }

    /**
     * Test the feature flag.
     */
    @Test
    public void testFeatureDisabled() throws Throwable {
        if (!isSmsCapable()) {
            // device not supporting sms. cannot run the test.
            return;
        }

        installAndCheckBound(APK_1, PACKAGE_A, SERVICE_1, USER_SYSTEM);

        updateConstants("sms_service_enabled=false");

        runWithRetries(DEFAULT_TIMEOUT_SEC, () -> {
            checkNotBoundWithError("null", USER_SYSTEM, "feature disabled");
        });

        updateConstants("sms_service_enabled=true");

        runWithRetries(DEFAULT_TIMEOUT_SEC, () -> {
            checkBound(PACKAGE_A, SERVICE_1, USER_SYSTEM);
        });
    }

    @Test
    public void testOomAdjustment() throws Throwable {
        if (!isSmsCapable()) {
            // device not supporting sms. cannot run the test.
            return;
        }

        installAndCheckBound(APK_1, PACKAGE_A, SERVICE_1, USER_SYSTEM);
        assertOomAdjustment(PACKAGE_A, PACKAGE_A_PROC, 200);
    }
}
