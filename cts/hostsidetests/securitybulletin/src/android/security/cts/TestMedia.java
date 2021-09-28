/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

package android.security.cts;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.compatibility.common.util.CrashUtils;

import android.platform.test.annotations.SecurityTest;
import org.junit.Test;
import org.junit.runner.RunWith;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import static org.junit.Assert.*;
import static org.junit.Assume.*;
import junit.framework.Assert;
import java.util.Arrays;
import java.util.ArrayList;

@RunWith(DeviceJUnit4ClassRunner.class)
public class TestMedia extends SecurityTestCase {


    /******************************************************************************
     * To prevent merge conflicts, add tests for N below this comment, before any
     * existing test methods
     ******************************************************************************/

    /******************************************************************************
     * To prevent merge conflicts, add tests for O below this comment, before any
     * existing test methods
     ******************************************************************************/

    /**
     * b/17769851
     * Vulnerability Behaviour: EXIT_VULNERABLE (113)
     */
    @SecurityTest(minPatchLevel = "2015-12")
    @Test
    public void testPocCVE_2015_6616() throws Exception {
        pocPusher.only64();
        String inputFiles[] = {"cve_2015_6616.mp4"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2015-6616",
                AdbUtils.TMP_PATH + inputFiles[0], inputFiles, AdbUtils.TMP_PATH, getDevice());
    }

    /**
     * b/37239013
     * Vulnerability Behaviour: EXIT_VULNERABLE (113)
     */
    @SecurityTest(minPatchLevel = "2017-07")
    @Test
    public void testPocCVE_2017_0697() throws Exception {
        String inputFiles[] = {"cve_2017_0697.mp4"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2017-0697",
                AdbUtils.TMP_PATH + inputFiles[0], inputFiles, AdbUtils.TMP_PATH, getDevice());
    }

    /**
     * b/127702368
     * Vulnerability Behaviour: EXIT_VULNERABLE (113)
     */
    @SecurityTest(minPatchLevel = "2019-08")
    @Test
    public void testPocCVE_2019_2126() throws Exception {
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2019-2126", null, getDevice());
    }

    /**
     * b/66969349
     * Vulnerability Behaviour: SIGSEGV in media.codec
     */
    @SecurityTest(minPatchLevel = "2018-01")
    @Test
    public void testPocCVE_2017_13180() throws Exception {
        String processPatternStrings[] = {"media\\.codec", "omx@\\d+?\\.\\d+?-service"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2017-13180", null, getDevice(),
                processPatternStrings);
    }

    /**
     * b/111210196
     * Vulnerability Behaviour: EXIT_VULNERABLE (113)
     */
    @SecurityTest(minPatchLevel = "2019-12")
    @Test
    public void testPocCVE_2019_2228() throws Exception {
        String inputFiles[] = {"cve_2019_2228_ipp.mp4"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2019-2228",
                AdbUtils.TMP_PATH + inputFiles[0], inputFiles, AdbUtils.TMP_PATH, getDevice());
    }

    /**
     * b/157650336
     * Vulnerability Behaviour: SIGSEGV in self / EXIT_VULNERABLE (113)
     */
    @SecurityTest(minPatchLevel = "2020-11")
    @Test
    public void testPocCVE_2020_0450() throws Exception {
        AdbUtils.assumeHasNfc(getDevice());
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2020-0450", null, getDevice());
    }

    /**
     * b/156997193
     * Vulnerability Behaviour: SIGABRT in self
     */
    @SecurityTest(minPatchLevel = "2020-11")
    @Test
    public void testPocCVE_2020_0409() throws Exception {
        String signals[] = {CrashUtils.SIGSEGV, CrashUtils.SIGBUS, CrashUtils.SIGABRT};
        String binaryName = "CVE-2020-0409";
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig(binaryName, getDevice());
        testConfig.config = new CrashUtils.Config().setProcessPatterns(binaryName);
        testConfig.config.setSignals(signals);
        AdbUtils.runPocAssertNoCrashesNotVulnerable(testConfig);
    }

    /**
     * b/156999009
     * Vulnerability Behaviour: SIGABRT in self
     */
    @SecurityTest(minPatchLevel = "2020-10")
    @Test
    public void testPocCVE_2020_0408() throws Exception {
        String signals[] = {CrashUtils.SIGSEGV, CrashUtils.SIGBUS, CrashUtils.SIGABRT};
        String binaryName = "CVE-2020-0408";
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig(binaryName, getDevice());
        testConfig.config = new CrashUtils.Config().setProcessPatterns(binaryName);
        testConfig.config.setSignals(signals);
        AdbUtils.runPocAssertNoCrashesNotVulnerable(testConfig);
    }

    /**
     * b/161894517
     * Vulnerability Behaviour: SIGABRT in self
     */
    @SecurityTest(minPatchLevel = "2020-10")
    @Test
    public void testPocCVE_2020_0421() throws Exception {
        String signals[] = {CrashUtils.SIGSEGV, CrashUtils.SIGBUS, CrashUtils.SIGABRT};
        String binaryName = "CVE-2020-0421";
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig(binaryName, getDevice());
        testConfig.config = new CrashUtils.Config().setProcessPatterns(binaryName);
        testConfig.config.setSignals(signals);
        AdbUtils.runPocAssertNoCrashesNotVulnerable(testConfig);
    }

    /**
     * b/132082342
     * Vulnerability Behaviour: SIGSEGV in self
     */
    @SecurityTest(minPatchLevel = "2019-08")
    @Test
    public void testPocCVE_2019_2133() throws Exception {
        AdbUtils.assumeHasNfc(getDevice());
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2019-2133", null, getDevice());
    }

    /**
     * b/132083376
     * Vulnerability Behaviour: SIGSEGV in self
     */
    @SecurityTest(minPatchLevel = "2019-08")
    @Test
    public void testPocCVE_2019_2134() throws Exception {
        AdbUtils.assumeHasNfc(getDevice());
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2019-2134", null, getDevice());
    }

    /**
     * b/31470908
     * Vulnerability Behaviour: SIGSEGV in self
     */
    @SecurityTest(minPatchLevel = "2017-04")
    @Test
    public void testPocCVE_2016_10244() throws Exception {
        String inputFiles[] = {"cve_2016_10244"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2016-10244",
                AdbUtils.TMP_PATH + inputFiles[0], inputFiles, AdbUtils.TMP_PATH, getDevice());
    }

    /**
     * b/27793367
     * Vulnerability Behaviour: SIGSEGV in media.codec
     */
    @SecurityTest(minPatchLevel = "2016-06")
    @Test
    public void testPocCVE_2016_2485() throws Exception {
        String inputFiles[] = {"cve_2016_2485.raw"};
        String processPatternStrings[] = {"media\\.codec", "omx@\\d+?\\.\\d+?-service"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2016-2485",
                AdbUtils.TMP_PATH + inputFiles[0], inputFiles, AdbUtils.TMP_PATH, getDevice(),
                processPatternStrings);
    }

    /**
     * b/141890807
     * Vulnerability Behaviour: EXIT_VULNERABLE (113)
     */
    @SecurityTest(minPatchLevel = "2020-01")
    @Test
    public void testPocCVE_2020_0007() throws Exception {
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2020-0007", null, getDevice());
    }

    /**
     * b/118372692
     * Vulnerability Behaviour: SIGSEGV in self
     */
    @SecurityTest(minPatchLevel = "2019-02")
    @Test
    public void testPocCVE_2019_1988() throws Exception {
        String inputFiles[] = {"cve_2019_1988.mp4"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2019-1988",
                AdbUtils.TMP_PATH + inputFiles[0], inputFiles, AdbUtils.TMP_PATH, getDevice());
    }

    /**
     * b/63522430
     * Vulnerability Behaviour: SIGSEGV in media.codec
     */
    @SecurityTest(minPatchLevel = "2018-01")
    @Test
    public void testPocCVE_2017_0817() throws Exception {
        String processPatternStrings[] = {"media\\.codec", "omx@\\d+?\\.\\d+?-service"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2017-0817", null, getDevice(),
                processPatternStrings);
    }

    /**
     * b/36104177
     * Vulnerability Behaviour: EXIT_VULNERABLE (113)
     */
    @SecurityTest(minPatchLevel = "2017-09")
    @Test
    public void testPocCVE_2017_0670() throws Exception {
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2017-0670", null, getDevice());
    }

    /**
     * b/68159767
     * Vulnerability Behaviour: EXIT_VULNERABLE (113)
     */
    @SecurityTest(minPatchLevel = "2018-02")
    @Test
    public void testPocCVE_2017_13234() throws Exception {
        String inputFiles[] = { "cve_2017_13234.xmf" };
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2017-13234",
                AdbUtils.TMP_PATH + inputFiles[0], inputFiles, AdbUtils.TMP_PATH, getDevice());
    }

    /**
     * b/74122779
     * Vulnerability Behaviour: SIGABRT in audioserver
     */
    @SecurityTest(minPatchLevel = "2018-07")
    @Test
    public void testPocCVE_2018_9428() throws Exception {
        String signals[] = {CrashUtils.SIGSEGV, CrashUtils.SIGBUS, CrashUtils.SIGABRT};
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig("CVE-2018-9428", getDevice());
    }

    /**
     * b/64340921
     * Vulnerability Behaviour: SIGABRT in audioserver
     */
    @SecurityTest(minPatchLevel = "2018-02")
    @Test
    public void testPocCVE_2017_0837() throws Exception {
        String signals[] = {CrashUtils.SIGSEGV, CrashUtils.SIGBUS, CrashUtils.SIGABRT};
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig("CVE-2017-0837", getDevice());
        testConfig.config = new CrashUtils.Config().setProcessPatterns("audioserver");
        testConfig.config.setSignals(signals);
        AdbUtils.runPocAssertNoCrashesNotVulnerable(testConfig);
    }

    /**
     * b/62151041 - Has 4 CVEs filed together
     */
    /** 1. CVE-2017-9047
     * Vulnerability Behaviour: SIGABRT by -fstack-protector
     */
    @Test
    @SecurityTest(minPatchLevel = "2018-09")
    public void testPocCVE_2018_9466_CVE_2017_9047() throws Exception {
        String binaryName = "CVE-2018-9466-CVE-2017-9047";
        String signals[] = {CrashUtils.SIGSEGV, CrashUtils.SIGBUS, CrashUtils.SIGABRT};
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig(binaryName, getDevice());
        testConfig.config = new CrashUtils.Config().setProcessPatterns(binaryName);
        testConfig.config.setSignals(signals);
        AdbUtils.runPocAssertNoCrashesNotVulnerable(testConfig);
    }

    /** 2. CVE-2017-9048
     * Vulnerability Behaviour: SIGABRT by -fstack-protector
     */
    @Test
    @SecurityTest(minPatchLevel = "2018-09")
    public void testPocCVE_2018_9466_CVE_2017_9048() throws Exception {
        String binaryName = "CVE-2018-9466-CVE-2017-9048";
        String signals[] = {CrashUtils.SIGSEGV, CrashUtils.SIGBUS, CrashUtils.SIGABRT};
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig(binaryName, getDevice());
        testConfig.config = new CrashUtils.Config().setProcessPatterns(binaryName);
        testConfig.config.setSignals(signals);
        AdbUtils.runPocAssertNoCrashesNotVulnerable(testConfig);
    }

    /** 3. CVE-2017-9049
     * Vulnerability Behaviour: SIGSEGV in self
     */
    @Test
    @SecurityTest(minPatchLevel = "2018-09")
    public void testPocCVE_2018_9466_CVE_2017_9049() throws Exception {
        String binaryName = "CVE-2018-9466-CVE-2017-9049";
        String inputFiles[] = {"cve_2018_9466_cve_2017_9049.xml"};
        String signals[] = {CrashUtils.SIGSEGV, CrashUtils.SIGBUS, CrashUtils.SIGABRT};
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig(binaryName, getDevice());
        testConfig.config = new CrashUtils.Config().setProcessPatterns(binaryName);
        testConfig.config.setSignals(signals);
        testConfig.arguments = AdbUtils.TMP_PATH + inputFiles[0];
        testConfig.inputFiles = Arrays.asList(inputFiles);
        testConfig.inputFilesDestination  = AdbUtils.TMP_PATH;
        AdbUtils.runPocAssertNoCrashesNotVulnerable(testConfig);
    }

    /** 4. CVE-2017-9050
     * Vulnerability Behaviour: SIGSEGV in self
     */
    @Test
    @SecurityTest(minPatchLevel = "2018-09")
    public void testPocCVE_2018_9466_CVE_2017_9050() throws Exception {
        String binaryName = "CVE-2018-9466-CVE-2017-9049";
        String inputFiles[] = {"cve_2018_9466_cve_2017_9050.xml"};
        String signals[] = {CrashUtils.SIGSEGV, CrashUtils.SIGBUS, CrashUtils.SIGABRT};
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig(binaryName, getDevice());
        testConfig.config = new CrashUtils.Config().setProcessPatterns(binaryName);
        testConfig.config.setSignals(signals);
        testConfig.arguments = AdbUtils.TMP_PATH + inputFiles[0];
        testConfig.inputFiles = Arrays.asList(inputFiles);
        testConfig.inputFilesDestination  = AdbUtils.TMP_PATH;
        AdbUtils.runPocAssertNoCrashesNotVulnerable(testConfig);
    }

    /**
     * b/23247055
     * Vulnerability Behaviour: SIGABRT in self
     */
    @SecurityTest(minPatchLevel = "2015-10")
    @Test
    public void testPocCVE_2015_3873() throws Exception {
        String inputFiles[] = {"cve_2015_3873.mp4"};
        String binaryName = "CVE-2015-3873";
        String signals[] = {CrashUtils.SIGSEGV, CrashUtils.SIGBUS, CrashUtils.SIGABRT};
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig(binaryName, getDevice());
        testConfig.config = new CrashUtils.Config().setProcessPatterns(binaryName);
        testConfig.config.setSignals(signals);
        testConfig.arguments = AdbUtils.TMP_PATH + inputFiles[0];
        testConfig.inputFiles = Arrays.asList(inputFiles);
        testConfig.inputFilesDestination  = AdbUtils.TMP_PATH;
        AdbUtils.runPocAssertNoCrashesNotVulnerable(testConfig);
    }

    /**
     * b/62948670
     * Vulnerability Behaviour: SIGSEGV in media.codec
     */
    @SecurityTest(minPatchLevel = "2017-11")
    @Test
    public void testPocCVE_2017_0840() throws Exception {
        pocPusher.only32();
        String processPatternStrings[] = {"media\\.codec", "omx@\\d+?\\.\\d+?-service"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2017-0840", null, getDevice(),
                processPatternStrings);
    }

    /**
     * b/69065651
     * Vulnerability Behaviour: SIGSEGV in media.codec
     */
    @SecurityTest(minPatchLevel = "2018-02")
    @Test
    public void testPocCVE_2017_13241() throws Exception {
        pocPusher.only32();
        String processPatternStrings[] = {"media\\.codec", "omx@\\d+?\\.\\d+?-service"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2017-13241", null, getDevice(),
                processPatternStrings);
    }

    /**
     * b/111603051
     * Vulnerability Behaviour: SIGSEGV in self
     */
    @SecurityTest(minPatchLevel = "2018-10")
    @Test
    public void testPocCVE_2018_9491() throws Exception {
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2018-9491", null, getDevice());
    }

    /**
     * b/79662501
     * Vulnerability Behaviour: EXIT_VULNERABLE (113)
     */
    @SecurityTest(minPatchLevel = "2018-09")
    @Test
    public void testPocCVE_2018_9472() throws Exception {
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2018-9472", null, getDevice());
    }

    /**
     * b/36554207
     * Vulnerability Behaviour: SIGSEGV in self
     **/
    @SecurityTest(minPatchLevel = "2017-06")
    @Test
    public void testPocCVE_2016_4658() throws Exception {
        String inputFiles[] = {"cve_2016_4658.xml"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2016-4658",
                AdbUtils.TMP_PATH + inputFiles[0] + " \"range(//namespace::*)\"", inputFiles,
                AdbUtils.TMP_PATH, getDevice());
    }

    /**
     * b/36554209
     * Vulnerability Behaviour: SIGSEGV in self
     **/
    @SecurityTest(minPatchLevel = "2017-06")
    @Test
    public void testPocCVE_2016_5131() throws Exception {
        String inputFiles[] = {"cve_2016_5131.xml"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2016-5131",
                AdbUtils.TMP_PATH + inputFiles[0] + " \"name(range-to(///doc))0+0+22\"", inputFiles,
                AdbUtils.TMP_PATH, getDevice());
    }

    /**
     * b/62800140
     * Vulnerability Behaviour: SIGSEGV in self
     */
    @SecurityTest(minPatchLevel = "2017-10")
    @Test
    public void testPocCVE_2017_0814() throws Exception {
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2017-0814", null, getDevice());
    }

    /**
     * b/65540999
     * Vulnerability Behaviour: Assert failure
     **/
    @SecurityTest(minPatchLevel = "2017-11")
    @Test
    public void testPocCVE_2017_0847() throws Exception {
        String cmdOut = AdbUtils.runCommandLine("ps -eo cmd,gid | grep mediametrics", getDevice());
        if (cmdOut.length() > 0) {
            String[] segment = cmdOut.split("\\s+");
            if (segment.length > 1) {
                if (segment[1].trim().equals("0")) {
                    Assert.fail("mediametrics has root group id");
                }
            }
        }
    }

    /**
     * b/112005441
     * Vulnerability Behaviour: EXIT_VULNERABLE (113)
     */
    @SecurityTest(minPatchLevel = "2019-09")
    @Test
    public void testPocCVE_2019_9313() throws Exception {
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2019-9313", null, getDevice());
    }

    /**
     * b/112159345
     * Vulnerability Behaviour: SIGSEGV in self
     **/
    @SecurityTest(minPatchLevel = "2018-01")
    @Test
    public void testPocCVE_2018_9527() throws Exception {
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2018-9527", null, getDevice());
    }

    /******************************************************************************
     * To prevent merge conflicts, add tests for P below this comment, before any
     * existing test methods
     ******************************************************************************/

    /**
     * b/158762825
     * Vulnerability Behaviour: SIGABRT in self
     */
    @SecurityTest(minPatchLevel = "2020-11")
    @Test
    public void testPocCVE_2020_0451() throws Exception {
        assumeFalse(moduleIsPlayManaged("com.google.android.media.swcodec"));
        String inputFiles[] = {"cve_2020_0451.aac"};
        String binaryName = "CVE-2020-0451";
        String signals[] = {CrashUtils.SIGSEGV, CrashUtils.SIGBUS, CrashUtils.SIGABRT};
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig(binaryName, getDevice());
        testConfig.config = new CrashUtils.Config().setProcessPatterns(binaryName);
        testConfig.config.setSignals(signals);
        testConfig.arguments = AdbUtils.TMP_PATH + inputFiles[0];
        testConfig.inputFiles = Arrays.asList(inputFiles);
        testConfig.inputFilesDestination = AdbUtils.TMP_PATH;
        AdbUtils.runPocAssertNoCrashesNotVulnerable(testConfig);
    }

    /**
     * b/112891564
     * Vulnerability Behaviour: SIGSEGV in self (Android P),
     *                          SIGABRT in self (Android Q onward)
     */
    @SecurityTest(minPatchLevel = "2018-11")
    @Test
    public void testPocCVE_2018_9537() throws Exception {
        String binaryName = "CVE-2018-9537";
        String signals[] = {CrashUtils.SIGSEGV, CrashUtils.SIGBUS, CrashUtils.SIGABRT};
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig(binaryName, getDevice());
        // example of check crash to skip:
        // Abort message: 'frameworks/av/media/extractors/mkv/MatroskaExtractor.cpp:548 CHECK(mCluster) failed.'
        testConfig.config = new CrashUtils.Config()
                .setProcessPatterns(binaryName)
                .appendAbortMessageExcludes("CHECK\\(.*?\\)");
        testConfig.config.setSignals(signals);
        AdbUtils.runPocAssertNoCrashesNotVulnerable(testConfig);
    }

    /******************************************************************************
     * To prevent merge conflicts, add tests for Q below this comment, before any
     * existing test methods
     ******************************************************************************/

    /**
     * b/143464314
     * Vulnerability Behaviour: SIGSEGV in self / EXIT_VULNERABLE (113)
     */
    @SecurityTest(minPatchLevel = "2020-10")
    @Test
    public void testPocCVE_2020_0213() throws Exception {
        assumeFalse(moduleIsPlayManaged("com.google.android.media.swcodec"));
        String inputFiles[] = {"cve_2020_0213.hevc", "cve_2020_0213_info.txt"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2020-0213",
            AdbUtils.TMP_PATH + inputFiles[0] + " " + AdbUtils.TMP_PATH + inputFiles[1],
            inputFiles, AdbUtils.TMP_PATH, getDevice());
    }

    /**
     * b/166268541
     * Vulnerability Behaviour: SIGSEGV in media.swcodec
     */
    @SecurityTest(minPatchLevel = "2020-12")
    @Test
    public void testPocCVE_2020_0470() throws Exception {
        String inputFiles[] = {"cve_2020_0470.mp4"};
        String processPatternStrings[] = {"media\\.swcodec"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2020-0470",
                AdbUtils.TMP_PATH + inputFiles[0], inputFiles, AdbUtils.TMP_PATH, getDevice(),
                processPatternStrings);
    }

    /**
     * b/120426980
     * Vulnerability Behaviour: SIGABRT in self
     */
    @SecurityTest(minPatchLevel = "2019-09")
    @Test
    public void testPocCVE_2019_9362() throws Exception {
        String signals[] = {CrashUtils.SIGSEGV, CrashUtils.SIGBUS, CrashUtils.SIGABRT};
        String binaryName = "CVE-2019-9362";
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig(binaryName, getDevice());
        testConfig.config = new CrashUtils.Config().setProcessPatterns(binaryName);
        testConfig.config.setSignals(signals);
        AdbUtils.runPocAssertNoCrashesNotVulnerable(testConfig);
    }

    /**
     * b/112661742
     * Vulnerability Behaviour: SIGABRT in self
     */
    @SecurityTest(minPatchLevel = "2019-09")
    @Test
    public void testPocCVE_2019_9308() throws Exception {
        String inputFiles[] = {"cve_2019_9308.mp4"};
        String binaryName = "CVE-2019-9308";
        String signals[] = {CrashUtils.SIGSEGV, CrashUtils.SIGBUS, CrashUtils.SIGABRT};
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig(binaryName, getDevice());
        testConfig.config = new CrashUtils.Config().setProcessPatterns(binaryName);
        testConfig.config.setSignals(signals);
        testConfig.arguments = AdbUtils.TMP_PATH + inputFiles[0];
        testConfig.inputFiles = Arrays.asList(inputFiles);
        testConfig.inputFilesDestination = AdbUtils.TMP_PATH;
        AdbUtils.runPocAssertNoCrashesNotVulnerable(testConfig);
    }

    /**
     * b/112662995
     * Vulnerability Behaviour: SIGABRT in self
     */
    @SecurityTest(minPatchLevel = "2019-09")
    @Test
    public void testPocCVE_2019_9357() throws Exception {
        String signals[] = {CrashUtils.SIGSEGV, CrashUtils.SIGBUS, CrashUtils.SIGABRT};
        String binaryName = "CVE-2019-9357";
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig(binaryName, getDevice());
        testConfig.config = new CrashUtils.Config().setProcessPatterns(binaryName);
        testConfig.config.setSignals(signals);
        AdbUtils.runPocAssertNoCrashesNotVulnerable(testConfig);
    }

    /**
     * b/109891727
     * Vulnerability Behaviour: SIGSEGV in media.codec
     */
    @SecurityTest(minPatchLevel = "2019-09")
    @Test
    public void testPocCVE_2019_9347() throws Exception {
        pocPusher.only32();
        String processPatternStrings[] = {"media\\.codec", "omx@\\d+?\\.\\d+?-service"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2019-9347", null, getDevice(),
                processPatternStrings);
    }
}
