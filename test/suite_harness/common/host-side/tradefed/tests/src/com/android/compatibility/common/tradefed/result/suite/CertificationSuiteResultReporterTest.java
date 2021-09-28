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
package com.android.compatibility.common.tradefed.result.suite;

import static org.junit.Assert.assertTrue;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.util.FileUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;

/** Unit tests for {@link CertificationSuiteResultReporter}. */
@RunWith(JUnit4.class)
public class CertificationSuiteResultReporterTest {
    private CertificationSuiteResultReporter mReporter;
    private IConfiguration mConfiguration;
    private IInvocationContext mContext;
    private CompatibilityBuildHelper mBuildHelper;
    private File mResultDir;
    private File mFakeDir;

    @Before
    public void setUp() throws Exception {
        mFakeDir = FileUtil.createTempDir("result-dir");
        mResultDir = new File(mFakeDir, "android-cts/results");
        mResultDir.mkdirs();

        IBuildInfo info = new BuildInfo();
        info.addBuildAttribute(CompatibilityBuildHelper.ROOT_DIR, mFakeDir.getAbsolutePath());
        info.addBuildAttribute(
                CompatibilityBuildHelper.START_TIME_MS, Long.toString(System.currentTimeMillis()));
        mContext = new InvocationContext();
        mContext.addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, info);

        mBuildHelper =
                new CompatibilityBuildHelper(info) {
                    @Override
                    public String getSuiteName() {
                        return "CTS";
                    }

                    @Override
                    public String getSuiteVersion() {
                        return "version";
                    }

                    @Override
                    public String getSuitePlan() {
                        return "cts";
                    }

                    @Override
                    public String getSuiteBuild() {
                        return "R1";
                    }
                };
    }

    @After
    public void tearDown() throws Exception {
        FileUtil.recursiveDelete(mFakeDir);
    }

    /** Check that the basic overall structure is good an contains all the information. */
    @Test
    public void testSuiteVariant() throws Exception {
        mConfiguration = new Configuration("test", "test");
        mConfiguration.setCommandLine(new String[] {"cts", "-m", "CtsGestureTestCases"});

        mReporter =
                new CertificationSuiteResultReporter() {
                    @Override
                    CompatibilityBuildHelper createBuildHelper() {
                        return mBuildHelper;
                    }
                };
        mReporter.setConfiguration(mConfiguration);

        mReporter.invocationStarted(mContext);
        mReporter.invocationEnded(500L);

        File reportFile = new File(mBuildHelper.getResultDir(), "test_result.xml");
        assertTrue(reportFile.exists());
        String content = FileUtil.readStringFromFile(reportFile);
        assertTrue(content.contains("suite_name=\"CTS\""));
        assertTrue(content.contains("suite_variant=\"CTS\""));
        assertTrue(content.contains("suite_version=\"version\""));
    }

    /** Check that when running cts-on-gsi the variant is changed. */
    @Test
    public void testSuiteVariantGSI() throws Exception {
        mConfiguration = new Configuration("test", "test");
        mConfiguration.setCommandLine(new String[] {"cts-on-gsi"});

        mReporter =
                new CertificationSuiteResultReporter() {
                    @Override
                    CompatibilityBuildHelper createBuildHelper() {
                        return mBuildHelper;
                    }
                };
        mReporter.setConfiguration(mConfiguration);

        mReporter.invocationStarted(mContext);
        mReporter.invocationEnded(500L);

        File reportFile = new File(mBuildHelper.getResultDir(), "test_result.xml");
        assertTrue(reportFile.exists());
        String content = FileUtil.readStringFromFile(reportFile);
        assertTrue(content.contains("suite_name=\"CTS\""));
        assertTrue(content.contains("suite_variant=\"CTS_ON_GSI\""));
        assertTrue(content.contains("suite_version=\"version\""));
    }

    /**
     * For the R release, ensure that CTS-on-GSI still report as VTS for APFE to ingest it properly
     */
    @Test
    public void testSuiteVariantGSI_R_Compatibility() throws Exception {
        mConfiguration = new Configuration("test", "test");
        mConfiguration.setCommandLine(new String[] {"cts-on-gsi"});

        mReporter =
                new CertificationSuiteResultReporter() {
                    @Override
                    CompatibilityBuildHelper createBuildHelper() {
                        return mBuildHelper;
                    }
                };
        OptionSetter setter = new OptionSetter(mReporter);
        setter.setOptionValue("cts-on-gsi-variant", "true");
        mReporter.setConfiguration(mConfiguration);

        mReporter.invocationStarted(mContext);
        mReporter.invocationEnded(500L);

        File reportFile = new File(mBuildHelper.getResultDir(), "test_result.xml");
        assertTrue(reportFile.exists());
        String content = FileUtil.readStringFromFile(reportFile);
        // Suite name is overridden to VTS for the R release
        assertTrue(content.contains("suite_name=\"VTS\""));
        assertTrue(content.contains("suite_variant=\"CTS_ON_GSI\""));
        assertTrue(content.contains("suite_version=\"version\""));
        // Ensure html is created
        assertTrue(new File(mBuildHelper.getResultDir(), "test_result.html").exists());
    }

    /**
     * For the R release, ensure that CTS-on-GSI still report as VTS for APFE to ingest it properly
     */
    @Test
    public void testSuiteVariantGSI_R_Compatibility_ATS() throws Exception {
        mConfiguration = new Configuration("test", "test");
        // ATS renames the config so we need to handle it.
        mConfiguration.setCommandLine(new String[] {"_cts-on-gsi.xml"});

        mReporter =
                new CertificationSuiteResultReporter() {
                    @Override
                    CompatibilityBuildHelper createBuildHelper() {
                        return mBuildHelper;
                    }
                };
        OptionSetter setter = new OptionSetter(mReporter);
        setter.setOptionValue("cts-on-gsi-variant", "true");
        mReporter.setConfiguration(mConfiguration);

        mReporter.invocationStarted(mContext);
        mReporter.invocationEnded(500L);

        File reportFile = new File(mBuildHelper.getResultDir(), "test_result.xml");
        assertTrue(reportFile.exists());
        String content = FileUtil.readStringFromFile(reportFile);
        // Suite name is overridden to VTS for the R release
        assertTrue(content.contains("suite_name=\"VTS\""));
        assertTrue(content.contains("suite_version=\"version\""));
    }
}
