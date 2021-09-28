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
package com.android.tradefed.cluster;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IDeviceConfiguration;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TextResultReporter;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.StubTargetPreparer;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.MultiMap;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Answers;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link ClusterCommandConfigBuilder}. */
@RunWith(JUnit4.class)
public class ClusterCommandConfigBuilderTest {
    private static final String REQUEST_ID = "request_id";
    private static final String COMMAND_ID = "command_id";
    private static final String TASK_ID = "task_id";
    private static final String COMMAND_LINE = "command_line";
    private static final String ATTEMPT_ID = "attempt_id";
    private static final String DEVICE_SERIAL = "serial";

    @Rule public final MockitoRule mockito = MockitoJUnit.rule();

    private File mWorkDir;
    private ClusterCommand mCommand;
    private TestEnvironment mTestEnvironment;
    private List<TestResource> mTestResources;
    private TestContext mTestContext;

    @Mock(answer = Answers.RETURNS_DEEP_STUBS)
    private IConfiguration mConfig;

    private ClusterCommandConfigBuilder builder;

    @Before
    public void setUp() throws IOException {
        mWorkDir = FileUtil.createTempDir(this.getClass().getSimpleName());
        mCommand = new ClusterCommand(REQUEST_ID, COMMAND_ID, TASK_ID, COMMAND_LINE, ATTEMPT_ID,
                ClusterCommand.RequestType.MANAGED, 0, 0);
        mCommand.setTargetDeviceSerials(List.of(DEVICE_SERIAL));
        mTestEnvironment = new TestEnvironment();
        mTestResources = new ArrayList<>();
        mTestContext = new TestContext();

        builder =
                new ClusterCommandConfigBuilder() {
                    @Override
                    IConfiguration initConfiguration() {
                        return mConfig;
                    }
                };
        builder.setWorkDir(mWorkDir)
                .setClusterCommand(mCommand)
                .setTestEnvironment(mTestEnvironment)
                .setTestResources(mTestResources)
                .setTestContext(mTestContext);
    }

    @After
    public void tearDown() {
        FileUtil.recursiveDelete(mWorkDir);
    }

    @Test
    public void testBuild_commandProperties() throws IOException, ConfigurationException {
        builder.build();
        // command properties and work directory were injected
        verify(mConfig, times(1)).injectOptionValue("cluster:request-id", REQUEST_ID);
        verify(mConfig, times(1)).injectOptionValue("cluster:command-id", COMMAND_ID);
        verify(mConfig, times(1)).injectOptionValue("cluster:attempt-id", ATTEMPT_ID);
        verify(mConfig, times(1)).injectOptionValue("cluster:command-line", COMMAND_LINE);
        verify(mConfig, times(1)).injectOptionValue("cluster:root-dir", mWorkDir.getAbsolutePath());
    }

    @Test
    public void testBuild_targetPreparers() throws IOException, ConfigurationException {
        // Configure a StubTargetPreparer with a single option
        MultiMap<String, String> options = new MultiMap<>();
        options.put("no-test-boolean-option", ""); // will flip value to false
        TradefedConfigObject preparerConfig =
                new TradefedConfigObject(
                        TradefedConfigObject.Type.TARGET_PREPARER,
                        StubTargetPreparer.class.getName(),
                        options);
        mTestEnvironment.addTradefedConfigObject(preparerConfig);

        builder.build();
        // StubTargetPreparer was added to the device configuration
        ArgumentCaptor<List<IDeviceConfiguration>> captor = ArgumentCaptor.forClass(List.class);
        verify(mConfig, times(1)).setDeviceConfigList(captor.capture());
        List<ITargetPreparer> preparers = captor.getValue().get(0).getTargetPreparers();
        assertEquals(1, preparers.size());
        assertThat(preparers.get(0), instanceOf(StubTargetPreparer.class));
        assertFalse(((StubTargetPreparer) preparers.get(0)).getTestBooleanOption());
    }

    @Test
    public void testBuild_resultReporters() throws IOException, ConfigurationException {
        // Configure a TextResultReporter
        TradefedConfigObject reporterConfig =
                new TradefedConfigObject(
                        TradefedConfigObject.Type.RESULT_REPORTER,
                        TextResultReporter.class.getName(),
                        new MultiMap<>());
        mTestEnvironment.addTradefedConfigObject(reporterConfig);

        // Keep track of result reporters
        List<ITestInvocationListener> reporters = new ArrayList<>();
        doReturn(reporters)
                .when(mConfig)
                .getConfigurationObjectList(Configuration.RESULT_REPORTER_TYPE_NAME);

        builder.build();
        // TextResultReporter was added to the configuration
        assertEquals(1, reporters.size());
        assertThat(reporters.get(0), instanceOf(TextResultReporter.class));
    }

    @Test
    public void testBuild_envVars() throws IOException, ConfigurationException {
        mTestEnvironment.addEnvVar("E1", "V1");
        mTestContext.addEnvVars(Map.of("E2", "V2"));

        builder.build();
        // work directory and environment variables from both sources were injected
        verify(mConfig, times(1))
                .injectOptionValue("cluster:env-var", "TF_WORK_DIR", mWorkDir.getAbsolutePath());
        verify(mConfig, times(1)).injectOptionValue("cluster:env-var", "E1", "V1");
        verify(mConfig, times(1)).injectOptionValue("cluster:env-var", "E2", "V2");
    }

    @Test
    public void testBuild_javaOptions() throws IOException, ConfigurationException {
        mTestEnvironment.addJvmOption("jvm_option");
        mTestEnvironment.addJavaProperty("java_property", "java_value");

        builder.build();
        // JVM options and java properties were injected
        verify(mConfig, times(1)).injectOptionValue("cluster:jvm-option", "jvm_option");
        verify(mConfig, times(1))
                .injectOptionValue("cluster:java-property", "java_property", "java_value");
    }

    @Test
    public void testBuild_outputFiles() throws IOException, ConfigurationException {
        mTestEnvironment.setOutputFileUploadUrl("base_url");
        mTestEnvironment.addOutputFilePattern("pattern");

        builder.build();
        // output URL was generated and output file patterns were injected
        verify(mConfig, times(1))
                .injectOptionValue(
                        "cluster:output-file-upload-url", "base_url/command_id/attempt_id/");
        verify(mConfig, times(1)).injectOptionValue("cluster:output-file-pattern", "pattern");
    }

    @Test
    public void testBuild_testResources() throws IOException, ConfigurationException {
        mTestResources.add(new TestResource("N1", "U1"));
        mTestContext.addTestResource(new TestResource("N2", "U2"));

        builder.build();
        // test resources from both sources were injected
        verify(mConfig, times(1)).injectOptionValue("cluster:test-resource", "N1", "U1");
        verify(mConfig, times(1)).injectOptionValue("cluster:test-resource", "N2", "U2");
    }

    @Test
    public void testBuild_extraOptions() throws IOException, ConfigurationException {
        mCommand.getExtraOptions().put("key", "hello");
        mCommand.getExtraOptions().put("key", "world");

        builder.build();
        // extra options with same key were injected
        verify(mConfig, times(1)).injectOptionValue("key", "hello");
        verify(mConfig, times(1)).injectOptionValue("key", "world");
    }
}
