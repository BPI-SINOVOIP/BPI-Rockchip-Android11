/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.game.qualification.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import com.android.annotations.VisibleForTesting;
import com.android.ddmlib.testrunner.RemoteAndroidTestRunner;
import com.android.game.qualification.ApkInfo;
import com.android.game.qualification.ResultData;
import com.android.game.qualification.metric.BaseGameQualificationMetricCollector;
import com.android.game.qualification.proto.ResultDataProto;
import com.android.game.qualification.testtype.GameQualificationHostsideController;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;

import com.google.common.io.ByteStreams;
import com.google.common.io.Files;

import org.junit.Assume;

import java.awt.image.BufferedImage;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.Collection;
import java.util.concurrent.TimeUnit;

import javax.imageio.ImageIO;

/**
 * Performance test designed to be used with {@link GameQualificationHostsideController}
 *
 * Tests must be enumerated with the {@link Test} enum and unlike junit tests, they will be run
 * sequentially.
 */
public class PerformanceTest {
    private static final String AJUR_RUNNER = "androidx.test.runner.AndroidJUnitRunner";
    private static final long DEFAULT_TEST_TIMEOUT_MS = 30 * 60 * 1000L; //30min
    private static final long DEFAULT_MAX_TIMEOUT_TO_OUTPUT_MS = 30 * 60 * 1000L; //30min

    private ApkInfo mApk;
    private String mApkDir;
    private ITestDevice mDevice;
    private Collection<BaseGameQualificationMetricCollector> mCollectors;
    private ITestInvocationListener mListener;
    private File mWorkingDirectory;
    private boolean allTestsPassed = true;

    public interface TestMethod {
        void run(PerformanceTest test) throws Exception;
    }

    public enum Test {
        SETUP("setUp", PerformanceTest::setUp, false),
        RUN("run", PerformanceTest::run, true),
        SCREENSHOT("screenshotTest", PerformanceTest::testScreenshot, false),
        TEARDOWN("tearDown", PerformanceTest::tearDown, false);

        private String mName;
        private TestMethod mMethod;
        private boolean mEnableCollectors;

        Test(String name, TestMethod method, boolean enableCollectors) {
            mName = name;
            mMethod = method;
            mEnableCollectors = enableCollectors;
        }

        public String getName() {
            return mName;
        }

        public TestMethod getMethod() {
            return mMethod;
        }

        public boolean isEnableCollectors() {
            return mEnableCollectors;
        }
    }

    public PerformanceTest(
            ITestDevice device,
            ITestInvocationListener listener,
            Collection<BaseGameQualificationMetricCollector> collectors,
            ApkInfo apk,
            String apkDir,
            File workingDirectory) {
        mApk = apk;
        mApkDir = apkDir;
        mDevice = device;
        mCollectors = collectors;
        mListener = listener;
        mWorkingDirectory = workingDirectory;
    }

    public void failed() {
        this.allTestsPassed = false;
    }

    // BEGIN TESTS

    private void setUp() throws DeviceNotAvailableException, IOException, InterruptedException {
        if (mApk.getScript() != null) {
            String cmd = mApk.getScript();
            CLog.i(
                    "Executing command: " + cmd + "\n"
                            + "Working directory: " + mWorkingDirectory.getPath());
            ProcessBuilder pb = new ProcessBuilder("sh", "-c", cmd);
            pb.environment().put("ANDROID_SERIAL", mDevice.getSerialNumber());
            pb.directory(mWorkingDirectory);
            pb.redirectErrorStream(true);

            Process p = pb.start();
            boolean finished = p.waitFor(30, TimeUnit.MINUTES);
            if (!finished || p.exitValue() != 0) {
                ByteArrayOutputStream os = new ByteArrayOutputStream();
                ByteStreams.copy(p.getInputStream(), os);
                String output = os.toString(StandardCharsets.UTF_8.name());
                if (!finished) {
                    output += "\n***TIMEOUT waiting for script to complete.***";
                    p.destroy();
                }
                fail("Execution of setup script returned non-zero value:\n" + output);
            }
        }

        File apkFile = findApk(mApk.getFileName());
        assertNotNull(
                String.format(
                        "Missing APK.  Unable to find %s in %s.\n",
                        mApk.getFileName(),
                        mApkDir),
                apkFile);
        CLog.i("Installing %s on %s.", apkFile.getName(), mDevice.getSerialNumber());
        mDevice.installPackage(apkFile, true);
    }

    private void run() throws DeviceNotAvailableException {
        Assume.assumeTrue(allTestsPassed);
        // APK Test.
        assertFalse(
                "Unable to unlock device: " + mDevice.getDeviceDescriptor(),
                mDevice.getKeyguardState().isKeyguardShowing());

        File apkFile = findApk(mApk.getFileName());
        assertNotNull(
                String.format(
                        "Missing APK.  Unable to find %s in %s.\n",
                        mApk.getFileName(),
                        mApkDir),
                apkFile);


        CollectingTestListener listener = new CollectingTestListener();
        runDeviceTests(
                GameQualificationHostsideController.PACKAGE,
                GameQualificationHostsideController.CLASS,
                "run[" + mApk.getName() + "]",
                listener);
        ResultDataProto.Result resultData = retrieveResultData();
        for (BaseGameQualificationMetricCollector collector : mCollectors) {
            collector.setDeviceResultData(resultData);
        }
        assertFalse(listener.hasFailedTests());
    }

    private void testScreenshot() throws IOException, DeviceNotAvailableException {
        Assume.assumeTrue(allTestsPassed);
        try (InputStreamSource screenSource = mDevice.getScreenshot()) {
            mListener.testLog(
                    String.format("screenshot-%s", mApk.getName()),
                    LogDataType.PNG,
                    screenSource);
            try (InputStream stream = screenSource.createInputStream()) {
                stream.reset();
                assertFalse(
                        "A screenshot was taken just after metric collection and it was black.",
                        isImageBlack(stream));
            }
        } catch (IOException e) {
            throw new IOException("Failed reading screenshot data:\n" + e.getMessage());
        }
    }

    private void tearDown() throws DeviceNotAvailableException {
        mDevice.uninstallPackage(mApk.getPackageName());
    }

    // END TESTS

    /** Find an apk in the apk-dir directory */
    private File findApk(String filename) {
        File file = new File(mApkDir, filename);
        if (file.exists()) {
            return file;
        }
        // If a default sample app is named Sample.apk, it is outputted to
        // $ANDROID_PRODUCT_OUT/data/app/Sample/Sample.apk.
        file = new File(mApkDir, Files.getNameWithoutExtension(filename) + "/" + filename);
        if (file.exists()) {
            return file;
        }
        return null;
    }

    /** Check if an image is black. */
    @VisibleForTesting
    static boolean isImageBlack(InputStream stream) throws IOException {
        BufferedImage img = ImageIO.read(stream);
        for (int i = 0; i < img.getWidth(); i++) {
            // Only check the middle portion of the image to avoid status bar.
            for (int j = img.getHeight() / 4; j < img.getHeight() * 3 / 4; j++) {
                int color = img.getRGB(i, j);
                // Check if pixel is non-black and not fully transparent.
                if ((color & 0x00ffffff) != 0 && (color >> 24) != 0) {
                    return false;
                }
            }
        }
        return true;
    }

    private ResultDataProto.Result retrieveResultData() throws DeviceNotAvailableException {
        File resultFile = mDevice.pullFileFromExternal(ResultData.RESULT_FILE_LOCATION);

        if (resultFile != null) {
            try (InputStream inputStream = new FileInputStream(resultFile)) {
                return ResultDataProto.Result.parseFrom(inputStream);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
        return null;
    }

    /**
     * Method to run an installed instrumentation package.
     *
     * @param pkgName the name of the package to run.
     * @param testClassName the name of the test class to run.
     * @param testMethodName the name of the method to run.
     */
    private void runDeviceTests(String pkgName, String testClassName, String testMethodName, CollectingTestListener listener)
            throws DeviceNotAvailableException {
        RemoteAndroidTestRunner testRunner =
                new RemoteAndroidTestRunner(pkgName, AJUR_RUNNER, mDevice.getIDevice());

        testRunner.setMethodName(testClassName, testMethodName);

        testRunner.addInstrumentationArg(
                "timeout_msec", Long.toString(DEFAULT_TEST_TIMEOUT_MS));
        testRunner.setMaxTimeout(DEFAULT_MAX_TIMEOUT_TO_OUTPUT_MS, TimeUnit.MILLISECONDS);

        mDevice.runInstrumentationTests(testRunner, listener);
    }
}
